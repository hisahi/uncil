/*******************************************************************************
 
Uncil -- VM header

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

#include <limits.h>
#include <stdlib.h>

#define UNCIL_DEFINES

#include "uarithm.h"
#include "uarr.h"
#include "ublob.h"
#include "udebug.h"
#include "ufunc.h"
#include "umem.h"
#include "umt.h"
#include "uncil.h"
#include "uobj.h"
#include "uops.h"
#include "ustr.h"
#include "uvali.h"
#include "uview.h"
#include "uvlq.h"
#include "uvm.h"
#include "uvop.h"

#if defined(__clang__) || (defined(__GNUC__) && (__GNUC__ > 4 \
                    || (__GNUC__ == 4 && __GNUC_MINOR__ >= 5)))
#define DEADCODE() ASSERT(0); __builtin_unreachable()
#elif defined(_MSC_VER)
#define DEADCODE() ASSERT(0); __assume(0)
#else
#define DEADCODE()
#endif

#if UNCIL_C99 && !NOINLINE && UINT_MAX > 65535U
#define MAYBEINLINE FORCEINLINE
#else
#define MAYBEINLINE INLINE
#endif

#if defined(__clang__) || (defined(__GNUC__) && (__GNUC__ > 3))
#define LIKELY(x) (__builtin_expect((x), 1))
#define UNLIKELY(x) (__builtin_expect((x), 0))
#else
#define LIKELY(x) (x)
#define UNLIKELY(x) (x)
#endif

#define MUST(x) if (UNLIKELY(e = (x))) goto vmerror;
#define THROWERR(x) longjmp(*env, (x))
#define THROWERRSTPC(x) do { w->pc = *pc; THROWERR(x); } while (0)
#define THROWERRVMPC(x) do { w->pc = vmpc; THROWERR(x); } while (0)

#if UNCIL_REGW != 2
#error update GETREGN
#endif
#define GETREGN() (pc += 2, pc[-2] | (pc[-1] << 8))
#define GETREG() &regs[GETREGN()]
#define GETVLQ() (UNLIKELY(*pc & 0x80)                                         \
                ? (tmp = 1 + (*pc & 0x7F), pc += tmp, unc__vlqdeczd(pc - tmp)) \
                : *pc++)

#define UNCIL_THREADED_VM 1

/* in ulibcoro.h */
extern Unc_View *unc__corofinish(Unc_View *w, int *e);

#define LITINT_SZ 2
FORCEINLINE Unc_Int unc__litint(Unc_View *w, const byte *pc) {
    byte l = pc[0], h = pc[1];
    return (Unc_Int)(h & 0x80 ? ((h << 8) | l) - (1 << 16) : ((h << 8) | l));
}

#define GETJUMPDST(w) unc__readjumpdst(w, pc)
#define JUMPWIDTH w->jumpw
FORCEINLINE Unc_Size unc__readjumpdst(Unc_View *w, const byte *pc) {
    Unc_Size s = *pc++;
    int j = JUMPWIDTH, sh = 0;
    while (--j) sh += CHAR_BIT, s |= *pc++ << sh;
    return s;
}

int unc__vmcheckpause(Unc_View *w) {
    if (w->flow == UNC_VIEW_FLOW_PAUSE) {
        ATOMICSSET(w->paused, 1);
        UNC_PAUSED(w);
        while (w->flow == UNC_VIEW_FLOW_PAUSE)
            UNC_YIELD();
        UNC_RESUMED(w);
        ATOMICSSET(w->paused, 0);
    }
    return w->flow == UNC_VIEW_FLOW_HALT ? UNCIL_ERR_HALT : 0;
}

INLINE void unc__vmpaused(Unc_View *w, const byte *pc, jmp_buf *env) {
    if (unc__vmcheckpause(w)) {
        w->pc = pc;
        longjmp(*env, UNCIL_ERR_HALT);
    }
}

#if UNCIL_MT_OK
#define CHECKPAUSE() if (UNLIKELY(w->flow)) unc__vmpaused(w, pc, &env);
#else
#define CHECKPAUSE()
#endif

FORCEINLINE void unc__vmrestoredepth(Unc_View *w, Unc_Stack *st, Unc_Size d) {
    Unc_Size n = st->top - st->base;
    ASSERT(n >= d);
    n -= d;
    while (n--)
        VDECREF(w, --st->top);
}

static void unc__vmshrinksreg(Unc_View *w) {
    /* try shrinking sreg if applicable */
    Unc_Size regc = w->sreg.end - w->sreg.base,
             regn = w->sreg.top - w->sreg.base,
             sregn = unc__suggeststacksize(regn);
    if (regc > sregn) {
        w->sreg.base = TMREALLOC(Unc_Value, &w->world->alloc, 0,
                w->sreg.base, regc, sregn);
        w->sreg.top = w->sreg.base + regn;
        w->sreg.end = w->sreg.base + sregn;
    }
}

MAYBEINLINE void unc__vmshrinksregheuristic(Unc_View *w, Unc_Size n) {
    Unc_Size c = w->sreg.end - w->sreg.base;
    if (UNLIKELY((c >> 2) > n)) unc__vmshrinksreg(w);
}

FORCEINLINE int unc__vmaddsreg(Unc_View *w, jmp_buf *env, Unc_Size n) {
    Unc_Value *rtop = w->sreg.top;
    Unc_Size q = w->sreg.top - w->sreg.base + n, c = w->sreg.end - w->sreg.base;
    if (UNLIKELY(q > c)) {
        int e = unc__stackreserve(w, &w->sreg, n);
        if (UNLIKELY(e)) THROWERR(e);
        unc__vmshrinksregheuristic(w, q);
        rtop = w->sreg.top;
    }
    while (n--) VINITNULL(rtop++);
    w->sreg.top = rtop;
    return 0;
}

FORCEINLINE void unc__restoreframe(Unc_View *w, Unc_Frame *f, int altpc) {    
    w->regs = w->sreg.base + f->regs_r;
    w->bounds = f->bounds_r;
    w->jbase = f->jbase_r;
    w->uncfname = f->uncfname_r;
    w->debugbase = f->debugbase_r;
    w->pc = altpc ? f->pc2_r : f->pc_r;
    w->jumpw = f->jumpw_r;
    w->region.top = w->region.base + f->region_r;
    w->boundcount = f->boundcount_r;
    if (w->program != f->program_r) {
        if ((w->program = f->program_r)) {
            w->bcode = w->program->code;
            w->bdata = w->program->data;
        }
    }
}

FORCEINLINE int unc__isframenext(int ft) {
    return ft == Unc_FrameNext || ft == Unc_FrameNextSpew;
}

MAYBEINLINE Unc_Frame *unc__exitframe(Unc_View *w) {
    Unc_Frame *f = --w->frames.top;
    Unc_Size regioncount, valuecount;
    int wvw = 0;
    int ft = f->type;
    int isnext = unc__isframenext(ft);
    Unc_Value wv;
    if (isnext)
        ft = ft == Unc_FrameNext ? Unc_FrameCall : Unc_FrameCallSpew;
    unc__stackwunwind(w, &w->swith, f->swith_r, 1);
    w->rwith.top = w->rwith.base + f->rwith_r;
    switch (ft) {
    case Unc_FrameCall:
        regioncount = (w->region.top - w->region.base) - f->region_r;
        valuecount = regioncount
                ? unc__stackdepth(&w->sval) - w->region.top[-1] : 0;
        isnext &= valuecount == 0;
        wvw = 1;
        if (!regioncount)
            VIMPOSE(w, &wv, &w->regs[0]);
        else if (valuecount)
            VIMPOSE(w, &wv, &w->sval.base[w->region.top[-1]]);
        else
            VINITNULL(&wv);
        unc__vmrestoredepth(w, &w->sreg, f->sreg_r);
        /* unc__vmshrinksregheuristic(w, f->sreg_r); */
        unc__vmrestoredepth(w, &w->sval, f->sval_r);
        break;
    case Unc_FrameCallSpew:
    case Unc_FrameMain:
        regioncount = (w->region.top - w->region.base) - f->region_r;
        valuecount = regioncount
                ? unc__stackdepth(&w->sval) - w->region.top[-1] : 0;
        isnext &= valuecount == 0;
        if (!regioncount) {
            unc__vmrestoredepth(w, &w->sval, f->sval_r);
            {
                int e;
                e = unc__stackpushv(w, &w->sval, &w->regs[0]);
                ASSERT(!e); /* we reserved the space, it should be there */
            }
            unc__vmrestoredepth(w, &w->sreg, f->sreg_r);
            /* unc__vmshrinksregheuristic(w, f->sreg_r); */
        } else {
            unc__stackpullrug(w, &w->sval, f->sval_r, w->region.top[-1]);
            unc__vmrestoredepth(w, &w->sreg, f->sreg_r);
            /* unc__vmshrinksregheuristic(w, f->sreg_r); */
        }
        break;
    default:
        DEADCODE();
        NEVER_();
    }
    --w->recurse;
    unc__restoreframe(w, f, isnext);
    if (wvw) VMOVE(w, &w->regs[f->target], &wv);
    return f;
}

MAYBEINLINE Unc_Frame *unc__exitframe0(Unc_View *w) {
    Unc_Frame *f = --w->frames.top;
    int ft = f->type;
    int isnext = unc__isframenext(ft);
    if (isnext)
        ft = ft == Unc_FrameNext ? Unc_FrameCall : Unc_FrameCallSpew;
    unc__stackwunwind(w, &w->swith, f->swith_r, 1);
    w->rwith.top = w->rwith.base + f->rwith_r;
    --w->recurse;
    unc__restoreframe(w, f, isnext);
    unc__vmrestoredepth(w, &w->sreg, f->sreg_r);
    /* unc__vmshrinksregheuristic(w, f->sreg_r); */
    unc__vmrestoredepth(w, &w->sval, f->sval_r);
    if (ft == Unc_FrameCall) {
        VSETNULL(w, &w->regs[f->target]);
    } else {
        ASSERT(ft == Unc_FrameCallSpew || ft == Unc_FrameMain);
    }
    return f;
}

MAYBEINLINE Unc_Frame *unc__exitframe1(Unc_View *w, Unc_Value *wv) {
    Unc_Frame *f = --w->frames.top;
    int ft = f->type;
    int isnext = unc__isframenext(ft);
    if (isnext)
        ft = ft == Unc_FrameNext ? Unc_FrameCall : Unc_FrameCallSpew;
    unc__stackwunwind(w, &w->swith, f->swith_r, 1);
    w->rwith.top = w->rwith.base + f->rwith_r;
    --w->recurse;
    unc__restoreframe(w, f, 0);
    if (ft == Unc_FrameCall) {
        VCOPY(w, &w->regs[f->target], wv);
        unc__vmrestoredepth(w, &w->sreg, f->sreg_r);
        /* unc__vmshrinksregheuristic(w, f->sreg_r); */
        unc__vmrestoredepth(w, &w->sval, f->sval_r);
    } else {
        int e;
        ASSERT(ft == Unc_FrameCallSpew || ft == Unc_FrameMain);
        unc__vmrestoredepth(w, &w->sreg, f->sreg_r);
        /* unc__vmshrinksregheuristic(w, f->sreg_r); */
        unc__vmrestoredepth(w, &w->sval, f->sval_r);
        e = unc__stackpushv(w, &w->sval, wv);
        ASSERT(!e); /* we reserved the space, it should be there */
    }
    return f;
}

FORCEINLINE void unc__exitccall(Unc_View *w, Unc_Frame *f, int e) {
    Unc_Size valuecount;
    switch (f->type) {
    case Unc_FrameCallC:
    {
        Unc_Value wv;
        w->cfunc = f->cfunc_r;
        valuecount = unc__stackdepth(&w->sval) - f->sval_r;
        if (valuecount)
            VIMPOSE(w, &wv, &w->sval.base[f->sval_r]);
        else
            VINITNULL(&wv);
        unc__vmrestoredepth(w, &w->sval, f->sreg_r);
        w->region.top = w->region.base + f->region_r;
        w->bounds = f->bounds_r;
        w->boundcount = f->boundcount_r;
        VMOVE(w, &w->regs[f->target], &wv);
        return;
    }
    case Unc_FrameCallCSpew:
        w->cfunc = f->cfunc_r;
        if (UNLIKELY(e)) {
            unc__vmrestoredepth(w, &w->sval, f->sreg_r);
        } else {
            ASSERT(unc__stackdepth(&w->sval) >= f->sval_r);
            valuecount = unc__stackdepth(&w->sval) - f->sval_r;
            unc__stackpullrug(w, &w->sval, f->sreg_r, f->sval_r);
        }
        w->region.top = w->region.base + f->region_r;
        w->bounds = f->bounds_r;
        w->boundcount = f->boundcount_r;
        return;
    default:
        DEADCODE();
        NEVER_();
    }
}

static Unc_Frame *unc__unwindframeerr(Unc_View *w) {
    Unc_Frame *f = --w->frames.top;
    unc__stackwunwind(w, &w->swith, f->swith_r, 1);
    w->rwith.top = w->rwith.base + f->rwith_r;
    switch (f->type) {
    case Unc_FrameMain:
    case Unc_FrameCall:
    case Unc_FrameCallSpew:
    case Unc_FrameNext:
    case Unc_FrameNextSpew:
        --w->recurse;
        unc__vmrestoredepth(w, &w->sreg, f->sreg_r);
        /* unc__vmshrinksregheuristic(w, f->sreg_r); */
    case Unc_FrameTry:
        unc__vmrestoredepth(w, &w->sval, f->sval_r);
        break;
    case Unc_FrameCallC:
    case Unc_FrameCallCSpew:
        unc__exitccall(w, f, 1);
        return f;
    default:
        DEADCODE();
        NEVER_();
    }
    unc__restoreframe(w, f, 0);
    return f;
}

FORCEINLINE void unc__doxpop(Unc_View *w) {
    Unc_Frame *f = --w->frames.top;
    unc__stackwunwind(w, &w->swith, f->swith_r, 1);
    ASSERT(f->type == Unc_FrameTry);
    w->rwith.top = w->rwith.base + f->rwith_r;
    w->region.top = w->region.base + f->region_r;
    unc__vmrestoredepth(w, &w->sval, f->sval_r);
}

FORCEINLINE void unc__unwindtotry(Unc_View *w) {
    while (w->frames.top[-1].type == Unc_FrameTry) unc__doxpop(w);
}

INLINE Unc_Frame *unc__saveframe(Unc_View *w, jmp_buf *env, const byte *pc) {
    Unc_Frame *f;
    if (w->frames.top == w->frames.end) {
        Unc_Frame *p = w->frames.base, *np;
        Unc_Size z = w->frames.end - p;
        Unc_Size nz = z + 4;
        np = TMREALLOC(Unc_Frame, &w->world->alloc, 0, p, z, nz);
        if (!np) {
            w->pc = pc;
            THROWERR(UNCIL_ERR_MEM);
        }
        w->frames.base = np;
        w->frames.top = np + (w->frames.top - p);
        w->frames.end = np + nz;
    }
    f = w->frames.top++;
    f->regs_r = w->regs - w->sreg.base;
    f->bounds_r = w->bounds;
    f->jbase_r = w->jbase;
    f->uncfname_r = w->uncfname;
    f->debugbase_r = w->debugbase;
    f->pc_r = pc;
    f->sreg_r = unc__stackdepth(&w->sreg);
    f->sval_r = unc__stackdepth(&w->sval);
    f->jumpw_r = w->jumpw;
    f->region_r = w->region.top - w->region.base;
    f->boundcount_r = w->boundcount;
    f->program_r = w->program;
    f->swith_r = unc__stackdepth(&w->swith);
    f->rwith_r = w->rwith.top - w->rwith.base;
    f->cfunc_r = w->cfunc;
    f->tails = 0;
    return f;
}

