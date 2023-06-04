/*******************************************************************************
 
Uncil -- builtin coroutine library impl

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
    /* VSETNULL(w, p->task);
      not needed, bound value */
    unc_destroy(p->view);
    return 0;
}

Unc_RetVal uncl_coro_c_canresume(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value v = UNC_BLANK;
    struct unc_coroutine *coro;

    unc_getprototype(w, &args.values[0], &v);
    if (unc_gettype(w, &args.values[0]) != Unc_TOpaque
            || !unc_issame(w, &v, unc_boundvalue(w, 0))) {
        VCLEAR(w, &v);
        return unc_throwexc(w, "type", "argument 1 is not a coroutine");
    }

    if (unc_trylockopaque(w, &args.values[0], NULL, (void **)&coro)) {
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
    
    return unc_returnlocal(w, 0, &v);
}

Unc_RetVal uncl_coro_current(Unc_View *w, Unc_Tuple args, void *udata) {
    return unc_push(w, 1, &w->coroutine);
}

Unc_RetVal uncl_coro_c_hasfinished(Unc_View *w, Unc_Tuple args,
                                       void *udata) {
    Unc_Value v = UNC_BLANK;
    struct unc_coroutine *coro;

    unc_getprototype(w, &args.values[0], &v);
    if (unc_gettype(w, &args.values[0]) != Unc_TOpaque
            || !unc_issame(w, &v, unc_boundvalue(w, 0))) {
        VCLEAR(w, &v);
        return unc_throwexc(w, "type", "argument 1 is not a coroutine");
    }

    if (unc_trylockopaque(w, &args.values[0], NULL, (void **)&coro)) {
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
    
    return unc_returnlocal(w, 0, &v);
}

Unc_RetVal uncl_coro_new(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
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
    
    return unc_returnlocal(w, 0, &v);
}

Unc_RetVal uncl_coro_c_resume(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Value v = UNC_BLANK;
    struct unc_coroutine *coro;
    int init;
    Unc_View *cw;

    unc_getprototype(w, &args.values[0], &v);
    if (unc_gettype(w, &args.values[0]) != Unc_TOpaque
            || !unc_issame(w, &v, unc_boundvalue(w, 0))) {
        VCLEAR(w, &v);
        return unc_throwexc(w, "type", "argument 1 is not a coroutine");
    }
    VCLEAR(w, &v);

    if (unc_trylockopaque(w, &args.values[0], NULL, (void **)&coro))
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
    e = unc0_stackpush(cw, &cw->sval, args.count - 1, args.values + 1);
    if (!e && init) {
        e = unc0_vmrpush(cw);
        if (!e) e = unc0_fcallv(cw, coro->task, args.count - 1, 1, 1, 0, 0);
        if (e) {
            unc0_restoredepth(cw, &cw->sval, 0);
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
    DEBUGPRINT(CORO, ("resuming %p => %p\n", (void *)w, (void *)cw));
    return UNCIL_ERR_TRAMPOLINE;
}

static Unc_View *unc0_coroyield(Unc_View *w, Unc_RetVal *e,
                                int finish, Unc_Tuple retval) {
    Unc_RetVal er = *e;
    Unc_View *cw;
    Unc_Opaque *s = LEFTOVER(Unc_Opaque, VGETENT(&w->coroutine));
    struct unc_coroutine *coro;
    UNC_LOCKL(s->lock);
    coro = s->data;
    cw = coro->returnto;
    ASSERT(cw);
    if (!er) {
        Unc_RetVal ex = unc0_stackpush(cw, &cw->sval,
                                       retval.count, retval.values);
        if (ex) *e = ex;
    } else
        unc0_errinfocopyfrom(cw, w);
    coro->status = er ? UNC_CORO_ST_ERROR 
                : (finish ? UNC_CORO_ST_DONE : UNC_CORO_ST_YIELD);
    UNC_UNLOCKL(s->lock);
    DEBUGPRINT(CORO, ("yielding %p => %p (f=%d, e=%d)\n",
                        (void *)w, (void *)cw, finish, er));
    return cw;
}

Unc_View *unc0_corofinish(Unc_View *w, Unc_RetVal *e) {
    Unc_Tuple rv;
    rv.count = unc0_stackdepth(&w->sval);
    rv.values = w->sval.base;
    return unc0_coroyield(w, e, 1, rv);
}

Unc_RetVal uncl_coro_yield(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    if (!VGETTYPE(&w->coroutine))
        return unc_throwexc(w, "usage", "cannot yield from main routine");
    e = 0;
    w->trampoline = unc0_coroyield(w, &e, 0, args);
    return e ? e : UNCIL_ERR_TRAMPOLINE;
}

Unc_RetVal uncl_coro_canyield(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value v = UNC_BLANK;
    unc_setbool(w, &v, VGETTYPE(&w->coroutine));
    return unc_returnlocal(w, 0, &v);
}

#define FN(x) &uncl_coro_##x, #x
static const Unc_ModuleCFunc lib[] = {
    { FN(canyield),    0, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(current),     0, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(yield),       0, 0, 1, UNC_CFUNC_CONCURRENT },
};

static const Unc_ModuleCFunc lib_coro[] = {
    { FN(new),         1, 1, 0, UNC_CFUNC_CONCURRENT },
};

static const Unc_ModuleCFunc lib_c[] = {
    { &uncl_coro_c_canresume,   "canresume",   1, 0, 0, UNC_CFUNC_CONCURRENT },
    { &uncl_coro_c_hasfinished, "hasfinished", 1, 0, 0, UNC_CFUNC_CONCURRENT },
    { &uncl_coro_c_resume,      "resume",      1, 0, 1, UNC_CFUNC_CONCURRENT },
};

Unc_RetVal uncilmain_coroutine(Unc_View *w) {
    Unc_RetVal e;
    Unc_Value coro = UNC_BLANK;

    e = unc_newobject(w, &coro, NULL);
    if (e) return e;

    e = unc_exportcfunctions(w, PASSARRAY(lib), 0, NULL, NULL);
    if (e) return e;

    e = unc_exportcfunctions(w, PASSARRAY(lib_coro), 1, &coro, NULL);
    if (e) return e;

    {
        Unc_Value ns = UNC_BLANK;
        e = unc_newstringc(w, &ns, "coroutine.coroutine");
        if (e) return e;
        e = unc_setattrc(w, &coro, "__name", &ns);
        if (e) return e;
        VCLEAR(w, &ns);
    }

    e = unc_attrcfunctions(w, &coro, PASSARRAY(lib_c), 1, &coro, NULL);
    if (e) return e;

    e = unc_setpublicc(w, "coroutine", &coro);
    if (e) return e;

    VCLEAR(w, &coro);
    return 0;
}
