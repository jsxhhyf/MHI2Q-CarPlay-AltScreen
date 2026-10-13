#ifndef QSHIM_TIME_H
#define QSHIM_TIME_H
#include <stddef.h>
typedef long time_t;
typedef int clockid_t;
struct timespec { long tv_sec; long tv_nsec; };
struct timeval { long tv_sec; long tv_usec; };   /* ref: SO_SNDTIMEO len 8 @0x35f4 */
/* ref: log_write @0x1f60 passes sp+28 to localtime_r and the next object
 * starts at sp+72, so struct tm is 44 bytes (9 ints, tm_gmtoff, tm_zone). */
struct tm {
    int tm_sec, tm_min, tm_hour, tm_mday, tm_mon, tm_year, tm_wday, tm_yday, tm_isdst;
    long tm_gmtoff;
    const char *tm_zone;
};
#define QSHIM_TM_ASSERT(name, c) typedef char qshim_tm_assert_##name[(c) ? 1 : -1]
QSHIM_TM_ASSERT(tm_44, sizeof(struct tm) == 44u);
#undef QSHIM_TM_ASSERT
#define CLOCK_REALTIME  0   /* ref: log_write @0x1f4c clock_gettime(0, ...) */
#define CLOCK_MONOTONIC 2   /* ref: clock_gettime(2, ...) */
extern int clock_gettime(clockid_t id, struct timespec *ts);
extern struct tm *localtime_r(const time_t *t, struct tm *out);
#endif