FORCEINLINE void unc__vmfcall(Unc_View *w, jmp_buf *env, Unc_Function *fn,
            Unc_Size argc, int st, int fromc, int allowc,
            Unc_RegFast target, const byte **pc) {
    int e;
    Unc_Frame *f;
    ASSERT(!fromc || st);
    if (w->recurse >= w->recurselimit)
        THROWERRSTPC(UNCIL_ERR_TOODEEP);
    if (fromc && !fn) {
        if (argc) {
            unc__stackpop(w, &w->sval, argc);
            argc = 0;
        }
        ASSERT(w->fmain.type == Unc_TFunction);
        fn = LEFTOVER(Unc_Function, VGETENT(&w->fmain));
    } else {
        ASSERT(fn);
        if (argc < fn->rargc)
            THROWERRSTPC(UNCIL_ERR_ARG_NOTENOUGHARGS);
        if (argc > fn->argc && !(fn->flags & UNC_FUNCTION_FLAG_ELLIPSIS))
            THROWERRSTPC(UNCIL_ERR_ARG_TOOMANYARGS);
    }

    if (fn->flags & UNC_FUNCTION_FLAG_CFUNC) {
        Unc_Entity *topent = w->world->etop;
        int cflags = fn->f.c.cflags;
        if (!allowc)
            THROWERRSTPC(UNCIL_ERR_ARG_NOCFUNC);

        f = unc__saveframe(w, env, *pc);

        if (argc < fn->argc) {
            /* need to allocate space for optionals */
            e = unc__stackpush(w, &w->sval, fn->argc - argc,
                                            fn->defaults + (argc - fn->rargc));
            if (UNLIKELY(e))
                THROWERRSTPC(e);
            f->sval_r += fn->argc - argc;
            argc = fn->argc;
        }

        f->type = st ? Unc_FrameCallCSpew : Unc_FrameCallC;
        f->target = target;
        /* reappropriate sreg_r which is not needed for C funcs */
        f->sreg_r = f->sval_r - argc;

        w->boundcount = fn->refc;
        w->bounds = fn->refs;
        w->cfunc = &fn->f.c;
        w->debugbase = NULL;
        if (cflags & UNC_CFUNC_EXCLUSIVE)
            UNC_PAUSE(w);
        else if (!(cflags & UNC_CFUNC_CONCURRENT))
            (void)UNC_LOCKFP(w, fn->f.c.lock);
        {
            Unc_Tuple args;
            args.count = argc;
            args.values = w->sval.top - args.count;
            e = (*fn->f.c.fc)(w, args, fn->f.c.udata);
        }
        if (!fromc && e && UNCIL_ERR_KIND(e) != UNCIL_ERR_KIND_TRAMPOLINE) {
            if (e != UNCIL_ERR_UNCIL)
                unc__errtoexcept(w, e, &w->exc);
            e = UNCIL_ERR_UNCIL;
            /* unc__errstackpush(w); duplicate */
        }
        {
            Unc_Entity *tmpent = w->world->etop;
            Unc_Size vid = w->vid;
            /* this relies on entities being on the list from the most recently
               to the least recently woken up ones, which is currently true */
            while (tmpent && tmpent != topent) {
                if (tmpent->creffed && tmpent->vid == vid)
                    tmpent->creffed = 0;
                tmpent = tmpent->down;
            }
        }
        if (cflags & UNC_CFUNC_EXCLUSIVE)
            UNC_RESUME(w);
        else if (!(cflags & UNC_CFUNC_CONCURRENT))
            UNC_UNLOCKF(fn->f.c.lock);
        if (e) {
            /* trampoline is used to jump from one VM context to another */
            if (UNCIL_ERR_KIND(e) == UNCIL_ERR_KIND_TRAMPOLINE
                    && (!w->trampoline || fromc))
                e = UNCIL_ERR_INTERNAL;
            else
                unc__exitccall(w, --w->frames.top, 0);
            THROWERRSTPC(e);
        } else unc__exitccall(w, --w->frames.top, 0);
        if (fromc) THROWERRSTPC(1);
    } else {
        Unc_Value *regs;
        f = unc__saveframe(w, env, *pc);
        
        /* allocate registers */
        unc__vmaddsreg(w, env, fn->f.u.regc);
        regs = w->sreg.top - fn->f.u.regc;

        f->type = fromc ? Unc_FrameMain :
                  st ? Unc_FrameCallSpew : Unc_FrameCall;
        f->target = target;
        f->sval_r -= argc;

        if (fn->flags & UNC_FUNCTION_FLAG_ELLIPSIS) {
            Unc_Size fargc = fn->argc, ofargc = argc - fargc;
            Unc_Entity *en = unc__wake(w, Unc_TArray);
            if (argc < fargc) ofargc = 0;
            if (!en)
                e = UNCIL_ERR_MEM;
            else
                e = unc__initarrayraw(w, LEFTOVER(Unc_Array, en),
                                    ofargc, w->sval.top - ofargc);
            if (UNLIKELY(e)) {
                w->sreg.top -= fn->f.u.regc;
                THROWERRSTPC(e);
            }
            VINITENT(&regs[fn->f.u.floc + fargc], Unc_TArray, en);
            w->sval.top -= ofargc;
            argc -= ofargc;
        }

        ++w->recurse;
        w->jumpw = fn->f.u.jumpw;
        w->regs = regs;
        w->bounds = fn->refs;
        w->boundcount = fn->refc;
        ASSERT(fn->f.u.program);
        if (UNLIKELY(w->program != fn->f.u.program))
            unc__wsetprogram(w, fn->f.u.program);
        *pc = w->jbase = w->bcode + fn->f.u.pc;
        w->uncfname = (fn->flags & UNC_FUNCTION_FLAG_NAMED)
                    ? w->bdata + fn->f.u.nameoff
                    : (const byte *)((fn->flags & UNC_FUNCTION_FLAG_MAIN)
                    ? "\x06<main>" : "\x0b<anonymous>");
        w->debugbase = w->bdata + fn->f.u.dbugoff;
        if (fn->argc) {
            w->sval.top -= argc;
            unc__memcpy(regs + fn->f.u.floc, w->sval.top,
                        argc * sizeof(Unc_Value));
            if (argc < fn->argc) {
                Unc_Size i, nc = fn->argc - argc, nf = argc - fn->rargc;
                unc__memcpy(regs + fn->f.u.floc + argc,
                            fn->defaults + nf, nc * sizeof(Unc_Value));
                for (i = 0; i < nc; ++i)
                    VINCREF(w, &fn->defaults[nf + i]);
            }
        } else if (st) {
            e = unc__stackreserve(w, &w->sval, 1);
            if (UNLIKELY(e)) {
                unc__unwindframeerr(w);
                THROWERRSTPC(e);
            }
        }
    }
}

int unc__fcall(Unc_View *w, Unc_Function *fn, Unc_Size argc, int st,
               int fromc, int allowc, Unc_RegFast target) {
    int e;
    jmp_buf env;
    if (fromc && !UNC_LOCKFQ(w->runlock))
        return UNCIL_ERR_LOGIC_CANNOTLOCK;
    if ((e = setjmp(env))) {
        if (UNCIL_IS_ERR(e)) {
            unc__vmrestoredepth(w, &w->sval, w->region.top[-1]);
            if (!fromc) --w->region.top;
        }
        UNC_UNLOCKF(w->runlock);
        return e;
    }
    unc__vmfcall(w, &env, fn, argc, st, fromc, allowc, target, &w->pc);
    UNC_UNLOCKF(w->runlock);
    return 0;
}

typedef struct Unc_FramePartial {
    Unc_FrameType type;
    const byte *pc_r;
    Unc_Size sval_r;
    Unc_Size target;
    const byte *pc2_r;
    Unc_Size tails;
} Unc_FramePartial;

FORCEINLINE int unc__shouldexitonframe(Unc_Frame *f) {
    return f->type == Unc_FrameMain;
}

FORCEINLINE int unc__shouldexitonpframe(Unc_FramePartial *f) {
    return f->type == Unc_FrameMain;
}

FORCEINLINE int unc__shouldspewonpframe(Unc_FramePartial *f) {
    switch (f->type) {
    case Unc_FrameCallSpew:
    case Unc_FrameCallCSpew:
    case Unc_FrameNextSpew:
    case Unc_FrameMain:
        return 1;
    default:
        return 0;
    }
}

static void unc__dotailpre(Unc_View *w, Unc_FramePartial *p, Unc_Frame *f) {
    ASSERT(f->type == Unc_FrameCall
        || f->type == Unc_FrameCallSpew
        || f->type == Unc_FrameNext
        || f->type == Unc_FrameNextSpew
        || f->type == Unc_FrameMain);
    unc__stackwunwind(w, &w->swith, f->swith_r, 1);
    w->rwith.top = w->rwith.base + f->rwith_r;
    --w->recurse;
    unc__restoreframe(w, f, 0);
    unc__vmrestoredepth(w, &w->sreg, f->sreg_r);
    /* unc__vmshrinksregheuristic(w, f->sreg_r); */
    p->type = f->type;
    p->pc_r = f->pc_r;
    p->target = f->target;
    p->sval_r = f->sval_r;
    p->pc2_r = f->pc2_r;
    p->tails = f->tails;
}

MAYBEINLINE void unc__dotailpost(Unc_View *w,
                                 Unc_FramePartial *p, Unc_Frame *f) {
    f->type = p->type;
    f->pc_r = p->pc_r;
    f->target = p->target;
    f->sval_r = p->sval_r;
    f->pc2_r = p->pc2_r;
    f->tails = p->tails + 1;
    unc__vmrestoredepth(w, &w->sval, p->sval_r);
}

MAYBEINLINE void unc__dotailpostc(Unc_View *w,
                                  Unc_FramePartial *p) {
}

int unc__fcallv(Unc_View *w, Unc_Value *v, Unc_Size argc,
                int spew, int fromc, int allowc, Unc_RegFast x) {
    switch (VGETTYPE(v)) {
    case Unc_TFunction:
        return unc__fcall(w, LEFTOVER(Unc_Function, VGETENT(v)), argc,
                          spew, fromc, allowc, x);
    case Unc_TBoundFunction:
    {
        int e;
        Unc_FunctionBound *b = LEFTOVER(Unc_FunctionBound, VGETENT(v));
        e = unc__stackinsertn(w, &w->sval, argc++, &b->boundto);
        if (e) return e;
        ASSERT(b->fn.type == Unc_TFunction);
        return unc__fcall(w, LEFTOVER(Unc_Function, VGETENT(&b->fn)), argc,
                          spew, fromc, allowc, x);
    }
    case Unc_TObject:
    case Unc_TOpaque:
    {
        Unc_Value *o;
        int e;
        e = unc__stackinsertn(w, &w->sval, argc++, v);
        if (e) return e;
unc__fcallv_call_again:
        e = unc__getprotomethod(w, v, PASSSTRL(OPOVERLOAD(call)), &o);
        if (e) return e;
        if (o) {
            switch (o->type) {
            case Unc_TFunction:
                return unc__fcall(w, LEFTOVER(Unc_Function, VGETENT(o)), argc,
                                  spew, fromc, allowc, x);
            case Unc_TBoundFunction:
                return unc__fcallv(w, o, argc, spew, fromc, allowc, x);
            case Unc_TObject:
            case Unc_TOpaque:
                v = o;
                goto unc__fcallv_call_again;
            default:
                ;
            }
        }
    }
    default:
        return UNCIL_ERR_TYPE_NOTFUNCTION;
    }
}

FORCEINLINE void unc__loadstrp(const byte *offp, Unc_Size *l, const byte **b) {
    *l = unc__vlqdecz(&offp);
    *b = offp;
}

FORCEINLINE void unc__loadstr(Unc_View *w, Unc_Size off,
                             Unc_Size *l, const byte **b) {
    unc__loadstrp(w->bdata + off, l, b);
}

void unc__loadstrpx(const byte *offp, Unc_Size *l, const byte **b) {
    unc__loadstrp(offp, l, b);
}

FORCEINLINE Unc_Size unc__diffregion(Unc_View *w) {
    ASSERT(w->region.top >= w->region.base ||
        !(w->frames.top > w->frames.base &&
            w->region.top - w->region.base <= w->frames.top[-1].region_r));
    return unc__stackdepth(&w->sval) - *--w->region.top;
}

FORCEINLINE void unc__getpub(Unc_View *w, jmp_buf *env,
                            Unc_Size sl, const byte *sb, Unc_Value *v) {
    Unc_Value *g;
    (void)UNC_LOCKFP(w, w->world->public_lock);
    if (w->import) {
        g = unc__gethtbls(w, w->exports, sl, sb);
        if (g) {
            UNC_UNLOCKF(w->world->public_lock);
            VCOPY(w, v, g);
            return;
        }
    }
    g = unc__gethtbls(w, w->pubs, sl, sb);
    UNC_UNLOCKF(w->world->public_lock);
    if (g)
        VCOPY(w, v, g);
    else
        longjmp(*env, unc__err_withname(w, UNCIL_ERR_ARG_NOSUCHNAME, sl, sb));
}

FORCEINLINE void unc__setpub(Unc_View *w, jmp_buf *env,
                            Unc_Size sl, const byte *sb, Unc_Value *v) {
    int e;
    Unc_Value *g;
    (void)UNC_LOCKFP(w, w->world->public_lock);
    e = unc__puthtbls(w, w->import ? w->exports : w->pubs, sl, sb, &g);
    if (UNLIKELY(e)) {
        UNC_UNLOCKF(w->world->public_lock);
        longjmp(*env, e);
    }
    VCOPY(w, g, v);
    UNC_UNLOCKF(w->world->public_lock);
}

INLINE void unc__delpub(Unc_View *w, jmp_buf *env,
                        Unc_Size sl, const byte *sb) {
    int e;
    (void)UNC_LOCKFP(w, w->world->public_lock);
    if (w->import) {
        e = unc__delhtbls(w, w->exports, sl, sb);
        if (UNLIKELY(e)) {
            UNC_UNLOCKF(w->world->public_lock);
            longjmp(*env, e);
        }
    }
    e = unc__delhtbls(w, w->pubs, sl, sb);
    if (UNLIKELY(e)) {
        UNC_UNLOCKF(w->world->public_lock);
        longjmp(*env, e);
    }
    UNC_UNLOCKF(w->world->public_lock);
}

