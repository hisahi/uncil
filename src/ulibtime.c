/*******************************************************************************
 
Uncil -- builtin time library impl

Copyright (c) 2021-2023 Sampo Hippeläinen (hisahi)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*******************************************************************************/

#include <stdio.h>

#define UNCIL_DEFINES

#include "uctype.h"
#include "uncil.h"
#include "uosdef.h"
#include "uxprintf.h"

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#if __STRICT_ANSI__
#undef UNCIL_IS_POSIX
#define UNCIL_IS_POSIX 0
#endif

#if UNCIL_C99
#include <tgmath.h>
#else
#include <math.h>
#endif
#include <time.h>

#if UNCIL_IS_POSIX || UNCIL_IS_WINDOWS
#define UNIXTIME 1
#endif

struct unc_tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
    long tm_usec;
};

INLINE int rangecheck(Unc_Int x, Unc_Int a, Unc_Int b) {
    return x < a || x > b;
}

static Unc_RetVal uncl_time_tm_fromobj(Unc_View *w, Unc_Value *o,
                                           struct unc_tm *tm) {
    Unc_Value v = UNC_BLANK;
    Unc_Int ui;
    int e;

    if ((e = unc_getattrc(w, o, "second", &v))) goto fromobj_fail;
    if (unc_getint(w, &v, &ui)) {
        e = unc_throwexc(w, "type", "datetime second must be an integer");
        goto fromobj_fail;
    }
    if ((e = rangecheck(ui, 0, 61))) {
        e = unc_throwexc(w, "type", "datetime second out of range");
        goto fromobj_fail;
    }
    tm->tm_sec = (int)ui;

    if ((e = unc_getattrc(w, o, "minute", &v))) goto fromobj_fail;
    if (unc_getint(w, &v, &ui)) {
        e = unc_throwexc(w, "type", "datetime minute must be an integer");
        goto fromobj_fail;
    }
    if ((e = rangecheck(ui, 0, 59))) {
        e = unc_throwexc(w, "type", "datetime minute out of range");
        goto fromobj_fail;
    }
    tm->tm_min = (int)ui;

    if ((e = unc_getattrc(w, o, "hour", &v))) goto fromobj_fail;
    if (unc_getint(w, &v, &ui)) {
        e = unc_throwexc(w, "type", "datetime hour must be an integer");
        goto fromobj_fail;
    }
    if ((e = rangecheck(ui, 0, 23))) {
        e = unc_throwexc(w, "type", "datetime hour out of range");
        goto fromobj_fail;
    }
    tm->tm_hour = (int)ui;

    if ((e = unc_getattrc(w, o, "day", &v))) goto fromobj_fail;
    if (unc_getint(w, &v, &ui)) {
        e = unc_throwexc(w, "type", "datetime day must be an integer");
        goto fromobj_fail;
    }
    if ((e = rangecheck(ui, 1, 31))) {
        e = unc_throwexc(w, "type", "datetime day out of range");
        goto fromobj_fail;
    }
    tm->tm_mday = (int)ui;

    if ((e = unc_getattrc(w, o, "month", &v))) goto fromobj_fail;
    if (unc_getint(w, &v, &ui)) {
        e = unc_throwexc(w, "type", "datetime month must be an integer");
        goto fromobj_fail;
    }
    if ((e = rangecheck(ui, 1, 12))) {
        e = unc_throwexc(w, "type", "datetime month out of range");
        goto fromobj_fail;
    }
    tm->tm_mon = (int)ui - 1;

    if ((e = unc_getattrc(w, o, "year", &v))) goto fromobj_fail;
    if (unc_getint(w, &v, &ui)) {
        e = unc_throwexc(w, "type", "datetime year must be an integer");
        goto fromobj_fail;
    }
    if ((e = rangecheck(ui, 1582, INT_MAX))) {
        e = unc_throwexc(w, "type", "datetime year out of range");
        goto fromobj_fail;
    }
    tm->tm_year = (int)(ui - 1900);

    if ((e = unc_getattrc(w, o, "dst", &v))) goto fromobj_fail;
    e = unc_getbool(w, &v, -1);
    if (UNCIL_IS_ERR_CMP(e)) goto fromobj_fail;
    tm->tm_isdst = e;
    e = 0;

    if ((e = unc_getattrc(w, o, "us", &v))) goto fromobj_fail;
    if (unc_getint(w, &v, &ui)) {
        e = unc_throwexc(w, "type", "datetime us must be an integer");
        goto fromobj_fail;
    }
    if ((e = rangecheck(ui, 0, 999999))) {
        e = unc_throwexc(w, "type", "datetime us out of range");
        goto fromobj_fail;
    }
    tm->tm_usec = (long)ui;

fromobj_fail:
    VCLEAR(w, &v);
    return e;
}

static Unc_RetVal uncl_time_tm_toobj(Unc_View *w, Unc_Value *o,
                                     Unc_Value *p, struct unc_tm *tm) {
    Unc_Value v = UNC_BLANK;
    int e;
    e = unc_newobject(w, o, p);
    if (e) goto toobj_fail;

    unc_setint(w, &v, tm->tm_sec);
    e = unc_setattrc(w, o, "second", &v);
    if (e) goto toobj_fail;

    unc_setint(w, &v, tm->tm_min);
    e = unc_setattrc(w, o, "minute", &v);
    if (e) goto toobj_fail;

    unc_setint(w, &v, tm->tm_hour);
    e = unc_setattrc(w, o, "hour", &v);
    if (e) goto toobj_fail;

    unc_setint(w, &v, tm->tm_mday);
    e = unc_setattrc(w, o, "day", &v);
    if (e) goto toobj_fail;

    unc_setint(w, &v, tm->tm_mon + 1);
    e = unc_setattrc(w, o, "month", &v);
    if (e) goto toobj_fail;

    unc_setint(w, &v, tm->tm_year + 1900L);
    e = unc_setattrc(w, o, "year", &v);
    if (e) goto toobj_fail;

    unc_setint(w, &v, tm->tm_wday ? tm->tm_wday : 7);
    e = unc_setattrc(w, o, "weekday", &v);
    if (e) goto toobj_fail;

    unc_setint(w, &v, tm->tm_yday + 1);
    e = unc_setattrc(w, o, "yearday", &v);
    if (e) goto toobj_fail;

    if (tm->tm_isdst < 0)
        unc_setnull(w, &v);
    else
        unc_setbool(w, &v, tm->tm_isdst);
    e = unc_setattrc(w, o, "dst", &v);
    if (e) goto toobj_fail;
    unc_setint(w, &v, tm->tm_usec);
    e = unc_setattrc(w, o, "us", &v);
    if (e) goto toobj_fail;
toobj_fail:
    VCLEAR(w, &v);
    return e;
}

static void unc0_utm2tm(struct tm *time, struct unc_tm *utime) {
    time->tm_sec = utime->tm_sec;  
    time->tm_min = utime->tm_min;  
    time->tm_hour = utime->tm_hour; 
    time->tm_mday = utime->tm_mday; 
    time->tm_mon = utime->tm_mon;  
    time->tm_year = utime->tm_year; 
    time->tm_wday = utime->tm_wday; 
    time->tm_yday = utime->tm_yday; 
    time->tm_isdst = utime->tm_isdst;
}

