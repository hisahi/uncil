/*******************************************************************************
 
Uncil -- multithreading header

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

#ifndef UNCIL_UMT_H
#define UNCIL_UMT_H

#include "uosdef.h"
#include "udef.h"

#define UNCIL_ALTLIGHTLOCK 0

struct Unc_View;

/* atomics etc. */

#if UNCIL_MT_OK && UNCIL_C11 && !__STDC_NO_ATOMICS__
/* use standard C11 atomics */
#include <stdatomic.h>
typedef atomic_flag Unc_AtomicFlag;
typedef _Atomic int Unc_AtomicSmall;
typedef _Atomic Unc_Size Unc_AtomicLarge;
#ifdef UNCIL_DEFINES
#define ATOMICSSET(a, x) (void)(a = (x))
#define ATOMICSINC(a) (++a)
#define ATOMICSDEC(a) (--a)
#define ATOMICSXCG(a, x) atomic_exchange(&(a), (x))
#define ATOMICLSET(a, x) (void)(a = (x))
#define ATOMICLINC(a) (++a)
#define ATOMICLDEC(a) (--a)
#define ATOMICFLAGTAS(a) atomic_flag_test_and_set(&(a))
#define ATOMICFLAGCLR(a) atomic_flag_clear(&(a))
#endif /* UNCIL_DEFINES */

#elif UNCIL_MT_OK && defined(__GNUC__) && (__GNUC__ >= 5 || \
                           (__GNUC__ >= 4 && __GNUC_MINOR__ >= 8))
/* use GNU C atomics */
typedef volatile char Unc_AtomicFlag;
typedef volatile sig_atomic_t Unc_AtomicSmall;
typedef volatile Unc_Size Unc_AtomicLarge;
#ifdef UNCIL_DEFINES
#define ATOMICSSET(a, x) __atomic_store_n(&(a), (x), __ATOMIC_SEQ_CST)
#define ATOMICSINC(a) __atomic_add_fetch(&(a), 1, __ATOMIC_SEQ_CST)
#define ATOMICSDEC(a) __atomic_sub_fetch(&(a), 1, __ATOMIC_SEQ_CST)
#define ATOMICSXCG(a, x) __atomic_exchange_n(&(a), (x), __ATOMIC_SEQ_CST)
#define ATOMICLSET(a, x) __atomic_store_n(&(a), (x), __ATOMIC_SEQ_CST)
#define ATOMICLINC(a) __atomic_add_fetch(&(a), 1, __ATOMIC_SEQ_CST)
#define ATOMICLDEC(a) __atomic_sub_fetch(&(a), 1, __ATOMIC_SEQ_CST)
#define ATOMICFLAGTAS(a) __atomic_test_and_set(&(a), __ATOMIC_SEQ_CST)
#define ATOMICFLAGCLR(a) __atomic_clear(&(a))
#endif /* UNCIL_DEFINES */

#else
/* don't use atomics */
typedef volatile sig_atomic_t Unc_AtomicFlag;
typedef volatile sig_atomic_t Unc_AtomicSmall;
typedef volatile Unc_Size Unc_AtomicLarge;
#ifdef UNCIL_DEFINES
#if UNCIL_MT_OK
#error "atomic operations required for multithreaded build"
#endif
int unc0_nonatomictas(Unc_AtomicFlag *v);
Unc_AtomicSmall unc0_nonatomicsxchg(Unc_AtomicSmall *var,
                                    Unc_AtomicSmall val);
#define ATOMICSSET(a, x) (a = (x))
#define ATOMICSINC(a) (++a)
#define ATOMICSDEC(a) (--a)
#define ATOMICSXCG(a, x) unc0_nonatomicsxchg(&(a), x)
#define ATOMICLSET(a, x) (a = (x))
#define ATOMICLINC(a) (++a)
#define ATOMICLDEC(a) (--a)
#define ATOMICFLAGTAS(a) unc0_nonatomictas(&(a))
#define ATOMICFLAGCLR(a) (a = 0)
#define UNC_NONATOMIC 1
#endif /* UNCIL_DEFINES */

#endif /* atomics */

