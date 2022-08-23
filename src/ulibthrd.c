/*******************************************************************************
 
Uncil -- builtin thread library impl

Copyright (c) 2021-2022 Sampo Hippeläinen (hisahi)

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

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#define UNCIL_DEFINES

#include "uarithm.h"
#include "udef.h"
#include "umt.h"
#include "uncil.h"
#include "uosdef.h"
#include "uval.h"
#include "uview.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#if UNCIL_IS_POSIX
#include <sys/time.h>
typedef struct unc0_countdown {
    clockid_t clk;
    struct timespec tp;
} unc0_countdown;
static int unc0_countdownfirst(unc0_countdown *t) {
    int e = 0;
    clockid_t clk;
#ifdef CLOCK_MONOTONIC_RAW
    if (!e) e = clock_gettime((clk = CLOCK_MONOTONIC_RAW), &t->tp);
#endif
    if (!e) e = clock_gettime((clk = CLOCK_MONOTONIC), &t->tp);
    if (!e) e = clock_gettime((clk = CLOCK_REALTIME), &t->tp);
    if (e) return e;
    t->clk = clk;
    return 0;
}
static int unc0_countdownnext(unc0_countdown *t, Unc_Float *f) {
    struct timespec t2;
    if (clock_gettime(t->clk, &t2))
        return 1;
    if (t2.tv_sec > t->tp.tv_sec || (t2.tv_sec == t->tp.tv_sec
                                && t2.tv_nsec > t->tp.tv_nsec)) {
        *f -= t2.tv_sec - t->tp.tv_sec;
        *f -= (t2.tv_nsec - t->tp.tv_nsec) * (Unc_Float)0.000000001;
    }
    t->tp = t2;
    return 0;
}
#else
typedef time_t unc0_countdown;
static int unc0_countdownfirst(unc0_countdown *t) {
    return time(t) == (time_t)(-1);
}
static int unc0_countdownnext(unc0_countdown *t, Unc_Float *f) {
    time_t t1;
    if (time(&t1) == (time_t)(-1)) return 1;
    *f -= difftime(t1, *t);
    *t = t1;
    return 0;
}
#endif

struct unc_thrd_thread;
struct unc_thrd_lock;
struct unc_thrd_rlock;
struct unc_thrd_sem;
struct unc_thrd_mon;

static int unc_thrd_thread_new(struct unc_thrd_thread *x);
static int unc_thrd_thread_start(struct unc_thrd_thread *x);
static int unc_thrd_thread_join(struct unc_thrd_thread *x);
static int unc_thrd_thread_jointimed(struct unc_thrd_thread *x, Unc_Float t);
static void unc_thrd_thread_kill(struct unc_thrd_thread *x);
static void unc_thrd_thread_detach(struct unc_thrd_thread *x);
static void unc_thrd_thread_finished(struct unc_thrd_thread *x);

static int unc_thrd_lock_new(struct unc_thrd_lock *lock);
static int unc_thrd_lock_acquire(struct unc_thrd_lock *lock);
static int unc_thrd_lock_acquiretimed(struct unc_thrd_lock *lock, Unc_Float t);
static int unc_thrd_lock_release(struct unc_thrd_lock *lock);
static void unc_thrd_lock_free(struct unc_thrd_lock *lock);

static int unc_thrd_rlock_new(struct unc_thrd_rlock *lock);
static int unc_thrd_rlock_acquire(struct unc_thrd_rlock *lock);
static int unc_thrd_rlock_acquiretimed(struct unc_thrd_rlock *lock,
                                        Unc_Float t);
static int unc_thrd_rlock_release(struct unc_thrd_rlock *lock);
static void unc_thrd_rlock_free(struct unc_thrd_rlock *rlock);

static int unc_thrd_sem_new(struct unc_thrd_sem *sem, Unc_Size cnt);
static int unc_thrd_sem_acquire(struct unc_thrd_sem *sem, Unc_Size cnt);
static int unc_thrd_sem_acquiretimed(struct unc_thrd_sem *sem,
                            Unc_Size cnt, Unc_Float t);
static int unc_thrd_sem_release(struct unc_thrd_sem *sem, Unc_Size cnt);
static void unc_thrd_sem_free(struct unc_thrd_sem *sem);

static int unc_thrd_mon_new(struct unc_thrd_mon *mon);
static int unc_thrd_mon_new_lock(struct unc_thrd_mon *mon,
                                 struct unc_thrd_lock *lock);
static int unc_thrd_mon_new_rlock(struct unc_thrd_mon *mon,
                                  struct unc_thrd_rlock *lock);
static int unc_thrd_mon_acquire(struct unc_thrd_mon *mon);
static int unc_thrd_mon_acquiretimed(struct unc_thrd_mon *mon, Unc_Float t);
static int unc_thrd_mon_release(struct unc_thrd_mon *mon);
static int unc_thrd_mon_wait(struct unc_thrd_mon *mon);
static int unc_thrd_mon_waittimed(struct unc_thrd_mon *mon, Unc_Float t);
static int unc_thrd_mon_notify(struct unc_thrd_mon *mon, Unc_Size count);
static int unc_thrd_mon_notifyall(struct unc_thrd_mon *mon);
static void unc_thrd_mon_free(struct unc_thrd_mon *mon);

static int unc_thrd_sleep(Unc_Float t);

struct unc_threadobj;
static int unc0_subthread(struct unc_threadobj *o);
INLINE struct unc_threadobj *unc0_getthreadobj(struct unc_thrd_thread *x);
INLINE int unc0_isthreadfinished(struct unc_threadobj *o);

#define THREADFAIL 1
#define THREADTIMEOUT 2
#define THREADBUSY 3
#define THREADUNDERLYING 4

#if UNCIL_MT_OK && UNCIL_MT_PTHREAD
#define SEMONMON 1
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <unistd.h>

static int unc_thrd_sleep(Unc_Float t) {
#if _POSIX_C_SOURCE >= 199309L
    struct timespec dur;
    dur.tv_sec = t;
    dur.tv_nsec = unc0_ffrac(t) * 1000000000UL;
    while (nanosleep(&dur, &dur))
        ;
#else
    unsigned int s = t;
    while ((s = sleep(s)))
        ;
    usleep(unc0_ffrac(t) * 1000000UL);
#endif
    return 0;
}

static int unc_thrd_makeerr(Unc_View *w, int e) {
    return e;
}

static int unc_thrd_fmterr(int e) {
    switch (e) {
    case 0:
        return 0;
    case ETIMEDOUT:
        return THREADTIMEOUT;
    case EBUSY:
        return THREADBUSY;
    case ENOMEM:
        return UNCIL_ERR_MEM;
    default:
        return THREADFAIL;
    }
}

static int unc_thrd_c11_make_timespec(struct timespec *ts, Unc_Float t) {
    if (clock_gettime(CLOCK_REALTIME, ts)) return 1;
    ts->tv_sec += t;
    ts->tv_nsec += unc0_ffrac(t) * 1000000000UL;
    if (ts->tv_nsec >= 1000000000UL) {
        ts->tv_nsec -= 1000000000UL;
        ++ts->tv_sec;
    }
    return 0;
}

struct unc_thrd_lock {
    pthread_mutex_t m;
};

struct unc_thrd_rlock {
    pthread_mutex_t m;
};

struct unc_thrd_mon {
    pthread_mutex_t m;
    pthread_mutex_t *pm;
    pthread_cond_t c;
};

struct unc_thrd_thread {
    Unc_AtomicSmall started;
    pthread_t t;
    struct unc_thrd_mon mon;
};

void *unc_thrd_thread__run(void *p) {
    unc0_subthread(p);
    return NULL;
}

static int unc_thrd_thread_new(struct unc_thrd_thread *x) {
    ATOMICSSET(x->started, 0);
    return unc_thrd_mon_new(&x->mon);
}

static int unc_thrd_thread_start(struct unc_thrd_thread *x) {
    if (ATOMICSXCG(x->started, 1))
        return THREADFAIL;
    return unc_thrd_fmterr(pthread_create(&x->t,
                NULL,
                &unc_thrd_thread__run,
                unc0_getthreadobj(x)));
}

static int unc_thrd_thread_join(struct unc_thrd_thread *x) {
    return unc_thrd_fmterr(pthread_join(x->t, NULL));
}

static int unc_thrd_thread_jointimed(struct unc_thrd_thread *x, Unc_Float t) {
    int e;
    unc0_countdown cd;
    struct unc_threadobj *o = unc0_getthreadobj(x);
    e = unc0_countdownfirst(&cd);
    if (e) return e;
    e = unc_thrd_mon_acquiretimed(&x->mon, t);
    if (e) return e;
    if (unc0_countdownnext(&cd, &t)) {
        unc_thrd_mon_release(&x->mon);
        return THREADFAIL;
    }
    while (!unc0_isthreadfinished(o)) {
        if (t < 0)
            t = 0;
        else if ((e = unc_thrd_mon_waittimed(&x->mon, t))) {
            unc_thrd_mon_release(&x->mon);
            return e;
        }
        if (!t) {
            unc_thrd_mon_release(&x->mon);
            return THREADTIMEOUT;
        }
        if (unc0_countdownnext(&cd, &t)) {
            unc_thrd_mon_release(&x->mon);
            return THREADFAIL;
        }
    }
    unc_thrd_mon_release(&x->mon);
    return 0;
}

static void unc_thrd_thread_kill(struct unc_thrd_thread *x) {
    if (x->started) {
        pthread_kill(x->t, SIGTERM);
        pthread_detach(x->t);
    }
}

static void unc_thrd_thread_detach(struct unc_thrd_thread *x) {
    if (x->started) pthread_detach(x->t);
}

static void unc_thrd_thread_finished(struct unc_thrd_thread *x) {
    unc_thrd_mon_notifyall(&x->mon);
}

static int unc_thrd_lock_new(struct unc_thrd_lock *lock) {
    return unc_thrd_fmterr(pthread_mutex_init(&lock->m, NULL));
}
static int unc_thrd_lock_acquire(struct unc_thrd_lock *lock) {
    return unc_thrd_fmterr(pthread_mutex_lock(&lock->m));
}
static int unc_thrd_lock_acquiretimed(struct unc_thrd_lock *lock, Unc_Float t) {
    struct timespec ts;
    if (!t) return unc_thrd_fmterr(pthread_mutex_trylock(&lock->m));
    if (unc_thrd_c11_make_timespec(&ts, t)) return THREADFAIL;
    return unc_thrd_fmterr(pthread_mutex_timedlock(&lock->m, &ts));
}
static int unc_thrd_lock_release(struct unc_thrd_lock *lock) {
    return unc_thrd_fmterr(pthread_mutex_unlock(&lock->m));
}
static void unc_thrd_lock_free(struct unc_thrd_lock *lock) {
    pthread_mutex_destroy(&lock->m);
}

static int unc_thrd_rlock_new(struct unc_thrd_rlock *lock) {
    int e;
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    e = unc_thrd_fmterr(pthread_mutex_init(&lock->m, &attr));
    pthread_mutexattr_destroy(&attr);
    return e;
}
static int unc_thrd_rlock_acquire(struct unc_thrd_rlock *lock) {
    return unc_thrd_fmterr(pthread_mutex_lock(&lock->m));
}
static int unc_thrd_rlock_acquiretimed(struct unc_thrd_rlock *lock,
                                        Unc_Float t) {
    struct timespec ts;
    if (!t) return unc_thrd_fmterr(pthread_mutex_trylock(&lock->m));
    if (unc_thrd_c11_make_timespec(&ts, t)) return THREADFAIL;
    return unc_thrd_fmterr(pthread_mutex_timedlock(&lock->m, &ts));
}
static int unc_thrd_rlock_release(struct unc_thrd_rlock *lock) {
    return unc_thrd_fmterr(pthread_mutex_unlock(&lock->m));
}
static void unc_thrd_rlock_free(struct unc_thrd_rlock *lock) {
    pthread_mutex_destroy(&lock->m);
}

static int unc_thrd_mon_new(struct unc_thrd_mon *mon) {
    int e = pthread_mutex_init(&mon->m, NULL);
    if (e) return unc_thrd_fmterr(e);
    e = pthread_cond_init(&mon->c, NULL);
    if (e) pthread_mutex_destroy(&mon->m);
    mon->pm = &mon->m;
    return unc_thrd_fmterr(e);
}
static int unc_thrd_mon_new_lock(struct unc_thrd_mon *mon,
                                 struct unc_thrd_lock *lock) {
    mon->pm = &lock->m;
    return unc_thrd_fmterr(pthread_cond_init(&mon->c, NULL));
}
static int unc_thrd_mon_new_rlock(struct unc_thrd_mon *mon,
                                  struct unc_thrd_rlock *lock) {
    mon->pm = &lock->m;
    return unc_thrd_fmterr(pthread_cond_init(&mon->c, NULL));
}
static int unc_thrd_mon_acquire(struct unc_thrd_mon *mon) {
    return unc_thrd_fmterr(pthread_mutex_lock(mon->pm));
}
static int unc_thrd_mon_acquiretimed(struct unc_thrd_mon *mon, Unc_Float t) {
    struct timespec ts;
    if (!t) return unc_thrd_fmterr(pthread_mutex_trylock(mon->pm));
    if (unc_thrd_c11_make_timespec(&ts, t)) return THREADFAIL;
    return unc_thrd_fmterr(pthread_mutex_timedlock(mon->pm, &ts));
}
static int unc_thrd_mon_release(struct unc_thrd_mon *mon) {
    return unc_thrd_fmterr(pthread_mutex_unlock(mon->pm));
}
static int unc_thrd_mon_wait(struct unc_thrd_mon *mon) {
    return unc_thrd_fmterr(pthread_cond_wait(&mon->c, mon->pm));
}
static int unc_thrd_mon_waittimed(struct unc_thrd_mon *mon, Unc_Float t) {
    struct timespec ts;
    if (unc_thrd_c11_make_timespec(&ts, t)) return THREADFAIL;
    return unc_thrd_fmterr(pthread_cond_timedwait(&mon->c, mon->pm, &ts));
}
static int unc_thrd_mon_notify(struct unc_thrd_mon *mon, Unc_Size n) {
    return unc_thrd_fmterr(pthread_cond_signal(&mon->c));
}
static int unc_thrd_mon_notifyall(struct unc_thrd_mon *mon) {
    return unc_thrd_fmterr(pthread_cond_broadcast(&mon->c));
}
static void unc_thrd_mon_free(struct unc_thrd_mon *mon) {
    pthread_cond_destroy(&mon->c);
    if (mon->pm == &mon->m) pthread_mutex_destroy(mon->pm);
}

#elif UNCIL_MT_OK && UNCIL_MT_C11
#define SEMONMON 1
#include <threads.h>

static int unc_thrd_sleep(Unc_Float t) {
    struct timespec dur;
    dur.tv_sec = t;
    dur.tv_nsec = unc0_ffrac(t) * 1000000000UL;
    while (thrd_sleep(&dur, &dur))
        ;
    return 0;
}

static int unc_thrd_makeerr(Unc_View *w, int e) {
    return e;
}

static int unc_thrd_fmterr(int e) {
    switch (e) {
    case thrd_success:
        return 0;
    case thrd_timedout:
        return THREADTIMEOUT;
    case thrd_busy:
        return THREADBUSY;
    case thrd_nomem:
        return UNCIL_ERR_MEM;
    /* case thrd_error: */
    default:
        return THREADFAIL;
    }
}