MAYBEINLINE void unc__vmobadd(Unc_View *w, jmp_buf *env,
                        Unc_Value *tr, Unc_Value *a, Unc_Value *b,
                        const byte *vmpc) {
    if (UNLIKELY(VGETTYPE(b) == Unc_TObject || VGETTYPE(b) == Unc_TOpaque))
        goto unc__vmobadd_o;
    switch (VGETTYPE(a)) {
    case Unc_TInt:
        switch (VGETTYPE(b)) {
        case Unc_TInt:
            if (UNLIKELY(ADDOVF(VGETINT(a), VGETINT(b))))
                VSETFLT(w, tr, (Unc_Float)VGETINT(a) + (Unc_Float)VGETINT(b));
            else
                VSETINT(w, tr, VGETINT(a) + VGETINT(b));
            return;
        case Unc_TFloat:
            VSETFLT(w, tr, (Unc_Float)VGETINT(a) + VGETFLT(b));
            return;
        default:
            THROWERRVMPC(unc__err_unsup2(w, VGETTYPE(a), VGETTYPE(b)));
        }
    case Unc_TFloat:
        switch (VGETTYPE(b)) {
        case Unc_TInt:
            VSETFLT(w, tr, VGETFLT(a) + (Unc_Float)VGETINT(b));
            return;
        case Unc_TFloat:
            VSETFLT(w, tr, VGETFLT(a) + VGETFLT(b));
            return;
        default:
            THROWERRVMPC(unc__err_unsup2(w, VGETTYPE(a), VGETTYPE(b)));
        }
    case Unc_TObject:
    case Unc_TOpaque:
    unc__vmobadd_o:
    {
        int e;
        Unc_Value vout;
        e = unc__vovlbinary(w, a, b, &vout, PASSSTRL(OPOVERLOAD(add)),
                                            PASSSTRL(OPOVERLOAD(add2)));
        if (e) {
            if (UNCIL_IS_ERR(e))
                THROWERRVMPC(e);
            VMOVE(w, tr, &vout);
            return;
        }
    }
    default:
        THROWERRVMPC(unc__err_unsup2(w, VGETTYPE(a), VGETTYPE(b)));
    }
}

MAYBEINLINE void unc__vmobsub(Unc_View *w, jmp_buf *env,
                        Unc_Value *tr, Unc_Value *a, Unc_Value *b,
                        const byte *vmpc) {
    if (UNLIKELY(VGETTYPE(b) == Unc_TObject || VGETTYPE(b) == Unc_TOpaque))
        goto unc__vmobsub_o;
    switch (VGETTYPE(a)) {
    case Unc_TInt:
        switch (VGETTYPE(b)) {
        case Unc_TInt:
            if (UNLIKELY(SUBOVF(VGETINT(a), VGETINT(b))))
                VSETFLT(w, tr, (Unc_Float)VGETINT(a) - (Unc_Float)VGETINT(b));
            else
                VSETINT(w, tr, VGETINT(a) - VGETINT(b));
            return;
        case Unc_TFloat:
            VSETFLT(w, tr, (Unc_Float)VGETINT(a) - VGETFLT(b));
            return;
        default:
            THROWERRVMPC(unc__err_unsup2(w, VGETTYPE(a), VGETTYPE(b)));
        }
    case Unc_TFloat:
        switch (VGETTYPE(b)) {
        case Unc_TInt:
            VSETFLT(w, tr, VGETFLT(a) - (Unc_Float)VGETINT(b));
            return;
        case Unc_TFloat:
            VSETFLT(w, tr, VGETFLT(a) - VGETFLT(b));
            return;
        default:
            THROWERRVMPC(unc__err_unsup2(w, VGETTYPE(a), VGETTYPE(b)));
        }
    case Unc_TObject:
    case Unc_TOpaque:
    unc__vmobsub_o:
    {
        int e;
        Unc_Value vout;
        e = unc__vovlbinary(w, a, b, &vout, PASSSTRL(OPOVERLOAD(sub)),
                                            PASSSTRL(OPOVERLOAD(sub2)));
        if (e) {
            if (UNCIL_IS_ERR(e))
                THROWERRVMPC(e);
            VMOVE(w, tr, &vout);
            return;
        }
    }
    default:
        THROWERRVMPC(unc__err_unsup2(w, VGETTYPE(a), VGETTYPE(b)));
    }
}

MAYBEINLINE void unc__vmobmul(Unc_View *w, jmp_buf *env,
                        Unc_Value *tr, Unc_Value *a, Unc_Value *b,
                        const byte *vmpc) {
    if (UNLIKELY(VGETTYPE(b) == Unc_TObject || VGETTYPE(b) == Unc_TOpaque))
        goto unc__vmobmul_o;
    switch (VGETTYPE(a)) {
    case Unc_TInt:
        switch (VGETTYPE(b)) {
        case Unc_TInt:
            if (UNLIKELY(MULOVF(VGETINT(a), VGETINT(b))))
                VSETFLT(w, tr, (Unc_Float)VGETINT(a) * (Unc_Float)VGETINT(b));
            else
                VSETINT(w, tr, VGETINT(a) * VGETINT(b));
            return;
        case Unc_TFloat:
            VSETFLT(w, tr, (Unc_Float)VGETINT(a) * VGETFLT(b));
            return;
        default:
            THROWERRVMPC(unc__err_unsup2(w, VGETTYPE(a), VGETTYPE(b)));
        }
    case Unc_TFloat:
        switch (VGETTYPE(b)) {
        case Unc_TInt:
            VSETFLT(w, tr, VGETFLT(a) * (Unc_Float)VGETINT(b));
            return;
        case Unc_TFloat:
            VSETFLT(w, tr, VGETFLT(a) * VGETFLT(b));
            return;
        default:
            THROWERRVMPC(unc__err_unsup2(w, VGETTYPE(a), VGETTYPE(b)));
        }
    case Unc_TObject:
    case Unc_TOpaque:
    unc__vmobmul_o:
    {
        int e;
        Unc_Value vout;
        e = unc__vovlbinary(w, a, b, &vout, PASSSTRL(OPOVERLOAD(mul)),
                                            PASSSTRL(OPOVERLOAD(mul2)));
        if (e) {
            if (UNCIL_IS_ERR(e))
                THROWERRVMPC(e);
            VMOVE(w, tr, &vout);
            return;
        }
    }
    default:
        THROWERRVMPC(unc__err_unsup2(w, VGETTYPE(a), VGETTYPE(b)));
    }
}

static void unc__vmobdiv(Unc_View *w, jmp_buf *env,
                        Unc_Value *tr, Unc_Value *a, Unc_Value *b,
                        const byte *vmpc) {
    if (UNLIKELY(VGETTYPE(b) == Unc_TObject || VGETTYPE(b) == Unc_TOpaque))
        goto unc__vmobdiv_o;
    switch (VGETTYPE(a)) {
    case Unc_TInt:
        switch (VGETTYPE(b)) {
        case Unc_TInt:
            if (VGETINT(b) == 0) THROWERRVMPC(UNCIL_ERR_ARG_DIVBYZERO);
            VSETFLT(w, tr, (Unc_Float)VGETINT(a) / (Unc_Float)VGETINT(b));
            return;
        case Unc_TFloat:
            if (VGETFLT(b) == 0) THROWERRVMPC(UNCIL_ERR_ARG_DIVBYZERO);
            VSETFLT(w, tr, (Unc_Float)VGETINT(a) / VGETFLT(b));
            return;
        default:
            THROWERRVMPC(unc__err_unsup2(w, VGETTYPE(a), VGETTYPE(b)));
        }
    case Unc_TFloat:
        switch (VGETTYPE(b)) {
        case Unc_TInt:
            if (VGETINT(b) == 0) THROWERRVMPC(UNCIL_ERR_ARG_DIVBYZERO);
            VSETFLT(w, tr, VGETFLT(a) / (Unc_Float)VGETINT(b));
            return;
        case Unc_TFloat:
            if (VGETFLT(b) == 0) THROWERRVMPC(UNCIL_ERR_ARG_DIVBYZERO);
            VSETFLT(w, tr, VGETFLT(a) / VGETFLT(b));
            return;
        default:
            THROWERRVMPC(unc__err_unsup2(w, VGETTYPE(a), VGETTYPE(b)));
        }
    case Unc_TObject:
    case Unc_TOpaque:
    unc__vmobdiv_o:
    {
        int e;
        Unc_Value vout;
        e = unc__vovlbinary(w, a, b, &vout, PASSSTRL(OPOVERLOAD(div)),
                                            PASSSTRL(OPOVERLOAD(div2)));
        if (e) {
            if (UNCIL_IS_ERR(e))
                THROWERRVMPC(e);
            VMOVE(w, tr, &vout);
            return;
        }
    }
    default:
        THROWERRVMPC(unc__err_unsup2(w, VGETTYPE(a), VGETTYPE(b)));
    }
}

static void unc__vmobidiv(Unc_View *w, jmp_buf *env,
                        Unc_Value *tr, Unc_Value *a, Unc_Value *b,
                        const byte *vmpc) {
    if (UNLIKELY(VGETTYPE(b) == Unc_TObject || VGETTYPE(b) == Unc_TOpaque))
        goto unc__vmobidiv_o;
    switch (VGETTYPE(a)) {
    case Unc_TInt:
        switch (VGETTYPE(b)) {
        case Unc_TInt:
            if (VGETINT(b) == 0) THROWERRVMPC(UNCIL_ERR_ARG_DIVBYZERO);
            VSETINT(w, tr, unc__iidiv(VGETINT(a), VGETINT(b)));
            return;
        case Unc_TFloat:
            if (VGETFLT(b) == 0) THROWERRVMPC(UNCIL_ERR_ARG_DIVBYZERO);
            VSETFLT(w, tr, unc__fidiv((Unc_Float)VGETINT(a), VGETFLT(b)));
            return;
        default:
            THROWERRVMPC(unc__err_unsup2(w, VGETTYPE(a), VGETTYPE(b)));
        }
    case Unc_TFloat:
        switch (VGETTYPE(b)) {
        case Unc_TInt:
            if (VGETINT(b) == 0) THROWERRVMPC(UNCIL_ERR_ARG_DIVBYZERO);
            VSETFLT(w, tr, unc__fidiv(VGETFLT(a), (Unc_Float)VGETINT(b)));
            return;
        case Unc_TFloat:
            if (VGETFLT(b) == 0) THROWERRVMPC(UNCIL_ERR_ARG_DIVBYZERO);
            VSETFLT(w, tr, unc__fidiv(VGETFLT(a), VGETFLT(b)));
            return;
        default:
            THROWERRVMPC(unc__err_unsup2(w, VGETTYPE(a), VGETTYPE(b)));
        }
        THROWERRVMPC(unc__err_unsup2(w, VGETTYPE(a), VGETTYPE(b)));
    case Unc_TObject:
    case Unc_TOpaque:
    unc__vmobidiv_o:
    {
        int e;
        Unc_Value vout;
        e = unc__vovlbinary(w, a, b, &vout, PASSSTRL(OPOVERLOAD(idiv)),
                                            PASSSTRL(OPOVERLOAD(idiv2)));
        if (e) {
            if (UNCIL_IS_ERR(e))
                THROWERRVMPC(e);
            VMOVE(w, tr, &vout);
            return;
        }
    }
    default:
        THROWERRVMPC(unc__err_unsup2(w, VGETTYPE(a), VGETTYPE(b)));
    }
}

static void unc__vmobmod(Unc_View *w, jmp_buf *env,
                        Unc_Value *tr, Unc_Value *a, Unc_Value *b,
                        const byte *vmpc) {
    if (UNLIKELY(VGETTYPE(b) == Unc_TObject || VGETTYPE(b) == Unc_TOpaque))
        goto unc__vmobmod_o;
    switch (VGETTYPE(a)) {
    case Unc_TInt:
        switch (VGETTYPE(b)) {
        case Unc_TInt:
            if (VGETINT(b) == 0) THROWERRVMPC(UNCIL_ERR_ARG_DIVBYZERO);
            VSETINT(w, tr, unc__imod(VGETINT(a), VGETINT(b)));
            return;
        case Unc_TFloat:
            if (VGETFLT(b) == 0) THROWERRVMPC(UNCIL_ERR_ARG_DIVBYZERO);
            VSETFLT(w, tr, unc__fmod((Unc_Float)VGETINT(a), VGETFLT(b)));
            return;
        default:
            THROWERRVMPC(unc__err_unsup2(w, VGETTYPE(a), VGETTYPE(b)));
        }
    case Unc_TFloat:
        switch (VGETTYPE(b)) {
        case Unc_TInt:
            if (VGETINT(b) == 0) THROWERRVMPC(UNCIL_ERR_ARG_DIVBYZERO);
            VSETFLT(w, tr, unc__fmod(VGETFLT(a), (Unc_Float)VGETINT(b)));
            return;
        case Unc_TFloat:
            if (VGETFLT(b) == 0) THROWERRVMPC(UNCIL_ERR_ARG_DIVBYZERO);
            VSETFLT(w, tr, unc__fmod(VGETFLT(a), VGETFLT(b)));
            return;
        default:
            THROWERRVMPC(unc__err_unsup2(w, VGETTYPE(a), VGETTYPE(b)));
        }
        THROWERRVMPC(unc__err_unsup2(w, VGETTYPE(a), VGETTYPE(b)));
    case Unc_TObject:
    case Unc_TOpaque:
    unc__vmobmod_o:
    {
        int e;
        Unc_Value vout;
        e = unc__vovlbinary(w, a, b, &vout, PASSSTRL(OPOVERLOAD(mod)),
                                            PASSSTRL(OPOVERLOAD(mod2)));
        if (e) {
            if (UNCIL_IS_ERR(e))
                THROWERRVMPC(e);
            VMOVE(w, tr, &vout);
            return;
        }
    }
    default:
        THROWERRVMPC(unc__err_unsup2(w, VGETTYPE(a), VGETTYPE(b)));
    }
}

static void unc__vmoband(Unc_View *w, jmp_buf *env,
                        Unc_Value *tr, Unc_Value *a, Unc_Value *b,
                        const byte *vmpc) {
    if (UNLIKELY(VGETTYPE(b) == Unc_TObject || VGETTYPE(b) == Unc_TOpaque))
        goto unc__vmoband_o;
    switch (VGETTYPE(a)) {
    case Unc_TBool:
        if (VGETTYPE(b) == Unc_TBool) {
            VSETBOOL(w, tr, VGETINT(a) && VGETINT(b));
            return;
        }
        THROWERRVMPC(unc__err_unsup2(w, VGETTYPE(a), VGETTYPE(b)));
    case Unc_TInt:
        if (VGETTYPE(b) == Unc_TInt) {
            VSETINT(w, tr, VGETINT(a) & VGETINT(b));
            return;
        }
        THROWERRVMPC(unc__err_unsup2(w, VGETTYPE(a), VGETTYPE(b)));
    case Unc_TObject:
    case Unc_TOpaque:
    unc__vmoband_o:
    {
        int e;
        Unc_Value vout;
        e = unc__vovlbinary(w, a, b, &vout, PASSSTRL(OPOVERLOAD(band)),
                                            PASSSTRL(OPOVERLOAD(band2)));
        if (e) {
            if (UNCIL_IS_ERR(e))
                THROWERRVMPC(e);
            VMOVE(w, tr, &vout);
            return;
        }
    }
    default:
        THROWERRVMPC(unc__err_unsup2(w, VGETTYPE(a), VGETTYPE(b)));
    }
}

