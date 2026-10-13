#ifndef QSHIM_SIGNAL_H
#define QSHIM_SIGNAL_H
/* ref: bus_init @0x4468..0x4494 builds a sigaction at sp+16: handler +0
 * (SIG_IGN = 1), sa_flags +4, sa_mask +8 (sigemptyset(sp+24)); SIGPIPE = 13.
 * spa_signal_guard_install copies it with ldm {r0-r3} (16 bytes) and strides
 * guard entries by 24 = int + sigaction + sig_atomic_t. */
#include <stddef.h>
typedef int sig_atomic_t;
typedef struct { long __bits[2]; } sigset_t;
typedef struct _qshim_opaque_siginfo siginfo_t;
struct sigaction {
    union {
        void (*_sa_handler)(int);
        void (*_sa_sigaction)(int, siginfo_t *, void *);
    } __sa_un;
    int sa_flags;
    sigset_t sa_mask;
};
#define sa_handler   __sa_un._sa_handler
#define sa_sigaction __sa_un._sa_sigaction
#define SIG_IGN ((void (*)(int))1)
#define SIGPIPE 13
#define QSHIM_SIG_ASSERT(name, c) typedef char qshim_sig_assert_##name[(c) ? 1 : -1]
QSHIM_SIG_ASSERT(sigaction_16, sizeof(struct sigaction) == 16u);
QSHIM_SIG_ASSERT(flags_4, offsetof(struct sigaction, sa_flags) == 4u);
QSHIM_SIG_ASSERT(mask_8, offsetof(struct sigaction, sa_mask) == 8u);
#undef QSHIM_SIG_ASSERT
extern int sigaction(int sig, const struct sigaction *act, struct sigaction *old);
extern int sigemptyset(sigset_t *set);
extern int raise(int sig);
#endif
