/*
 * Integer division helpers for the clang build of libcarplay_rgi_meta.so.
 *
 * The SDP reference links __aeabi_idiv(__divsi3), __aeabi_idivmod and
 * __aeabi_uldivmod statically from libgcc.a and imports only __aeabi_uidiv /
 * __aeabi_uidivmod from the vehicle libc.so.3.  Nothing proves libc exports the
 * signed or 64-bit helpers, so this build defines them locally (hidden, never
 * exported) and leans only on the proven unsigned 32-bit import.
 *
 * Signatures follow the ARM RTABI: __aeabi_idivmod returns {quot r0, rem r1};
 * __aeabi_uldivmod returns {quot r0:r1, rem r2:r3}.  Nothing here is on a hot
 * path (stb_image header parsing and one call in hook_framework_init).
 */
#include <stdint.h>

#define QRT_HIDDEN __attribute__((visibility("hidden")))

static uint32_t qrt_abs(int32_t v)
{
    return v < 0 ? 0u - (uint32_t)v : (uint32_t)v;
}

QRT_HIDDEN int32_t __aeabi_idiv(int32_t n, int32_t d)
{
    uint32_t q = qrt_abs(n) / qrt_abs(d);   /* -> imported __aeabi_uidiv */
    return (n ^ d) < 0 ? (int32_t)(0u - q) : (int32_t)q;
}

/* A 64-bit return value travels in r0 (low) and r1 (high). */
QRT_HIDDEN uint64_t __aeabi_idivmod(int32_t n, int32_t d)
{
    int32_t q = __aeabi_idiv(n, d);
    uint32_t r = (uint32_t)n - (uint32_t)q * (uint32_t)d;
    return ((uint64_t)r << 32) | (uint32_t)q;
}

/* Restoring shift-subtract division.  carry keeps the result exact when
 * d > 2^63 and the partial remainder's top bit is shifted out.  d == 0 yields
 * q = ~0 and r = n, matching no particular libgcc behaviour; callers never
 * divide by zero. */
QRT_HIDDEN uint64_t qrt_udivmoddi4(uint64_t n, uint64_t d, uint64_t *rem)
{
    uint64_t q = 0, r = 0;
    int i;

    for (i = 63; i >= 0; i--) {
        uint64_t carry = r >> 63;
        r = (r << 1) | ((n >> i) & 1u);
        if (carry || r >= d) {
            r -= d;
            q |= (uint64_t)1 << i;
        }
    }
    if (d == 0)
        r = n;
    *rem = r;
    return q;
}

/* n in r0:r1, d in r2:r3.  qrt_udivmoddi4 takes the remainder pointer as its
 * fifth argument word at [sp]; the remainder itself lives at [sp, #8].  sp
 * stays 8-byte aligned throughout (push 8 + sub 16). */
__attribute__((naked)) QRT_HIDDEN void __aeabi_uldivmod(void)
{
    __asm__ volatile(
        "push   {r4, lr}\n\t"
        "sub    sp, sp, #16\n\t"
        "add    r4, sp, #8\n\t"
        "str    r4, [sp]\n\t"
        "bl     qrt_udivmoddi4\n\t"
        "ldrd   r2, r3, [sp, #8]\n\t"
        "add    sp, sp, #16\n\t"
        "pop    {r4, pc}\n\t");
}
