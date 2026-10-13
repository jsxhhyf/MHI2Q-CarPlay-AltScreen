#ifndef QSHIM_PTHREAD_H
#define QSHIM_PTHREAD_H
#include <stddef.h>
#include <time.h>
/* QNX sync objects.  Sizes come from .symtab object sizes and the initializers
 * from the initial .data words of the reference's static locks:
 *   g_lock / g_inject_lock ...      size 8   80000000 ffffffff
 *   g_cond / g_inject_cond ...      size 8   fffffffb ffffffff
 *   g_fw_lock_once / g_modules_once size 12  00000000 80000000 ffffffff
 *   g_htable_rw                     size 48  0 0 0 0 0 80000000 ffffffff
 *                                   fffffffb ffffffff fffffffb ffffffff fffffffe */
typedef int pthread_t;
typedef struct { int __count; unsigned __owner; } pthread_mutex_t;
typedef struct { int __count; unsigned __owner; } pthread_cond_t;
typedef struct { int __once; pthread_mutex_t __mutex; } pthread_once_t;
typedef struct {
    int __active, __spare, __blockedwriters, __blockedreaders, __heavy;
    pthread_mutex_t __lock;
    pthread_cond_t __rcond, __wcond;
    unsigned __owner;
} pthread_rwlock_t;
typedef struct _qshim_opaque_attr pthread_attr_t;
typedef struct _qshim_opaque_mutexattr pthread_mutexattr_t;
typedef struct _qshim_opaque_condattr pthread_condattr_t;

#define PTHREAD_MUTEX_INITIALIZER  { (int)0x80000000, 0xffffffffu }
#define PTHREAD_COND_INITIALIZER   { (int)0xfffffffb, 0xffffffffu }
#define PTHREAD_ONCE_INIT          { 0, PTHREAD_MUTEX_INITIALIZER }
#define PTHREAD_RWLOCK_INITIALIZER { 0, 0, 0, 0, 0, PTHREAD_MUTEX_INITIALIZER, \
        PTHREAD_COND_INITIALIZER, PTHREAD_COND_INITIALIZER, 0xfffffffeu }

#define QSHIM_PT_ASSERT(name, c) typedef char qshim_pt_assert_##name[(c) ? 1 : -1]
QSHIM_PT_ASSERT(mutex_8, sizeof(pthread_mutex_t) == 8u);
QSHIM_PT_ASSERT(cond_8, sizeof(pthread_cond_t) == 8u);
QSHIM_PT_ASSERT(once_12, sizeof(pthread_once_t) == 12u);
QSHIM_PT_ASSERT(rwlock_48, sizeof(pthread_rwlock_t) == 48u);
QSHIM_PT_ASSERT(rwlock_owner_44, offsetof(pthread_rwlock_t, __owner) == 44u);
#undef QSHIM_PT_ASSERT

extern int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                          void *(*entry)(void *), void *arg);
extern int pthread_join(pthread_t thread, void **value);
extern int pthread_once(pthread_once_t *once, void (*fn)(void));
extern int pthread_mutex_init(pthread_mutex_t *m, const pthread_mutexattr_t *a);
extern int pthread_mutex_lock(pthread_mutex_t *m);
extern int pthread_mutex_unlock(pthread_mutex_t *m);
extern int pthread_cond_init(pthread_cond_t *c, const pthread_condattr_t *a);
extern int pthread_cond_wait(pthread_cond_t *c, pthread_mutex_t *m);
extern int pthread_cond_timedwait(pthread_cond_t *c, pthread_mutex_t *m,
                                  const struct timespec *abstime);
extern int pthread_cond_signal(pthread_cond_t *c);
extern int pthread_cond_broadcast(pthread_cond_t *c);
extern int pthread_rwlock_rdlock(pthread_rwlock_t *l);
extern int pthread_rwlock_wrlock(pthread_rwlock_t *l);
extern int pthread_rwlock_unlock(pthread_rwlock_t *l);
#endif