static int unc_thrd_c11_make_timespec(struct timespec *ts, Unc_Float t) {
    if (!timespec_get(ts, TIME_UTC)) return 1;
    ts->tv_sec += t;
    ts->tv_nsec += unc0_ffrac(t) * 1000000000UL;
    if (ts->tv_nsec >= 1000000000UL) {
        ts->tv_nsec -= 1000000000UL;
        ++ts->tv_sec;
    }
    return 0;
}

struct unc_thrd_lock {
    mtx_t m;
};

struct unc_thrd_rlock {
    mtx_t m;
};

struct unc_thrd_mon {
    mtx_t m;
    mtx_t *pm;
    cnd_t c;
};

struct unc_thrd_thread {
    Unc_AtomicSmall started;
    thrd_t t;
    struct unc_thrd_mon mon;
};

int unc_thrd_thread__run(void *p) {
    thrd_exit(unc0_subthread(p) ? EXIT_FAILURE : EXIT_SUCCESS);
}

static int unc_thrd_thread_new(struct unc_thrd_thread *x) {
    ATOMICSSET(x->started, 0);
    return unc_thrd_mon_new(&x->mon);
}

static int unc_thrd_thread_start(struct unc_thrd_thread *x) {
    if (ATOMICSXCG(x->started, 1))
        return THREADFAIL;
    return unc_thrd_fmterr(thrd_create(&x->t,
                &unc_thrd_thread__run, unc0_getthreadobj(x)));
}

static int unc_thrd_thread_join(struct unc_thrd_thread *x) {
    return unc_thrd_fmterr(thrd_join(x->t, NULL));
}

static int unc_thrd_thread_jointimed(struct unc_thrd_thread *x, Unc_Float t) {
    int e;
    unc0_countdown cd;
    struct unc_threadobj *o = unc0_getthreadobj(x);
    e = unc0_countdownfirst(&cd);
    if (e) return e;
    e = unc_thrd_mon_acquiretimed(&x->mon, t);
    if (e) return e;
    if (unc0_countdownnext(&cd, &t)) {
        unc_thrd_mon_release(&x->mon);
        return THREADFAIL;
    }
    while (!unc0_isthreadfinished(o)) {
        if (t < 0)
            t = 0;
        else if ((e = unc_thrd_mon_waittimed(&x->mon, t))) {
            unc_thrd_mon_release(&x->mon);
            return e;
        }
        if (!t) {
            unc_thrd_mon_release(&x->mon);
            return THREADTIMEOUT;
        }
        if (unc0_countdownnext(&cd, &t)) {
            unc_thrd_mon_release(&x->mon);
            return THREADFAIL;
        }
    }
    unc_thrd_mon_release(&x->mon);
    return 0;
}

static void unc_thrd_thread_kill(struct unc_thrd_thread *x) {
    if (x->started) thrd_detach(x->t);
}

static void unc_thrd_thread_detach(struct unc_thrd_thread *x) {
    if (x->started) thrd_detach(x->t);
}

static void unc_thrd_thread_finished(struct unc_thrd_thread *x) {
    unc_thrd_mon_notifyall(&x->mon);
}

