/*******************************************************************************
 
Uncil -- builtin coroutine library impl

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

#define UNCIL_DEFINES

#include "udef.h"
#include "uerr.h"
#include "umt.h"
#include "uncil.h"
#include "uvm.h"

enum unc_coroutine_status {
    UNC_CORO_ST_INIT,
    UNC_CORO_ST_RUN,
    UNC_CORO_ST_YIELD,
    UNC_CORO_ST_DONE,
    UNC_CORO_ST_ERROR
};

struct unc_coroutine {
    Unc_AtomicSmall status;
    Unc_View *view;
    Unc_Value *task;
    Unc_View *returnto;
};

Unc_RetVal unc_coroutine_destr(Unc_View *w, size_t n, void *data) {
    struct unc_coroutine *p = data;
    /* unc_clear(w, p->task);
      not needed, bound value */
    unc_destroy(p->view);
    return 0;
}

Unc_RetVal unc__lib_coro_c_canresume(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value v = UNC_BLANK;
    struct unc_coroutine *coro;

    unc_getprototype(w, &args.values[0], &v);
    if (unc_gettype(w, &args.values[0]) != Unc_TOpaque
            || !unc_issame(w, &v, unc_boundvalue(w, 0))) {
        unc_clear(w, &v);
        return unc_throwexc(w, "type", "argument is not a coroutine");
    }

    if (unc_lockopaque(w, &args.values[0], NULL, (void **)&coro)) {
        unc_setbool(w, &v, 0);
    } else {
        switch ((int)coro->status) {
        case UNC_CORO_ST_INIT:
        case UNC_CORO_ST_YIELD:
            unc_setbool(w, &v, 1);
            break;
        default:
            unc_setbool(w, &v, 0);
        }
        unc_unlock(w, &args.values[0]);
    }
    
    return unc_pushmove(w, &v, NULL);
}

Unc_RetVal unc__lib_coro_current(Unc_View *w, Unc_Tuple args, void *udata) {
    return unc_push(w, 1, &w->coroutine, NULL);
}

Unc_RetVal unc__lib_coro_c_hasfinished(Unc_View *w, Unc_Tuple args,
                                       void *udata) {
    Unc_Value v = UNC_BLANK;
    struct unc_coroutine *coro;

    unc_getprototype(w, &args.values[0], &v);
    if (unc_gettype(w, &args.values[0]) != Unc_TOpaque
            || !unc_issame(w, &v, unc_boundvalue(w, 0))) {
        unc_clear(w, &v);
        return unc_throwexc(w, "type", "argument is not a coroutine");
    }

    if (unc_lockopaque(w, &args.values[0], NULL, (void **)&coro)) {
        unc_setbool(w, &v, 0);
    } else {
        switch ((int)coro->status) {
        case UNC_CORO_ST_INIT:
        case UNC_CORO_ST_RUN:
        case UNC_CORO_ST_YIELD:
            unc_setbool(w, &v, 0);
            break;
        default:
            unc_setbool(w, &v, 1);
        }
        unc_unlock(w, &args.values[0]);
    }
    
    return unc_pushmove(w, &v, NULL);
}

Unc_RetVal unc__lib_coro_new(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    Unc_Value v = UNC_BLANK;
    struct unc_coroutine coro, *pcoro;

    if (!unc_iscallable(w, &args.values[0]))
        return unc_throwexc(w, "type", "argument must be callable");

    coro.status = UNC_CORO_ST_INIT;
    coro.returnto = NULL;
    coro.view = unc_dup(w);
    if (!coro.view) return UNCIL_ERR_MEM;
    e = unc_newopaque(w, &v, unc_boundvalue(w, 0),
                      sizeof(coro), (void **)&pcoro,
                      &unc_coroutine_destr,
                      1, NULL, 0, NULL);
    if (!e) {
        coro.task = unc_opaqueboundvalue(w, &v, 0);
        unc_copy(w, coro.task, &args.values[0]);
        *pcoro = coro;
        unc_unlock(w, &v);
        unc_copy(w, &coro.view->coroutine, &v);
    } else
        unc_coroutine_destr(w, sizeof(coro), &coro);
    
    return unc_pushmove(w, &v, NULL);
}

Unc_RetVal unc__lib_coro_c_resume(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    Unc_Value v = UNC_BLANK;
    struct unc_coroutine *coro;
    int init;
    Unc_View *cw;

    unc_getprototype(w, &args.values[0], &v);
    if (unc_gettype(w, &args.values[0]) != Unc_TOpaque
            || !unc_issame(w, &v, unc_boundvalue(w, 0))) {
        unc_clear(w, &v);
        return unc_throwexc(w, "type", "argument is not a coroutine");
    }
    unc_clear(w, &v);

    if (unc_lockopaque(w, &args.values[0], NULL, (void **)&coro))
        return unc_throwexc(w, "value", "cannot resume this coroutine");
    
    e = 0;
    switch ((int)coro->status) {
    case UNC_CORO_ST_INIT:
        init = 1;
        break;
    case UNC_CORO_ST_YIELD:
        init = 0;
        break;
    case UNC_CORO_ST_RUN:
        e = unc_throwexc(w, "value", "cannot resume a running coroutine");
        break;
    case UNC_CORO_ST_DONE:
    case UNC_CORO_ST_ERROR:
        e = unc_throwexc(w, "value", "cannot resume a finished coroutine");
    }
    if (e) {
        unc_unlock(w, &args.values[0]);
        return e;
    }

    cw = coro->view;
    e = unc__stackpush(cw, &cw->sval, args.count - 1, args.values + 1);
    if (!e && init) {
        e = unc__vmrpush(cw);
        if (!e) e = unc__fcallv(cw, coro->task, args.count - 1, 1, 1, 0, 0);
        if (e) {
            unc__restoredepth(cw, &cw->sval, 0);
            cw->region.top = cw->region.base;
        }
    }
    if (e) {
        unc_unlock(w, &args.values[0]);
        return e;
    }

    coro->status = UNC_CORO_ST_RUN;
    coro->returnto = w;
    w->trampoline = cw;
    unc_unlock(w, &args.values[0]);
#if DEBUGPRINT_CORO
    printf("resuming %p => %p\n", (void *)w, (void *)cw);
#endif
    return UNCIL_ERR_TRAMPOLINE;
}

