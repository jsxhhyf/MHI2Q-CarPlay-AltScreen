# QNX 6.5 ARM shim headers for the clang build of libcarplay_rgi_meta.so

`Tools/build_rgi_meta_clang.sh` compiles `rgi_native/src` with Debian clang 19
and `-nostdinc`, using only these headers plus clang's freestanding ones
(stdint, stddef, stdarg, stdbool, limits).  No QNX SDP is involved.

Every layout and constant here is taken from the SDP-built reference
`rgi_meta/libcarplay_rgi_meta.so` (GCC 8.5, full .symtab), never from Linux or
memory.  The `ref:` comments name the evidence: a symbol size, initial .data
words, or an instruction address in that binary.  Linux values are wrong for
several of them (ENOTCONN is 257, sockaddr_in carries sin_len, O_CREAT is
0x100), so do not "fix" a value without new target evidence.

Only what rgi_native uses is declared; an unknown name fails the compile
instead of silently picking a host value.

`../qnxrt/aeabi_div.c` supplies `__aeabi_idiv`, `__aeabi_idivmod` and
`__aeabi_uldivmod` locally.  The reference links them statically from
libgcc.a; only `__aeabi_uidiv`/`__aeabi_uidivmod` are proven libc.so.3 exports.

Checks (all inside `altscreen-clang19:local`, project at `/work`):

    sh /work/Tools/build_rgi_meta_clang.sh
    python3 -I /work/Tools/compare_rgi_meta_abi.py \
        /work/Toolbox/carplay_alt_screen/rgi_meta/libcarplay_rgi_meta.so \
        /work/Toolbox/carplay_alt_screen/dev-build/rgi-clang/libcarplay_rgi_meta.so
    sh /work/Tools/test_qnxrt_aeabi.sh   # needs qemu-user (altscreen-clang19-qemu:local)

The comparison checks exports, imports and symbol versions, DT_NEEDED, ARM ABI
attributes and the size of every named data object (`g_fw` 0x570, `g_rgd`
0x15498 embed all locks); every tolerated difference is listed with its reason
in the script.