static int unc_thrd_lock_new(struct unc_thrd_lock *lock) {
    return unc_thrd_fmterr(mtx_init(&lock->m, mtx_timed));
}
static int unc_thrd_lock_acquire(struct unc_thrd_lock *lock) {
    return unc_thrd_fmterr(mtx_lock(&lock->m));
}
static int unc_thrd_lock_acquiretimed(struct unc_thrd_lock *lock, Unc_Float t) {
    struct timespec ts;
    if (!t) return unc_thrd_fmterr(mtx_trylock(&lock->m));
    if (unc_thrd_c11_make_timespec(&ts, t)) return THREADFAIL;
    return unc_thrd_fmterr(mtx_timedlock(&lock->m, &ts));
}
static int unc_thrd_lock_release(struct unc_thrd_lock *lock) {
    return unc_thrd_fmterr(mtx_unlock(&lock->m));
}
static void unc_thrd_lock_free(struct unc_thrd_lock *lock) {
    mtx_destroy(&lock->m);
}

static int unc_thrd_rlock_new(struct unc_thrd_rlock *lock) {
    return unc_thrd_fmterr(mtx_init(&lock->m, mtx_timed | mtx_recursive));
}
static int unc_thrd_rlock_acquire(struct unc_thrd_rlock *lock) {
    return unc_thrd_fmterr(mtx_lock(&lock->m));
}
static int unc_thrd_rlock_acquiretimed(struct unc_thrd_rlock *lock,
                                        Unc_Float t) {
    struct timespec ts;
    if (!t) return unc_thrd_fmterr(mtx_trylock(&lock->m));
    if (unc_thrd_c11_make_timespec(&ts, t)) return THREADFAIL;
    return unc_thrd_fmterr(mtx_timedlock(&lock->m, &ts));
}
static int unc_thrd_rlock_release(struct unc_thrd_rlock *lock) {
    return unc_thrd_fmterr(mtx_unlock(&lock->m));
}
static void unc_thrd_rlock_free(struct unc_thrd_rlock *lock) {
    mtx_destroy(&lock->m);
}

static int unc_thrd_mon_new(struct unc_thrd_mon *mon) {
    int e = mtx_init(&mon->m, mtx_timed);
    if (e) return unc_thrd_fmterr(e);
    e = cnd_init(&mon->c);
    if (e) mtx_destroy(&mon->m);
    mon->pm = &mon->m;
    return unc_thrd_fmterr(e);
}
static int unc_thrd_mon_new_lock(struct unc_thrd_mon *mon,
                                 struct unc_thrd_lock *lock) {
    mon->pm = &lock->m;
    return unc_thrd_fmterr(cnd_init(&mon->c));
}
static int unc_thrd_mon_new_rlock(struct unc_thrd_mon *mon,
                                  struct unc_thrd_rlock *lock) {
    mon->pm = &lock->m;
    return unc_thrd_fmterr(cnd_init(&mon->c));
}
static int unc_thrd_mon_acquire(struct unc_thrd_mon *mon) {
    return unc_thrd_fmterr(mtx_lock(mon->pm));
}
static int unc_thrd_mon_acquiretimed(struct unc_thrd_mon *mon, Unc_Float t) {
    struct timespec ts;
    if (!t) return unc_thrd_fmterr(mtx_trylock(mon->pm));
    if (unc_thrd_c11_make_timespec(&ts, t)) return THREADFAIL;
    return unc_thrd_fmterr(mtx_timedlock(mon->pm, &ts));
}
static int unc_thrd_mon_release(struct unc_thrd_mon *mon) {
    return unc_thrd_fmterr(mtx_unlock(mon->pm));
}
static int unc_thrd_mon_wait(struct unc_thrd_mon *mon) {
    return unc_thrd_fmterr(cnd_wait(&mon->c, mon->pm));
}
static int unc_thrd_mon_waittimed(struct unc_thrd_mon *mon, Unc_Float t) {
    struct timespec ts;
    if (unc_thrd_c11_make_timespec(&ts, t)) return THREADFAIL;
    return unc_thrd_fmterr(cnd_timedwait(&mon->c, mon->pm, &ts));
}
static int unc_thrd_mon_notify(struct unc_thrd_mon *mon, Unc_Size n) {
    return unc_thrd_fmterr(cnd_signal(&mon->c));
}
static int unc_thrd_mon_notifyall(struct unc_thrd_mon *mon) {
    return unc_thrd_fmterr(cnd_broadcast(&mon->c));
}
static void unc_thrd_mon_free(struct unc_thrd_mon *mon) {
    cnd_destroy(&mon->c);
    if (mon->pm == &mon->m) mtx_destroy(mon->pm);
}

#elif UNCIL_MT_OK
#error implement unc_thrd_ calls for your library!
#else
struct unc_thrd_thread { char t_; };
struct unc_thrd_lock { char t_; };
struct unc_thrd_rlock { char t_; };
struct unc_thrd_sem { char t_; };
struct unc_thrd_mon { char t_; };

static int unc_thrd_makeerr(Unc_View *w, int e) {
    return e;
}

static int unc_thrd_sleep(Unc_Float t) {
    time_t t0 = time(NULL), t1;
    if (t0 == (time_t)(-1)) return UNCIL_ERR_INTERNAL;
    while (time(&t1) != (time_t)(-1) && (difftime(t1, t0) < t))
        UNC_YIELD();
    return 0;
}

static int unc_thrd_thread_new(struct unc_thrd_thread *x) {
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
}
static int unc_thrd_thread_start(struct unc_thrd_thread *x) {
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
}
static int unc_thrd_thread_join(struct unc_thrd_thread *x) {
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
}
static int unc_thrd_thread_jointimed(struct unc_thrd_thread *x, Unc_Float t) {
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
}
static void unc_thrd_thread_kill(struct unc_thrd_thread *x) { }
static void unc_thrd_thread_detach(struct unc_thrd_thread *x) { }
static void unc_thrd_thread_finished(struct unc_thrd_thread *x) { }

static int unc_thrd_lock_new(struct unc_thrd_lock *lock) {
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
}
static int unc_thrd_lock_acquire(struct unc_thrd_lock *lock) {
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
}
static int unc_thrd_lock_acquiretimed(struct unc_thrd_lock *lock, Unc_Float t) {
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
}
static int unc_thrd_lock_release(struct unc_thrd_lock *lock) {
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
}
static void unc_thrd_lock_free(struct unc_thrd_lock *lock) { }

static int unc_thrd_rlock_new(struct unc_thrd_rlock *lock) {
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
}
static int unc_thrd_rlock_acquire(struct unc_thrd_rlock *lock) {
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
}
static int unc_thrd_rlock_acquiretimed(struct unc_thrd_rlock *lock,
                                        Unc_Float t) {
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
}
static int unc_thrd_rlock_release(struct unc_thrd_rlock *lock) {
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
}
static void unc_thrd_rlock_free(struct unc_thrd_rlock *lock) { }

static int unc_thrd_sem_new(struct unc_thrd_sem *sem, Unc_Size n) {
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
}
static int unc_thrd_sem_acquire(struct unc_thrd_sem *sem, Unc_Size n) {
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
}
static int unc_thrd_sem_acquiretimed(struct unc_thrd_sem *sem,
                              Unc_Size n, Unc_Float t) {
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
}
static int unc_thrd_sem_release(struct unc_thrd_sem *sem, Unc_Size n) {
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
}
static void unc_thrd_sem_free(struct unc_thrd_sem *sem) { }

static int unc_thrd_mon_new(struct unc_thrd_mon *mon) {
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
}
static int unc_thrd_mon_new_lock(struct unc_thrd_mon *mon,
                                 struct unc_thrd_lock *lock) {
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
}
static int unc_thrd_mon_new_rlock(struct unc_thrd_mon *mon,
                                  struct unc_thrd_rlock *lock) {
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
}
static int unc_thrd_mon_acquire(struct unc_thrd_mon *mon) {
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
}
static int unc_thrd_mon_acquiretimed(struct unc_thrd_mon *mon, Unc_Float t) {
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
}
static int unc_thrd_mon_release(struct unc_thrd_mon *mon) {
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
}
static int unc_thrd_mon_wait(struct unc_thrd_mon *mon) {
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
}
static int unc_thrd_mon_waittimed(struct unc_thrd_mon *mon, Unc_Float t) {
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
}
static int unc_thrd_mon_notify(struct unc_thrd_mon *mon, Unc_Size n) {
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
}
static int unc_thrd_mon_notifyall(struct unc_thrd_mon *mon) {
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
}
static void unc_thrd_mon_free(struct unc_thrd_mon *mon) { }
#endif

#if SEMONMON
struct unc_thrd_sem {
    struct unc_thrd_mon mon;
    Unc_Size counter;
};

static int unc_thrd_sem_new(struct unc_thrd_sem *sem, Unc_Size cnt) {
    sem->counter = cnt;
    return unc_thrd_mon_new(&sem->mon);
}

static int unc_thrd_sem_acquire(struct unc_thrd_sem *sem, Unc_Size cnt) {
    int e;
    e = unc_thrd_mon_acquire(&sem->mon);
    if (e) return e;
    while (sem->counter < cnt) {
        if ((e = unc_thrd_mon_wait(&sem->mon))) {
            unc_thrd_mon_release(&sem->mon);
            return e;
        }
    }
    sem->counter -= cnt;
    unc_thrd_mon_release(&sem->mon);
    return 0;
}

