#ifndef QSHIM_ERRNO_H
#define QSHIM_ERRNO_H
extern int *__get_errno_ptr(void);
#define errno (*__get_errno_ptr())
#define EINTR      4    /* ref: read_all/write_all cmp #4 after __get_errno_ptr */
#define EBUSY      16   /* ref: spa_signal_guard_install errno store */
#define EEXIST     17   /* ref: ensure_dir cmp #17 */
#define EINVAL     22   /* ref: spa_signal_guard_install errno store */
#define ENOSPC     28   /* ref: spa_signal_guard_install errno store */
#define ENOTCONN   257  /* ref: send_frame @0x2eb8 movw #257 (Linux is 57) */
#define ETIMEDOUT  260  /* ref: hook_framework_shutdown cmp #260 */
#endif