static void unc0_tm2utm(struct tm *time, struct unc_tm *utime, long usec) {
    utime->tm_sec = time->tm_sec;  
    utime->tm_min = time->tm_min;  
    utime->tm_hour = time->tm_hour; 
    utime->tm_mday = time->tm_mday; 
    utime->tm_mon = time->tm_mon;  
    utime->tm_year = time->tm_year; 
    utime->tm_wday = time->tm_wday; 
    utime->tm_yday = time->tm_yday; 
    utime->tm_isdst = time->tm_isdst;
    utime->tm_usec = usec;
}

#define UNCIL_TZ_UTC 0
#define UNCIL_TZ_LOCAL 1
#define UNCIL_TZ_EXTERN 2

typedef struct Unc_Timezone {
    int type;
    long offset;
    long offsetdst;
    char *name;
    char *namedst;
    int nalloc;
} Unc_Timezone;

int uncl_time_tz_free(Unc_View *w, size_t n, void *data) {
    Unc_Timezone *tz = data;
    if (tz->nalloc) {
        unc_mfree(w, tz->name);
        if (tz->name != tz->namedst) unc_mfree(w, tz->namedst);
    }
    return 0;
}

static Unc_RetVal uncl_time_maketz(Unc_View *w, Unc_Value *v,
                                Unc_Value *pr, Unc_Timezone **tz) {
    return unc_newopaque(w, v, pr, sizeof(Unc_Timezone), (void **)tz,
                        &uncl_time_tz_free, 0, NULL, 0, NULL);
}

Unc_RetVal uncl_time_tz_utc(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value v = UNC_BLANK;
    Unc_Timezone *tz;
    int e = uncl_time_maketz(w, &v, unc_boundvalue(w, 0), &tz);
    if (e) return e;
    tz->type = UNCIL_TZ_UTC;
    tz->offsetdst = tz->offset = 0;
    tz->namedst = tz->name = "UTC";
    tz->nalloc = 0;
    unc_unlock(w, &v);
    return unc_returnlocal(w, 0, &v);
}

static const int m2d[] =
    { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 };

INLINE int isleapyear(long y) {
    return !(y % 400) || ((y % 100) && !(y % 4));
}

static int dayspermonth(int month, int year) {
    if (isleapyear(year) && month == 1)
        return 29;
    return m2d[month + 1] - m2d[month];
}

INLINE int daysperyear(long y) {
    return isleapyear(y) ? 366 : 365;
}

static int getdayofyear(int mday, int mon, long year) {
    return mday - 1 + m2d[mon] + (isleapyear(year) && mon > 1);
}

static long getdayfromyearmonth(long year, int month) {
    long day;
    int leap = isleapyear(year);
    day = (year - 1970) * 365;
    if (year < 1968)
        day -= (1971 - year) / 4;
    else
        day += (year - 1968) / 4;
    if (year < 1900)
        day += (1999 - year) / 100;
    else
        day -= (year - 1900) / 100;
    if (year < 1600)
        day -= (1999 - year) / 400;
    else
        day += (year - 1600) / 400;
    if (leap && month <= 1) day -= 1;
    return day + m2d[month];
}

static void getweekinfo(long year, int yday, int *week, int *wday) {
    long day = getdayfromyearmonth(year, 0);
    int wday0 = (day % 7 + 7 + 4) % 7;
    int yday0 = (7 - (wday0 + 3)) % 7;
    *wday = (wday0 + yday) % 7;
    *week = (7 + yday - yday0) / 7;
}

#if UNIXTIME
static time_t unc0_mkgmtime(struct tm *time) {
    long year = time->tm_year + 1900;
    long day;
    int month = ((time->tm_mon % 12) + 12) % 12;
    if (time->tm_mon < 0)
        year -= (-time->tm_mon + 11) / 12;
    else
        year += time->tm_mon / 12;
    day = getdayfromyearmonth(year, month) + time->tm_mday - 1;
    if (time->tm_isdst < 0) time->tm_isdst = 0;
    time->tm_mon = month;
    time->tm_mday = m2d[month] + time->tm_mday - 1;
    time->tm_wday = (day - 4) % 7;
    return 60 * (60 * (24 * day + time->tm_hour)
                                + time->tm_min)
                                + time->tm_sec;
}
#endif

#if UNCIL_MT_OK
UNC_LOCKSTATICL(loctime)
#endif

static time_t unc0_timegm_fallback(struct tm *time, int y);

static time_t unc0_timegm(struct tm *time) {
    time_t t;
    time->tm_isdst = 0;
#if UNCIL_IS_WINDOWS && _MSC_VER >= 1400
    UNC_LOCKL(loctime);
    t = _mkgmtime(time);
    UNC_UNLOCKL(loctime);
#elif UNCIL_IS_POSIX
    UNC_LOCKL(loctime);
    t = timegm(time);
    UNC_UNLOCKL(loctime);
#elif UNIXTIME
#define UNCIL_MKGMTIME 1
    UNC_LOCKL(loctime);
    t = unc0_mkgmtime(time);
    UNC_UNLOCKL(loctime);
#else
    t = unc0_timegm_fallback(time, time->tm_year);
#endif
    return t;
}

struct tm *unc0_gmtime(const time_t *timer, struct tm *buf) {
#if UNCIL_IS_POSIX || UNCIL_IS_C23
    return gmtime_r(timer, buf);
#else
    struct tm *ret;
    UNC_LOCKL(loctime);
    if (buf) {
        ret = gmtime(timer);
        if (ret) *buf = *ret, ret = buf;
    } else {
        ret = gmtime(timer);
    }
    UNC_UNLOCKL(loctime);
    return ret;
#endif
}

struct tm *unc0_localtime(const time_t *timer, struct tm *buf) {
#if UNCIL_IS_POSIX || UNCIL_IS_C23
    return localtime_r(timer, buf);
#else
    struct tm *ret;
    UNC_LOCKL(loctime);
    if (buf) {
        ret = localtime(timer);
        if (ret) *buf = *ret, ret = buf;
    } else {
        ret = localtime(timer);
    }
    UNC_UNLOCKL(loctime);
    return ret;
#endif
}

struct tm *unc0_gmtimex(time_t timer, struct tm *buf) {
    return unc0_gmtime(&timer, buf);
}

struct tm *unc0_localtimex(time_t timer, struct tm *buf) {
    return unc0_localtime(&timer, buf);
}

static void unc0_timeshift_tm(struct tm *time, long offset) {
    if (offset < 0) {
        offset = -offset;
        time->tm_sec -= offset % 60; offset /= 60;
        time->tm_min -= offset % 60; offset /= 60;
        time->tm_hour -= offset % 24; offset /= 24;
        time->tm_mday -= offset;
        if (time->tm_sec < 0) time->tm_sec += 60, --time->tm_min;
        if (time->tm_min < 0) time->tm_min += 60, --time->tm_hour;
        if (time->tm_hour < 0) time->tm_hour += 24, --time->tm_mday;
    } else if (offset > 0) {
        time->tm_sec += offset % 60; offset /= 60;
        time->tm_min += offset % 60; offset /= 60;
        time->tm_hour += offset % 24; offset /= 24;
        time->tm_mday += offset;
        if (time->tm_sec >= 60) time->tm_sec -= 60, ++time->tm_min;
        if (time->tm_min >= 60) time->tm_min -= 60, ++time->tm_hour;
        if (time->tm_hour >= 24) time->tm_hour -= 24, ++time->tm_mday;
    }
}