static int unc_thrd_sem_acquiretimed(struct unc_thrd_sem *sem,
                            Unc_Size cnt, Unc_Float timeout) {
    int e;
    unc0_countdown cd;
    e = unc0_countdownfirst(&cd);
    if (e) return e;
    e = unc_thrd_mon_acquiretimed(&sem->mon, timeout);
    if (e) return e;
    if (unc0_countdownnext(&cd, &timeout)) {
        unc_thrd_mon_release(&sem->mon);
        return THREADFAIL;
    }
    while (sem->counter < cnt) {
        if (timeout < 0)
            timeout = 0;
        else if ((e = unc_thrd_mon_waittimed(&sem->mon, timeout))) {
            unc_thrd_mon_release(&sem->mon);
            return e;
        }
        if (!timeout) {
            unc_thrd_mon_release(&sem->mon);
            return THREADTIMEOUT;
        }
        if (unc0_countdownnext(&cd, &timeout)) {
            unc_thrd_mon_release(&sem->mon);
            return THREADFAIL;
        }
    }
    sem->counter -= cnt;
    unc_thrd_mon_release(&sem->mon);
    return 0;
}

static int unc_thrd_sem_release(struct unc_thrd_sem *sem, Unc_Size cnt) {
    int e = unc_thrd_mon_acquire(&sem->mon);
    if (e) return e;
    sem->counter += cnt;
    unc_thrd_mon_notifyall(&sem->mon);
    unc_thrd_mon_release(&sem->mon);
    return 0;
}

static void unc_thrd_sem_free(struct unc_thrd_sem *sem) {
    unc_thrd_mon_free(&sem->mon);
}
#endif

#if UNCIL_MT_OK

struct unc_threadobj {
    Unc_Thread u;
    Unc_Value f;
    Unc_Size a;
    Unc_AtomicSmall f_run;
    Unc_AtomicSmall f_done;
    Unc_AtomicSmall f_detach;
    struct unc_thrd_thread t;
    UNC_LOCKLIGHT(lock)
    int phase;
};

static int unc0_subthread(struct unc_threadobj *o) {
    int e;
    Unc_View *w = o->u.view;
    Unc_World *ww = w->world;
    struct unc_thrd_thread *x = &o->t;
    int fromcode = 0;

    UNC_LOCKL(o->lock);
    UNC_UNLOCKL(o->lock);
    if (o->f_detach) {
        e = UNCIL_ERR_HALT;
        goto unc0_subthread_detached;
    }

    if (ww->finalize) {
        e = UNCIL_ERR_LOGIC_FINISHING;
        fromcode = 1;
    } else {
        Unc_Pile pile;
        Unc_Size an;
        Unc_Value f = UNC_BLANK;
        UNC_LOCKL(o->lock);
        an = o->a;
        if (o->f_detach) {
            UNC_UNLOCKL(o->lock);
            e = UNCIL_ERR_HALT;
            fromcode = 1;
        } else {
            unc_move(w, &f, &o->f);
            UNC_UNLOCKL(o->lock);
            e = unc_call(w, &f, an, &pile);
            if (!e) unc_discard(w, &pile);
        }
        unc_clear(w, &f);
    }

    if (e) {
        /* error! */
        Unc_Value exc = UNC_BLANK;
        char buf[256];
        int e2;
        Unc_Size esn;
        char *esc;
        if (fromcode)
            unc_getexceptionfromcode(w, &exc, e);
        else
            unc_getexception(w, &exc);
        e2 = unc_exceptiontostringn(w, &exc, &esn, &esc);
        if (e2) {
            esn = sizeof(buf);
            esc = buf;
            if (unc_exceptiontostring(w, &exc, &esn, buf))
                strcpy(buf, "unknown error in thread");
        }
        fprintf(stderr, "unhandled Uncil exception in thread\n%s\n", esc);
        if (esc != buf) unc_mfree(w, esc);
    }

    if (ATOMICSXCG(o->f_done, 1))
        goto unc0_subthread_detached;
    unc_thrd_thread_finished(x);

    UNC_LOCKL(o->lock);
    o->u.view = NULL;
    VSETNULL(w, &o->f);
    unc_destroy(w);
    UNC_UNLOCKL(o->lock);
    return e;
unc0_subthread_detached:
    VSETNULL(w, &o->f);
    unc_destroy(w);
    return e;
}

INLINE struct unc_threadobj *unc0_getthreadobj(struct unc_thrd_thread *x) {
    return (struct unc_threadobj *)((char *)(x)
                            - offsetof(struct unc_threadobj, t));
}

INLINE int unc0_isthreadfinished(struct unc_threadobj *o) {
    return o->f_done;
}

void unc0_waitonviewthread(Unc_View *w) {
    if (VGETTYPE(&w->threadme) == Unc_TOpaque) {
        struct unc_threadobj *o =
            LEFTOVER(Unc_Opaque, VGETENT(&w->threadme))->data;
        struct unc_thrd_thread *t = &o->t;
        if (!(o->u.flags & UNCIL_THREAD_FLAG_DAEMON)) {
            while (unc_thrd_thread_join(t))
                ;
        }
    }
}
#endif

static Unc_RetVal unc0_thread_thread_destr(Unc_View *w, size_t n, void *data) {
#if UNCIL_MT_OK
    /* a thread should never get destroyed if it is still running */
    struct unc_threadobj *o = data;
    ATOMICSSET(o->f_detach, 1);
    if (o->phase >= 1)
        UNC_LOCKL(o->lock);
    if (o->phase >= 2)
        unc_thrd_thread_kill(&o->t);
    if (!o->f_run) {
        VSETNULL(o->u.view, &o->f);
        unc_destroy(o->u.view);
    }
    if (o->phase >= 1) {
        UNC_UNLOCKL(o->lock);
        UNC_LOCKFINAL(o->lock);
    }
#endif
    return 0;
}

static Unc_RetVal unc0_thread_lock_destr(Unc_View *w, size_t n, void *data) {
    unc_thrd_lock_free((struct unc_thrd_lock *)data);
    return 0;
}

static Unc_RetVal unc0_thread_rlock_destr(Unc_View *w, size_t n, void *data) {
    unc_thrd_rlock_free((struct unc_thrd_rlock *)data);
    return 0;
}

static Unc_RetVal unc0_thread_sem_destr(Unc_View *w, size_t n, void *data) {
    unc_thrd_sem_free((struct unc_thrd_sem *)data);
    return 0;
}

static Unc_RetVal unc0_thread_mon_destr(Unc_View *w, size_t n, void *data) {
    unc_thrd_mon_free((struct unc_thrd_mon *)data);
    return 0;
}

static int unc0_lib_thread_makeerr(Unc_View *w, int e) {
    switch (e) {
    case THREADFAIL:
        return unc_throwexc(w, "system", "synchronization error");
    case THREADTIMEOUT:
        return unc_throwexc(w, "system", "timed out before acquire/wait");
    case THREADBUSY:
        return unc_throwexc(w, "system", "synchronization resource busy");
    case THREADUNDERLYING:
        return unc_thrd_makeerr(w, e);
    default:
        return e;
    }
}

static Unc_RetVal unc0_thread_checktype(Unc_View *w, Unc_Value *v,
                                        Unc_Value *prototype,
                                        void **outp,
                                        const char *emsg) {
    if (unc_gettype(w, v) == Unc_TOpaque) {
        int eq;
        Unc_Value proto = UNC_BLANK;
        unc_getprototype(w, v, &proto);
        eq = unc_issame(w, &proto, prototype);
        unc_clear(w, &proto);
        if (eq) {
            int e;
            void *opq;
            e = unc_lockopaque(w, v, NULL, &opq);
            if (e) return e;
            *outp = opq;
            return 0;
        }
    }
    if (!emsg) return UNCIL_ERR_TYPE_NOTOPAQUE;
    return unc_throwexc(w, "type", emsg);
}

static Unc_RetVal unc0_lib_thread_thread_new(
                    Unc_View *w, Unc_Tuple args, void *udata) {
#if UNCIL_MT_OK
    int e;
    Unc_Value v = UNC_BLANK;
    Unc_Size an;
    Unc_Value *a;
    Unc_View *zw;
    struct unc_threadobj *thr;
    int daemon;
    if (!unc_iscallable(w, &args.values[0]))
        return unc_throwexc(w, "type", "thread function must be callable");
    switch (unc_gettype(w, &args.values[1])) {
    case Unc_TNull:
        a = NULL;
        an = 0;
        break;
    case Unc_TArray:
        e = unc_lockarray(w, &args.values[1], &an, &a);
        if (e) return e;
        break;
    default:
        return unc_throwexc(w, "type", "thread function args must be an array");
    }
    e = unc_newopaque(w, &v, unc_boundvalue(w, 0),
                        sizeof(struct unc_threadobj), (void **)&thr,
                        &unc0_thread_thread_destr, 0, NULL, 0, NULL);
    if (e) {
        unc_unlock(w, &args.values[1]);
        return e;
    }
    daemon = unc_getbool(w, &args.values[2],
                    w->vtype == Unc_ViewTypeSubDaemon);
    thr->phase = 0;
    VIMPOSE(w, &thr->f, &args.values[0]);
    thr->a = an;
    ATOMICSSET(thr->f_run, 0);
    ATOMICSSET(thr->f_done, 0);
    ATOMICSSET(thr->f_detach, 0);
    e = UNC_LOCKINITL(thr->lock);
    if (e)
        zw = thr->u.view = NULL;
    else {
        thr->phase = 1;
        (void)UNC_LOCKFP(w, w->world->viewlist_lock);
        zw = thr->u.view = unc0_newview(w->world,
                daemon ? Unc_ViewTypeSubDaemon : Unc_ViewTypeSub);
        UNC_UNLOCKF(w->world->viewlist_lock);
        if (!zw)
            e = UNCIL_ERR_MEM;
        else {
            unc_copyprogram(zw, w);
            if (an) e = unc_push(zw, an, a, NULL);
        }
    }
    unc_unlock(w, &args.values[1]);
    thr->u.up = thr->u.down = NULL;
    thr->u.flags = daemon ? UNCIL_THREAD_FLAG_DAEMON : 0;
    if (!e) {
        e = unc_thrd_thread_new(&thr->t);
        if (e) e = unc0_lib_thread_makeerr(w, e);
        else thr->phase = 2;
    }
    if (e && zw)
        unc_destroy(zw);
    unc_unlock(w, &v);
    if (!e) e = unc_push(w, 1, &v, NULL);
    unc_clear(w, &v);
    return e;
#else
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
}

