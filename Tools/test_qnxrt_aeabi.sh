#!/bin/sh
# Run rgi_native/qnxrt/aeabi_div.c on ARM (qemu-arm) against Python-computed
# vectors.  Needs clang 19 + lld + qemu-user + python3, e.g. inside
# altscreen-clang19-qemu:local with the project mounted at /work:
#   sh /work/Tools/test_qnxrt_aeabi.sh
set -eu
export LC_ALL=C

PROJECT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
QNXRT="$PROJECT/Toolbox/carplay_alt_screen/rgi_native/qnxrt"
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT

python3 -I - "$OUT/aeabi_vectors.h" <<'PY'
import random, sys
rnd = random.Random(0x5200)
M32, M64 = (1 << 32) - 1, (1 << 64) - 1
I32_MIN, I32_MAX = -(1 << 31), (1 << 31) - 1

def s32(v):
    v &= M32
    return v - (1 << 32) if v >> 31 else v

def cdiv(n, d):
    """C99 truncating division for the signed 32-bit vectors."""
    if n == I32_MIN and d == -1:
        return I32_MIN, 0          # wraps on ARM (libgcc and this helper)
    q = abs(n) // abs(d)
    q = -q if (n < 0) != (d < 0) else q
    return q, n - q * d

edge32 = [0, 1, -1, 2, -2, 3, 7, -7, 10, 1000000, I32_MAX, I32_MIN, I32_MIN + 1,
          0x40000000, -0x40000000, 0x7fff, -0x8000]
pairs = [(n, d) for n in edge32 for d in edge32 if d != 0]
pairs += [(s32(rnd.getrandbits(32)), s32(rnd.getrandbits(rnd.choice((4, 12, 20, 32))) or 1))
          for _ in range(3000)]
pairs = [(n, d) for n, d in pairs if d != 0]

edge64 = [0, 1, 2, 3, 10, 1000, 1000000, 1000000000, M32, 1 << 32, (1 << 32) + 1,
          (1 << 63) - 1, 1 << 63, (1 << 63) + 1, M64 - 1, M64, 0x123456789abcdef0]
upairs = [(n, d) for n in edge64 for d in edge64 if d != 0]
upairs += [(rnd.getrandbits(64), rnd.getrandbits(rnd.choice((8, 31, 32, 33, 48, 63, 64))) or 1)
           for _ in range(3000)]

with open(sys.argv[1], 'w') as f:
    f.write('static const struct { int32_t n, d, q, r; } s32_vec[] = {\n')
    for n, d in pairs:
        q, r = cdiv(n, d)
        # Hex bit patterns avoid the INT32_MIN literal pitfall.
        f.write('  {(int32_t)%#xu, (int32_t)%#xu, (int32_t)%#xu, (int32_t)%#xu},\n' % (
            n & M32, d & M32, q & M32, r & M32))
    f.write('};\nstatic const struct { uint64_t n, d, q, r; } u64_vec[] = {\n')
    for n, d in upairs:
        f.write('  {%#xull, %#xull, %#xull, %#xull},\n' % (n, d, n // d, n % d))
    f.write('};\n')
print('vectors: %d signed32, %d unsigned64' % (len(pairs), len(upairs)))
PY

# Same code-generation flags as the module build (build_rgi_meta_clang.sh).
CF="--target=armv7-linux-gnueabi -march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=softfp -marm"
CF="$CF -O2 -std=gnu99 -fvisibility=hidden -ffreestanding -fno-builtin -fno-stack-protector"
CF="$CF -fno-exceptions -fno-asynchronous-unwind-tables -I$OUT"
clang $CF -c "$QNXRT/aeabi_div.c" -o "$OUT/aeabi_div.o"
clang $CF -c "$QNXRT/test_aeabi_div.c" -o "$OUT/test.o"
clang --target=armv7-linux-gnueabi -fuse-ld=lld -nostdlib -static -Wl,-e,_start \
      "$OUT/test.o" "$OUT/aeabi_div.o" -o "$OUT/test_aeabi_div"

# The helpers must be the ones under test, never another implementation.
for sym in __aeabi_idiv __aeabi_idivmod __aeabi_uldivmod; do
  llvm-nm-19 "$OUT/aeabi_div.o" | grep -q " T $sym\$" || { echo "QNXRT_AEABI_TEST=FAIL $sym not defined"; exit 1; }
done
# The test must really call them through the RTABI, not inline the math.
for sym in __aeabi_idiv __aeabi_idivmod __aeabi_uldivmod; do
  llvm-nm-19 "$OUT/test.o" | grep -q " U $sym\$" || { echo "QNXRT_AEABI_TEST=FAIL test does not call $sym"; exit 1; }
done
# qrt_udivmoddi4 must not recurse into __aeabi_uldivmod.
if llvm-objdump-19 -d --disassemble-symbols=qrt_udivmoddi4 "$OUT/aeabi_div.o" | grep -q '__aeabi'; then
  echo "QNXRT_AEABI_TEST=FAIL qrt_udivmoddi4 calls a runtime helper"; exit 1
fi

if qemu-arm "$OUT/test_aeabi_div"; then
  echo "QNXRT_AEABI_TEST=PASS"
else
  echo "QNXRT_AEABI_TEST=FAIL ($? failures)"; exit 1
fi