static long unc0_timeunshift_tm(struct tm *t1, struct tm *t0) {
    int dayoff = 0;
    if (t1->tm_mday != t0->tm_mday
            || t1->tm_mon != t0->tm_mon
            || t1->tm_year != t0->tm_year) {
        long d1 = t1->tm_mday - 1 + m2d[t1->tm_mon];
        long d0 = t0->tm_mday - 1 + m2d[t0->tm_mon];
        if (t1->tm_year != t0->tm_year) {
            ASSERT(t1->tm_year - t0->tm_year <= 1
                && t1->tm_year - t0->tm_year >= -1);
            if (t0->tm_year < t1->tm_year)
                d1 += daysperyear(t0->tm_year + 1900L);
            else if (t1->tm_year < t0->tm_year)
                d0 += daysperyear(t1->tm_year + 1900L);
        }
        dayoff = d1 - d0;
    }
    return ((dayoff * 24 + t1->tm_hour - t0->tm_hour)
                    * 60 + t1->tm_min - t0->tm_min)
                    * 60 + t1->tm_sec - t0->tm_sec;
}

static time_t unc0_time(long *us) {
#if UNCIL_IS_POSIX
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts))
        return (time_t)-1;
    if (us) *us = (long)(ts.tv_nsec / 1000);
    return ts.tv_sec;
#elif UNCIL_IS_C11
    struct timespec ts;
    if (!timespec_get(&ts, TIME_UTC))
        return (time_t)-1;
    if (us) *us = (long)(ts.tv_nsec / 1000);
    return ts.tv_sec;
#else
    if (us) *us = 0;
    return time(NULL);
#endif
}

static int unc0_tzlocalinfo(long *offset, long *offsetdst) {
    long o, odst;
    struct tm tm = { 0 }, utm;
    time_t t = unc0_time(NULL);
#if UNIXTIME
    time_t t0;
#endif
    if (t == (time_t)(-1)) return 1;
#if !UNIXTIME
    if (!unc0_gmtimex(t, &tm)) return 1;
    tm.tm_mday = 11;
    tm.tm_mon = 6;
    tm.tm_hour = 0;
    tm.tm_min = 0;
    tm.tm_sec = 0;
    tm.tm_isdst = -1;
    t = mktime(&tm);
    if (t == (time_t)(-1)) return 1;
#endif
    if (!unc0_gmtimex(t, &utm)) return 1;
    if (!unc0_localtimex(t, &tm)) return 1;

#if UNIXTIME
    t0 = unc0_mkgmtime(&tm);
    odst = o = t0 - t;
#else
    odst = o = unc0_timeunshift_tm(&tm, &utm);
#endif
    if (tm.tm_isdst >= 0) {
        /* try to determine DST */
        int isdst = !!tm.tm_isdst;
        int i;
#if UNIXTIME
        time_t td = t;
        for (i = 1; i <= 12; ++i) {
            td += 30 * 86400;
            if (td < t) return 1;
            if (!unc0_localtimex(td, &tm)) return 1;
            if (tm.tm_isdst < 0) return 1;
            if (!!tm.tm_isdst != isdst) {
                t0 = unc0_mkgmtime(&tm);
                odst = t0 - td;
                break;
            }
        }
#else
        for (i = 1; i <= 12; ++i) {
            ++tm.tm_mon;
            tm.tm_isdst = -1;
            UNC_LOCKL(loctime);
            t = mktime(&tm);
            UNC_UNLOCKL(loctime);
            if (t == (time_t)(-1)) return 1;
            if (!!tm.tm_isdst != isdst) {
                if (!unc0_gmtimex(t, &utm)) return 1;
                odst = unc0_timeunshift_tm(&tm, &utm);
                break;
            }
        }
#endif
        if (isdst) {
            long t = o;
            o = odst;
            odst = t;
        }
    }
    *offset = o;
    *offsetdst = odst;
    return 0;
}

static int unc0_tzlocalhasdst(void) {
    long offset, offsetdst;
    if (unc0_tzlocalinfo(&offset, &offsetdst))
        return -1;
    return offset != offsetdst;
}

static long unc0_tzlocaloffset(void) {
    long offset, offsetdst;
    if (unc0_tzlocalinfo(&offset, &offsetdst))
        return LONG_MIN;
    return offset;
}

static long unc0_tzlocaloffsetdst(void) {
    long offset, offsetdst;
    if (unc0_tzlocalinfo(&offset, &offsetdst))
        return LONG_MIN;
    return offsetdst;
}

INLINE time_t unc0_timeshift(time_t t, long offset) {
#if UNIXTIME
    return t - offset;
#else
    struct tm tm;
    if (!unc0_localtimex(t, &tm)) return (time_t)(-1);
    unc0_timeshift_tm(&tm, offset);
    return unc0_timegm(&tm);
#endif
}

#if !UNIXTIME
INLINE int unc0_tm_bsearch(struct tm *t1, struct tm *t0) {
    if (t1->tm_year > t0->tm_year) return 1;
    if (t1->tm_year < t0->tm_year) return -1;
    if (t1->tm_mon  > t0->tm_mon) return 1;
    if (t1->tm_mon  < t0->tm_mon) return -1;
    if (t1->tm_mday > t0->tm_mday) return 1;
    if (t1->tm_mday < t0->tm_mday) return -1;
    if (t1->tm_hour > t0->tm_hour) return 1;
    if (t1->tm_hour < t0->tm_hour) return -1;
    if (t1->tm_min  > t0->tm_min) return 1;
    if (t1->tm_min  < t0->tm_min) return -1;
    if (t1->tm_sec  > t0->tm_sec) return 1;
    if (t1->tm_sec  < t0->tm_sec) return -1;
    return 0;
}

/* really bad fallbacks for timegm that will probably break */
static time_t unc0_timegm_fallback2(struct tm *time, int y, time_t guess) {
    /* check if time_t is linear and increasing */
    time_t t0, t1, t2, tsec;
    int e;
    struct tm testtm = { 0 };
    testtm.tm_year = y;
    t0 = mktime(&testtm);
    if (t0 == (time_t)-1) return t0;
    testtm.tm_mday += 127;
    t1 = mktime(&testtm);
    if (t1 == (time_t)-1) return t1;
    testtm.tm_mday += 127;
    t2 = mktime(&testtm);
    if (t2 == (time_t)-1) return t2;
    if (t2 - t1 != t1 - t0 || t1 < t0) return (time_t)-1;
    tsec = (t1 - t0) / (127 * 86400);
    /* +/- 24 hours from guess */
    t0 = guess - tsec * 86400;
    t1 = guess + tsec * 86400;
    while (t1 - t0 >= tsec) {
        t2 = (t0 + t1) / 2;
        if (!unc0_gmtime(&t2, &testtm))
            return (time_t)-1;
        e = unc0_tm_bsearch(time, &testtm);
        if (!e) return t2;
        else if (e < 0) {
            t1 = t2;
        } else {
            t0 = t2 + tsec;
        }
    }
    t2 = (t0 + t1) / 2;
    if (!unc0_gmtimex(t2, &testtm)) return (time_t)-1;
    if (time->tm_sec != testtm.tm_sec || time->tm_min != testtm.tm_min
        || time->tm_hour != testtm.tm_hour || time->tm_mday != testtm.tm_mday
        || time->tm_mon != testtm.tm_mon || time->tm_year != testtm.tm_year)
        return (time_t)-1;
    return t2;
}