static void unc__vmobbor(Unc_View *w, jmp_buf *env,
                        Unc_Value *tr, Unc_Value *a, Unc_Value *b,
                        const byte *vmpc) {
    if (UNLIKELY(VGETTYPE(b) == Unc_TObject || VGETTYPE(b) == Unc_TOpaque))
        goto unc__vmobbor_o;
    switch (VGETTYPE(a)) {
    case Unc_TBool:
        if (VGETTYPE(b) == Unc_TBool) {
            VSETBOOL(w, tr, VGETINT(a) || VGETINT(b));
            return;
        }
        THROWERRVMPC(unc__err_unsup2(w, VGETTYPE(a), VGETTYPE(b)));
    case Unc_TInt:
        if (VGETTYPE(b) == Unc_TInt) {
            VSETINT(w, tr, VGETINT(a) | VGETINT(b));
            return;
        }
        THROWERRVMPC(unc__err_unsup2(w, VGETTYPE(a), VGETTYPE(b)));
    case Unc_TObject:
    case Unc_TOpaque:
    unc__vmobbor_o:
    {
        int e;
        Unc_Value vout;
        e = unc__vovlbinary(w, a, b, &vout, PASSSTRL(OPOVERLOAD(bor)),
                                            PASSSTRL(OPOVERLOAD(bor2)));
        if (e) {
            if (UNCIL_IS_ERR(e))
                THROWERRVMPC(e);
            VMOVE(w, tr, &vout);
            return;
        }
    }
    default:
        THROWERRVMPC(unc__err_unsup2(w, VGETTYPE(a), VGETTYPE(b)));
    }
}

static void unc__vmobxor(Unc_View *w, jmp_buf *env,
                        Unc_Value *tr, Unc_Value *a, Unc_Value *b,
                        const byte *vmpc) {
    if (UNLIKELY(VGETTYPE(b) == Unc_TObject || VGETTYPE(b) == Unc_TOpaque))
        goto unc__vmobxor_o;
    switch (VGETTYPE(a)) {
    case Unc_TInt:
        if (VGETTYPE(b) == Unc_TInt) {
            VSETINT(w, tr, VGETINT(a) ^ VGETINT(b));
            return;
        }
        THROWERRVMPC(unc__err_unsup2(w, VGETTYPE(a), VGETTYPE(b)));
    case Unc_TObject:
    case Unc_TOpaque:
    unc__vmobxor_o:
    {
        int e;
        Unc_Value vout;
        e = unc__vovlbinary(w, a, b, &vout, PASSSTRL(OPOVERLOAD(bxor)),
                                            PASSSTRL(OPOVERLOAD(bxor2)));
        if (e) {
            if (UNCIL_IS_ERR(e))
                THROWERRVMPC(e);
            VMOVE(w, tr, &vout);
            return;
        }
    }
    default:
        THROWERRVMPC(unc__err_unsup2(w, VGETTYPE(a), VGETTYPE(b)));
    }
}

static void unc__vmobshl(Unc_View *w, jmp_buf *env,
                        Unc_Value *tr, Unc_Value *a, Unc_Value *b,
                        const byte *vmpc) {
    if (UNLIKELY(VGETTYPE(b) == Unc_TObject || VGETTYPE(b) == Unc_TOpaque))
        goto unc__vmobshl_o;
    switch (VGETTYPE(a)) {
    case Unc_TInt:
        if (VGETTYPE(b) == Unc_TInt) {
            VSETINT(w, tr, unc__shiftl(VGETINT(a), VGETINT(b)));
            return;
        }
        THROWERRVMPC(unc__err_unsup2(w, VGETTYPE(a), VGETTYPE(b)));
    case Unc_TObject:
    case Unc_TOpaque:
    unc__vmobshl_o:
    {
        int e;
        Unc_Value vout;
        e = unc__vovlbinary(w, a, b, &vout, PASSSTRL(OPOVERLOAD(shl)),
                                            PASSSTRL(OPOVERLOAD(shl2)));
        if (e) {
            if (UNCIL_IS_ERR(e))
                THROWERRVMPC(e);
            VMOVE(w, tr, &vout);
            return;
        }
    }
    default:
        THROWERRVMPC(unc__err_unsup2(w, VGETTYPE(a), VGETTYPE(b)));
    }
}

static void unc__vmobshr(Unc_View *w, jmp_buf *env,
                        Unc_Value *tr, Unc_Value *a, Unc_Value *b,
                        const byte *vmpc) {
    if (UNLIKELY(VGETTYPE(b) == Unc_TObject || VGETTYPE(b) == Unc_TOpaque))
        goto unc__vmobshr_o;
    switch (VGETTYPE(a)) {
    case Unc_TInt:
        if (VGETTYPE(b) == Unc_TInt) {
            VSETINT(w, tr, unc__shiftr(VGETINT(a), VGETINT(b)));
            return;
        }
        THROWERRVMPC(unc__err_unsup2(w, VGETTYPE(a), VGETTYPE(b)));
    case Unc_TObject:
    case Unc_TOpaque:
    unc__vmobshr_o:
    {
        int e;
        Unc_Value vout;
        e = unc__vovlbinary(w, a, b, &vout, PASSSTRL(OPOVERLOAD(shr)),
                                            PASSSTRL(OPOVERLOAD(shr2)));
        if (e) {
            if (UNCIL_IS_ERR(e))
                THROWERRVMPC(e);
            VMOVE(w, tr, &vout);
            return;
        }
    }
    default:
        THROWERRVMPC(unc__err_unsup2(w, VGETTYPE(a), VGETTYPE(b)));
    }
}

static int unc__vveq_i(Unc_View *w, jmp_buf *env,
                            Unc_Value *a, Unc_Value *b,
                             const byte *vmpc) {
    if (VGETTYPE(b) == Unc_TObject || VGETTYPE(b) == Unc_TOpaque)
        goto unc__vveq_o;
    switch (VGETTYPE(a)) {
    case Unc_TNull:
        return VGETTYPE(a) == VGETTYPE(b);
    case Unc_TBool:
        return VGETTYPE(a) == VGETTYPE(b) && VGETINT(a) == VGETINT(b);
    case Unc_TInt:
        switch (VGETTYPE(b)) {
        case Unc_TInt:
            return VGETINT(a) == VGETINT(b);
        case Unc_TFloat:
            return (Unc_Float)VGETINT(a) == VGETFLT(b);
        default:
            return 0;
        }
    case Unc_TFloat:
        switch (VGETTYPE(b)) {
        case Unc_TInt:
            return VGETFLT(a) == (Unc_Float)VGETINT(b);
        case Unc_TFloat:
            return VGETFLT(a) == VGETFLT(b);
        default:
            return 0;
        }
    case Unc_TString:
        if (VGETTYPE(a) != VGETTYPE(b))
            return 0;
        return unc__streq(LEFTOVER(Unc_String, VGETENT(a)),
                          LEFTOVER(Unc_String, VGETENT(b)));
    case Unc_TBlob:
        if (VGETTYPE(a) != VGETTYPE(b))
            return 0;
        return unc__blobeq(LEFTOVER(Unc_Blob, VGETENT(a)),
                           LEFTOVER(Unc_Blob, VGETENT(b)));
    case Unc_TObject:
    case Unc_TOpaque:
unc__vveq_o:
        {
            Unc_Value vout;
            int e = unc__vovlbinary(w, a, b, &vout, PASSSTRL(OPOVERLOAD(eq)),
                                                    PASSSTRL(OPOVERLOAD(eq2)));
            if (e) {
                if (UNCIL_IS_ERR(e))
                    THROWERRVMPC(e);
                if (!UNCIL_OF_REFTYPE(&vout))
                    return unc__vcvt2bool(w, &vout);
                e = unc__vcvt2bool(w, &vout);
                VDECREF(w, &vout);
                return e;
            }
        }
    case Unc_TArray:
    case Unc_TTable:
    case Unc_TFunction:
    case Unc_TWeakRef:
        return VGETTYPE(a) == VGETTYPE(b) && VGETENT(a) == VGETENT(b);
    case Unc_TBoundFunction:
        if (VGETTYPE(a) != VGETTYPE(b))
            return 0;
        {
            Unc_FunctionBound *b1 = LEFTOVER(Unc_FunctionBound, VGETENT(a));
            Unc_FunctionBound *b2 = LEFTOVER(Unc_FunctionBound, VGETENT(b));
            return VGETENT(&b1->boundto) == VGETENT(&b2->boundto)
                    && VGETENT(&b1->fn) == VGETENT(&b2->fn);
        }
    case Unc_TOpaquePtr:
        return VGETTYPE(a) == VGETTYPE(b) && VGETPTR(a) == VGETPTR(b);
    default:
        NEVER();
    }
}

static int unc__vvclt_i(Unc_View *w, jmp_buf *env,
                             Unc_Value *a, Unc_Value *b,
                             const byte *vmpc) {
    if (VGETTYPE(b) == Unc_TObject || VGETTYPE(b) == Unc_TOpaque)
        goto unc__vvclt_o;
    switch (VGETTYPE(a)) {
    case Unc_TInt:
        switch (VGETTYPE(b)) {
        case Unc_TInt:
            return VGETINT(a) < VGETINT(b);
        case Unc_TFloat:
            return (Unc_Float)VGETINT(a) < VGETFLT(b);
        default:
            THROWERRVMPC(unc__err_unsup2(w, VGETTYPE(a), VGETTYPE(b)));
        }
    case Unc_TFloat:
        switch (VGETTYPE(b)) {
        case Unc_TInt:
            return VGETFLT(a) < (Unc_Float)VGETINT(b);
        case Unc_TFloat:
            return VGETFLT(a) < VGETFLT(b);
        default:
            THROWERRVMPC(unc__err_unsup2(w, VGETTYPE(a), VGETTYPE(b)));
        }
    case Unc_TString:
        if (VGETTYPE(a) != VGETTYPE(b))
            THROWERRVMPC(unc__err_unsup2(w, VGETTYPE(a), VGETTYPE(b)));
        return unc__cmpstr(LEFTOVER(Unc_String, VGETENT(a)),
                           LEFTOVER(Unc_String, VGETENT(b))) < 0;
    case Unc_TBlob:
        if (VGETTYPE(a) != VGETTYPE(b))
            THROWERRVMPC(unc__err_unsup2(w, VGETTYPE(a), VGETTYPE(b)));
        return unc__cmpblob(LEFTOVER(Unc_Blob, VGETENT(a)),
                            LEFTOVER(Unc_Blob, VGETENT(b))) < 0;
    case Unc_TObject:
    case Unc_TOpaque:
unc__vvclt_o:
    {
        int e;
        Unc_Value vout;
        e = unc__vovlbinary(w, a, b, &vout, PASSSTRL(OPOVERLOAD(cmp)),
                                            PASSSTRL(OPOVERLOAD(cmp2)));
        if (e) {
            /*
            if (UNCIL_IS_ERR(e)) THROWERR(e);
            if (w->recurse >= w->recurselimit) {
                unc__decref(w, vout);
                return UNCIL_ERR_TOODEEP;
            }
            ++w->recurse;
            e = unc__vvclt(w, vout, unc__vint(0));
            --w->recurse;
            VDECREF(w, &vout);
            return e;
            */
            switch (VGETTYPE(&vout)) {
            case Unc_TInt:
                return VGETINT(&vout) < 0;
            case Unc_TFloat:
                return VGETFLT(&vout) < 0;
            default:
                break;
            }
        }
    }
    default:
        THROWERRVMPC(unc__err_unsup2(w, VGETTYPE(a), VGETTYPE(b)));
    }
}

FORCEINLINE void unc__vmobceq(Unc_View *w, jmp_buf *env,
                        Unc_Value *tr, Unc_Value *a, Unc_Value *b,
                        const byte *vmpc) {
    VSETBOOL(w, tr, unc__vveq_i(w, env, a, b, vmpc));
}

FORCEINLINE void unc__vmobclt(Unc_View *w, jmp_buf *env,
                        Unc_Value *tr, Unc_Value *a, Unc_Value *b,
                        const byte *vmpc) {
    VSETBOOL(w, tr, unc__vvclt_i(w, env, a, b, vmpc));
}

int unc__vveq_j(Unc_View *w, Unc_Value *a, Unc_Value *b) {
    int e;
    jmp_buf env;
    if (!(e = setjmp(env)))
        e = unc__vveq_i(w, &env, a, b, w->pc);
    return e;
}

int unc__vvclt_j(Unc_View *w, Unc_Value *a, Unc_Value *b) {
    int e;
    jmp_buf env;
    if (!(e = setjmp(env)))
        e = unc__vvclt_i(w, &env, a, b, w->pc);
    return e;
}

static void unc__vmoupos(Unc_View *w, jmp_buf *env,
                         Unc_Value *tr, Unc_Value *in,
                         const byte *vmpc) {
    switch (VGETTYPE(in)) {
    case Unc_TInt:
    case Unc_TFloat:
        VCOPY(w, tr, in);
        return;
    case Unc_TObject:
    case Unc_TOpaque:
    {
        int e;
        Unc_Value vout;
        e = unc__vovlunary(w, in, &vout, PASSSTRL(OPOVERLOAD(posit)));
        if (e) {
            if (UNCIL_IS_ERR(e))
                THROWERRVMPC(e);
            VMOVE(w, tr, &vout);
            return;
        }
    }
    default:
        THROWERRVMPC(unc__err_unsup1(w, VGETTYPE(in)));
    }
}

MAYBEINLINE void unc__vmouneg(Unc_View *w, jmp_buf *env,
                        Unc_Value *tr, Unc_Value *in,
                        const byte *vmpc) {
    switch (VGETTYPE(in)) {
    case Unc_TInt:
        if (UNLIKELY(NEGOVF(VGETINT(in))))
            VSETFLT(w, tr, -(Unc_Float)VGETINT(in));
        else
            VSETINT(w, tr, -VGETINT(in));
        return;
    case Unc_TFloat:
        VSETFLT(w, tr, -VGETFLT(in));
        return;
    case Unc_TObject:
    case Unc_TOpaque:
    {
        int e;
        Unc_Value vout;
        e = unc__vovlunary(w, in, &vout, PASSSTRL(OPOVERLOAD(negate)));
        if (e) {
            if (UNCIL_IS_ERR(e))
                THROWERRVMPC(e);
            VMOVE(w, tr, &vout);
            return;
        }
    }
    default:
        THROWERRVMPC(unc__err_unsup1(w, VGETTYPE(in)));
    }
}

static void unc__vmouxor(Unc_View *w, jmp_buf *env,
                        Unc_Value *tr, Unc_Value *in,
                        const byte *vmpc) {
    switch (VGETTYPE(in)) {
    case Unc_TInt:
        VSETINT(w, tr, ~VGETINT(in));
        return;
    case Unc_TObject:
    case Unc_TOpaque:
    {
        int e;
        Unc_Value vout;
        e = unc__vovlunary(w, in, &vout, PASSSTRL(OPOVERLOAD(invert)));
        if (e) {
            if (UNCIL_IS_ERR(e))
                THROWERRVMPC(e);
            VMOVE(w, tr, &vout);
            return;
        }
    }
    default:
        THROWERRVMPC(unc__err_unsup1(w, VGETTYPE(in)));
    }
}

static int makestrcatss(Unc_View *w, Unc_Value *out,
                        Unc_String *a, Unc_String *b) {
    int e;
    Unc_Entity *en = unc__wake(w, Unc_TString);
    if (!en)
        return UNCIL_ERR_MEM;
    e = unc__initstringfromcatlr(&w->world->alloc, LEFTOVER(Unc_String, en),
                                a, b);
    if (e) {
        unc__unwake(en, w);
        return e;
    }
    VINITENT(out, Unc_TString, en);
    return 0;
}

static int makestrcatsr(Unc_View *w, Unc_Value *out,
                        Unc_String *a, Unc_Size bn, const byte *bb) {
    int e;
    Unc_Entity *en = unc__wake(w, Unc_TString);
    if (!en)
        return UNCIL_ERR_MEM;
    e = unc__initstringfromcatl(&w->world->alloc, LEFTOVER(Unc_String, en),
                                a, bn, bb);
    if (e) {
        unc__unwake(en, w);
        return e;
    }
    VINITENT(out, Unc_TString, en);
    return 0;
}