static Unc_View *unc__coroyield(Unc_View *w, int *e,
                                int finish, Unc_Tuple retval) {
    int er = *e;
    Unc_View *cw;
    Unc_Opaque *s = LEFTOVER(Unc_Opaque, VGETENT(&w->coroutine));
    struct unc_coroutine *coro;
    UNC_LOCKL(s->lock);
    coro = s->data;
    cw = coro->returnto;
    ASSERT(cw);
    if (!er) {
        int ex = unc__stackpush(cw, &cw->sval, retval.count, retval.values);
        if (ex) *e = ex;
    } else
        unc__errinfocopyfrom(cw, w);
    coro->status = er ? UNC_CORO_ST_ERROR 
                : (finish ? UNC_CORO_ST_DONE : UNC_CORO_ST_YIELD);
    UNC_UNLOCKL(s->lock);
#if DEBUGPRINT_CORO
    printf("yielding %p => %p (f=%d, e=%d)\n", (void *)w, (void *)cw,
                                                finish, er);
#endif
    return cw;
}

Unc_View *unc__corofinish(Unc_View *w, int *e) {
    Unc_Tuple rv;
    rv.count = unc__stackdepth(&w->sval);
    rv.values = w->sval.base;
    return unc__coroyield(w, e, 1, rv);
}

Unc_RetVal unc__lib_coro_yield(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    if (!VGETTYPE(&w->coroutine))
        return unc_throwexc(w, "usage", "cannot yield from main routine");
    e = 0;
    w->trampoline = unc__coroyield(w, &e, 0, args);
    return e ? e : UNCIL_ERR_TRAMPOLINE;
}

Unc_RetVal unc__lib_coro_canyield(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value v = UNC_BLANK;
    unc_setbool(w, &v, VGETTYPE(&w->coroutine));
    return unc_pushmove(w, &v, NULL);
}

Unc_RetVal uncilmain_coroutine(Unc_View *w) {
    Unc_RetVal e;
    Unc_Value coro = UNC_BLANK;

    e = unc_newobject(w, &coro, NULL);
    if (e) return e;

    e = unc_exportcfunction(w, "canyield", &unc__lib_coro_canyield,
                            UNC_CFUNC_CONCURRENT,
                            0, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;

    e = unc_exportcfunction(w, "current", &unc__lib_coro_current,
                            UNC_CFUNC_CONCURRENT,
                            0, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;

    e = unc_exportcfunction(w, "new", &unc__lib_coro_new,
                            UNC_CFUNC_DEFAULT,
                            1, 0, 1, NULL, 1, &coro, 0, NULL, NULL);
    if (e) return e;

    e = unc_exportcfunction(w, "yield", &unc__lib_coro_yield,
                            UNC_CFUNC_CONCURRENT,
                            0, 1, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;

    {
        Unc_Value ns = UNC_BLANK;
        e = unc_newstringc(w, &ns, "coroutine.coroutine");
        if (e) return e;
        e = unc_setattrc(w, &coro, "__name", &ns);
        if (e) return e;
        unc_clear(w, &ns);
    }

    {
        Unc_Value fn = UNC_BLANK;
        e = unc_newcfunction(w, &fn, &unc__lib_coro_c_canresume,
                             UNC_CFUNC_CONCURRENT,
                             1, 0, 0, NULL,
                             1, &coro, 0, NULL, "canresume", NULL);
        if (e) return e;
        e = unc_setattrc(w, &coro, "canresume", &fn);
        if (e) return e;
        
        e = unc_newcfunction(w, &fn, &unc__lib_coro_c_hasfinished,
                             UNC_CFUNC_CONCURRENT,
                             1, 0, 0, NULL,
                             1, &coro, 0, NULL, "hasfinished", NULL);
        if (e) return e;
        e = unc_setattrc(w, &coro, "hasfinished", &fn);
        if (e) return e;
        
        e = unc_newcfunction(w, &fn, &unc__lib_coro_c_resume,
                             UNC_CFUNC_CONCURRENT,
                             1, 1, 0, NULL,
                             1, &coro, 0, NULL, "resume", NULL);
        if (e) return e;
        e = unc_setattrc(w, &coro, "resume", &fn);
        if (e) return e;
        unc_clear(w, &fn);
    }

    e = unc_setpublicc(w, "coroutine", &coro);
    if (e) return e;

    unc_clear(w, &coro);
    return 0;
}