static time_t unc0_timegm_fallback(struct tm *time, int y) {
    time_t t;
    struct tm gtm = { 0 }, ltm = { 0 }, rtm = *time;
    /* try looking for time without DST */
    gtm.tm_year = y;
    gtm.tm_mon = -1;
    gtm.tm_mday = 11;
    gtm.tm_isdst = 0;
    do {
        ++gtm.tm_mon;
        UNC_LOCKL(loctime);
        t = mktime(&gtm);
        UNC_UNLOCKL(loctime);
        if (!unc0_gmtimex(t, &gtm)) return (time_t)-1;
        if (!unc0_localtimex(t, &ltm)) return (time_t)-1;
        /* loop until DST not in effect */
    } while (ltm.tm_isdst > 0);
    /* assume that if local time is 3 hours ahead of UTC then
       the local time that corresponds to the UTC must be the one
       that is 3 hours behind the specified time, etc.
       none of which is guaranteed to work, of course */
    rtm.tm_sec  = rtm.tm_sec  + ltm.tm_sec  - gtm.tm_sec;
    rtm.tm_min  = rtm.tm_min  + ltm.tm_min  - gtm.tm_min;
    rtm.tm_hour = rtm.tm_hour + ltm.tm_hour - gtm.tm_hour;
    rtm.tm_mday = rtm.tm_mday + ltm.tm_mday - gtm.tm_mday;
    rtm.tm_mon  = rtm.tm_mon  + ltm.tm_mon  - gtm.tm_mon;
    rtm.tm_year = rtm.tm_year + ltm.tm_year - gtm.tm_year;
    rtm.tm_isdst = 0;
    UNC_LOCKL(loctime);
    t = mktime(&rtm);
    UNC_UNLOCKL(loctime);
    /* better to fail than to give the wrong result */
    if (!unc0_gmtimex(t, &gtm)) return (time_t)-1;
    ltm = *time;
    if (ltm.tm_sec != gtm.tm_sec || ltm.tm_min != gtm.tm_min
            || ltm.tm_hour != gtm.tm_hour || ltm.tm_mday != gtm.tm_mday
            || ltm.tm_mon != gtm.tm_mon || ltm.tm_year != gtm.tm_year)
        return unc0_timegm_fallback2(time, y, t);
    return t;
}
#endif

Unc_RetVal uncl_time_tz_local(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value v = UNC_BLANK;
    Unc_Timezone *ptz, tz;
    Unc_RetVal e;
    if (!unc0_tzlocalinfo(&tz.offset, &tz.offsetdst)) {
        tz.type = UNCIL_TZ_LOCAL;
        tz.name = "Local";
        tz.namedst = tz.offsetdst == tz.offset ? tz.name : "Local (DST)";
        tz.nalloc = 0;

        e = uncl_time_maketz(w, &v, unc_boundvalue(w, 0), &ptz);
        if (e) return e;
        *ptz = tz;
        unc_unlock(w, &v);
    }
    return unc_returnlocal(w, 0, &v);
}

Unc_RetVal uncl_time_tz_get(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value v = UNC_BLANK;
    Unc_Size sn;
    const char *sb;
    Unc_RetVal e;
    int local;
    if (unc_gettype(w, &args.values[0])) {
        e = unc_getstring(w, &args.values[0], &sn, &sb);
        if (e) return e;
        local = 0;
    } else {
        local = 1;
    }
#if 0
    { /* TODO implement timezone support */
        Unc_Timezone *tz;
    }
#else
    (void)local;
#endif
    return unc_returnlocal(w, 0, &v);
}

Unc_RetVal uncl_time_tz_dst(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value p = UNC_BLANK;
    Unc_Timezone *tz;
    Unc_RetVal e;
    unc_getprototype(w, &args.values[0], &p);
    if (unc_gettype(w, &args.values[0]) != Unc_TOpaque
            || !unc_issame(w, &p, unc_boundvalue(w, 0))) {
        VCLEAR(w, &p);
        return unc_throwexc(w, "type", "argument is not a time zone");
    }
    VCLEAR(w, &p);
    e = unc_lockopaque(w, &args.values[0], NULL, (void **)&tz);
    if (e) return e;
    switch (tz->type) {
    case UNCIL_TZ_UTC:
        unc_setbool(w, &p, 0);
        break;
    case UNCIL_TZ_LOCAL:
        unc_setbool(w, &p, unc0_tzlocalhasdst());
        break;
    case UNCIL_TZ_EXTERN:
    default:
        unc_setbool(w, &p, tz->offset != tz->offsetdst);
    }
    e = unc_returnlocal(w, 0, &p);
    unc_unlock(w, &args.values[0]);
    return e;
}

Unc_RetVal uncl_time_tz_offset(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value p = UNC_BLANK;
    Unc_Timezone *tz;
    Unc_RetVal e;
    unc_getprototype(w, &args.values[0], &p);
    if (unc_gettype(w, &args.values[0]) != Unc_TOpaque
            || !unc_issame(w, &p, unc_boundvalue(w, 0))) {
        VCLEAR(w, &p);
        return unc_throwexc(w, "type", "argument is not a time zone");
    }
    VCLEAR(w, &p);
    e = unc_lockopaque(w, &args.values[0], NULL, (void **)&tz);
    if (e) return e;
    switch (tz->type) {
    case UNCIL_TZ_UTC:
        unc_setint(w, &p, 0);
        break;
    case UNCIL_TZ_LOCAL:
    {
        long i = unc0_tzlocaloffset();
        if (i == LONG_MIN)
            unc_setnull(w, &p);
        else
            unc_setint(w, &p, i);
        break;
    }
    case UNCIL_TZ_EXTERN:
    default:
        unc_setint(w, &p, tz->offset);
    }
    e = unc_returnlocal(w, 0, &p);
    unc_unlock(w, &args.values[0]);
    return e;
}

