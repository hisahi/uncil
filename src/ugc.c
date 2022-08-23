/*******************************************************************************
 
Uncil -- garbage collector impl

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

#define UNCIL_DEFINES

#include "uarr.h"
#include "udebug.h"
#include "ufunc.h"
#include "ugc.h"
#include "umodule.h"
#include "uncil.h"
#include "uobj.h"
#include "uopaque.h"
#include "uval.h"
#include "uvali.h"
#include "uvop.h"

/* unmarked value, must be 0 */
#define UNC_GC_RED 0
/* value that has been marked but which may have children to mark */
#define UNC_GC_YELLOW 1
/* value and its immediate children marked */
#define UNC_GC_GREEN 2

void unc0_gcdefaults(Unc_GC *gc) {
    gc->enabled = 1;
    gc->entitylimit = 800;
}

INLINE Unc_Size unc0_gccollect_root_val(Unc_Value *v) {
    if (UNCIL_OF_REFTYPE(v) && !VGETENT(v)->mark) {
        VGETENT(v)->mark = UNC_GC_YELLOW;
        return 1;
    }
    return 0;
}

static Unc_Size unc0_gccollect_root_htbl(Unc_HTblS *h) {
    Unc_Size z = 0;
    if (h) {
        Unc_Size i = 0, c = h->capacity;
        Unc_HTblS_V *nx = NULL;
        for (;;) {
            while (!nx && i < c)
                nx = h->buckets[i++];
            if (!nx && i >= c)
                break;
            z += unc0_gccollect_root_val(&nx->val);
            nx = nx->next;
        }
    }
    return z;
}

static Unc_Size unc0_gccollect_root_stack(Unc_Stack *s) {
    Unc_Size z = 0;
    Unc_Value *j = s->base, *e = s->top;
    while (j != e)
        z += unc0_gccollect_root_val(j++);
    return z;
}

static Unc_Size unc0_gccollect_root(Unc_World *w) {
    Unc_View *v = w->view;
    Unc_Size y = 0;
    y += unc0_gccollect_root_htbl(&w->pubs);
    y += unc0_gccollect_root_htbl(&w->modulecache);
    y += unc0_gccollect_root_val(&w->met_str);
    y += unc0_gccollect_root_val(&w->met_blob);
    y += unc0_gccollect_root_val(&w->met_arr);
    y += unc0_gccollect_root_val(&w->met_dict);
    y += unc0_gccollect_root_val(&w->io_file);
    y += unc0_gccollect_root_val(&w->exc_oom);
    y += unc0_gccollect_root_val(&w->modulepaths);
    y += unc0_gccollect_root_val(&w->moduledlpaths);
    while (v) {
        Unc_ModuleFrame *mf = v->mframes;
        y += unc0_gccollect_root_stack(&v->sval);
        y += unc0_gccollect_root_stack(&v->sreg);
        y += unc0_gccollect_root_htbl(v->pubs);
        y += unc0_gccollect_root_htbl(v->exports);
        y += unc0_gccollect_root_val(&v->met_str);
        y += unc0_gccollect_root_val(&v->met_blob);
        y += unc0_gccollect_root_val(&v->met_arr);
        y += unc0_gccollect_root_val(&v->met_dict);
        y += unc0_gccollect_root_val(&v->fmain);
        y += unc0_gccollect_root_val(&v->exc);
        y += unc0_gccollect_root_val(&v->coroutine);
#if UNCIL_MT_OK
        y += unc0_gccollect_root_val(&v->threadme);
#endif
        while (mf) {
            y += unc0_gccollect_root_stack(&mf->sreg);
            y += unc0_gccollect_root_htbl(mf->pubs);
            y += unc0_gccollect_root_htbl(mf->exports);
            y += unc0_gccollect_root_val(&mf->fmain);
            mf = mf->nextf;
        }
        v = v->nextview;
    }
    return y;
}

#define UNC_GC_MARK_MAXDEPTH 32

static Unc_Size unc0_gccollect_mark_ent(Unc_Entity *e, int depth);

INLINE Unc_Size unc0_gccollect_mark_val(Unc_Value *v, int depth) {
    return UNCIL_OF_REFTYPE(v)
        ? unc0_gccollect_mark_ent(VGETENT(v), depth + 1)
        : 0;
}

