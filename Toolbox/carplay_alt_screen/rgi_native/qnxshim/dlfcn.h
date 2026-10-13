#ifndef QSHIM_DLFCN_H
#define QSHIM_DLFCN_H
extern void *dlsym(void *handle, const char *name);
/* QNX Neutrino handles, verified for the universal hook against libc.so.3. */
#define RTLD_DEFAULT ((void *)-2L)
#define RTLD_NEXT    ((void *)-3L)
#endif
