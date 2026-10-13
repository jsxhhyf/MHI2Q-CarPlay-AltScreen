#ifndef QSHIM_STDIO_H
#define QSHIM_STDIO_H
#include <stddef.h>
#include <stdarg.h>
typedef struct _qshim_opaque_file FILE;
extern int snprintf(char *s, size_t n, const char *fmt, ...);
extern int vsnprintf(char *s, size_t n, const char *fmt, va_list ap);
extern int rename(const char *from, const char *to);
#define EOF (-1)
#endif