static Unc_Size unc0_gccollect_mark_hv(Unc_HTblV *h, int depth) {
    Unc_Size y = 0, i = 0, c = h->capacity;
    Unc_HTblV_V *nx = NULL;
    for (;;) {
        while (!nx && i < c)
            nx = h->buckets[i++];
        if (!nx && i >= c)
            break;
        y += unc0_gccollect_mark_val(&nx->key, depth);
        y += unc0_gccollect_mark_val(&nx->val, depth);
        nx = nx->next;
    }
    return y;
}

INLINE Unc_Size unc0_gccollect_mark_a(Unc_Array *e, int depth) {
    Unc_Size y = 0, i = 0, c = e->size;
    for (i = 0; i < c; ++i)
        y += unc0_gccollect_mark_val(&e->data[i], depth);
    return y;
}

INLINE Unc_Size unc0_gccollect_mark_d(Unc_Dict *e, int depth) {
    return unc0_gccollect_mark_hv(&e->data, depth);
}

INLINE Unc_Size unc0_gccollect_mark_o(Unc_Object *e, int depth) {
    return unc0_gccollect_mark_hv(&e->data, depth)
         + unc0_gccollect_mark_val(&e->prototype, depth);
}

INLINE Unc_Size unc0_gccollect_mark_opaq(Unc_Opaque *e, int depth) {
    Unc_Size i, refc = e->refc,
             y = unc0_gccollect_mark_val(&e->prototype, depth);
    for (i = 0; i < refc; ++i)
        y += unc0_gccollect_mark_ent(e->refs[i], depth + 1);
    return y;
}

INLINE Unc_Size unc0_gccollect_mark_fn(Unc_Function *e, int depth) {
    Unc_Size y = 0, i, oargc = e->argc - e->rargc, refc = e->refc;
    for (i = 0; i < oargc; ++i)
        y += unc0_gccollect_mark_val(&e->defaults[i], depth);
    for (i = 0; i < refc; ++i)
        y += unc0_gccollect_mark_ent(e->refs[i], depth + 1);
    return y;
}

INLINE Unc_Size unc0_gccollect_mark_bfn(Unc_FunctionBound *e, int depth) {
    return unc0_gccollect_mark_val(&e->boundto, depth)
         + unc0_gccollect_mark_val(&e->fn, depth);
}

static Unc_Size unc0_gccollect_mark_ent(Unc_Entity *e, int depth) {
    Unc_Size y = 0;
    switch (e->mark) {
    case UNC_GC_RED:
        e->mark = UNC_GC_YELLOW;
        ++y;
        break;
    case UNC_GC_YELLOW:
        break;
    case UNC_GC_GREEN:
        return 0;
    }
    if (depth < UNC_GC_MARK_MAXDEPTH) {
        if (e->mark == UNC_GC_YELLOW) {
            --y; /* wrap-around is fine, we are adding this value */
            e->mark = UNC_GC_GREEN;
        }
        switch (e->type) {
        case Unc_TString:
        case Unc_TBlob:
        case Unc_TWeakRef:
            break;
        case Unc_TRef:
            y += unc0_gccollect_mark_val(LEFTOVER(Unc_Value, e), depth);
            break;
        case Unc_TArray:
            y += unc0_gccollect_mark_a(LEFTOVER(Unc_Array, e), depth);
            break;
        case Unc_TTable:
            y += unc0_gccollect_mark_d(LEFTOVER(Unc_Dict, e), depth);
            break;
        case Unc_TObject:
            y += unc0_gccollect_mark_o(LEFTOVER(Unc_Object, e), depth);
            break;
        case Unc_TFunction:
            y += unc0_gccollect_mark_fn(LEFTOVER(Unc_Function, e), depth);
            break;
        case Unc_TOpaque:
            y += unc0_gccollect_mark_opaq(LEFTOVER(Unc_Opaque, e), depth);
            break;
        case Unc_TBoundFunction:
            y += unc0_gccollect_mark_bfn(LEFTOVER(Unc_FunctionBound, e), depth);
            break;
        default:
            NEVER_();
        }
    }
    return y;
}

static void unc0_gccollect_mark(Unc_World *w, Unc_Size yellows) {
    while (yellows) {
        Unc_Entity *e = w->etop;
        while (e) {
            if (e->mark == UNC_GC_YELLOW)
                yellows += unc0_gccollect_mark_ent(e, 0);
            e = e->down;
        }
    }
}

