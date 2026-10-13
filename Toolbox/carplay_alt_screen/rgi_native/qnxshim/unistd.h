#ifndef QSHIM_UNISTD_H
#define QSHIM_UNISTD_H
#include <stddef.h>
typedef int ssize_t;   /* QNX _Ssize_t; matches clang %zd on ILP32 */
typedef int pid_t;
extern ssize_t read(int fd, void *buf, size_t n);
extern ssize_t write(int fd, const void *buf, size_t n);
extern int close(int fd);
extern int unlink(const char *path);
extern int symlink(const char *target, const char *path);
extern int access(const char *path, int mode);
extern int fsync(int fd);
extern pid_t getpid(void);
extern int usleep(unsigned int useconds);
extern void _exit(int status);
#define F_OK 0   /* ref: access(path, 0) in log_verbose */
#endif