/* locks etc. 
    LOCKLIGHT has no guarantees other than that it's a mutex.
    LOCKFULL should be re-entrant!
    UNC_LOCKL(x) and UNC_UNLOCKL(x) are for (un)locking LOCKLIGHTs
    UNC_LOCKF(x) and UNC_UNLOCKF(x) are for (un)locking LOCKFULLs
        none of the above funcs may fail with valid parameters
    UNC_LOCKLQ(x) tries to lock x and returns <>0 if locked, =0 if not
    UNC_LOCKFP(w, x) locks a LOCKFULL x or pauses a view w if a pause
        has been requested. returns <>0 iff a pause occurred
    UNC_LOCKFQ(x) tries to lock x and returns <>0 if locked, =0 if not
    UNC_LOCKINITL(x) and UNC_LOCKINITF(x) initialize those two lock types
        (may return != 0 in case of failure)
    UNC_LOCKSTATICF(x) should declare a static variable named x that is
        initialized with a lock, if possible. if not, define
        UNC_LOCKSTATICFINIT0(x) and UNC_LOCKSTATICFINIT1(x).
        UNC_LOCKSTATICFINIT0 is always outside of a function, while
        UNC_LOCKSTATICFINIT1 is before the first use of that lock.
        define them as empty if you don't need them
    UNC_LOCKFINAL(x) and UNC_LOCKFINAF(x) deinitialize those two lock types
        (these two shall never fail as the lock should be unlocked)
    UNC_YIELD() should yield the thread
    UNC_PAUSE(view) should pause all views except view
        (the current view being executed)
    UNC_RESUME(view) should resume all views, including the given view
    UNC_PAUSED(view) and UNC_RESUMED(view) are used by views to signal that
        they are paused/resumed
*/

#if UNCIL_MT_OK && UNCIL_MT_PTHREAD
#include <pthread.h>
#if !UNCIL_ALTLIGHTLOCK
#define UNC_LOCKLIGHT(name) pthread_mutex_t name;
#else
#define UNC_LOCKLIGHT(name) Unc_AtomicFlag name;
#endif
#define UNC_LOCKFULL(name) pthread_mutex_t name;
#ifdef UNCIL_DEFINES
#include <sched.h>
#if INLINEEXTOK
INLINEEXT void unc0_locklight_(Unc_AtomicFlag *x) {
    while (ATOMICFLAGTAS(*x)) sched_yield();
}
#else
void unc0_locklight_(Unc_AtomicFlag *x);
#endif
int unc0_pthread_init(pthread_mutex_t *mutex);
void unc0_pthread_lock(pthread_mutex_t *mutex);
int unc0_pthread_lockorpause(struct Unc_View *view, pthread_mutex_t *mutex);
void unc0_pthread_pause(struct Unc_View *view);
void unc0_pthread_resume(struct Unc_View *view);
void unc0_pthread_paused(struct Unc_View *view);
void unc0_pthread_resumed(struct Unc_View *view);

#if !UNCIL_ALTLIGHTLOCK
#define UNC_LOCKSTATICL(x) static pthread_mutex_t x = PTHREAD_MUTEX_INITIALIZER;
#define UNC_LOCKINITL(x) pthread_mutex_init(&(x), NULL)
#define UNC_LOCKL(x) unc0_pthread_lock(&(x))
#define UNC_LOCKLQ(x) (!pthread_mutex_trylock(&(x)))
#define UNC_UNLOCKL(x) pthread_mutex_unlock(&(x))
#define UNC_LOCKFINAL(x) pthread_mutex_destroy(&(x))
#else
#define UNC_LOCKSTATICL(x) static Unc_AtomicFlag x = 0;
#define UNC_LOCKINITL(x) ((void)(ATOMICFLAGCLR(x)), 0)
#define UNC_LOCKL(x) unc0_locklight_(&(x))
#define UNC_LOCKLQ(x) !ATOMICFLAGTAS(x)
#define UNC_UNLOCKL(x) ATOMICFLAGCLR(x)
#define UNC_LOCKFINAL(x)
#endif

#define UNC_LOCKSTATICF(x) static pthread_mutex_t x = PTHREAD_MUTEX_INITIALIZER;
#define UNC_LOCKSTATICFINIT0(x)
#define UNC_LOCKSTATICFINIT1(x)
#define UNC_LOCKINITF(x) unc0_pthread_init(&(x))
#define UNC_LOCKF(x) unc0_pthread_lock(&(x))
#define UNC_LOCKFP(w, x) unc0_pthread_lockorpause(w, &(x))
#define UNC_LOCKFQ(x) (!pthread_mutex_trylock(&(x)))
#define UNC_UNLOCKF(x) pthread_mutex_unlock(&(x))
#define UNC_LOCKFINAF(x) pthread_mutex_destroy(&(x))

#define UNC_YIELD() sched_yield()
#define UNC_PAUSE(view) unc0_pthread_pause(view)
#define UNC_RESUME(view) unc0_pthread_resume(view)
#define UNC_PAUSED(view) unc0_pthread_paused(view)
#define UNC_RESUMED(view) unc0_pthread_resumed(view)
#endif /* UNCIL_DEFINES */