static Unc_RetVal unc0_lib_thread_thread_start(
                    Unc_View *w, Unc_Tuple args, void *udata) {
#if UNCIL_MT_OK
    int e;
    struct unc_threadobj *thr;
    e = unc0_thread_checktype(w, &args.values[0], unc_boundvalue(w, 0),
                    (void **)&thr, "argument not a thread.thread");
    if (e) return e;

    UNC_LOCKL(thr->lock);
    if (thr->f_run)
        e = unc_throwexc(w, "value", "thread already started");
    if (thr->f_detach)
        e = unc_throwexc(w, "value", "thread not valid");
    if (!e) {
        ATOMICSSET(thr->f_run, 1);
        e = unc_thrd_thread_start(&thr->t);
        if (e) e = unc0_lib_thread_makeerr(w, e);
        else {
            Unc_View *zw = thr->u.view;
            VIMPOSE(zw, &zw->threadme, &args.values[0]);
        }
    }
    UNC_UNLOCKL(thr->lock);
    unc_unlock(w, &args.values[0]);
    return e;
#else
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
}

static Unc_RetVal unc0_lib_thread_thread_join(
                    Unc_View *w, Unc_Tuple args, void *udata) {
#if UNCIL_MT_OK
    int e, finished;
    struct unc_threadobj *thr;
    e = unc0_thread_checktype(w, &args.values[0], unc_boundvalue(w, 0),
                    (void **)&thr, "argument not a thread.thread");
    if (e) return e;

    UNC_LOCKL(thr->lock);
    if (!thr->f_run)
        e = unc_throwexc(w, "value", "thread not started");
    if (thr->f_detach)
        e = unc_throwexc(w, "value", "thread not valid");
    finished = thr->f_done;
    UNC_UNLOCKL(thr->lock);
    if (!e && !finished) {
        unc_vmpause(w);
        e = unc_thrd_thread_join(&thr->t);
        unc_vmresume(w);
    }
    if (e) e = unc0_lib_thread_makeerr(w, e);
    unc_unlock(w, &args.values[0]);
    return e;
#else
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
}

static Unc_RetVal unc0_lib_thread_thread_jointimed(
                    Unc_View *w, Unc_Tuple args, void *udata) {
#if UNCIL_MT_OK
    int e, finished;
    Unc_Float to;
    Unc_Value v = UNC_BLANK;
    struct unc_threadobj *thr;
    e = unc_getfloat(w, &args.values[1], &to);
    if (e) return e;
    if (to < 0)
        return unc_throwexc(w, "value", "timeout cannot be negative");
    if (!unc0_fisfinite(to))
        return unc_throwexc(w, "value", "timeout must be finite");
    e = unc0_thread_checktype(w, &args.values[0], unc_boundvalue(w, 0),
                    (void **)&thr, "argument not a thread.thread");
    if (e) return e;

    UNC_LOCKL(thr->lock);
    if (!thr->f_run)
        e = unc_throwexc(w, "value", "thread not started");
    if (thr->f_detach)
        e = unc_throwexc(w, "value", "thread not valid");
    finished = thr->f_done;
    UNC_UNLOCKL(thr->lock);
    if (!e && !finished) {
        unc_vmpause(w);
        e = unc_thrd_thread_jointimed(&thr->t, to);
        if (e == THREADTIMEOUT)
            e = 0;
        unc_vmresume(w);
        unc_setbool(w, &v, !e);
    }
    if (e) e = unc0_lib_thread_makeerr(w, e);
    unc_unlock(w, &args.values[0]);
    if (!e) e = unc_pushmove(w, &v, NULL);
    return e;
#else
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
}

static Unc_RetVal unc0_lib_thread_thread_halt(
                    Unc_View *w, Unc_Tuple args, void *udata) {
#if UNCIL_MT_OK
    int e;
    struct unc_threadobj *thr;
    e = unc0_thread_checktype(w, &args.values[0], unc_boundvalue(w, 0),
                    (void **)&thr, "argument not a thread.thread");
    if (e) return e;

    UNC_LOCKL(thr->lock);
    if (!thr->f_run)
        e = unc_throwexc(w, "value", "thread not started");
    if (thr->f_detach)
        e = unc_throwexc(w, "value", "thread not valid");
    if (!e && !thr->f_done) unc0_haltview(thr->u.view);
    UNC_UNLOCKL(thr->lock);
    unc_unlock(w, &args.values[0]);
    return e;
#else
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
}

static Unc_RetVal unc0_lib_thread_thread_hasfinished(
                    Unc_View *w, Unc_Tuple args, void *udata) {
#if UNCIL_MT_OK
    int e;
    Unc_Value v = UNC_BLANK;
    struct unc_threadobj *thr;
    e = unc0_thread_checktype(w, &args.values[0], unc_boundvalue(w, 0),
                    (void **)&thr, "argument not a thread.thread");
    if (e) return e;

    UNC_LOCKL(thr->lock);
    if (thr->f_detach)
        e = unc_throwexc(w, "value", "thread not valid");
    unc_setbool(w, &v, thr->f_done);
    UNC_UNLOCKL(thr->lock);
    unc_unlock(w, &args.values[0]);
    if (e) return e;
    return unc_pushmove(w, &v, NULL);
#else
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
}

static Unc_RetVal unc0_lib_thread_lock_new(
                    Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    Unc_Value v = UNC_BLANK;
    struct unc_thrd_lock *lock;
    e = unc_newopaque(w, &v, unc_boundvalue(w, 0),
                        sizeof(struct unc_thrd_lock), (void **)&lock,
                        &unc0_thread_lock_destr, 0, NULL, 0, NULL);
    if (e) return e;
    e = unc_thrd_lock_new(lock);
    if (e) e = unc0_lib_thread_makeerr(w, e);
    unc_unlock(w, &v);
    if (!e) e = unc_pushmove(w, &v, NULL);
    if (e) unc_clear(w, &v);
    return e;
}

static Unc_RetVal unc0_lib_thread_lock_acquire(
                    Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    struct unc_thrd_lock *lock;
    e = unc0_thread_checktype(w, &args.values[0], unc_boundvalue(w, 0),
                    (void **)&lock, "argument not a thread.lock");
    if (e) return e;
    unc_vmpause(w);
    e = unc_thrd_lock_acquire(lock);
    unc_vmresume(w);
    if (e) e = unc0_lib_thread_makeerr(w, e);
    unc_unlock(w, &args.values[0]);
    return e;
}

static Unc_RetVal unc0_lib_thread_lock_acquiretimed(
                    Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    Unc_Float to;
    struct unc_thrd_lock *lock;
    e = unc_getfloat(w, &args.values[1], &to);
    if (e) return e;
    if (to < 0)
        return unc_throwexc(w, "value", "timeout cannot be negative");
    if (!unc0_fisfinite(to))
        return unc_throwexc(w, "value", "timeout must be finite");
    e = unc0_thread_checktype(w, &args.values[0], unc_boundvalue(w, 0),
                    (void **)&lock, "argument not a thread.lock");
    if (e) return e;
    unc_vmpause(w);
    e = unc_thrd_lock_acquiretimed(lock, to);
    unc_vmresume(w);
    if (e) e = unc0_lib_thread_makeerr(w, e);
    unc_unlock(w, &args.values[0]);
    return e;
}

static Unc_RetVal unc0_lib_thread_lock_release(
                    Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    struct unc_thrd_lock *lock;
    e = unc0_thread_checktype(w, &args.values[0], unc_boundvalue(w, 0),
                    (void **)&lock, "argument not a thread.lock");
    if (e) return e;
    e = unc_thrd_lock_release(lock);
    if (e) e = unc0_lib_thread_makeerr(w, e);
    unc_unlock(w, &args.values[0]);
    return e;
}

static Unc_RetVal unc0_lib_thread_rlock_new(
                    Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    Unc_Value v = UNC_BLANK;
    struct unc_thrd_rlock *rlock;
    e = unc_newopaque(w, &v, unc_boundvalue(w, 0),
                        sizeof(struct unc_thrd_rlock), (void **)&rlock,
                        &unc0_thread_rlock_destr, 0, NULL, 0, NULL);
    if (e) return e;
    e = unc_thrd_rlock_new(rlock);
    if (e) e = unc0_lib_thread_makeerr(w, e);
    unc_unlock(w, &v);
    if (!e) e = unc_pushmove(w, &v, NULL);
    if (e) unc_clear(w, &v);
    return e;
}