static int makestrcatrs(Unc_View *w, Unc_Value *out,
                        Unc_Size an, const byte *ab, Unc_String *b) {
    int e;
    Unc_Entity *en = unc__wake(w, Unc_TString);
    if (!en)
        return UNCIL_ERR_MEM;
    e = unc__initstringfromcatr(&w->world->alloc, LEFTOVER(Unc_String, en),
                                an, ab, b);
    if (e) {
        unc__unwake(en, w);
        return e;
    }
    VINITENT(out, Unc_TString, en);
    return 0;
}

static int makestrcatrr(Unc_View *w, Unc_Value *out,
                        Unc_Size an, const byte * RESTRICT ab,
                        Unc_Size bn, const byte * RESTRICT bb) {
    int e;
    Unc_Entity *en = unc__wake(w, Unc_TString);
    if (!en)
        return UNCIL_ERR_MEM;
    e = unc__initstringfromcat(&w->world->alloc, LEFTOVER(Unc_String, en),
                                an, ab, bn, bb);
    if (e) {
        unc__unwake(en, w);
        return e;
    }
    VINITENT(out, Unc_TString, en);
    return 0;
}

static int makeblobcatss(Unc_View *w, Unc_Value *out,
                         Unc_Blob *a, Unc_Blob *b) {
    int e;
    byte *p;
    Unc_Entity *en = unc__wake(w, Unc_TBlob);
    if (!en)
        return UNCIL_ERR_MEM;
    e = unc__initblobraw(&w->world->alloc, LEFTOVER(Unc_Blob, en),
                                a->size + b->size, &p);
    if (e) {
        unc__unwake(en, w);
        return e;
    }
    unc__memcpy(p, a->data, a->size);
    unc__memcpy(p + a->size, b->data, b->size);
    VINITENT(out, Unc_TBlob, en);
    return 0;
}

static int makearrcatss(Unc_View *w, Unc_Value *out,
                        Unc_Array *a, Unc_Array *b) {
    
    int e;
    Unc_Entity *en = unc__wake(w, Unc_TArray);
    if (!en)
        return UNCIL_ERR_MEM;
    e = unc__initarrayfromcat(w, LEFTOVER(Unc_Array, en),
                              a->size, a->data, b->size, b->data);
    if (e) {
        unc__unwake(en, w);
        return e;
    }
    VINITENT(out, Unc_TArray, en);
    return 0;
}

static void unc__vmobcat(Unc_View *w, jmp_buf *env,
                         Unc_Value *tr, Unc_Value *a, Unc_Value *b,
                         const byte *vmpc) {
    Unc_Value out;
    int e;
    if (UNLIKELY(VGETTYPE(b) == Unc_TObject || VGETTYPE(b) == Unc_TOpaque))
        goto unc__vmobcat_o;
    switch (VGETTYPE(a)) {
    case Unc_TString:
        if (VGETTYPE(b) == Unc_TString) {
            e = makestrcatss(w, &out, LEFTOVER(Unc_String, VGETENT(a)),
                                      LEFTOVER(Unc_String, VGETENT(b)));
            if (e) THROWERRVMPC(e);
            VCOPY(w, tr, &out);
            return;
        }
        THROWERRVMPC(unc__err_unsup2(w, VGETTYPE(a), VGETTYPE(b)));
    case Unc_TBlob:
        if (VGETTYPE(b) == Unc_TBlob) {
            e = makeblobcatss(w, &out, LEFTOVER(Unc_Blob, VGETENT(a)),
                                       LEFTOVER(Unc_Blob, VGETENT(b)));
            if (e) THROWERRVMPC(e);
            VCOPY(w, tr, &out);
            return;
        }
        THROWERRVMPC(unc__err_unsup2(w, VGETTYPE(a), VGETTYPE(b)));
    case Unc_TArray:
        if (VGETTYPE(b) == Unc_TArray) {
            e = makearrcatss(w, &out, LEFTOVER(Unc_Array, VGETENT(a)),
                                      LEFTOVER(Unc_Array, VGETENT(b)));
            if (e) THROWERRVMPC(e);
            VCOPY(w, tr, &out);
            return;
        }
        THROWERRVMPC(unc__err_unsup2(w, VGETTYPE(a), VGETTYPE(b)));
    case Unc_TObject:
    case Unc_TOpaque:
    unc__vmobcat_o:
    {
        int e;
        Unc_Value vout;
        e = unc__vovlbinary(w, a, b, &vout, PASSSTRL(OPOVERLOAD(cat)),
                                            PASSSTRL(OPOVERLOAD(cat2)));
        if (e) {
            if (UNCIL_IS_ERR(e))
                THROWERRVMPC(e);
            VMOVE(w, tr, &vout);
            return;
        }
    }
    default:
        THROWERRVMPC(unc__err_unsup2(w, VGETTYPE(a), VGETTYPE(b)));
    }
}

#define DOUNARYOPR(op) do {                                                    \
        Unc_Value *s = GETREG();                                               \
        Unc_Value *a = GETREG();                                               \
        op(w, &env, s, a, pc);                                                 \
    } while (0)

#define DOUNARYOPL(op) do {                                                    \
        Unc_Value *s = GETREG();                                               \
        Unc_Value a;                                                           \
        VINITINT(&a, unc__litint(w, pc)); pc += LITINT_SZ;                     \
        op(w, &env, s, &a, pc);                                                \
    } while (0)

#define DOBINARYOPRR(op) do {                                                  \
        Unc_Value *s = GETREG();                                               \
        Unc_Value *a = GETREG();                                               \
        Unc_Value *b = GETREG();                                               \
        op(w, &env, s, a, b, pc);                                              \
    } while (0)

#define DOBINARYOPRL(op) do {                                                  \
        Unc_Value *s = GETREG();                                               \
        Unc_Value *a = GETREG();                                               \
        Unc_Value b;                                                           \
        VINITINT(&b, unc__litint(w, pc)); pc += LITINT_SZ;                     \
        op(w, &env, s, a, &b, pc);                                             \
    } while (0)

#define DOBINARYOPLR(op) do {                                                  \
        Unc_Value *s = GETREG();                                               \
        Unc_Value a;                                                           \
        Unc_Value *b;                                                          \
        VINITINT(&a, unc__litint(w, pc)); pc += LITINT_SZ;                     \
        b = GETREG();                                                          \
        op(w, &env, s, &a, b, pc);                                             \
    } while (0)

#define DOBINARYOPLL(op) do {                                                  \
        Unc_Value *s = GETREG();                                               \
        Unc_Value a, b;                                                        \
        VINITINT(&a, unc__litint(w, pc)); pc += LITINT_SZ;                     \
        VINITINT(&b, unc__litint(w, pc)); pc += LITINT_SZ;                     \
        op(w, &env, s, &a, &b, pc);                                            \
    } while (0)

INLINE void dofmake(Unc_View *w, jmp_buf *env, Unc_Value *tr,
                    Unc_Size offset, const byte *vmpc) {
    int e;
    Unc_Entity *en = unc__wake(w, Unc_TFunction);
    if (!en) THROWERRVMPC(UNCIL_ERR_MEM);
    e = unc__initfuncu(w, LEFTOVER(Unc_Function, en), w->program, offset, 0);
    if (UNLIKELY(e)) {
        unc__unwake(en, w);
        THROWERRVMPC(e);
    }
    VSETENT(w, tr, Unc_TFunction, en);
}

INLINE void dofbind(Unc_View *w, jmp_buf *env, Unc_Value *tr,
                    Unc_Value *a, Unc_Value *b, const byte *vmpc) {
    int e;
    Unc_Entity *en = unc__wake(w, Unc_TBoundFunction);
    if (!en) THROWERRVMPC(UNCIL_ERR_MEM);
    e = unc__initbfunc(w, LEFTOVER(Unc_FunctionBound, en), a, b);
    if (UNLIKELY(e)) {
        unc__unwake(en, w);
        THROWERRVMPC(e);
    }
    VSETENT(w, tr, Unc_TBoundFunction, en);
}

INLINE void dofcall_o(Unc_View *w, jmp_buf *env, Unc_Size argc, int spew,
                    Unc_RegFast dst, Unc_Value *fn, const byte **pc);

static void dofcall(Unc_View *w, jmp_buf *env, Unc_Size argc, int spew,
                    Unc_RegFast dst, Unc_Value *fn, const byte **pc) {
    switch (VGETTYPE(fn)) {
    case Unc_TBoundFunction:
    {
        int e;
        Unc_FunctionBound *b = LEFTOVER(Unc_FunctionBound, VGETENT(fn));
        e = unc__stackinsertn(w, &w->sval, argc++, &b->boundto);
        if (UNLIKELY(e))
            THROWERRSTPC(e);
        fn = &b->fn;
        ASSERT(VGETTYPE(fn) == Unc_TFunction);
        /* fall-through... */
    }
    case Unc_TFunction:
    {
        unc__vmfcall(w, env, LEFTOVER(Unc_Function, VGETENT(fn)),
                     argc, spew, 0, 1, dst, pc);
        return;
    }
    case Unc_TWeakRef:
        if (argc) {
            THROWERRSTPC(UNCIL_ERR_ARG_TOOMANYARGS);
        } else {
            Unc_Value v;
            unc__fetchweak(w, fn, &v);
            if (spew) {
                int e = unc__stackpushv(w, &w->sval, &v);
                if (UNLIKELY(e))
                    THROWERRSTPC(e);
            } else {
                VCOPY(w, &w->regs[dst], &v);
            }
        }
        return;
    case Unc_TObject:
    case Unc_TOpaque:
        dofcall_o(w, env, argc, spew, dst, fn, pc);
        return;
    default:
        THROWERRSTPC(UNCIL_ERR_TYPE_NOTFUNCTION);
    }
}

INLINE void dofcall_o(Unc_View *w, jmp_buf *env, Unc_Size argc, int spew,
                    Unc_RegFast dst, Unc_Value *fn, const byte **pc) {
    Unc_Value *o;
    int e;
    e = unc__stackinsertn(w, &w->sval, argc++, fn);
    if (e) THROWERRSTPC(e);
dofcall_again:
    e = unc__getprotomethod(w, fn, PASSSTRL(OPOVERLOAD(call)), &o);
    if (e) THROWERRSTPC(e);
    if (o) {
        switch (o->type) {
        case Unc_TFunction:
            dofcall(w, env, argc, spew, dst, o, pc);
            return;
        case Unc_TBoundFunction:
        {
            Unc_FunctionBound *b = LEFTOVER(Unc_FunctionBound, VGETENT(o));
            e = unc__stackinsertn(w, &w->sval, argc - 1, &b->boundto);
            ++argc;
            if (UNLIKELY(e))
                THROWERRSTPC(e);
            dofcall(w, env, argc, spew, dst, &b->fn, pc);
            return;
        }
        case Unc_TObject:
        case Unc_TOpaque:
            fn = o;
            goto dofcall_again;
        default:
            ;
        }
    }
}

int unc__vmrpush(Unc_View *w) {
    if (w->region.top == w->region.end) {
        Unc_Size z = w->region.end - w->region.base, nz = z + 16;
        Unc_Size *p = TMREALLOC(Unc_Size, &w->world->alloc, 0,
                                       w->region.base, z, nz);
        if (!p) return UNCIL_ERR_MEM;
        w->region.base = p;
        w->region.top = p + z;
        w->region.end = p + nz;
    }
    *w->region.top++ = unc__stackdepth(&w->sval);
    return 0;
}

INLINE void domlist(Unc_View *w, jmp_buf *env, Unc_Value *tr,
                    int pop, Unc_Size a, Unc_Size b, const byte *vmpc) {
    int e;
    Unc_Size argc;
    Unc_Entity *en = unc__wake(w, Unc_TArray);
    ASSERT(w->region.top >= w->region.base ||
        !(w->frames.top > w->frames.base &&
            w->region.top - w->region.base <= w->frames.top[-1].region_r));
    argc = (w->sval.top - w->sval.base) - w->region.top[-1];
    if (!en) {
        unc__vmrestoredepth(w, &w->sval, *--w->region.top);
        THROWERRVMPC(UNCIL_ERR_MEM);
    }
    e = unc__initarray(w, LEFTOVER(Unc_Array, en), argc - a - b,
                                     w->sval.top - argc + a);
    if (pop) unc__vmrestoredepth(w, &w->sval, *--w->region.top);
    if (UNLIKELY(e)) {
        unc__unwake(en, w);
        THROWERRVMPC(e);
    }
    VSETENT(w, tr, Unc_TArray, en);
}

FORCEINLINE void checkstackeq(Unc_View *w, jmp_buf *env, Unc_Size n,
                    const byte *vmpc) {
    Unc_Size q;
    ASSERT(w->region.top > w->region.base);
    q = unc__stackdepth(&w->sval) - w->region.top[-1];
    if (q != n)
        THROWERRVMPC(q > n ? UNCIL_ERR_LOGIC_UNPACKTOOMANY
                       : UNCIL_ERR_LOGIC_UNPACKTOOFEW);
}

FORCEINLINE void checkstackge(Unc_View *w, jmp_buf *env, Unc_Size n,
                    const byte *vmpc) {
    Unc_Size q;
    ASSERT(w->region.top > w->region.base);
    q = unc__stackdepth(&w->sval) - w->region.top[-1];
    if (q < n)
        THROWERRVMPC(UNCIL_ERR_LOGIC_UNPACKTOOFEW);
}

INLINE void dondict(Unc_View *w, jmp_buf *env, Unc_Value *tr,
                    const byte *vmpc) {
    int e;
    Unc_Entity *en = unc__wake(w, Unc_TTable);
    if (!en) THROWERRVMPC(UNCIL_ERR_MEM);
    e = unc__initdict(w, LEFTOVER(Unc_Dict, en));
    if (UNLIKELY(e)) {
        unc__unwake(en, w);
        THROWERRVMPC(e);
    }
    VSETENT(w, tr, Unc_TTable, en);
}

static void dolspr(Unc_View *w, jmp_buf *env, int spew,
                   Unc_Value *tr, Unc_Value *list, const byte *vmpc) {
    switch (VGETTYPE(list)) {
    case Unc_TArray:
    {
        Unc_Array *a = LEFTOVER(Unc_Array, VGETENT(list));
        if (spew) {
            int e = unc__stackpush(w, &w->sval, a->size, a->data);
            if (UNLIKELY(e))
                THROWERRVMPC(e);
        } else if (a->size) {
            VCOPY(w, tr, &a->data[0]);
        } else {
            VSETNULL(w, tr);
        }
        return;
    }
    case Unc_TObject:
    case Unc_TOpaque:
    {
        int e;
        Unc_Value vout;
        e = unc__vovlunary(w, list, &vout, PASSSTRL(OPOVERLOAD(iter)));
        if (e) {
            Unc_Size d = unc__stackdepth(&w->sval), nd;
            if (UNCIL_IS_ERR(e))
                THROWERRVMPC(e);
            if (w->recurse >= w->recurselimit)
                THROWERRVMPC(UNCIL_ERR_TOODEEP);
            ++w->recurse;
            for (;;) {
                e = unc__fcallv(w, &vout, 0, 1, 0, 1, 0);
                if (UNCIL_IS_ERR(e)) {
                    THROWERRVMPC(e);
                }
                nd = unc__stackdepth(&w->sval);
                if (d == nd)
                    break;
                nd = d + 1;
                unc__vmrestoredepth(w, &w->sval, nd);
            }
            --w->recurse;
            return;
        }
        THROWERRVMPC(UNCIL_ERR_ARG_NOTITERABLE);
    }
    default:
        THROWERRVMPC(UNCIL_ERR_ARG_NOTITERABLE);
    }
}

