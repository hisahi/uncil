/*******************************************************************************
 
Uncil -- multithreading impl

Copyright (c) 2021 Sampo Hippeläinen (hisahi)

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

#include "ucommon.h"
#include "umt.h"
#include "uvm.h"

#if UNC_NONATOMIC
/* only as a fallback */
int unc__nonatomictas(Unc_AtomicFlag *v) {
    Unc_AtomicFlag a = *v;
    *v = 1;
    return a;
}

Unc_AtomicSmall unc__nonatomicsxchg(Unc_AtomicSmall *var,
                                    Unc_AtomicSmall val) {
    Unc_AtomicSmall old = *var;
    *var = val;
    return old;
}
#endif

#if UNCIL_MT_OK
void unc__mtpause(Unc_View *view) {
    Unc_World *w = view->world;
    Unc_View *v;
    if (view) ATOMICSSET(view->paused, 1);
    UNC_LOCKFP(view, w->viewlist_lock);
    v = w->view;
    while (v) {
        ATOMICSSET(v->flow, UNC_VIEW_FLOW_PAUSE);
        v = v->nextview;
    }
    
    v = w->view;
    while (v) {
        /*while (!UNC_LOCKFQ(w->runlock))
            UNC_YIELD();*/
        UNC_LOCKF(v->runlock);
        UNC_UNLOCKF(v->runlock);
        v = v->nextview;
    }

    UNC_UNLOCKF(w->viewlist_lock);
}
#endif

#if UNCIL_MT_OK && UNCIL_MT_PTHREAD
#include <time.h>

#if INLINEEXTOK
INLINEHERE void unc__locklight_(Unc_AtomicFlag *x);
#else
void unc__locklight_(Unc_AtomicFlag *x) {
    while (ATOMICFLAGTAS(*x)) sched_yield();
}
#endif

int unc__pthread_init(pthread_mutex_t *mutex) {
    int e;
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    e = pthread_mutex_init(mutex, &attr);
    pthread_mutexattr_destroy(&attr);
    return e;
}

void unc__pthread_lock(pthread_mutex_t *mutex) {
    while (pthread_mutex_lock(mutex))
        ;
}

int unc__pthread_lockorpause(Unc_View *view, pthread_mutex_t *mutex) {
    int e = 0;
    while (pthread_mutex_trylock(mutex)) {
        if (view->flow) {
            unc__vmcheckpause(view);
            e = 1;
        }
        UNC_YIELD();
    }
    return e;
}

void unc__pthread_pause(Unc_View *view) {
    unc__mtpause(view);
}

void unc__pthread_resume(Unc_View *view) {
    Unc_World *w = view->world;
    Unc_View *v;
    UNC_LOCKF(w->viewlist_lock);
    v = w->view;
    while (v) {
        ATOMICSSET(v->flow, UNC_VIEW_FLOW_RUN);
        v = v->nextview;
    }
    UNC_UNLOCKF(w->viewlist_lock);
    if (view) ATOMICSSET(view->paused, 0);
}

void unc__pthread_paused(Unc_View *view) { (void)view; }

void unc__pthread_resumed(Unc_View *view) { (void)view; }

#elif UNCIL_MT_OK && UNCIL_MT_C11

INLINEHERE void unc__locklight_(Unc_AtomicFlag *x);

int unc__c11_init(mtx_t *mutex) {
    return mtx_init(mutex, mtx_plain | mtx_recursive) != thrd_success;
}

INLINEHERE void unc__c11_lock(mtx_t *mutex);

int unc__c11_lockorpause(Unc_View *view, mtx_t *mutex) {
    int e = 0;
    while (mtx_trylock(mutex) != thrd_success) {
        if (view->flow) {
            unc__vmcheckpause(view);
            e = 1;
        }
        UNC_YIELD();
    }
    return e;
}

void unc__c11_pause(Unc_View *view) {
    unc__mtpause(view);
}

void unc__c11_resume(Unc_View *view) {
    Unc_World *w = view->world;
    Unc_View *v;
    UNC_LOCKF(w->viewlist_lock);
    v = w->view;
    while (v) {
        ATOMICSSET(v->flow, UNC_VIEW_FLOW_RUN);
        v = v->nextview;
    }
    UNC_UNLOCKF(w->viewlist_lock);
    if (view) ATOMICSSET(view->paused, 0);
}

void unc__c11_paused(Unc_View *view) { (void)view; }

void unc__c11_resumed(Unc_View *view) { (void)view; }

#endif