#elif UNCIL_MT_OK && UNCIL_MT_C11
/* C11 standard stuff */
#include <stdlib.h>
#include <threads.h>
#if !UNCIL_ALTLIGHTLOCK
#define UNC_LOCKLIGHT(name) mtx_t name;
#else
#define UNC_LOCKLIGHT(name) Unc_AtomicFlag name;
#endif
#define UNC_LOCKFULL(name) mtx_t name;
#ifdef UNCIL_DEFINES
INLINEEXT void unc0_locklight_(Unc_AtomicFlag *x) {
    while (ATOMICFLAGTAS(*x)) thrd_yield();
}
int unc0_c11_init(mtx_t *mutex);
INLINEEXT void unc0_c11_lock(mtx_t *mutex) {
    while (mtx_lock(mutex) != thrd_success)
        ;
}
int unc0_c11_lockorpause(struct Unc_View *view, mtx_t *mutex);
void unc0_c11_pause(struct Unc_View *view);
void unc0_c11_resume(struct Unc_View *view);
void unc0_c11_paused(struct Unc_View *view);
void unc0_c11_resumed(struct Unc_View *view);

#if !UNCIL_ALTLIGHTLOCK
#define UNC_LOCKSTATICL(x)
#define UNC_LOCKINITL(x) mtx_init(&(x), mtx_plain) != thrd_success
#define UNC_LOCKL(x) unc0_c11_lock(&(x))
#define UNC_LOCKLQ(x) (mtx_trylock(&(x)) == thrd_success)
#define UNC_UNLOCKL(x) (void)mtx_unlock(&(x))
#define UNC_LOCKFINAL(x) mtx_destroy(&(x))
#else
#define UNC_LOCKSTATICL(x) static Unc_AtomicFlag x = ATOMIC_FLAG_INIT;
#define UNC_LOCKINITL(x) ((void)(ATOMICFLAGCLR(x)), 0)
#define UNC_LOCKL(x) unc0_locklight_(&(x))
#define UNC_LOCKLQ(x) !ATOMICFLAGTAS(x)
#define UNC_UNLOCKL(x) ATOMICFLAGCLR(x)
#define UNC_LOCKFINAL(x)
#endif

#define UNC_LOCKSTATICF(x) static mtx_t x;
#define UNC_LOCKSTATICFINIT0(x)                                                \
               static once_flag uncil_static_init0_flag_##x = ONCE_FLAG_INIT;  \
                                static void uncil_static_init0_##x(void) {     \
                                    if (UNC_LOCKINITF(x)) abort(); }
#define UNC_LOCKSTATICFINIT1(x) call_once(&uncil_static_init0_flag_##x,        \
                                          &uncil_static_init0_##x)
#define UNC_LOCKINITF(x) unc0_c11_init(&(x))
#define UNC_LOCKF(x) unc0_c11_lock(&(x))
#define UNC_LOCKFP(w, x) unc0_c11_lockorpause(w, &(x))
#define UNC_LOCKFQ(x) (mtx_trylock(&(x)) == thrd_success)
#define UNC_UNLOCKF(x) (void)mtx_unlock(&(x))
#define UNC_LOCKFINAF(x) mtx_destroy(&(x))

#define UNC_YIELD() thrd_yield()
#define UNC_PAUSE(view) unc0_c11_pause(view)
#define UNC_RESUME(view) unc0_c11_resume(view)
#define UNC_PAUSED(view) unc0_c11_paused(view)
#define UNC_RESUMED(view) unc0_c11_resumed(view)
#endif /* UNCIL_DEFINES */

#elif UNCIL_MT_OK
#error "missing multithreading lock implementation"
#else
#define UNC_LOCKLIGHT(name)
#define UNC_LOCKFULL(name)
#ifdef UNCIL_DEFINES
#define UNC_LOCKSTATICL(x)
#define UNC_LOCKINITL(x) 0
#define UNC_LOCKL(x)
#define UNC_LOCKLQ(x) 1
#define UNC_UNLOCKL(x)
#define UNC_LOCKFINAL(x)

#define UNC_LOCKSTATICF(x)
#define UNC_LOCKSTATICFINIT0(x)
#define UNC_LOCKSTATICFINIT1(x)
#define UNC_LOCKINITF(x) 0
#define UNC_LOCKF(x)
#define UNC_LOCKFQ(x) 1
#define UNC_LOCKFP(w, x) 0
#define UNC_UNLOCKF(x)
#define UNC_LOCKFINAF(x)

#define UNC_YIELD()
#define UNC_PAUSE(view)
#define UNC_RESUME(view)
#define UNC_PAUSED(view)
#define UNC_RESUMED(view)
#endif /* UNCIL_DEFINES */

#endif /* locks etc. */

#endif /* UNCIL_UMT_H */