static Unc_RetVal unc0_lib_thread_rlock_acquire(
                    Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    struct unc_thrd_rlock *rlock;
    e = unc0_thread_checktype(w, &args.values[0], unc_boundvalue(w, 0),
                    (void **)&rlock, "argument not a thread.rlock");
    if (e) return e;
    unc_vmpause(w);
    e = unc_thrd_rlock_acquire(rlock);
    unc_vmresume(w);
    if (e) e = unc0_lib_thread_makeerr(w, e);
    unc_unlock(w, &args.values[0]);
    return e;
}

static Unc_RetVal unc0_lib_thread_rlock_acquiretimed(
                    Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    Unc_Float to;
    struct unc_thrd_rlock *rlock;
    e = unc_getfloat(w, &args.values[1], &to);
    if (e) return e;
    if (to < 0)
        return unc_throwexc(w, "value", "timeout cannot be negative");
    if (!unc0_fisfinite(to))
        return unc_throwexc(w, "value", "timeout must be finite");
    e = unc0_thread_checktype(w, &args.values[0], unc_boundvalue(w, 0),
                    (void **)&rlock, "argument not a thread.rlock");
    if (e) return e;
    unc_vmpause(w);
    e = unc_thrd_rlock_acquiretimed(rlock, to);
    unc_vmresume(w);
    if (e) e = unc0_lib_thread_makeerr(w, e);
    unc_unlock(w, &args.values[0]);
    return e;
}

static Unc_RetVal unc0_lib_thread_rlock_release(
                    Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    struct unc_thrd_rlock *rlock;
    e = unc0_thread_checktype(w, &args.values[0], unc_boundvalue(w, 0),
                    (void **)&rlock, "argument not a thread.rlock");
    if (e) return e;
    e = unc_thrd_rlock_release(rlock);
    if (e) e = unc0_lib_thread_makeerr(w, e);
    unc_unlock(w, &args.values[0]);
    return e;
}

static Unc_RetVal unc0_lib_thread_sem_new(
                    Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    Unc_Value v = UNC_BLANK;
    Unc_Int ui;
    struct unc_thrd_sem *sem;
    e = unc_getint(w, &args.values[0], &ui);
    if (e) return e;
    if (ui < 0) return unc_throwexc(w, "value",
                    "semaphore may not be initialized with a negative value");
    e = unc_newopaque(w, &v, unc_boundvalue(w, 0),
                        sizeof(struct unc_thrd_sem), (void **)&sem,
                        &unc0_thread_sem_destr, 0, NULL, 0, NULL);
    if (e) return e;
    e = unc_thrd_sem_new(sem, ui);
    if (e) e = unc0_lib_thread_makeerr(w, e);
    unc_unlock(w, &v);
    if (!e) e = unc_pushmove(w, &v, NULL);
    if (e) unc_clear(w, &v);
    return e;
}

static Unc_RetVal unc0_lib_thread_sem_acquire(
                    Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    Unc_Int ui;
    struct unc_thrd_sem *sem;
    if (unc_gettype(w, &args.values[1]))
        e = unc_getint(w, &args.values[1], &ui);
    else
        e = 0, ui = 1;
    if (e) return e;
    if (ui < 0) return unc_throwexc(w, "value",
                    "may not acquire a semaphore a negative number of times");
    e = unc0_thread_checktype(w, &args.values[0], unc_boundvalue(w, 0),
                    (void **)&sem, "argument not a thread.semaphore");
    if (e) return e;
    unc_vmpause(w);
    e = unc_thrd_sem_acquire(sem, ui);
    unc_vmresume(w);
    if (e) e = unc0_lib_thread_makeerr(w, e);
    unc_unlock(w, &args.values[0]);
    return e;
}

static Unc_RetVal unc0_lib_thread_sem_acquiretimed(
                    Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    Unc_Int ui;
    Unc_Float to;
    struct unc_thrd_sem *sem;
    if (unc_gettype(w, &args.values[2]))
        e = unc_getint(w, &args.values[2], &ui);
    else
        e = 0, ui = 1;
    if (e) return e;
    if (ui < 0) return unc_throwexc(w, "value",
                    "may not acquire a semaphore a negative number of times");
    e = unc_getfloat(w, &args.values[1], &to);
    if (e) return e;
    if (to < 0)
        return unc_throwexc(w, "value", "timeout cannot be negative");
    if (!unc0_fisfinite(to))
        return unc_throwexc(w, "value", "timeout must be finite");
    e = unc0_thread_checktype(w, &args.values[0], unc_boundvalue(w, 0),
                    (void **)&sem, "argument not a thread.semaphore");
    if (e) return e;
    unc_vmpause(w);
    e = unc_thrd_sem_acquiretimed(sem, ui, to);
    unc_vmresume(w);
    if (e) e = unc0_lib_thread_makeerr(w, e);
    unc_unlock(w, &args.values[0]);
    return e;
}

static Unc_RetVal unc0_lib_thread_sem_release(
                    Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    Unc_Int ui;
    struct unc_thrd_sem *sem;
    if (unc_gettype(w, &args.values[1]))
        e = unc_getint(w, &args.values[1], &ui);
    else
        e = 0, ui = 1;
    if (e) return e;
    if (ui < 0) return unc_throwexc(w, "value",
                    "may not release a semaphore a negative number of times");
    e = unc0_thread_checktype(w, &args.values[0], unc_boundvalue(w, 0),
                    (void **)&sem, "argument not a thread.semaphore");
    if (e) return e;
    e = unc_thrd_sem_release(sem, ui);
    if (e) e = unc0_lib_thread_makeerr(w, e);
    unc_unlock(w, &args.values[0]);
    return e;
}

static Unc_RetVal unc0_lib_thread_mon_new(
                    Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    Unc_Value v = UNC_BLANK;
    struct unc_thrd_mon *mon;
    e = unc_newopaque(w, &v, unc_boundvalue(w, 0),
                        sizeof(struct unc_thrd_mon), (void **)&mon,
                        &unc0_thread_mon_destr, 1, NULL, 0, NULL);
    if (e) return e;
    if (unc_gettype(w, &args.values[0])) {
        void *lock;
        e = unc0_thread_checktype(w, &args.values[0], unc_boundvalue(w, 2),
                        (void **)&lock, NULL);
        if (!e) {
            e = unc_thrd_mon_new_rlock(mon, (struct unc_thrd_rlock *)lock);
        } else {
            e = unc0_thread_checktype(w, &args.values[0], unc_boundvalue(w, 1),
                            (void **)&lock, "argument not a lock");
            if (e) return e;
            e = unc_thrd_mon_new_lock(mon, (struct unc_thrd_lock *)lock);
        }
        unc_copy(w, unc_opaqueboundvalue(w, &v, 0), &args.values[0]);
    } else
        e = unc_thrd_mon_new(mon);
    if (e) e = unc0_lib_thread_makeerr(w, e);
    unc_unlock(w, &v);
    if (!e) e = unc_pushmove(w, &v, NULL);
    if (e) unc_clear(w, &v);
    return e;
}

static Unc_RetVal unc0_lib_thread_mon_acquire(
                    Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    struct unc_thrd_mon *mon;
    e = unc0_thread_checktype(w, &args.values[0], unc_boundvalue(w, 0),
                    (void **)&mon, "argument not a thread.monitor");
    if (e) return e;
    unc_vmpause(w);
    e = unc_thrd_mon_acquire(mon);
    unc_vmresume(w);
    if (e) e = unc0_lib_thread_makeerr(w, e);
    unc_unlock(w, &args.values[0]);
    return e;
}

static Unc_RetVal unc0_lib_thread_mon_acquiretimed(
                    Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    Unc_Float to;
    struct unc_thrd_mon *mon;
    e = unc_getfloat(w, &args.values[1], &to);
    if (e) return e;
    if (to < 0)
        return unc_throwexc(w, "value", "timeout cannot be negative");
    if (!unc0_fisfinite(to))
        return unc_throwexc(w, "value", "timeout must be finite");
    e = unc0_thread_checktype(w, &args.values[0], unc_boundvalue(w, 0),
                    (void **)&mon, "argument not a thread.monitor");
    if (e) return e;
    unc_vmpause(w);
    e = unc_thrd_mon_acquiretimed(mon, to);
    unc_vmresume(w);
    if (e) e = unc0_lib_thread_makeerr(w, e);
    unc_unlock(w, &args.values[0]);
    return e;
}

static Unc_RetVal unc0_lib_thread_mon_release(
                    Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    struct unc_thrd_mon *mon;
    e = unc0_thread_checktype(w, &args.values[0], unc_boundvalue(w, 0),
                    (void **)&mon, "argument not a thread.monitor");
    if (e) return e;
    e = unc_thrd_mon_release(mon);
    if (e) e = unc0_lib_thread_makeerr(w, e);
    unc_unlock(w, &args.values[0]);
    return e;
}

static Unc_RetVal unc0_lib_thread_mon_wait(
                    Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    struct unc_thrd_mon *mon;
    e = unc0_thread_checktype(w, &args.values[0], unc_boundvalue(w, 0),
                    (void **)&mon, "argument not a thread.monitor");
    if (e) return e;
    unc_vmpause(w);
    e = unc_thrd_mon_wait(mon);
    unc_vmresume(w);
    if (e) e = unc0_lib_thread_makeerr(w, e);
    unc_unlock(w, &args.values[0]);
    return e;
}

