#ifndef QSHIM_ASSERT_H
#define QSHIM_ASSERT_H
/* ref: stbi__jpeg_huff_decode @0x108cc -> __assert(expr, file, line, func). */
extern void __assert(const char *expr, const char *file, unsigned line, const char *func);
#ifdef NDEBUG
#define assert(e) ((void)0)
#else
#define assert(e) ((e) ? (void)0 : __assert(#e, __FILE__, __LINE__, __func__))
#endif
#endif