FORCEINLINE int unc__fastvcvt2bool(Unc_View *w, jmp_buf *env,
                                   Unc_Value *v, const byte *vmpc) {
    switch (VGETTYPE(v)) {
    case Unc_TNull:
        return 0;
    case Unc_TBool:
        return VGETBOOL(v);
    default:
    {
        int e;
        e = unc__vcvt2bool(w, v);
        if (UNCIL_IS_ERR(e))
            THROWERRVMPC(e);
        return e;
    }
    }
}

INLINE Unc_Size unc__vmgetlineno(Unc_View *w) {
    Unc_Size total = w->pc - w->jbase;
    const byte *dp = w->debugbase;
    Unc_Size subtract;
    Unc_Size lineno;
    Unc_Int adjustment;
    if (!dp) return 0;
    lineno = unc__vlqdecz(&dp);
    do {
        subtract = unc__vlqdecz(&dp);
        if (total < subtract)
            break;
        total -= subtract;
        adjustment = unc__vlqdeci(&dp);
        lineno += adjustment;
    } while (*dp);
    return lineno;
}

INLINE void unc__unrecover(Unc_View *w) {
    ++w->frames.top;
    ++w->recurse;
}

#ifndef UNCIL_SPLIT_DISPATCH
#define UNCIL_SPLIT_DISPATCH 0
#endif

#define INSTRLIST() \
    OD_(NOP     )   OD_(LDNUM   )   OD_(LDINT   )   OD_(LDFLT   )              \
    OD_(LDBLF   )   OD_(LDBLT   )   OD_(LDSTR   )   OD_(LDNUL   )              \
    OD_(LDSTK   )   OD_(LDPUB   )   OD_(LDBIND  )   OD_(LDSTKN  )              \
    OD_(LDATTR  )   OD_(LDATTRQ )   OD_(LDINDX  )   OD_(LDINDXQ )              \
    OD_(MOV     )   OD_(STPUB   )   OI_(x12)        OI_(x13)                   \
    OD_(STATTR  )   OD_(STWITH  )   OD_(STINDX  )   OI_(x17)                   \
    OD_(STSTK   )   OI_(x19)        OD_(STBIND  )   OI_(x1B)                   \
    OI_(x1C)        OI_(x1D)        OI_(x1E)        OI_(x1F)                   \
    OI_(x20)        OD_(DEPUB   )   OI_(x22)        OI_(x23)                   \
    OD_(DEATTR  )   OI_(x25)        OD_(DEINDX  )   OI_(x27)                   \
    OI_(x28)        OI_(x29)        OI_(x2A)        OI_(x2B)                   \
    OI_(x2C)        OI_(x2D)        OI_(x2E)        OI_(x2F)                   \
    OI_(x30)        OI_(x31)        OI_(x32)        OI_(x33)                   \
    OI_(x34)        OI_(x35)        OI_(x36)        OI_(x37)                   \
    OI_(x38)        OI_(x39)        OI_(x3A)        OI_(x3B)                   \
    OD_(LDATTRF )   OI_(x3D)        OI_(x3E)        OI_(x3F)                   \
    OD_(ADD_RR  )   OD_(SUB_RR  )   OD_(MUL_RR  )   OD_(DIV_RR  )              \
    OD_(IDIV_RR )   OD_(MOD_RR  )   OD_(AND_RR  )   OD_(BOR_RR  )              \
    OD_(XOR_RR  )   OD_(SHL_RR  )   OD_(SHR_RR  )   OD_(CAT_RR  )              \
    OD_(CEQ_RR  )   OD_(CLT_RR  )   OI_(x4E)        OI_(x4F)                   \
    OD_(ADD_RL  )   OD_(SUB_RL  )   OD_(MUL_RL  )   OD_(DIV_RL  )              \
    OD_(IDIV_RL )   OD_(MOD_RL  )   OD_(AND_RL  )   OD_(BOR_RL  )              \
    OD_(XOR_RL  )   OD_(SHL_RL  )   OD_(SHR_RL  )   OD_(CAT_RL  )              \
    OD_(CEQ_RL  )   OD_(CLT_RL  )   OI_(x5E)        OI_(x5F)                   \
    OD_(ADD_LR  )   OD_(SUB_LR  )   OD_(MUL_LR  )   OD_(DIV_LR  )              \
    OD_(IDIV_LR )   OD_(MOD_LR  )   OD_(AND_LR  )   OD_(BOR_LR  )              \
    OD_(XOR_LR  )   OD_(SHL_LR  )   OD_(SHR_LR  )   OD_(CAT_LR  )              \
    OD_(CEQ_LR  )   OD_(CLT_LR  )   OI_(x6E)        OI_(x6F)                   \
    OD_(ADD_LL  )   OD_(SUB_LL  )   OD_(MUL_LL  )   OD_(DIV_LL  )              \
    OD_(IDIV_LL )   OD_(MOD_LL  )   OD_(AND_LL  )   OD_(BOR_LL  )              \
    OD_(XOR_LL  )   OD_(SHL_LL  )   OD_(SHR_LL  )   OD_(CAT_LL  )              \
    OD_(CEQ_LL  )   OD_(CLT_LL  )   OI_(x7E)        OI_(x7F)                   \
    OD_(LNOT_R  )   OD_(UPOS_R  )   OD_(UNEG_R  )   OD_(UXOR_R  )              \
    OI_(x84)        OI_(x85)        OI_(x86)        OI_(x87)                   \
    OI_(x88)        OI_(x89)        OI_(x8A)        OI_(x8B)                   \
    OI_(x8C)        OI_(x8D)        OI_(x8E)        OI_(x8F)                   \
    OD_(LNOT_L  )   OD_(UPOS_L  )   OD_(UNEG_L  )   OD_(UXOR_L  )              \
    OI_(x94)        OI_(x95)        OI_(x96)        OI_(x97)                   \
    OI_(x98)        OI_(x99)        OI_(x9A)        OI_(x9B)                   \
    OI_(x9C)        OI_(x9D)        OI_(x9E)        OI_(x9F)                   \
    OI_(xA0)        OI_(xA1)        OI_(xA2)        OI_(xA3)                   \
    OI_(xA4)        OI_(xA5)        OI_(xA6)        OI_(xA7)                   \
    OI_(xA8)        OI_(xA9)        OI_(xAA)        OI_(xAB)                   \
    OI_(xAC)        OI_(xAD)        OI_(xAE)        OI_(xAF)                   \
    OI_(xB0)        OI_(xB1)        OI_(xB2)        OI_(xB3)                   \
    OI_(xB4)        OI_(xB5)        OI_(xB6)        OI_(xB7)                   \
    OI_(xB8)        OI_(xB9)        OI_(xBA)        OI_(xBB)                   \
    OI_(xBC)        OI_(xBD)        OI_(xBE)        OI_(xBF)                   \
    OD_(IFF     )   OD_(IFT     )   OD_(JMP     )   OD_(EXIT    )              \
    OD_(EXIT0   )   OD_(EXIT1   )   OD_(WPUSH   )   OD_(WPOP    )              \
    OD_(RPUSH   )   OD_(RPOP    )   OD_(XPUSH   )   OD_(XPOP    )              \
    OD_(LSPRS   )   OD_(LSPR    )   OD_(CSTK    )   OD_(CSTKG   )              \
    OD_(MLIST   )   OD_(NDICT   )   OD_(MLISTP  )   OD_(IITER   )              \
    OD_(FMAKE   )   OD_(FBIND   )   OD_(INEXTS  )   OD_(INEXT   )              \
    OD_(DCALLS  )   OD_(DCALL   )   OD_(DTAIL   )   OI_(xDB)                   \
    OD_(FCALLS  )   OD_(FCALL   )   OD_(FTAIL   )   OI_(xDF)                   \
    OI_(xE0)        OI_(xE1)        OI_(xE2)        OI_(xE3)                   \
    OI_(xE4)        OI_(xE5)        OI_(xE6)        OI_(xE7)                   \
    OI_(xE8)        OI_(xE9)        OI_(xEA)        OI_(xEB)                   \
    OI_(xEC)        OI_(xED)        OI_(xEE)        OI_(xEF)                   \
    OI_(xF0)        OI_(xF1)        OI_(xF2)        OI_(xF3)                   \
    OI_(xF4)        OI_(xF5)        OI_(xF6)        OI_(xF7)                   \
    OI_(xF8)        OI_(xF9)        OI_(xFA)        OI_(xFB)                   \
    OI_(xFC)        OI_(xFD)        OI_(xFE)        OD_(DEL     )

#if __GNUC__ && !__STRICT_ANSI__ && !DEBUGPRINT_INSTRS && !UNCIL_THREADED_VM
#define LABELNAME(x) UVM_##x
#define OD_(x) && LABELNAME(x),
#define OI_(x) && LABELNAME(INV##x),
#define OPCODE(x) LABELNAME(x) :
#define OPCODEINV(x) UVM_INV##x :
#define GOTONEXT() goto *instrjumps[*pc++]
#define DISPATCH() GOTONEXT();
#define DISPATCH_END()
#define DISPATCH_VARS static const void *const instrjumps[] = { INSTRLIST() };
#elif UNCIL_SPLIT_DISPATCH && !DEBUGPRINT_INSTRS
#define LABELNAME(x) UVM_##x
#define OPCODE(x) LABELNAME(x) :
#define OPCODEINV(x) UVM_INV##x :
#define OPCODEFROM(x) UNC_I_##x
#define OPCODEHEX(x) 0##x
#define OD_(x) case OPCODEFROM(x): goto LABELNAME(x);
#define OI_(x)
#define GOTONEXT() switch (*pc++) {                                            \
        INSTRLIST()                                                            \
        default:                                                               \
            DEADCODE();                                                        \
            NEVER();                                                           \
        }
#define DISPATCH() GOTONEXT()
#define DISPATCH_END()
#define DISPATCH_VARS
#else
#define DISPATCH() switch (*pc++) {
#define DISPATCH_END()                                                         \
        default:                                                               \
            DEADCODE();                                                        \
            NEVER();                                                           \
        }
#define DISPATCH_VARS
#define CASEJUMP(lbl) case lbl :
#define OPCODE(x) CASEJUMP( UNC_I_##x )
#define OPCODEINV(x) CASEJUMP( 0##x )
#define GOTONEXT() goto nextinstr
#define DEFINENEXTINSTR 1
#endif

#if DEBUGPRINT_INSTRS
void pcurinstrdump(Unc_View *w, const byte *pc);
#endif

#define COMMIT() w->pc = pc
#define REFRESH() pc = rpc, regs = w->regs
#define UNCOMMIT() pc = w->pc, regs = w->regs