Unc_RetVal uncl_time_tz_offsetdst(Unc_View *w,
                                      Unc_Tuple args, void *udata) {
    Unc_Value p = UNC_BLANK;
    Unc_Timezone *tz;
    Unc_RetVal e;
    unc_getprototype(w, &args.values[0], &p);
    if (unc_gettype(w, &args.values[0]) != Unc_TOpaque
            || !unc_issame(w, &p, unc_boundvalue(w, 0))) {
        VCLEAR(w, &p);
        return unc_throwexc(w, "type", "argument is not a time zone");
    }
    VCLEAR(w, &p);
    e = unc_lockopaque(w, &args.values[0], NULL, (void **)&tz);
    if (e) return e;
    switch (tz->type) {
    case UNCIL_TZ_UTC:
        unc_setint(w, &p, 0);
        break;
    case UNCIL_TZ_LOCAL:
    {
        long i = unc0_tzlocaloffsetdst();
        if (i == LONG_MIN)
            unc_setnull(w, &p);
        else
            unc_setint(w, &p, i);
        break;
    }
    case UNCIL_TZ_EXTERN:
    default:
        unc_setint(w, &p, tz->offsetdst);
    }
    e = unc_returnlocal(w, 0, &p);
    unc_unlock(w, &args.values[0]);
    return e;
}

Unc_RetVal unc0_fromtime(struct unc_tm *putm,
                         Unc_Timezone *utz,
                         time_t t,
                         long usec) {
    struct tm tm;
    switch (utz->type) {
    case UNCIL_TZ_UTC:
        if (!unc0_gmtimex(t, &tm))
            return 1;
        unc0_tm2utm(&tm, putm, usec);
        return 0;
    case UNCIL_TZ_LOCAL:
        if (!unc0_localtimex(t, &tm))
            return 1;
        unc0_tm2utm(&tm, putm, usec);
        return 0;
    case UNCIL_TZ_EXTERN:
    default:
        NEVER();
    }
}

Unc_RetVal unc0_totime(struct unc_tm *putm,
                       Unc_Timezone *utz,
                       time_t *result) {
    time_t t;
    struct tm tm;
    unc0_utm2tm(&tm, putm);
    switch (utz->type) {
    case UNCIL_TZ_UTC:
        t = unc0_timegm(&tm);
        if (t == (time_t)-1)
            return 1;
        *result = t;
        return 0;
    case UNCIL_TZ_LOCAL:
        UNC_LOCKL(loctime);
        t = mktime(&tm);
        UNC_UNLOCKL(loctime);
        if (t == (time_t)-1)
            return 1;
        *result = t;
        return 0;
    case UNCIL_TZ_EXTERN:
#if 0
    case UNCIL_TZ_INTERN:
        t = unc0_timegm(&tm);
        if (t == (time_t)-1)
            return 1;
#if UNIXTIME
        t -= tm.tm_isdst ? utz->offsetdst : utz->offset;
#else
        t = unc0_timeshift(t, tm.tm_isdst ? utz->offsetdst : utz->offset);
#endif
        if (t == (time_t)-1)
            return 1;
        *result = t;
        return 0;
#endif
    default:
        NEVER();
    }
}

Unc_RetVal uncl_time_tz_islocal(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value p = UNC_BLANK;
    Unc_Timezone *tz;
    Unc_RetVal e;
    unc_getprototype(w, &args.values[0], &p);
    if (unc_gettype(w, &args.values[0]) != Unc_TOpaque
            || !unc_issame(w, &p, unc_boundvalue(w, 0))) {
        VCLEAR(w, &p);
        return unc_throwexc(w, "type", "argument is not a time zone");
    }
    VCLEAR(w, &p);
    e = unc_lockopaque(w, &args.values[0], NULL, (void **)&tz);
    if (e) return e;
    unc_setbool(w, &p, tz->type == UNCIL_TZ_LOCAL);
    e = unc_returnlocal(w, 0, &p);
    unc_unlock(w, &args.values[0]);
    return e;
}

Unc_RetVal uncl_time_tz_name(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value p = UNC_BLANK;
    Unc_Timezone *tz;
    Unc_RetVal e;
    unc_getprototype(w, &args.values[0], &p);
    if (unc_gettype(w, &args.values[0]) != Unc_TOpaque
            || !unc_issame(w, &p, unc_boundvalue(w, 0))) {
        VCLEAR(w, &p);
        return unc_throwexc(w, "type", "argument is not a time zone");
    }
    VCLEAR(w, &p);
    e = unc_lockopaque(w, &args.values[0], NULL, (void **)&tz);
    if (e) return e;
    e = unc_newstringc(w, &p, tz->name);
    e = unc_returnlocal(w, e, &p);
    unc_unlock(w, &args.values[0]);
    return e;
}

Unc_RetVal uncl_time_tz_namedst(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value p = UNC_BLANK;
    Unc_Timezone *tz;
    Unc_RetVal e;
    unc_getprototype(w, &args.values[0], &p);
    if (unc_gettype(w, &args.values[0]) != Unc_TOpaque
            || !unc_issame(w, &p, unc_boundvalue(w, 0))) {
        VCLEAR(w, &p);
        return unc_throwexc(w, "type", "argument is not a time zone");
    }
    VCLEAR(w, &p);
    e = unc_lockopaque(w, &args.values[0], NULL, (void **)&tz);
    if (e) return e;
    e = unc_newstringc(w, &p, tz->namedst);
    e = unc_returnlocal(w, e, &p);
    unc_unlock(w, &args.values[0]);
    return e;
}

Unc_RetVal uncl_time_clock(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value v = UNC_BLANK;
    unc_setfloat(w, &v, clock());
    return unc_returnlocal(w, 0, &v);
}

Unc_RetVal uncl_time_time(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value v = UNC_BLANK;
    long usec;
    time_t t = unc0_time(&usec);
    if (t == (time_t)-1)
        unc_setnull(w, &v);
    else
#if UNIXTIME
        unc_setfloat(w, &v, t + usec / (Unc_Float)1000000.0);
#else
        unc_setfloat(w, &v, t);
#endif
    return unc_returnlocal(w, 0, &v);
}

Unc_RetVal uncl_time_timefrac(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Value v = UNC_BLANK;
    long usec;
    time_t t = unc0_time(&usec);
    if (t == (time_t)-1) {
        unc_setnull(w, &v);
        e = unc_returnlocal(w, 0, &v);
        e = unc_returnlocal(w, e, &v);
        return e;
    }
    unc_setint(w, &v, (Unc_Int)t);
    e = unc_returnlocal(w, 0, &v);
    if (e) return e;
    unc_setfloat(w, &v, usec / (Unc_Float)1000000.0);
    return unc_returnlocal(w, e, &v);
}

Unc_RetVal uncl_time_dt_gmtime(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Value v = UNC_BLANK;
    struct tm tm;
    struct unc_tm utm;
    time_t t = unc0_time(&utm.tm_usec);
    if (t == (time_t)-1)
        return unc_throwexc(w, "internal", "failed to get time");
    if (!unc0_gmtimex(t, &tm))
        return unc_throwexc(w, "internal", "failed to get time");
    unc0_tm2utm(&tm, &utm, utm.tm_usec);
    e = uncl_time_tm_toobj(w, &v, unc_boundvalue(w, 0), &utm);
    return unc_returnlocal(w, e, &v);
}

Unc_RetVal uncl_time_dt_localtime(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Value v = UNC_BLANK;
    struct tm tm;
    struct unc_tm utm;
    time_t t = unc0_time(&utm.tm_usec);
    if (t == (time_t)-1)
        return unc_throwexc(w, "internal", "failed to get time");
    if (!unc0_localtimex(t, &tm))
        return unc_throwexc(w, "internal", "failed to get time");
    unc0_tm2utm(&tm, &utm, utm.tm_usec);
    e = uncl_time_tm_toobj(w, &v, unc_boundvalue(w, 0), &utm);
    return unc_returnlocal(w, e, &v);
}