static void unc0_gccollect_presweep(Unc_World *w, Unc_View *v) {
    Unc_Entity *e = w->etop;
    while (e) {
        if (e->creffed)
            e->mark = UNC_GC_GREEN;
        else if (!e->mark) {
            switch (e->type) {
            case Unc_TOpaque:
                unc0_graceopaque(v, LEFTOVER(Unc_Opaque, e));
                break;
            default:
                ;
            }
        }
        e = e->down;
    }
}

Unc_Size unc0_suggeststacksize(Unc_Size s) {
    Unc_Size j = 1;
    s += 7;
    while (s & (s + 1))
        s |= s >> j;
    return s + 1;
}

static void unc0_gccollect_minimize(Unc_Allocator *alloc, Unc_View *w) {
    Unc_Size s, t, u;
    s = w->sval.top - w->sval.base;
    t = w->sval.end - w->sval.base;
    u = unc0_suggeststacksize(s);
    if (u < t) {
        w->sval.base = TMREALLOC(Unc_Value, alloc, 0, w->sval.base, t, u);
        w->sval.top = w->sval.base + s;
        w->sval.end = w->sval.base + u;
    }
    /* not safe to do here, as this can be called from a bunch of different
       places and the VM may still be running.
    s = w->sreg.top - w->sreg.base;
    t = w->sreg.end - w->sreg.base;
    u = unc0_suggeststacksize(s);
    if (u < t) {
        w->sreg.base = TMREALLOC(Unc_Value, alloc, 0, w->sreg.base, t, u);
        w->sreg.top = w->sreg.base + s;
        w->sreg.end = w->sreg.base + u;
    }
    */
    s = w->region.top - w->region.base;
    t = w->region.end - w->region.base;
    u = unc0_suggeststacksize(s);
    if (u < t) {
        w->region.base = TMREALLOC(Unc_Size, alloc, 0, w->region.base, t, u);
        w->region.top = w->region.base + s;
        w->region.end = w->region.base + u;
    }
    s = w->frames.top - w->frames.base;
    t = w->frames.end - w->frames.base;
    u = unc0_suggeststacksize(s);
    if (u < t) {
        w->frames.base = TMREALLOC(Unc_Frame, alloc, 0, w->frames.base, t, u);
        w->frames.top = w->frames.base + s;
        w->frames.end = w->frames.base + u;
    }
    s = w->swith.top - w->swith.base;
    t = w->swith.end - w->swith.base;
    u = unc0_suggeststacksize(s);
    if (u < t) {
        w->swith.base = TMREALLOC(Unc_Value, alloc, 0, w->swith.base, t, u);
        w->swith.top = w->swith.base + s;
        w->swith.end = w->swith.base + u;
    }
    s = w->rwith.top - w->rwith.base;
    t = w->rwith.end - w->rwith.base;
    u = unc0_suggeststacksize(s);
    if (u < t) {
        w->rwith.base = TMREALLOC(Unc_Size, alloc, 0, w->rwith.base, t, u);
        w->rwith.top = w->rwith.base + s;
        w->rwith.end = w->rwith.base + u;
    }
}

static void unc0_gccollect_sweep(Unc_World *w) {
    Unc_View *v = w->view;
    Unc_Entity *e = w->etop, *ee;
    while (v) {
        Unc_Size i;
        for (i = 0; i < UNC_SLEEPER_VALUES; ++i)
            v->sleepers[i] = NULL;
        v->entityload = 0;
        unc0_gccollect_minimize(&w->alloc, v);
        v = v->nextview;
    }
    while (e) {
        ee = e->down;
        ASSERT(e->mark != UNC_GC_YELLOW);
        if (e->mark == UNC_GC_GREEN) {
            e->mark = 0;
        } else if (IS_SLEEPING(e)) {
            unc0_wreck(e, w);
        } else if (!e->mark) {
            unc0_scrap(e, &w->alloc, NULL);
            unc0_wreck(e, w);
        }
        e = ee;
    }
}

void unc0_gccollect(Unc_World *w, Unc_View *v) {
    UNC_PAUSE(v);
    unc0_gccollect_mark(w, unc0_gccollect_root(w));
    unc0_gccollect_presweep(w, v ? v : w->view);
    unc0_gccollect_sweep(w);
    UNC_RESUME(v);
}
