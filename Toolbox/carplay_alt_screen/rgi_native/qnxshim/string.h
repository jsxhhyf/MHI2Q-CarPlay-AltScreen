#ifndef QSHIM_STRING_H
#define QSHIM_STRING_H
#include <stddef.h>
extern void *memcpy(void *d, const void *s, size_t n);
extern void *memmove(void *d, const void *s, size_t n);
extern void *memset(void *s, int c, size_t n);
extern int memcmp(const void *a, const void *b, size_t n);
extern size_t strlen(const char *s);
extern int strcmp(const char *a, const char *b);
extern char *strstr(const char *h, const char *n);
extern char *strcpy(char *d, const char *s);
extern char *strdup(const char *s);
extern char *strerror(int errnum);
#endif