static Unc_RetVal unc0_lib_thread_mon_waittimed(
                    Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    Unc_Float to;
    struct unc_thrd_mon *mon;
    e = unc_getfloat(w, &args.values[1], &to);
    if (e) return e;
    if (to < 0)
        return unc_throwexc(w, "value", "timeout cannot be negative");
    if (!unc0_fisfinite(to))
        return unc_throwexc(w, "value", "timeout must be finite");
    e = unc0_thread_checktype(w, &args.values[0], unc_boundvalue(w, 0),
                    (void **)&mon, "argument not a thread.monitor");
    if (e) return e;
    unc_vmpause(w);
    e = unc_thrd_mon_waittimed(mon, to);
    unc_vmresume(w);
    if (e) e = unc0_lib_thread_makeerr(w, e);
    unc_unlock(w, &args.values[0]);
    return e;
}

static Unc_RetVal unc0_lib_thread_mon_notify(
                    Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    Unc_Int ui;
    struct unc_thrd_mon *mon;
    if (unc_gettype(w, &args.values[1]))
        e = unc_getint(w, &args.values[1], &ui);
    else
        e = 0, ui = 1;
    if (e) return e;
    if (ui < 0) return unc_throwexc(w, "value",
                "may not try to notify a monitor a negative number of times");
    if (ui == 0) return 0;
    e = unc0_thread_checktype(w, &args.values[0], unc_boundvalue(w, 0),
                    (void **)&mon, "argument not a thread.monitor");
    if (e) return e;
    e = unc_thrd_mon_notify(mon, ui);
    if (e) e = unc0_lib_thread_makeerr(w, e);
    unc_unlock(w, &args.values[0]);
    return e;
}

static Unc_RetVal unc0_lib_thread_mon_notifyall(
                    Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    struct unc_thrd_mon *mon;
    e = unc0_thread_checktype(w, &args.values[0], unc_boundvalue(w, 0),
                    (void **)&mon, "argument not a thread.monitor");
    if (e) return e;
    e = unc_thrd_mon_notifyall(mon);
    if (e) e = unc0_lib_thread_makeerr(w, e);
    unc_unlock(w, &args.values[0]);
    return e;
}

static Unc_RetVal unc0_lib_thread_sleep(
                    Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    Unc_Float to;
    e = unc_getfloat(w, &args.values[0], &to);
    if (e) return e;
    if (to < 0)
        return unc_throwexc(w, "value", "sleep time cannot be negative");
    if (!unc0_fisfinite(to))
        return unc_throwexc(w, "value", "sleep time must be finite");
    unc_vmpause(w);
    e = unc_thrd_sleep(to);
    unc_vmresume(w);
    return e;
}

static Unc_RetVal unc0_lib_thread_yield(
                    Unc_View *w, Unc_Tuple args, void *udata) {
    unc_vmpause(w);
    UNC_YIELD();
    unc_vmresume(w);
    return 0;
}