Unc_RetVal uncl_time_dt_mktime(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Value v = UNC_BLANK;
    time_t t;
    struct tm tm;
    struct unc_tm utm;

    unc_getprototype(w, &args.values[0], &v);
    if (unc_gettype(w, &args.values[0]) != Unc_TObject
            || !unc_issame(w, &v, unc_boundvalue(w, 0))) {
        VCLEAR(w, &v);
        return unc_throwexc(w, "type", "argument 1 is not a datetime");
    }
    VCLEAR(w, &v);
    e = uncl_time_tm_fromobj(w, &args.values[0], &utm);
    if (e) return e;

    unc0_utm2tm(&tm, &utm);
    UNC_LOCKL(loctime);
    t = mktime(&tm);
    UNC_UNLOCKL(loctime);
    if (t == (time_t)-1)
        return unc_throwexc(w, "value", "cannot represent this datetime");

    unc_setfloat(w, &v, t);
    return unc_returnlocal(w, 0, &v);
}

Unc_RetVal uncl_time_dt_fromtime(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Value v = UNC_BLANK;
    struct unc_tm utm;
    Unc_Int ui;
    Unc_Float uf;
    Unc_Timezone utz, *ptz;
    long usec = 0;

    time_t t;
    if (unc_getint(w, &args.values[0], &ui)) {
        if ((e = unc_getfloat(w, &args.values[0], &uf)))
            return e;
        t = (time_t)uf;
#if UNIXTIME
        { Unc_Float i; usec = (long)(1000000 * modf(uf, &i)); }
#endif
    } else
        t = (time_t)ui;
    if (unc_gettype(w, &args.values[1])) {
        unc_getprototype(w, &args.values[1], &v);
        if (unc_gettype(w, &args.values[1]) != Unc_TOpaque
                || !unc_issame(w, &v, unc_boundvalue(w, 1))) {
            VCLEAR(w, &v);
            return unc_throwexc(w, "type", "argument 2 is not a timezone");
        }
        if ((e = unc_lockopaque(w, &args.values[1], NULL, (void **)&ptz))) {
            VCLEAR(w, &v);
            return e;
        }
        utz = *ptz;
        unc_unlock(w, &args.values[1]);
    } else {
        utz.type = UNCIL_TZ_UTC;
    }
    e = unc0_fromtime(&utm, &utz, t, usec);
    if (e)
        return unc_throwexc(w, "value", "cannot represent as datetime");
    e = uncl_time_tm_toobj(w, &v, unc_boundvalue(w, 0), &utm);
    return unc_returnlocal(w, e, &v);
}

Unc_RetVal uncl_time_tz_now(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value p = UNC_BLANK;
    Unc_Timezone *tz;
    struct unc_tm utm;
    Unc_RetVal e;
    long usec;
    time_t t;
    unc_getprototype(w, &args.values[0], &p);
    if (unc_gettype(w, &args.values[0]) != Unc_TOpaque
            || !unc_issame(w, &p, unc_boundvalue(w, 1))) {
        VCLEAR(w, &p);
        return unc_throwexc(w, "type", "argument is not a time zone");
    }
    t = unc0_time(&usec);
    if (t == (time_t)-1)
        return unc_throwexc(w, "system", "failed to get the current time");
    VCLEAR(w, &p);
    e = unc_lockopaque(w, &args.values[0], NULL, (void **)&tz);
    if (e) return e;
    e = unc0_fromtime(&utm, tz, t, usec);
    if (e)
        return unc_throwexc(w, "value", "cannot represent as datetime");
    e = uncl_time_tm_toobj(w, &p, unc_boundvalue(w, 0), &utm);
    unc_unlock(w, &args.values[0]);
    return unc_returnlocal(w, e, &p);
}

static int readidig(const char **pc, int d, int *out) {
    int o = 0;
    int n;
    const char *c = *pc;
    while (d--) {
        n = *c++;
        if (!unc0_isdigit(n)) return 1;
        o = o * 10 + (n - '0');
    }
    *pc = c;
    *out = o;
    return 0;
}

Unc_RetVal uncl_time_dt_fromiso(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    int c;
    Unc_Value v = UNC_BLANK;
    struct unc_tm utm;
    int hastz;
    long offset;
    Unc_Size bn;
    int year;
    const char *bb;

    if ((e = unc_getstring(w, &args.values[0], &bn, &bb)))
        return e;
    if (readidig(&bb, 4, &year)) goto uncl_time_dt_fromiso_fail;
    if (*bb == '-') ++bb;
    if (readidig(&bb, 2, &utm.tm_mon)) goto uncl_time_dt_fromiso_fail;
    else --utm.tm_mon;
    if (*bb == '-') ++bb;
    if (readidig(&bb, 2, &utm.tm_mday)) goto uncl_time_dt_fromiso_fail;
    if (*bb++ != 'T') goto uncl_time_dt_fromiso_fail;
    if (readidig(&bb, 2, &utm.tm_hour)) goto uncl_time_dt_fromiso_fail;
    if (*bb == ':') ++bb;
    if (readidig(&bb, 2, &utm.tm_min)) goto uncl_time_dt_fromiso_fail;
    if (*bb == ':') ++bb;
    if (readidig(&bb, 2, &utm.tm_sec)) goto uncl_time_dt_fromiso_fail;
    if (year < 1583
            || utm.tm_mon < 0 || utm.tm_mon > 11
            || utm.tm_mday < 1 || utm.tm_mday > dayspermonth(utm.tm_mon, year)
            || utm.tm_hour < 0 || utm.tm_hour > 23
            || utm.tm_min < 0 || utm.tm_min > 59
            || utm.tm_sec < 0 || utm.tm_sec > 60)
        goto uncl_time_dt_fromiso_fail;
    utm.tm_year = year - 1900;
    utm.tm_isdst = -1;
    utm.tm_usec = 0;
    {
        int week;
        utm.tm_yday = getdayofyear(utm.tm_mday, utm.tm_mon, year);
        getweekinfo(year, utm.tm_yday, &week, &utm.tm_wday);
    }
    if (*bb) {
        int neg;
        int tmh, tmm;
        c = *bb++;
        hastz = 1;
        switch (c) {
        case 'Z':
            offset = 0;
            break;
        case '+':
            neg = 0;
            goto fromiso_tz;
        case '-':
            neg = 1;
            goto fromiso_tz;
        fromiso_tz:
            if (readidig(&bb, 2, &tmh)) goto uncl_time_dt_fromiso_fail;
            if (*bb == ':') ++bb;
            if (readidig(&bb, 2, &tmm)) goto uncl_time_dt_fromiso_fail;
            if (tmh < 0 || tmh > 24 || tmm < 0 || tmm > 59) e = 1;
            else offset = (neg ? -1 : 1) * (tmh * 60 + tmm) * 60;
        }
    }
    if (e) {
uncl_time_dt_fromiso_fail:
        return unc_throwexc(w, "value",
            "invalid or unsupported ISO 8601 timestamp");
    }

    e = uncl_time_tm_toobj(w, &v, unc_boundvalue(w, 0), &utm);
    e = unc_returnlocal(w, e, &v);
    if (!e) {
        if (hastz)
            unc_setint(w, &v, offset);
        else
            unc_setnull(w, &v);
        e = unc_returnlocal(w, e, &v);
    }
    return e;
}