int unc__run(Unc_View *w) {
    int e;
    const Unc_View *origw = w;
    register const byte *pc = w->pc;
    Unc_Value *regs = w->regs;
    jmp_buf env;
    DISPATCH_VARS

    if (!UNC_LOCKFQ(w->runlock))
        return UNCIL_ERR_LOGIC_CANNOTLOCK;

    if ((e = setjmp(env))) {
        pc = w->pc;
        goto vmerror;
    }
    CHECKPAUSE();

#ifdef DEFINENEXTINSTR
nextinstr:
#endif
#if DEBUGPRINT_INSTRS
    pcurinstrdump(w, pc);
#endif

    DISPATCH()
    OPCODE(NOP)
        GOTONEXT();
    OPCODE(LDNUM)
    {
        Unc_Value *s = GETREG();
        VSETINT(w, s, unc__litint(w, pc));
        pc += LITINT_SZ;
        GOTONEXT();
    }
    OPCODE(LDINT)
    {
        Unc_Value *s = GETREG();
        const byte *tpc = pc;
        VSETINT(w, s, unc__vlqdeci(&tpc));
        pc = tpc;
        GOTONEXT();
    }
    OPCODE(LDFLT)
    {
        Unc_Value *s = GETREG();
        Unc_Float f;
        unc__memcpy(&f, pc, sizeof(Unc_Float));
        pc += sizeof(Unc_Float);
        VSETFLT(w, s, f);
        GOTONEXT();
    }
    OPCODE(LDBLF)
        VSETBOOL(w, GETREG(), 0);
        GOTONEXT();
    OPCODE(LDBLT)
        VSETBOOL(w, GETREG(), 1);
        GOTONEXT();
    OPCODE(LDSTR)
    {
        unsigned tmp;
        Unc_Value *s = GETREG();
        Unc_Size sl;
        const byte *sb;
        Unc_Value v = UNC_BLANK;
        unc__loadstr(w, GETVLQ(), &sl, &sb);
        CHECKPAUSE();
        MUST(unc_newstring(w, &v, sl, (const char *)sb));
        VMOVE(w, s, &v);
        GOTONEXT();
    }
    OPCODE(LDNUL)
        VSETNULL(w, GETREG());
        GOTONEXT();
    OPCODE(LDSTK)
    {
        unsigned tmp;
        Unc_Value *s = GETREG();
        ASSERT(w->region.top > w->region.base);
        VCOPY(w, s, &w->sval.base[w->region.top[-1] + GETVLQ()]);
        GOTONEXT();
    }
    OPCODE(LDPUB)
    {
        unsigned tmp;
        Unc_Value *s = GETREG();
        Unc_Size sl;
        const byte *sb;
        unc__loadstr(w, GETVLQ(), &sl, &sb);
        COMMIT();
        unc__getpub(w, &env, sl, sb, s);
        GOTONEXT();
    }
    OPCODE(LDBIND)
    {
        Unc_Value *s = GETREG();
        Unc_RegFast bn = GETREGN();
        Unc_ValueRef *r;
        ASSERT(bn < w->boundcount);
        r = LEFTOVER(Unc_ValueRef, w->bounds[bn]);
        UNC_LOCKL(r->lock);
        VCOPY(w, s, &r->v);
        UNC_UNLOCKL(r->lock);
        GOTONEXT();
    }
    OPCODE(LDSTKN)
    {
        unsigned tmp;
        Unc_Value *s = GETREG();
        Unc_Size t = GETVLQ();
        VCOPY(w, s, w->sval.top - t);
        GOTONEXT();
    }
    OPCODE(LDATTR)
    {
        unsigned tmp;
        Unc_Value *s = GETREG();
        Unc_Value *a = GETREG();
        Unc_Size sl;
        const byte *sb;
        unc__loadstr(w, GETVLQ(), &sl, &sb);
        MUST(unc__vgetattr(w, a, sl, sb, 0, s));
        GOTONEXT();
    }
    OPCODE(LDATTRQ)
    {
        unsigned tmp;
        Unc_Value *s = GETREG();
        Unc_Value *a = GETREG();
        Unc_Size sl;
        const byte *sb;
        unc__loadstr(w, GETVLQ(), &sl, &sb);
        MUST(unc__vgetattr(w, a, sl, sb, 1, s));
        GOTONEXT();
    }
    OPCODE(LDINDX)
    {
        Unc_Value *s = GETREG();
        Unc_Value *a = GETREG();
        Unc_Value *b = GETREG();
        MUST(unc__vgetindx(w, a, b, 0, s));
        GOTONEXT();
    }
    OPCODE(LDINDXQ)
    {
        Unc_Value *s = GETREG();
        Unc_Value *a = GETREG();
        Unc_Value *b = GETREG();
        MUST(unc__vgetindx(w, a, b, 1, s));
        GOTONEXT();
    }
    OPCODE(MOV)
    {
        Unc_Value *s = GETREG();
        Unc_Value *a = GETREG();
        VCOPY(w, s, a);
        GOTONEXT();
    }
    OPCODE(STPUB)
    {
        unsigned tmp;
        Unc_Value *s = GETREG();
        Unc_Size sl;
        const byte *sb;
        unc__loadstr(w, GETVLQ(), &sl, &sb);
        CHECKPAUSE();
        COMMIT();
        unc__setpub(w, &env, sl, sb, s);
        GOTONEXT();
    }
    OPCODE(STATTR)
    {
        unsigned tmp;
        Unc_Value *s = GETREG();
        Unc_Value *a = GETREG();
        Unc_Size sl;
        const byte *sb;
        unc__loadstr(w, GETVLQ(), &sl, &sb);
        CHECKPAUSE();
        MUST(unc__vsetattr(w, a, sl, sb, s));
        GOTONEXT();
    }
    OPCODE(STWITH)
    {
        Unc_Value *s = GETREG();
        CHECKPAUSE();
        MUST(unc__stackpushv(w, &w->swith, s));
        MUST(unc__vdowith(w, s));
        GOTONEXT();
    }
    OPCODE(STINDX)
    {
        Unc_Value *s = GETREG();
        Unc_Value *a = GETREG();
        Unc_Value *b = GETREG();
        CHECKPAUSE();
        MUST(unc__vsetindx(w, a, b, s));
        GOTONEXT();
    }
    OPCODE(STSTK)
        if (w->sval.top == w->sval.end) {
            CHECKPAUSE();
            MUST(unc__stackreserve(w, &w->sval, 1));
        }
        VIMPOSE(w, w->sval.top++, GETREG());
        GOTONEXT();
    OPCODE(STBIND)
    {
        Unc_Value *s = GETREG();
        Unc_RegFast bn = GETREGN();
        Unc_ValueRef *r;
        ASSERT(bn < w->boundcount);
        CHECKPAUSE();
        r = LEFTOVER(Unc_ValueRef, w->bounds[bn]);
        UNC_LOCKL(r->lock);
        VCOPY(w, &r->v, s);
        UNC_UNLOCKL(r->lock);
        GOTONEXT();
    }
    OPCODE(DEPUB)
    {
        unsigned tmp;
        Unc_Size sl;
        const byte *sb;
        unc__loadstr(w, GETVLQ(), &sl, &sb);
        CHECKPAUSE();
        COMMIT();
        unc__delpub(w, &env, sl, sb);
        GOTONEXT();
    }
    OPCODE(DEATTR)
    {
        unsigned tmp;
        Unc_Value *a = GETREG();
        Unc_Size sl;
        const byte *sb;
        unc__loadstr(w, GETVLQ(), &sl, &sb);
        CHECKPAUSE();
        MUST(unc__vdelattr(w, a, sl, sb));
        GOTONEXT();
    }
    OPCODE(DEINDX)
    {
        Unc_Value *a = GETREG();
        Unc_Value *b = GETREG();
        CHECKPAUSE();
        MUST(unc__vdelindx(w, a, b));
        GOTONEXT();
    }
    OPCODE(LDATTRF)
    {
        unsigned tmp;
        Unc_Value *s = GETREG();
        Unc_Value *a = GETREG();
        Unc_Size sl;
        const byte *sb;
        unc__loadstr(w, GETVLQ(), &sl, &sb);
        CHECKPAUSE();
        MUST(unc__vgetattrf(w, a, sl, sb, 0, s));
        GOTONEXT();
    }
    OPCODE(ADD_RR)
        DOBINARYOPRR(unc__vmobadd);
        GOTONEXT();
    OPCODE(SUB_RR)
        DOBINARYOPRR(unc__vmobsub);
        GOTONEXT();
    OPCODE(MUL_RR)
        DOBINARYOPRR(unc__vmobmul);
        GOTONEXT();
    OPCODE(DIV_RR)
        DOBINARYOPRR(unc__vmobdiv);
        GOTONEXT();
    OPCODE(IDIV_RR)
        DOBINARYOPRR(unc__vmobidiv);
        GOTONEXT();
    OPCODE(MOD_RR)
        DOBINARYOPRR(unc__vmobmod);
        GOTONEXT();
    OPCODE(AND_RR)
        DOBINARYOPRR(unc__vmoband);
        GOTONEXT();
    OPCODE(BOR_RR)
        DOBINARYOPRR(unc__vmobbor);
        GOTONEXT();
    OPCODE(XOR_RR)
        DOBINARYOPRR(unc__vmobxor);
        GOTONEXT();
    OPCODE(SHL_RR)
        DOBINARYOPRR(unc__vmobshl);
        GOTONEXT();
    OPCODE(SHR_RR)
        DOBINARYOPRR(unc__vmobshr);
        GOTONEXT();
    OPCODE(CAT_RR)
        DOBINARYOPRR(unc__vmobcat);
        GOTONEXT();
    OPCODE(CEQ_RR)
        DOBINARYOPRR(unc__vmobceq);
        GOTONEXT();
    OPCODE(CLT_RR)
        DOBINARYOPRR(unc__vmobclt);
        GOTONEXT();
    OPCODE(ADD_RL)
        DOBINARYOPRL(unc__vmobadd);
        GOTONEXT();
    OPCODE(SUB_RL)
        DOBINARYOPRL(unc__vmobsub);
        GOTONEXT();
    OPCODE(MUL_RL)
        DOBINARYOPRL(unc__vmobmul);
        GOTONEXT();
    OPCODE(DIV_RL)
        DOBINARYOPRL(unc__vmobdiv);
        GOTONEXT();
    OPCODE(IDIV_RL)
        DOBINARYOPRL(unc__vmobidiv);
        GOTONEXT();
    OPCODE(MOD_RL)
        DOBINARYOPRL(unc__vmobmod);
        GOTONEXT();
    OPCODE(AND_RL)
        DOBINARYOPRL(unc__vmoband);
        GOTONEXT();
    OPCODE(BOR_RL)
        DOBINARYOPRL(unc__vmobbor);
        GOTONEXT();
    OPCODE(XOR_RL)
        DOBINARYOPRL(unc__vmobxor);
        GOTONEXT();
    OPCODE(SHL_RL)
        DOBINARYOPRL(unc__vmobshl);
        GOTONEXT();
    OPCODE(SHR_RL)
        DOBINARYOPRL(unc__vmobshr);
        GOTONEXT();
    OPCODE(CAT_RL)
        DOBINARYOPRL(unc__vmobcat);
        GOTONEXT();
    OPCODE(CEQ_RL)
        DOBINARYOPRL(unc__vmobceq);
        GOTONEXT();
    OPCODE(CLT_RL)
        DOBINARYOPRL(unc__vmobclt);
        GOTONEXT();
    OPCODE(ADD_LR)
        DOBINARYOPLR(unc__vmobadd);
        GOTONEXT();
    OPCODE(SUB_LR)
        DOBINARYOPLR(unc__vmobsub);
        GOTONEXT();
    OPCODE(MUL_LR)
        DOBINARYOPLR(unc__vmobmul);
        GOTONEXT();
    OPCODE(DIV_LR)
        DOBINARYOPLR(unc__vmobdiv);
        GOTONEXT();
    OPCODE(IDIV_LR)
        DOBINARYOPLR(unc__vmobidiv);
        GOTONEXT();
    OPCODE(MOD_LR)
        DOBINARYOPLR(unc__vmobmod);
        GOTONEXT();
    OPCODE(AND_LR)
        DOBINARYOPLR(unc__vmoband);
        GOTONEXT();
    OPCODE(BOR_LR)
        DOBINARYOPLR(unc__vmobbor);
        GOTONEXT();
    OPCODE(XOR_LR)
        DOBINARYOPLR(unc__vmobxor);
        GOTONEXT();
    OPCODE(SHL_LR)
        DOBINARYOPLR(unc__vmobshl);
        GOTONEXT();
    OPCODE(SHR_LR)
        DOBINARYOPLR(unc__vmobshr);
        GOTONEXT();
    OPCODE(CAT_LR)
        DOBINARYOPLR(unc__vmobcat);
        GOTONEXT();
    OPCODE(CEQ_LR)
        DOBINARYOPLR(unc__vmobceq);
        GOTONEXT();
    OPCODE(CLT_LR)
        DOBINARYOPLR(unc__vmobclt);
        GOTONEXT();
    OPCODE(ADD_LL)
        DOBINARYOPLL(unc__vmobadd);
        GOTONEXT();
    OPCODE(SUB_LL)
        DOBINARYOPLL(unc__vmobsub);
        GOTONEXT();
    OPCODE(MUL_LL)
        DOBINARYOPLL(unc__vmobmul);
        GOTONEXT();
    OPCODE(DIV_LL)
        DOBINARYOPLL(unc__vmobdiv);
        GOTONEXT();
    OPCODE(IDIV_LL)
        DOBINARYOPLL(unc__vmobidiv);
        GOTONEXT();
    OPCODE(MOD_LL)
        DOBINARYOPLL(unc__vmobmod);
        GOTONEXT();
    OPCODE(AND_LL)
        DOBINARYOPLL(unc__vmoband);
        GOTONEXT();
    OPCODE(BOR_LL)
        DOBINARYOPLL(unc__vmobbor);
        GOTONEXT();
    OPCODE(XOR_LL)
        DOBINARYOPLL(unc__vmobxor);
        GOTONEXT();
    OPCODE(SHL_LL)
        DOBINARYOPLL(unc__vmobshl);
        GOTONEXT();
    OPCODE(SHR_LL)
        DOBINARYOPLL(unc__vmobshr);
        GOTONEXT();
    OPCODE(CAT_LL)
        DOBINARYOPLL(unc__vmobcat);
        GOTONEXT();
    OPCODE(CEQ_LL)
        DOBINARYOPLL(unc__vmobceq);
        GOTONEXT();
    OPCODE(CLT_LL)
        DOBINARYOPLL(unc__vmobclt);
        GOTONEXT();
    OPCODE(LNOT_R)
    {
        Unc_Value *s = GETREG();
        Unc_Value *a = GETREG();
        e = unc__fastvcvt2bool(w, &env, a, pc);
        if (UNCIL_IS_ERR(e)) goto vmerror;
        VSETBOOL(w, s, !e);
        GOTONEXT();
    }
    OPCODE(UPOS_R)
        DOUNARYOPR(unc__vmoupos);
        GOTONEXT();
    OPCODE(UNEG_R)
        DOUNARYOPR(unc__vmouneg);
        GOTONEXT();
    OPCODE(UXOR_R)
        DOUNARYOPR(unc__vmouxor);
        GOTONEXT();
    OPCODE(LNOT_L)
    {
        Unc_Value *s = GETREG();
        Unc_Value v;
        VSETINT(w, &v, unc__litint(w, pc));
        pc += LITINT_SZ;
        e = unc__fastvcvt2bool(w, &env, &v, pc);
        if (UNCIL_IS_ERR(e)) goto vmerror;
        VSETBOOL(w, s, !e);
        GOTONEXT();
    }
    OPCODE(UPOS_L)
        DOUNARYOPL(unc__vmoupos);
        GOTONEXT();
    OPCODE(UNEG_L)
        DOUNARYOPL(unc__vmouneg);
        GOTONEXT();
    OPCODE(UXOR_L)
        DOUNARYOPL(unc__vmouxor);
        GOTONEXT();
    OPCODE(IFF)
    {
        Unc_Value *s = GETREG();
        if (!unc__fastvcvt2bool(w, &env, s, pc)) {
            pc = w->jbase + GETJUMPDST(w);
            CHECKPAUSE();
        } else
            pc += JUMPWIDTH;
        CHECKPAUSE();
        GOTONEXT();
    }
    OPCODE(IFT)
    {
        Unc_Value *s = GETREG();
        if (unc__fastvcvt2bool(w, &env, s, pc)) {
            pc = w->jbase + GETJUMPDST(w);
            CHECKPAUSE();
        } else
            pc += JUMPWIDTH;
        GOTONEXT();
    }
    OPCODE(JMP)
        pc = w->jbase + GETJUMPDST(w);
        CHECKPAUSE();
        GOTONEXT();
    OPCODE(EXIT)
    {
        Unc_Frame *f;
        CHECKPAUSE();
        unc__unwindtotry(w);
        f = unc__exitframe(w);
        if (!f)
            goto vmerror;
        if (unc__shouldexitonframe(f))
            goto vmexit;
        UNCOMMIT();
        GOTONEXT();
    }
    OPCODE(EXIT0)
    {
        Unc_Frame *f;
        CHECKPAUSE();
        unc__unwindtotry(w);
        f = unc__exitframe0(w);
        if (!f)
            goto vmerror;
        if (unc__shouldexitonframe(f))
            goto vmexit;
        UNCOMMIT();
        GOTONEXT();
    }
    OPCODE(EXIT1)
    {
        Unc_Frame *f;
        CHECKPAUSE();
        unc__unwindtotry(w);
        f = unc__exitframe1(w, GETREG());
        if (!f)
            goto vmerror;
        if (unc__shouldexitonframe(f))
            goto vmexit;
        UNCOMMIT();
        GOTONEXT();
    }
    OPCODE(WPUSH)
        if (w->rwith.top == w->rwith.end) {
            Unc_Size z = w->rwith.end - w->rwith.base, nz = z + 16, *p;
            CHECKPAUSE();
            p = TMREALLOC(Unc_Size, &w->world->alloc, 0, w->rwith.base, z, nz);
            if (!p) {
                e = UNCIL_ERR_MEM;
                goto vmerror;
            }
            w->rwith.base = p;
            w->rwith.top = p + z;
            w->rwith.end = p + nz;
        }
        *w->rwith.top++ = unc__stackdepth(&w->swith);
        GOTONEXT();
    OPCODE(WPOP)
        unc__stackwunwind(w, &w->swith, *--w->rwith.top, 0);
        GOTONEXT();
    OPCODE(RPUSH)
        if (w->region.top == w->region.end) {
            Unc_Size z = w->region.end - w->region.base, nz = z + 16, *p;
            CHECKPAUSE();
            p = TMREALLOC(Unc_Size, &w->world->alloc, 0, w->region.base, z, nz);
            if (!p) {
                e = UNCIL_ERR_MEM;
                goto vmerror;
            }
            w->region.base = p;
            w->region.top = p + z;
            w->region.end = p + nz;
        }
        *w->region.top++ = unc__stackdepth(&w->sval);
        GOTONEXT();
    OPCODE(RPOP)
        unc__vmrestoredepth(w, &w->sval, *--w->region.top);
        GOTONEXT();
    OPCODE(XPUSH)
    {
        Unc_Frame *f;
        CHECKPAUSE();
        f = unc__saveframe(w, &env, pc);
        f->type = Unc_FrameTry;
        f->target = GETJUMPDST(w);
        pc += JUMPWIDTH;
        GOTONEXT();
    }
    OPCODE(XPOP)
        unc__doxpop(w);
        GOTONEXT();
    OPCODE(LSPRS)
    {
        Unc_Value *a = GETREG();
        CHECKPAUSE();
        COMMIT();
        dolspr(w, &env, 1, 0, a, pc);
        GOTONEXT();
    }
    OPCODE(LSPR)
    {
        Unc_Value *s = GETREG();
        Unc_Value *a = GETREG();
        CHECKPAUSE();
        COMMIT();
        dolspr(w, &env, 0, s, a, pc);
        GOTONEXT();
    }
    OPCODE(CSTK)
    {
        Unc_Size vlq;
        unsigned tmp;
        vlq = GETVLQ();
        checkstackeq(w, &env, vlq, pc);
        GOTONEXT();
    }
    OPCODE(CSTKG)
    {
        Unc_Size vlq;
        unsigned tmp;
        vlq = GETVLQ();
        checkstackge(w, &env, vlq, pc);
        GOTONEXT();
    }
    OPCODE(MLIST)
    {
        Unc_Value *s = GETREG();
        CHECKPAUSE();
        domlist(w, &env, s, 1, 0, 0, pc);
        GOTONEXT();
    }
    OPCODE(NDICT)
    {
        Unc_Value *s = GETREG();
        CHECKPAUSE();
        dondict(w, &env, s, pc);
        GOTONEXT();
    }
    OPCODE(MLISTP)
    {
        unsigned tmp;
        Unc_Value *s = GETREG();
        Unc_Size a = GETVLQ();
        Unc_Size b = GETVLQ();
        CHECKPAUSE();
        domlist(w, &env, s, 0, a, b, pc);
        GOTONEXT();
    }
    OPCODE(IITER)
    {
        Unc_Value *s = GETREG();
        CHECKPAUSE();
        MUST(unc__vgetiter(w, s, GETREG()));
        GOTONEXT();
    }
    OPCODE(FMAKE)
    {
        unsigned tmp;
        Unc_Size vlq;
        Unc_Value *s = GETREG();
        vlq = GETVLQ();
        CHECKPAUSE();
        dofmake(w, &env, s, vlq, pc);
        GOTONEXT();
    }
    OPCODE(FBIND)
    {
        Unc_Value *s = GETREG();
        Unc_Value *a = GETREG();
        Unc_Value *b = GETREG();
        CHECKPAUSE();
        dofbind(w, &env, s, a, b, pc);
        GOTONEXT();
    }
    OPCODE(INEXTS)
    {
        Unc_Value *t = GETREG();
        const byte *tpc = w->jbase + GETJUMPDST(w), *rpc = (pc += JUMPWIDTH);
        Unc_Size sd = unc__stackdepth(&w->sval);
        Unc_Frame *oldtop = w->frames.top;
        pc += JUMPWIDTH;
        CHECKPAUSE();
        dofcall(w, &env, 0, 1, 0, t, &rpc);
        if (oldtop == w->frames.top) {
            /* C call */
            if (sd == unc__stackdepth(&w->sval))
                rpc = tpc;
        } else {
            ASSERT(oldtop->type == Unc_FrameCallSpew);
            oldtop->type = Unc_FrameNextSpew;
            oldtop->pc2_r = tpc;
        }
        REFRESH();
        GOTONEXT();
    }
    OPCODE(INEXT)
    {
        Unc_RegFast s = GETREGN();
        Unc_Value *t = GETREG();
        const byte *tpc = w->jbase + GETJUMPDST(w), *rpc = (pc += JUMPWIDTH);
        Unc_Size sd = unc__stackdepth(&w->sval);
        Unc_Frame *oldtop = w->frames.top;
        CHECKPAUSE();
        dofcall(w, &env, 0, 1, 0, t, &rpc);
        if (oldtop == w->frames.top) {
            /* C call */
            if (sd == unc__stackdepth(&w->sval)) {
                rpc = tpc;
            } else {
                VCOPY(w, &regs[s], &w->sval.base[sd]);
                unc__vmrestoredepth(w, &w->sval, sd);
            }
        } else {
            ASSERT(oldtop->type == Unc_FrameCallSpew);
            oldtop->type = Unc_FrameNext;
            oldtop->target = s;
            oldtop->pc2_r = tpc;
        }
        REFRESH();
        GOTONEXT();
    }
    OPCODE(DCALLS)
    {
        Unc_Size argc = *pc++;
        Unc_Value *t = GETREG();
        const byte *rpc = pc;
        CHECKPAUSE();
        dofcall(w, &env, argc, 1, 0, t, &rpc);
        REFRESH();
        GOTONEXT();
    }
    OPCODE(DCALL)
    {
        Unc_Size argc = *pc++;
        Unc_RegFast s = GETREGN();
        Unc_Value *t = GETREG();
        const byte *rpc = pc;
        CHECKPAUSE();
        dofcall(w, &env, argc, 0, s, t, &rpc);
        REFRESH();
        GOTONEXT();
    }
    OPCODE(DTAIL)
    {
        Unc_Size argc = *pc++;
        Unc_Value *t = GETREG();
        Unc_Frame *oldtop;
        Unc_FramePartial f;
        const byte *rpc = pc;
        CHECKPAUSE();
        unc__unwindtotry(w);
        oldtop = --w->frames.top;
        unc__dotailpre(w, &f, oldtop);
        if (unc__shouldexitonpframe(&f)) {
            /* if we are in main, we need to catch errors */
            jmp_buf tenv;
            if ((e = setjmp(tenv))) {
                if (UNCIL_ERR_KIND(e) == UNCIL_ERR_KIND_TRAMPOLINE) {
                    w->corotail = 1;
                    goto vmboing;
                }
                if (w->frames.top == w->frames.base) {
                    /* error before saveframe */
                    ASSERT(w->frames.top == oldtop);
                    unc__unrecover(w);
                }
                goto vmerrormainctail;
            } else
                dofcall(w, &tenv, argc,
                    unc__shouldspewonpframe(&f), f.target, t, &rpc);
        } else
            dofcall(w, &env, argc,
                unc__shouldspewonpframe(&f), f.target, t, &rpc);
        if (UNLIKELY(oldtop == w->frames.top)) {
            /* recover from C call */
            if (unc__shouldexitonpframe(&f))
                goto vmexit;
            unc__dotailpostc(w, &f);
            UNCOMMIT();
        } else {
            unc__dotailpost(w, &f, oldtop);
            REFRESH();
        }
        GOTONEXT();
    }
    OPCODE(FCALLS)
    {
        Unc_Size argc = unc__diffregion(w);
        Unc_Value *t = GETREG();
        const byte *rpc = pc;
        CHECKPAUSE();
        dofcall(w, &env, argc, 1, 0, t, &rpc);
        REFRESH();
        GOTONEXT();
    }
    OPCODE(FCALL)
    {
        Unc_Size argc = unc__diffregion(w);
        Unc_RegFast s = GETREGN();
        Unc_Value *t = GETREG();
        const byte *rpc = pc;
        CHECKPAUSE();
        dofcall(w, &env, argc, 0, s, t, &rpc);
        REFRESH();
        GOTONEXT();
    }
    OPCODE(FTAIL)
    {
        Unc_Size argc = unc__diffregion(w);
        Unc_Value *t = GETREG();
        Unc_Frame *oldtop;
        Unc_FramePartial f;
        const byte *rpc = pc;
        CHECKPAUSE();
        unc__unwindtotry(w);
        oldtop = --w->frames.top;
        unc__dotailpre(w, &f, oldtop);
        if (unc__shouldexitonpframe(&f)) {
            /* if we are in main, we need to catch errors */
            jmp_buf tenv;
            if ((e = setjmp(tenv))) {
                if (UNCIL_ERR_KIND(e) == UNCIL_ERR_KIND_TRAMPOLINE) {
                    w->corotail = 1;
                    goto vmboing;
                }
                if (w->frames.top == w->frames.base) {
                    /* error before saveframe */
                    ASSERT(w->frames.top == oldtop);
                    unc__unrecover(w);
                }
                goto vmerrormainctail;
            } else
                dofcall(w, &tenv, argc,
                    unc__shouldspewonpframe(&f), f.target, t, &rpc);
        } else
            dofcall(w, &env, argc,
                unc__shouldspewonpframe(&f), f.target, t, &rpc);
        if (UNLIKELY(oldtop == w->frames.top)) {
            /* recover from C call */
            if (unc__shouldexitonpframe(&f))
                goto vmexit;
            unc__dotailpostc(w, &f);
            UNCOMMIT();
        } else {
            unc__dotailpost(w, &f, oldtop);
            REFRESH();
        }
        GOTONEXT();
    }
    OPCODEINV(x12)
    OPCODEINV(x13)
    OPCODEINV(x17)
    OPCODEINV(x19)
    OPCODEINV(x1B)
    OPCODEINV(x1C)
    OPCODEINV(x1D)
    OPCODEINV(x1E)
    OPCODEINV(x1F)
    OPCODEINV(x20)
    OPCODEINV(x22)
    OPCODEINV(x23)
    OPCODEINV(x25)
    OPCODEINV(x27)
    OPCODEINV(x28)
    OPCODEINV(x29)
    OPCODEINV(x2A)
    OPCODEINV(x2B)
    OPCODEINV(x2C)
    OPCODEINV(x2D)
    OPCODEINV(x2E)
    OPCODEINV(x2F)
    OPCODEINV(x30)
    OPCODEINV(x31)
    OPCODEINV(x32)
    OPCODEINV(x33)
    OPCODEINV(x34)
    OPCODEINV(x35)
    OPCODEINV(x36)
    OPCODEINV(x37)
    OPCODEINV(x38)
    OPCODEINV(x39)
    OPCODEINV(x3A)
    OPCODEINV(x3B)
    OPCODEINV(x3D)
    OPCODEINV(x3E)
    OPCODEINV(x3F)
    OPCODEINV(x4E)
    OPCODEINV(x4F)
    OPCODEINV(x5E)
    OPCODEINV(x5F)
    OPCODEINV(x6E)
    OPCODEINV(x6F)
    OPCODEINV(x7E)
    OPCODEINV(x7F)
    OPCODEINV(x84)
    OPCODEINV(x85)
    OPCODEINV(x86)
    OPCODEINV(x87)
    OPCODEINV(x88)
    OPCODEINV(x89)
    OPCODEINV(x8A)
    OPCODEINV(x8B)
    OPCODEINV(x8C)
    OPCODEINV(x8D)
    OPCODEINV(x8E)
    OPCODEINV(x8F)
    OPCODEINV(x94)
    OPCODEINV(x95)
    OPCODEINV(x96)
    OPCODEINV(x97)
    OPCODEINV(x98)
    OPCODEINV(x99)
    OPCODEINV(x9A)
    OPCODEINV(x9B)
    OPCODEINV(x9C)
    OPCODEINV(x9D)
    OPCODEINV(x9E)
    OPCODEINV(x9F)
    OPCODEINV(xA0)
    OPCODEINV(xA1)
    OPCODEINV(xA2)
    OPCODEINV(xA3)
    OPCODEINV(xA4)
    OPCODEINV(xA5)
    OPCODEINV(xA6)
    OPCODEINV(xA7)
    OPCODEINV(xA8)
    OPCODEINV(xA9)
    OPCODEINV(xAA)
    OPCODEINV(xAB)
    OPCODEINV(xAC)
    OPCODEINV(xAD)
    OPCODEINV(xAE)
    OPCODEINV(xAF)
    OPCODEINV(xB0)
    OPCODEINV(xB1)
    OPCODEINV(xB2)
    OPCODEINV(xB3)
    OPCODEINV(xB4)
    OPCODEINV(xB5)
    OPCODEINV(xB6)
    OPCODEINV(xB7)
    OPCODEINV(xB8)
    OPCODEINV(xB9)
    OPCODEINV(xBA)
    OPCODEINV(xBB)
    OPCODEINV(xBC)
    OPCODEINV(xBD)
    OPCODEINV(xBE)
    OPCODEINV(xBF)
    OPCODEINV(xDB)
    OPCODEINV(xDF)
    OPCODEINV(xE0)
    OPCODEINV(xE1)
    OPCODEINV(xE2)
    OPCODEINV(xE3)
    OPCODEINV(xE4)
    OPCODEINV(xE5)
    OPCODEINV(xE6)
    OPCODEINV(xE7)
    OPCODEINV(xE8)
    OPCODEINV(xE9)
    OPCODEINV(xEA)
    OPCODEINV(xEB)
    OPCODEINV(xEC)
    OPCODEINV(xED)
    OPCODEINV(xEE)
    OPCODEINV(xEF)
    OPCODEINV(xF0)
    OPCODEINV(xF1)
    OPCODEINV(xF2)
    OPCODEINV(xF3)
    OPCODEINV(xF4)
    OPCODEINV(xF5)
    OPCODEINV(xF6)
    OPCODEINV(xF7)
    OPCODEINV(xF8)
    OPCODEINV(xF9)
    OPCODEINV(xFA)
    OPCODEINV(xFB)
    OPCODEINV(xFC)
    OPCODEINV(xFD)
    OPCODEINV(xFE)
    OPCODE(DEL)
        GOTONEXT();
    DISPATCH_END();
    
    DEADCODE();
    NEVER();
vmerror:
    CHECKPAUSE();
    /* stack unwind */
    if (e == UNCIL_ERR_HALT) {
        while (!unc__shouldexitonframe(unc__unwindframeerr(w)))
            ;
        goto vmexit;
    }
    if (UNCIL_ERR_KIND(e) == UNCIL_ERR_KIND_TRAMPOLINE) {
        w->corotail = 0;
        goto vmboing;
    }
    if (e != UNCIL_ERR_UNCIL)
        unc__errtoexcept(w, e, &w->exc);
    e = UNCIL_ERR_UNCIL;
    for (;;) {
        Unc_Frame *f;
        unc__errstackpush(w, unc__vmgetlineno(w));
        f = unc__unwindframeerr(w);
        if (f->type == Unc_FrameTry) {
            /* try block */
            unc__vmshrinksreg(w);
            VCOPY(w, w->regs, &w->exc);
            pc = w->jbase + f->target;
            regs = w->regs;
            break;
        } else if (unc__shouldexitonframe(f)) {
            /* nope, sorry */
            goto vmexit;
        }
    }
    GOTONEXT();
vmboing:
    CHECKPAUSE();
    {
        Unc_View *t = w->trampoline;
        ASSERT(t);
        w->trampoline = NULL;
        UNC_UNLOCKF(w->runlock);
        if (!UNC_LOCKFQ(t->runlock)) {
            e = UNCIL_ERR_LOGIC_CANNOTLOCK;
            goto vmerror;
        }
        w = t;
        UNCOMMIT();
        GOTONEXT();
    }
vmerrormainctail:
    CHECKPAUSE();
    ASSERT(w->frames.top > w->frames.base);
    if (e != UNCIL_ERR_HALT) {
        if (e != UNCIL_ERR_UNCIL)
            unc__errtoexcept(w, e, &w->exc);
        unc__errstackpush(w, 0);
    }
    unc__unwindframeerr(w);
    /* nope, sorry */
vmexit:
    if (VGETTYPE(&w->coroutine) && w != origw) {
        int pe = e;
        if (UNCIL_ERR_KIND(pe) == UNCIL_ERR_KIND_TRAMPOLINE)
            pe = 0;
        UNC_UNLOCKF(w->runlock);
        w = unc__corofinish(w, &pe);
        UNC_LOCKF(w->runlock);
        if (e)
            unc__errstackpushcoro(w);
        e = pe;
        ASSERT(w);
        if (w->corotail)
            goto vmexit;
        if (e)
            goto vmerror;
        UNCOMMIT();
        GOTONEXT();
    }
    UNC_UNLOCKF(w->runlock);
    return e;
}