Unc_RetVal uncilmain_thread(Unc_View *w) {
    Unc_RetVal e;
    Unc_Value thread_thread = UNC_BLANK;
    Unc_Value thread_lock = UNC_BLANK;
    Unc_Value thread_rlock = UNC_BLANK;
    Unc_Value thread_semaphore = UNC_BLANK;
    Unc_Value thread_monitor = UNC_BLANK;
    
    e = unc_newtable(w, &thread_thread);
    if (!e) e = unc_newtable(w, &thread_lock);
    if (!e) e = unc_newtable(w, &thread_rlock);
    if (!e) e = unc_newtable(w, &thread_semaphore);
    if (!e) e = unc_newtable(w, &thread_monitor);
    if (e) goto uncilmain_thread_fail;
    
    e = unc_exportcfunction(w, "sleep", &unc0_lib_thread_sleep,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;

    e = unc_exportcfunction(w, "yield", &unc0_lib_thread_yield,
                            UNC_CFUNC_CONCURRENT,
                            0, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;

    {
        Unc_Value v = UNC_BLANK;
#if UNCIL_MT_OK
        unc_setbool(w, &v, 1);
#else
        unc_setbool(w, &v, 0);
#endif
        e = unc_setpublicc(w, "threaded", &v);
        if (e) goto uncilmain_thread_fail;
    }
    {
        Unc_Value v = UNC_BLANK;
#if UNCIL_MT_OK
        e = unc_newstringc(w, &v, UNCIL_MT_PROVIDER);
        if (e) goto uncilmain_thread_fail;
#else
        unc_setnull(w, &v);
#endif
        e = unc_setpublicc(w, "threader", &v);
        if (e) goto uncilmain_thread_fail;
    }
    
    {
        Unc_Value v = UNC_BLANK;
        e = unc_newstringc(w, &v, "thread.thread");
        if (e) goto uncilmain_thread_fail;
        e = unc_setattrc(w, &thread_thread, OPOVERLOAD(name), &v);
        if (e) goto uncilmain_thread_fail;
        
        e = unc_newstringc(w, &v, "thread.lock");
        if (e) goto uncilmain_thread_fail;
        e = unc_setattrc(w, &thread_lock, OPOVERLOAD(name), &v);
        if (e) goto uncilmain_thread_fail;
        
        e = unc_newstringc(w, &v, "thread.rlock");
        if (e) goto uncilmain_thread_fail;
        e = unc_setattrc(w, &thread_rlock, OPOVERLOAD(name), &v);
        if (e) goto uncilmain_thread_fail;
        
        e = unc_newstringc(w, &v, "thread.semaphore");
        if (e) goto uncilmain_thread_fail;
        e = unc_setattrc(w, &thread_semaphore, OPOVERLOAD(name), &v);
        if (e) goto uncilmain_thread_fail;
        
        e = unc_newstringc(w, &v, "thread.monitor");
        if (e) goto uncilmain_thread_fail;
        e = unc_setattrc(w, &thread_monitor, OPOVERLOAD(name), &v);
        if (e) goto uncilmain_thread_fail;

        e = unc_newcfunction(w, &v, &unc0_lib_thread_thread_new,
                             UNC_CFUNC_DEFAULT,
                             2, 0, 1, NULL,
                             1, &thread_thread, 0, NULL, "new", NULL);
        if (e) goto uncilmain_thread_fail;
        e = unc_setattrc(w, &thread_thread, "new", &v);
        if (e) goto uncilmain_thread_fail;

        e = unc_newcfunction(w, &v, &unc0_lib_thread_thread_start,
                             UNC_CFUNC_CONCURRENT,
                             1, 0, 0, NULL,
                             1, &thread_thread, 0, NULL, "start", NULL);
        if (e) goto uncilmain_thread_fail;
        e = unc_setattrc(w, &thread_thread, "start", &v);
        if (e) goto uncilmain_thread_fail;

        e = unc_newcfunction(w, &v, &unc0_lib_thread_thread_hasfinished,
                             UNC_CFUNC_CONCURRENT,
                             1, 0, 0, NULL,
                             1, &thread_thread, 0, NULL, "hasfinished", NULL);
        if (e) goto uncilmain_thread_fail;
        e = unc_setattrc(w, &thread_thread, "hasfinished", &v);
        if (e) goto uncilmain_thread_fail;

        e = unc_newcfunction(w, &v, &unc0_lib_thread_thread_halt,
                             UNC_CFUNC_CONCURRENT,
                             1, 0, 0, NULL,
                             1, &thread_thread, 0, NULL, "halt", NULL);
        if (e) goto uncilmain_thread_fail;
        e = unc_setattrc(w, &thread_thread, "halt", &v);
        if (e) goto uncilmain_thread_fail;

        e = unc_newcfunction(w, &v, &unc0_lib_thread_thread_join,
                             UNC_CFUNC_CONCURRENT,
                             1, 0, 0, NULL,
                             1, &thread_thread, 0, NULL, "join", NULL);
        if (e) goto uncilmain_thread_fail;
        e = unc_setattrc(w, &thread_thread, "join", &v);
        if (e) goto uncilmain_thread_fail;

        e = unc_newcfunction(w, &v, &unc0_lib_thread_thread_jointimed,
                             UNC_CFUNC_CONCURRENT,
                             2, 0, 0, NULL,
                             1, &thread_thread, 0, NULL, "jointimed", NULL);
        if (e) goto uncilmain_thread_fail;
        e = unc_setattrc(w, &thread_thread, "jointimed", &v);
        if (e) goto uncilmain_thread_fail;

        e = unc_newcfunction(w, &v, &unc0_lib_thread_lock_acquire,
                             UNC_CFUNC_CONCURRENT,
                             1, 0, 0, NULL,
                             1, &thread_lock, 0, NULL, "acquire", NULL);
        if (e) goto uncilmain_thread_fail;
        e = unc_setattrc(w, &thread_lock, "acquire", &v);
        if (e) goto uncilmain_thread_fail;
        e = unc_setattrc(w, &thread_lock, "__open", &v);
        if (e) goto uncilmain_thread_fail;

        e = unc_newcfunction(w, &v, &unc0_lib_thread_lock_acquiretimed,
                             UNC_CFUNC_CONCURRENT,
                             2, 0, 0, NULL,
                             1, &thread_lock, 0, NULL, "acquiretimed", NULL);
        if (e) goto uncilmain_thread_fail;
        e = unc_setattrc(w, &thread_lock, "acquiretimed", &v);
        if (e) goto uncilmain_thread_fail;

        e = unc_newcfunction(w, &v, &unc0_lib_thread_lock_new,
                             UNC_CFUNC_CONCURRENT,
                             0, 0, 0, NULL,
                             1, &thread_lock, 0, NULL, "new", NULL);
        if (e) goto uncilmain_thread_fail;
        e = unc_setattrc(w, &thread_lock, "new", &v);
        if (e) goto uncilmain_thread_fail;

        e = unc_newcfunction(w, &v, &unc0_lib_thread_lock_release,
                             UNC_CFUNC_CONCURRENT,
                             1, 0, 0, NULL,
                             1, &thread_lock, 0, NULL, "release", NULL);
        if (e) goto uncilmain_thread_fail;
        e = unc_setattrc(w, &thread_lock, "release", &v);
        if (e) goto uncilmain_thread_fail;
        e = unc_setattrc(w, &thread_lock, "__close", &v);
        if (e) goto uncilmain_thread_fail;

        e = unc_newcfunction(w, &v, &unc0_lib_thread_rlock_acquire,
                             UNC_CFUNC_CONCURRENT,
                             1, 0, 0, NULL,
                             1, &thread_rlock, 0, NULL, "acquire", NULL);
        if (e) goto uncilmain_thread_fail;
        e = unc_setattrc(w, &thread_rlock, "acquire", &v);
        if (e) goto uncilmain_thread_fail;
        e = unc_setattrc(w, &thread_rlock, "__open", &v);
        if (e) goto uncilmain_thread_fail;

        e = unc_newcfunction(w, &v, &unc0_lib_thread_rlock_acquiretimed,
                             UNC_CFUNC_CONCURRENT,
                             2, 0, 0, NULL,
                             1, &thread_rlock, 0, NULL, "acquiretimed", NULL);
        if (e) goto uncilmain_thread_fail;
        e = unc_setattrc(w, &thread_rlock, "acquiretimed", &v);
        if (e) goto uncilmain_thread_fail;

        e = unc_newcfunction(w, &v, &unc0_lib_thread_rlock_new,
                             UNC_CFUNC_CONCURRENT,
                             0, 0, 0, NULL,
                             1, &thread_rlock, 0, NULL, "new", NULL);
        if (e) goto uncilmain_thread_fail;
        e = unc_setattrc(w, &thread_rlock, "new", &v);
        if (e) goto uncilmain_thread_fail;

        e = unc_newcfunction(w, &v, &unc0_lib_thread_rlock_release,
                             UNC_CFUNC_CONCURRENT,
                             1, 0, 0, NULL,
                             1, &thread_rlock, 0, NULL, "release", NULL);
        if (e) goto uncilmain_thread_fail;
        e = unc_setattrc(w, &thread_rlock, "release", &v);
        if (e) goto uncilmain_thread_fail;
        e = unc_setattrc(w, &thread_rlock, "__close", &v);
        if (e) goto uncilmain_thread_fail;

        e = unc_newcfunction(w, &v, &unc0_lib_thread_sem_acquire,
                             UNC_CFUNC_CONCURRENT,
                             1, 0, 1, NULL,
                             1, &thread_semaphore, 0, NULL, "acquire", NULL);
        if (e) goto uncilmain_thread_fail;
        e = unc_setattrc(w, &thread_semaphore, "acquire", &v);
        if (e) goto uncilmain_thread_fail;
        e = unc_setattrc(w, &thread_semaphore, "__open", &v);
        if (e) goto uncilmain_thread_fail;

        e = unc_newcfunction(w, &v, &unc0_lib_thread_sem_acquiretimed,
                             UNC_CFUNC_CONCURRENT,
                             2, 0, 1, NULL,
                             1, &thread_semaphore, 0, NULL,
                                    "acquiretimed", NULL);
        if (e) goto uncilmain_thread_fail;
        e = unc_setattrc(w, &thread_semaphore, "acquiretimed", &v);
        if (e) goto uncilmain_thread_fail;

        e = unc_newcfunction(w, &v, &unc0_lib_thread_sem_new,
                             UNC_CFUNC_CONCURRENT,
                             1, 0, 0, NULL,
                             1, &thread_semaphore, 0, NULL, "new", NULL);
        if (e) goto uncilmain_thread_fail;
        e = unc_setattrc(w, &thread_semaphore, "new", &v);
        if (e) goto uncilmain_thread_fail;

        e = unc_newcfunction(w, &v, &unc0_lib_thread_sem_release,
                             UNC_CFUNC_CONCURRENT,
                             1, 0, 1, NULL,
                             1, &thread_semaphore, 0, NULL, "release", NULL);
        if (e) goto uncilmain_thread_fail;
        e = unc_setattrc(w, &thread_semaphore, "release", &v);
        if (e) goto uncilmain_thread_fail;
        e = unc_setattrc(w, &thread_semaphore, "__open", &v);
        if (e) goto uncilmain_thread_fail;

        e = unc_newcfunction(w, &v, &unc0_lib_thread_mon_acquire,
                             UNC_CFUNC_CONCURRENT,
                             1, 0, 0, NULL,
                             1, &thread_monitor, 0, NULL, "acquire", NULL);
        if (e) goto uncilmain_thread_fail;
        e = unc_setattrc(w, &thread_monitor, "acquire", &v);
        if (e) goto uncilmain_thread_fail;
        e = unc_setattrc(w, &thread_monitor, "__open", &v);
        if (e) goto uncilmain_thread_fail;

        e = unc_newcfunction(w, &v, &unc0_lib_thread_mon_acquiretimed,
                             UNC_CFUNC_CONCURRENT,
                             2, 0, 0, NULL,
                             1, &thread_monitor, 0, NULL, "acquiretimed", NULL);
        if (e) goto uncilmain_thread_fail;
        e = unc_setattrc(w, &thread_monitor, "acquiretimed", &v);
        if (e) goto uncilmain_thread_fail;

        {
            Unc_Value thread_mon_new_binds[3] = UNC_BLANKS;
            unc_copy(w, &thread_mon_new_binds[0], &thread_monitor);
            unc_copy(w, &thread_mon_new_binds[1], &thread_lock);
            unc_copy(w, &thread_mon_new_binds[2], &thread_rlock);
            e = unc_newcfunction(w, &v, &unc0_lib_thread_mon_new,
                                UNC_CFUNC_CONCURRENT,
                                0, 0, 1, NULL,
                                3, thread_mon_new_binds, 0, NULL, "new", NULL);
            unc_clearmany(w, ARRAYSIZE(thread_mon_new_binds),
                            thread_mon_new_binds);
            if (e) goto uncilmain_thread_fail;
            e = unc_setattrc(w, &thread_monitor, "new", &v);
            if (e) goto uncilmain_thread_fail;
        }

        e = unc_newcfunction(w, &v, &unc0_lib_thread_mon_release,
                             UNC_CFUNC_CONCURRENT,
                             1, 0, 0, NULL,
                             1, &thread_monitor, 0, NULL, "release", NULL);
        if (e) goto uncilmain_thread_fail;
        e = unc_setattrc(w, &thread_monitor, "release", &v);
        if (e) goto uncilmain_thread_fail;
        e = unc_setattrc(w, &thread_monitor, "__close", &v);
        if (e) goto uncilmain_thread_fail;

        e = unc_newcfunction(w, &v, &unc0_lib_thread_mon_notify,
                             UNC_CFUNC_CONCURRENT,
                             1, 0, 1, NULL,
                             1, &thread_monitor, 0, NULL, "notify", NULL);
        if (e) goto uncilmain_thread_fail;
        e = unc_setattrc(w, &thread_monitor, "notify", &v);
        if (e) goto uncilmain_thread_fail;

        e = unc_newcfunction(w, &v, &unc0_lib_thread_mon_notifyall,
                             UNC_CFUNC_CONCURRENT,
                             1, 0, 0, NULL,
                             1, &thread_monitor, 0, NULL, "notifyall", NULL);
        if (e) goto uncilmain_thread_fail;
        e = unc_setattrc(w, &thread_monitor, "notifyall", &v);
        if (e) goto uncilmain_thread_fail;

        e = unc_newcfunction(w, &v, &unc0_lib_thread_mon_wait,
                             UNC_CFUNC_CONCURRENT,
                             1, 0, 0, NULL,
                             1, &thread_monitor, 0, NULL, "wait", NULL);
        if (e) goto uncilmain_thread_fail;
        e = unc_setattrc(w, &thread_monitor, "wait", &v);
        if (e) goto uncilmain_thread_fail;

        e = unc_newcfunction(w, &v, &unc0_lib_thread_mon_waittimed,
                             UNC_CFUNC_CONCURRENT,
                             2, 0, 0, NULL,
                             1, &thread_monitor, 0, NULL, "waittimed", NULL);
        if (e) goto uncilmain_thread_fail;
        e = unc_setattrc(w, &thread_monitor, "waittimed", &v);
        if (e) goto uncilmain_thread_fail;

        unc_clear(w, &v);
    }

    e = unc_setpublicc(w, "thread", &thread_thread);
    if (!e) e = unc_setpublicc(w, "lock", &thread_lock);
    if (!e) e = unc_setpublicc(w, "rlock", &thread_rlock);
    if (!e) e = unc_setpublicc(w, "semaphore", &thread_semaphore);
    if (!e) e = unc_setpublicc(w, "monitor", &thread_monitor);
uncilmain_thread_fail:
    unc_clear(w, &thread_thread);
    unc_clear(w, &thread_lock);
    unc_clear(w, &thread_rlock);
    unc_clear(w, &thread_semaphore);
    unc_clear(w, &thread_monitor);
    return e;
}