Unc_RetVal uncl_time_dt_new(Unc_View *w,
                                      Unc_Tuple args, void *udata) {
    int i;
    Unc_RetVal e;
    Unc_Value v = UNC_BLANK;
    Unc_Int values[6];
    struct unc_tm utm;

    for (i = 0; i < 6; ++i) {
        if ((e = unc_getint(w, &args.values[i], &values[i])))
            return e;
    }
    e = unc_getbool(w, &args.values[6], -1);
    if (UNCIL_IS_ERR_CMP(e)) return e;
    utm.tm_isdst = e;
    if (values[0] < 1583 || values[0] > INT_MAX)
        return unc_throwexc(w, "value", "year out of range");
    if (values[1] < 1 || values[1] > 12)
        return unc_throwexc(w, "value", "month out of range");
    if (values[2] < 1 || values[2] > dayspermonth(values[1] - 1, values[0]))
        return unc_throwexc(w, "value", "day out of range");
    if (values[3] < 0 || values[3] > 23)
        return unc_throwexc(w, "value", "hour out of range");
    if (values[4] < 0 || values[4] > 59)
        return unc_throwexc(w, "value", "minute out of range");
    if (values[5] < 0 || values[5] > 60)
        return unc_throwexc(w, "value", "second out of range");
    utm.tm_year = (int)(values[0] - 1900);
    utm.tm_mon = (int)(values[1] - 1);
    utm.tm_mday = (int)(values[2]);
    utm.tm_hour = (int)(values[3]);
    utm.tm_min = (int)(values[4]);
    utm.tm_sec = (int)(values[5]);
    utm.tm_usec = 0;
    {
        int week;
        utm.tm_yday = getdayofyear(utm.tm_mday, utm.tm_mon, values[0]);
        getweekinfo(values[0], utm.tm_yday, &week, &utm.tm_wday);
    }
    
    e = uncl_time_tm_toobj(w, &v, unc_boundvalue(w, 0), &utm);
    return unc_returnlocal(w, e, &v);
}

static Unc_RetVal uncl_time_dt_totime_i(
            Unc_View *w, Unc_Tuple args, time_t *result, struct unc_tm *putm) {
    Unc_RetVal e;
    Unc_Value v = UNC_BLANK;
    struct unc_tm utm;
    Unc_Timezone utz, *ptz;

    unc_getprototype(w, &args.values[0], &v);
    if (unc_gettype(w, &args.values[0]) != Unc_TObject
            || !unc_issame(w, &v, unc_boundvalue(w, 0))) {
        VCLEAR(w, &v);
        return unc_throwexc(w, "type", "argument 1 is not a datetime");
    }
    if (unc_gettype(w, &args.values[1])) {
        unc_getprototype(w, &args.values[1], &v);
        if (unc_gettype(w, &args.values[1]) != Unc_TOpaque
                || !unc_issame(w, &v, unc_boundvalue(w, 1))) {
            VCLEAR(w, &v);
            return unc_throwexc(w, "type", "argument 2 is not a timezone");
        }
        if ((e = unc_lockopaque(w, &args.values[1], NULL, (void **)&ptz))) {
            VCLEAR(w, &v);
            return e;
        }
        utz = *ptz;
        unc_unlock(w, &args.values[1]);
    } else {
        utz.type = UNCIL_TZ_UTC;
    }
    VCLEAR(w, &v);
    e = uncl_time_tm_fromobj(w, &args.values[0], &utm);
    if (e) return e;
    *putm = utm;
    return unc0_totime(putm, &utz, result)
        ? unc_throwexc(w, "value", "cannot represent this datetime")
        : 0;
}

Unc_RetVal uncl_time_dt_totime(Unc_View *w, Unc_Tuple args, void *udata) {
    struct unc_tm utm;
    time_t t;
    Unc_Value v = UNC_BLANK;
    Unc_RetVal e = uncl_time_dt_totime_i(w, args, &t, &utm);
    if (e) return e;
#if UNIXTIME
    t += (time_t)(utm.tm_usec / 1000000.0);
#endif
    unc_setfloat(w, &v, t);
    return unc_returnlocal(w, 0, &v);
}

Unc_RetVal uncl_time_dt_totimefrac(Unc_View *w, Unc_Tuple args, void *udata) {
    struct unc_tm utm;
    time_t t;
    Unc_Value v = UNC_BLANK;
    Unc_RetVal e = uncl_time_dt_totime_i(w, args, &t, &utm);
    if (e) return e;
    unc_setfloat(w, &v, t);
    e = unc_returnlocal(w, 0, &v);
    if (!e) {
        Unc_Float f;
#if UNIXTIME
        f = utm.tm_usec / 1000000.0;
#else
        f = 0;
#endif
        unc_setfloat(w, &v, f);
        e = unc_returnlocal(w, 0, &v);
    }
    return e;
}

Unc_RetVal uncl_time_dt_convert(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value v = UNC_BLANK;
    struct unc_tm utm;
    Unc_Timezone utz, *ptz;
    time_t t;
    Unc_RetVal e = uncl_time_dt_totime_i(w, args, &t, &utm);
    if (e) return e;
#if UNIXTIME
    t += (time_t)(utm.tm_usec / 1000000.0);
#endif

    unc_getprototype(w, &args.values[2], &v);
    if (unc_gettype(w, &args.values[2]) != Unc_TOpaque
            || !unc_issame(w, &v, unc_boundvalue(w, 1))) {
        VCLEAR(w, &v);
        return unc_throwexc(w, "type", "argument 3 is not a timezone");
    }
    if ((e = unc_lockopaque(w, &args.values[2], NULL, (void **)&ptz))) {
        VCLEAR(w, &v);
        return e;
    }
    utz = *ptz;
    unc_unlock(w, &args.values[2]);
    e = unc0_fromtime(&utm, &utz, t, utm.tm_usec);
    if (e)
        return unc_throwexc(w, "value", "cannot represent as datetime");
    e = uncl_time_tm_toobj(w, &v, unc_boundvalue(w, 0), &utm);
    return unc_returnlocal(w, e, &v);
}

