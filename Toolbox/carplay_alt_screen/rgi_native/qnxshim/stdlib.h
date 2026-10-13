#ifndef QSHIM_STDLIB_H
#define QSHIM_STDLIB_H
#include <stddef.h>
extern void *malloc(size_t n);
extern void *calloc(size_t n, size_t s);
extern void *realloc(void *p, size_t n);
extern void free(void *p);
extern void abort(void);
#endif
