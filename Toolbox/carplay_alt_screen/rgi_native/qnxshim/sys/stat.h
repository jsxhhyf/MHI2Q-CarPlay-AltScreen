#ifndef QSHIM_SYS_STAT_H
#define QSHIM_SYS_STAT_H
/* QNX 6.x legacy 72-byte stat, same layout as src/qnxshim/sys/stat.h.
 * ref: ensure_dir @0x1021c stat buffer sp+8 with the next object at sp+80;
 * log_writer_main @0x1878 reads st_size at buffer+8. */
#include <stddef.h>
#include <stdint.h>
typedef uint32_t mode_t;
struct stat {
    uint32_t st_ino, st_ino_hi;
    int32_t  st_size, st_size_hi;   /* +8 */
    uint32_t st_dev, st_rdev, st_uid, st_gid;
    int32_t  st_mtime, st_atime, st_ctime;
    uint32_t st_mode;               /* +44 */
    uint32_t st_nlink;
    int32_t  st_blocksize, st_nblocks, st_blksize, st_blocks, st_blocks_hi;
};
#define QSHIM_ST_ASSERT(name, c) typedef char qshim_st_assert_##name[(c) ? 1 : -1]
QSHIM_ST_ASSERT(size_72, sizeof(struct stat) == 72u);
QSHIM_ST_ASSERT(size_8, offsetof(struct stat, st_size) == 8u);
QSHIM_ST_ASSERT(mode_44, offsetof(struct stat, st_mode) == 44u);
#undef QSHIM_ST_ASSERT
extern int stat(const char *path, struct stat *buf);
extern int fstat(int fd, struct stat *buf);
extern int mkdir(const char *path, mode_t mode);
#define S_ISDIR(m) (((m) & 0170000) == 0040000)
#define S_ISREG(m) (((m) & 0170000) == 0100000)
#endif