Unc_RetVal uncl_time_dt_toiso(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Value v = UNC_BLANK;
    struct unc_tm utm;
    int hastz = 0;
    Unc_Timezone utz, *ptz;
    char buf[64], *obuf = buf;
    long offset;

    unc_getprototype(w, &args.values[0], &v);
    if (unc_gettype(w, &args.values[0]) != Unc_TObject
            || !unc_issame(w, &v, unc_boundvalue(w, 0))) {
        VCLEAR(w, &v);
        return unc_throwexc(w, "type", "argument 1 is not a datetime");
    }
    if (unc_gettype(w, &args.values[1])) {
        unc_getprototype(w, &args.values[1], &v);
        if (unc_gettype(w, &args.values[1]) != Unc_TOpaque
                || !unc_issame(w, &v, unc_boundvalue(w, 1))) {
            VCLEAR(w, &v);
            return unc_throwexc(w, "type", "argument 2 is not a timezone");
        }
        if ((e = unc_lockopaque(w, &args.values[1], NULL, (void **)&ptz))) {
            VCLEAR(w, &v);
            return e;
        }
        utz = *ptz;
        unc_unlock(w, &args.values[1]);
        hastz = 1;
    }
    VCLEAR(w, &v);
    e = uncl_time_tm_fromobj(w, &args.values[0], &utm);
    if (e) return e;
    obuf += unc0_xsnprintf(obuf, sizeof(buf), 0,
            "%.4d-%.2d-%.2dT%.2d:%.2d:%.2d",
            utm.tm_year + 1900, utm.tm_mon + 1, utm.tm_mday,
            utm.tm_hour, utm.tm_min, utm.tm_sec);
    if (hastz) {
        if (utm.tm_isdst > 0) {
            if (utz.type == UNCIL_TZ_LOCAL) {
                offset = unc0_tzlocaloffsetdst();
                if (offset == LONG_MIN)
                    hastz = 0;
            } else
                offset = utz.offsetdst;
        } else {
            if (utz.type == UNCIL_TZ_LOCAL) {
                offset = unc0_tzlocaloffset();
                if (offset == LONG_MIN)
                    hastz = 0;
            } else
                offset = utz.offset;
        }
    }
    if (hastz) {
        int neg = offset < 0;
        if (neg) offset = -offset;
        offset /= 60;
        if (offset) {
            unc0_xsnprintf(obuf, sizeof(buf) - (obuf - buf), 0, "%c%02d:%02d",
                    neg ? '-' : '+', (int)(offset / 60), (int)(offset % 60));
        } else {
            *obuf += 'Z';
        }
    }
    e = unc_newstringc(w, &v, buf);
    return unc_returnlocal(w, e, &v);
}

#define FN(x) &uncl_time_##x, #x
static const Unc_ModuleCFunc lib[] = {
    { FN(clock),        0, 0, 0, UNC_CFUNC_DEFAULT    },
    { FN(time),         0, 0, 0, UNC_CFUNC_DEFAULT    },
    { FN(timefrac),     0, 0, 0, UNC_CFUNC_DEFAULT    },
};

#define FNdt(x) &uncl_time_dt_##x, #x
static const Unc_ModuleCFunc lib_datetime[] = {
    { FNdt(gmtime),     0, 1, 0, UNC_CFUNC_DEFAULT    },
    { FNdt(localtime),  0, 1, 0, UNC_CFUNC_DEFAULT    },
    { FNdt(fromiso),    1, 0, 0, UNC_CFUNC_CONCURRENT },
};

static const Unc_ModuleCFunc lib_datetime_protos[] = {
    { FNdt(mktime),     1, 0, 0, UNC_CFUNC_CONCURRENT },
    { FNdt(toiso),      1, 1, 0, UNC_CFUNC_CONCURRENT },
    { FNdt(totime),     1, 1, 0, UNC_CFUNC_CONCURRENT },
    { FNdt(totimefrac), 1, 1, 0, UNC_CFUNC_CONCURRENT },
    { FNdt(convert),    3, 0, 0, UNC_CFUNC_CONCURRENT },
    { FNdt(new),        7, 0, 0, UNC_CFUNC_CONCURRENT },
    { FNdt(fromtime),   1, 1, 0, UNC_CFUNC_CONCURRENT },
};

#define FNtz(x) &uncl_time_tz_##x, #x
static const Unc_ModuleCFunc lib_timezone[] = {
    { FNtz(dst),        1, 0, 0, UNC_CFUNC_CONCURRENT },
    { FNtz(islocal),    1, 0, 0, UNC_CFUNC_CONCURRENT },
    { FNtz(name),       1, 0, 0, UNC_CFUNC_CONCURRENT },
    { FNtz(namedst),    1, 0, 0, UNC_CFUNC_CONCURRENT },
    { FNtz(offset),     1, 0, 0, UNC_CFUNC_CONCURRENT },
    { FNtz(offsetdst),  1, 0, 0, UNC_CFUNC_CONCURRENT },
    { FNtz(get),        1, 0, 0, UNC_CFUNC_CONCURRENT },
    { FNtz(local),      0, 0, 0, UNC_CFUNC_CONCURRENT },
    { FNtz(utc),        0, 0, 0, UNC_CFUNC_CONCURRENT },
};

static const Unc_ModuleCFunc lib_timezone_protos[] = {
    { FNtz(now),        1, 0, 0, UNC_CFUNC_CONCURRENT },
};

#define time_datetime protos[0]
#define time_timezone protos[1]
Unc_RetVal uncilmain_time(Unc_View *w) {
    Unc_RetVal e;
    Unc_Value protos[2] = UNC_BLANKS;

#if UNCIL_MT_OK
    e = UNC_LOCKINITL(loctime);
    if (e) return e;
#endif

    e = unc_newobject(w, &time_datetime, NULL);
    if (e) return e;

    e = unc_newobject(w, &time_timezone, NULL);
    if (e) return e;

    e = unc_exportcfunctions(w, PASSARRAY(lib), 0, 0, NULL);
    if (e) return e;

    e = unc_attrcfunctions(w, &time_datetime, PASSARRAY(lib_datetime),
                           1, &time_datetime, NULL);
    if (e) return e;

    e = unc_attrcfunctions(w, &time_timezone, PASSARRAY(lib_timezone),
                           1, &time_timezone, NULL);
    if (e) return e;

    e = unc_attrcfunctions(w, &time_datetime, PASSARRAY(lib_datetime_protos),
                           2, protos, NULL);
    if (e) return e;

    e = unc_attrcfunctions(w, &time_timezone, PASSARRAY(lib_timezone_protos),
                           2, protos, NULL);
    if (e) return e;

    {
        Unc_Value v = UNC_BLANK;
#if UNIXTIME
        unc_setbool(w, &v, 1);
#else
        unc_setbool(w, &v, 0);
#endif
        e = unc_setpublicc(w, "unix", &v);
        if (e) return e;
    }

    {
        Unc_Value ns = UNC_BLANK;
        e = unc_newstringc(w, &ns, "time.datetime");
        if (e) return e;
        e = unc_setattrc(w, &time_datetime, "__name", &ns);
        if (e) return e;
        VCLEAR(w, &ns);
    }

    {
        Unc_Value ns = UNC_BLANK;
        e = unc_newstringc(w, &ns, "time.timezone");
        if (e) return e;
        e = unc_setattrc(w, &time_timezone, "__name", &ns);
        if (e) return e;
        VCLEAR(w, &ns);
    }

    e = unc_setpublicc(w, "datetime", &time_datetime);
    if (e) return e;

    e = unc_setpublicc(w, "timezone", &time_timezone);
    if (e) return e;

    VCLEAR(w, &time_datetime);
    VCLEAR(w, &time_timezone);
    return 0;
}
