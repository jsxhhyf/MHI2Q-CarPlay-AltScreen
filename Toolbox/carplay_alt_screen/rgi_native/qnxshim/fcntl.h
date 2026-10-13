#ifndef QSHIM_FCNTL_H
#define QSHIM_FCNTL_H
/* ref: open() flags 0x301 = WRONLY|CREAT|TRUNC, 0x109 = WRONLY|CREAT|APPEND,
 * 0 = RDONLY (log_writer_main @0x1994/@0x19bc, hook_process_is_dio_manager). */
#define O_RDONLY 0
#define O_WRONLY 1
#define O_APPEND 0x0008
#define O_CREAT  0x0100
#define O_TRUNC  0x0200
extern int open(const char *path, int oflag, ...);
#endif
