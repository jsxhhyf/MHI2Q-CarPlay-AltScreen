#!/bin/sh
# Build libcarplay_rgi_meta.so without the QNX 6.5 SDP.
#
# Debian clang 19 + lld, -nostdinc against rgi_native/qnxshim (every layout and
# constant there is checked against the SDP-built reference), and stub DSOs
# that only exist to give the output the reference's DT_NEEDED and symbol
# versions.  libc is resolved at run time from dio_manager's global scope,
# exactly as in the reference, which has no libc DT_NEEDED either.
#
# Usage (inside altscreen-clang19:local with the project mounted at /work):
#   sh /work/Tools/build_rgi_meta_clang.sh [OUT]
set -eu
export LC_ALL=C

PROJECT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SRC="$PROJECT/Toolbox/carplay_alt_screen/rgi_native/src"
SHIM="$PROJECT/Toolbox/carplay_alt_screen/rgi_native/qnxshim"
QNXRT="$PROJECT/Toolbox/carplay_alt_screen/rgi_native/qnxrt"
OUT="${1:-$PROJECT/Toolbox/carplay_alt_screen/dev-build/rgi-clang}"
STUBS="$OUT/stubs"
TARGET=armv7-linux-gnueabi

# Same source list and language flags as Tools/build_rgi_qnx.sh (SDP build).
SRCS="framework/logging.c framework/state_trace.c framework/signal_guard.c framework/bus.c framework/iap2_protocol.c framework/hook_framework.c routeguidance/rgd_tlv.c routeguidance/rgd_identify.c routeguidance/rgd_hook.c coverart/jpeg_safety.c coverart/coverart_stream.c coverart/coverart_hook.c main.c"
CF="--target=$TARGET -march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=softfp -marm"
CF="$CF -fPIC -O2 -std=gnu99 -D__QNX__ -fvisibility=hidden -fdata-sections -ffunction-sections"
CF="$CF -fno-stack-protector -fno-exceptions -fno-asynchronous-unwind-tables"
# memcmp()==0 would otherwise become bcmp, which the reference never imports.
CF="$CF -fno-builtin-bcmp"
CF="$CF -nostdinc -isystem $(clang -print-file-name=include) -I$SHIM -I$SRC"
LF="-fuse-ld=lld -nostdlib -shared -Wl,--gc-sections,--build-id=none,--hash-style=sysv"
LF="$LF -Wl,--no-rosegment,-z,norelro,-z,max-page-size=4096 -L$STUBS"

rm -rf "$OUT"
mkdir -p "$OUT" "$STUBS"

# Stub DSOs.  QNX 6.5 libsocket.so.3 carries its socket calls under version
# node libsocket.so.2 (reference .gnu.version_r); inet_addr is in libc.
cat > "$OUT/socket_stub.c" <<'EOF'
int socket(void) { return 0; } int connect(void) { return 0; }
int setsockopt(void) { return 0; } int shutdown(void) { return 0; }
int send(void) { return 0; } int recv(void) { return 0; }
EOF
printf 'libsocket.so.2 { global: socket; connect; setsockopt; shutdown; send; recv; local: *; };\n' > "$OUT/socket_stub.map"
echo 'int compress2(void) { return 0; } int compressBound(void) { return 0; } int crc32(void) { return 0; }' > "$OUT/z_stub.c"
clang --target=$TARGET -fPIC -c "$OUT/socket_stub.c" -o "$OUT/socket_stub.o"
clang --target=$TARGET -fPIC -c "$OUT/z_stub.c" -o "$OUT/z_stub.o"
clang --target=$TARGET -fuse-ld=lld -nostdlib -shared -Wl,-soname,libsocket.so.3 \
      -Wl,--version-script,"$OUT/socket_stub.map" "$OUT/socket_stub.o" -o "$STUBS/libsocket.so.3"
clang --target=$TARGET -fuse-ld=lld -nostdlib -shared -Wl,-soname,libz.so.2 \
      "$OUT/z_stub.o" -o "$STUBS/libz.so.2"

OBJS=""
for c in $SRCS; do
  o="$OUT/obj/${c%.c}.o"
  mkdir -p "$(dirname "$o")"
  clang $CF -c "$SRC/$c" -o "$o"
  OBJS="$OBJS $o"
done

# Signed and 64-bit division helpers: static, as libgcc.a is in the reference.
clang $CF -c "$QNXRT/aeabi_div.c" -o "$OUT/obj/aeabi_div.o"
OBJS="$OBJS $OUT/obj/aeabi_div.o"

SO="$OUT/libcarplay_rgi_meta.so"
clang --target=$TARGET $LF -Wl,--version-script,"$SRC/carplay_hook.exports.map" \
      $OBJS -Wl,-l:libz.so.2 -Wl,-l:libsocket.so.3 -o "$SO"

# The QNX toolchain stamps EABI5 + EF_ARM_HASENTRY; match the reference.
printf '\002\000\000\005' | dd of="$SO" bs=1 seek=36 conv=notrunc status=none

python3 "$PROJECT/Toolbox/carplay_alt_screen/src/tools/validate_qnx_load_layout.py" "$SO" > "$OUT/QNX_LOAD_LAYOUT.json"
echo "RGI_META_CLANG_BUILD=PASS $SO"
