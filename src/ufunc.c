/*******************************************************************************
 
Uncil -- function impl

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

#include <string.h>

#define UNCIL_DEFINES

#include "udebug.h"
#include "ufunc.h"
#include "umt.h"
#include "uncil.h"
#include "uops.h"
#include "uprog.h"
#include "uval.h"
#include "uvlq.h"

INLINE Unc_Size dec_ca(const byte *b) {
    return unc__clqdeczd(UNC_BYTES_IN_FCODEADDR, b);
}

int unc__initfuncu(Unc_View *w, Unc_Function *fn,
                   Unc_Program *program, Unc_Size in_off,
                   int fromc) {
    /* see ucomp.c pushfunc for format */
    Unc_FunctionUnc fu;
    const byte *in = program->data + in_off;
    Unc_Size offset = dec_ca(in), exh, inh, tmp, oargc, lr;
    int e = 0, optok = 0;

    in += UNC_BYTES_IN_FCODEADDR;
    fu.dbugoff = dec_ca(in);
    in += UNC_BYTES_IN_FCODEADDR;
    fu.jumpw = *in++;
    if (!fromc && w->region.top > w->region.base)
        optok = 1, lr = *--w->region.top;
    fn->flags = (int)unc__vlqdecz(&in) & ~UNC_FUNCTION_FLAG_CFUNC;
    fu.program = unc__progincref(program);
    fu.regc = unc__vlqdecz(&in);
    fu.floc = unc__vlqdecz(&in);
    exh = unc__vlqdecz(&in);
    fn->rargc = unc__vlqdecz(&in);
    oargc = unc__vlqdecz(&in);
    fn->argc = fn->rargc + oargc;
    inh = unc__vlqdecz(&in);
    if (oargc) {
        Unc_Allocator *alloc = &w->world->alloc;
        if (!(fn->defaults = TMALLOC(Unc_Value, alloc, Unc_AllocFunc, oargc)))
            e = UNCIL_ERR_MEM;
    } else
        fn->defaults = NULL;
    fn->refc = exh + inh;
    if (!e && fn->refc) {
        Unc_Size i;
        Unc_Allocator *alloc = &w->world->alloc;
        if (!(fn->refs = TMALLOC(Unc_Entity *, alloc, Unc_AllocFunc, fn->refc)))
            e = UNCIL_ERR_MEM;
        
        if (!e) {
            /* create new bound values */
            for (i = 0; i < exh; ++i) {
                Unc_Entity *x = unc__wake(w, Unc_TRef);
                if (!x) {
                    e = UNCIL_ERR_MEM;
                    break;
                }
                e = unc__bind(x, w, NULL);
                if (e) break;
                fn->refs[i] = x;
                UNCIL_INCREFE(w, x);
            }

            if (e) {
                while (i--)
                    unc__hibernate(fn->refs[i], w);
            }
        }

        for (tmp = 0; tmp < inh; ++tmp) {
            /* copies of already existing bound values */
            Unc_Size s = unc__vlqdecz(&in);
            if (!e) {
                fn->refs[exh + tmp] = w->bounds[s];
                UNCIL_INCREFE(w, fn->refs[exh + tmp]);
            }
        }

        if (e) {
            TMFREE(Unc_Value, alloc, fn->defaults, oargc);
            TMFREE(Unc_Entity *, alloc, fn->refs, fn->refc);
        }
    } else
        fn->refs = NULL;

    /* fu.lineno = unc__vlqdecz(&in); */
    tmp = unc__vlqdecz(&in);
    if (fn->flags & UNC_FUNCTION_FLAG_NAMED)
        fu.nameoff = tmp;

    if (!e) {
        if (optok) {
            Unc_Value *defsrc = &w->sval.base[lr];
            Unc_Size i;
            for (i = 0; i < oargc; ++i)
                VIMPOSE(w, &fn->defaults[i], &defsrc[i]);
        } else if (oargc)
            NEVER();
    }
    fu.pc = offset;
    fn->f.u = fu;
    return e;
}

int unc__initfuncc(Unc_View *w, Unc_Function *fn, Unc_CFunc fcp,
            Unc_Size argcount, int flags, int cflags,
            Unc_Size optcount, Unc_Value *defaults,
            Unc_Size refcount, Unc_Value *initvalues,
            Unc_Size refcopycount, Unc_Size *refcopies,
            const char *fname, void *udata) {
    int e = 0;
    Unc_FunctionC fc;
    Unc_Allocator *alloc = &w->world->alloc;
    fc.fc = fcp;
    if (fname) {
        fc.namelen = strlen(fname);
        fc.name = unc__malloc(alloc, Unc_AllocExternal, fc.namelen);
        if (!fc.name)
            fc.namelen = 0;
        else
            unc__memcpy(fc.name, fname, fc.namelen);
    } else {
        fc.namelen = 0;
        fc.name = NULL;
    }
    fc.udata = udata;
    fc.cflags = cflags;
    if (UNC_LOCKINITF(fc.lock))
        return e;
    fn->flags = flags | UNC_FUNCTION_FLAG_CFUNC;
    fn->rargc = argcount;
    fn->argc = argcount + optcount;
    fn->refc = refcount + refcopycount;
    if (optcount) {
        if (!(fn->defaults = TMALLOC(Unc_Value, alloc,
                                     Unc_AllocFunc, optcount)))
            e = UNCIL_ERR_MEM;
    } else
        fn->defaults = NULL;
    if (!e && fn->refc) {
        Unc_Size i, bc = w->boundcount;
        if (!(fn->refs = TMALLOC(Unc_Entity *, alloc, Unc_AllocFunc, fn->refc)))
            e = UNCIL_ERR_MEM;
        
        if (!e) {
            /* create new bound values */
            for (i = 0; i < refcount; ++i) {
                Unc_Entity *x = unc__wake(w, Unc_TRef);
                if (!x) {
                    e = UNCIL_ERR_MEM;
                    break;
                }
                e = unc__bind(x, w, &initvalues[i]);
                if (e) break;
                fn->refs[i] = x;
                UNCIL_INCREFE(w, x);
            }

            if (e) {
                while (i--)
                    unc__hibernate(fn->refs[i], w);
            }
        }

        if (!e)
            for (i = 0; i < refcopycount; ++i) {
                if (refcopies[i] >= bc) {
                    e = UNCIL_ERR_ARG_REFCOPYOUTOFBOUNDS;
                    break;
                }
            }

        if (!e)
            /* copies of already existing bound values */
            for (i = 0; i < refcopycount; ++i) {
                fn->refs[refcount + i] = w->bounds[refcopies[i]];
                UNCIL_INCREFE(w, fn->refs[refcount + i]);
            }

        if (e)
            TMFREE(Unc_Entity *, alloc, fn->refs, fn->refc);
    } else
        fn->refs = NULL;

    if (!e) {
        Unc_Size i;
        if (defaults)
            for (i = 0; i < optcount; ++i)
                VIMPOSE(w, &fn->defaults[i], &defaults[i]);
        else {
            for (i = 0; i < optcount; ++i)
                VINITNULL(&fn->defaults[i]);
        }
    }
    fn->f.c = fc;
    return e;
}

int unc__initbfunc(Unc_View *w, Unc_FunctionBound *bfn,
            Unc_Value *fn, Unc_Value *boundto) {
    switch (VGETTYPE(fn)) {
    case Unc_TFunction:
    case Unc_TObject:
    case Unc_TOpaque:
        break;
    default:
        return UNCIL_ERR_ARG_CANNOTBINDFUNC;
    }
    switch (VGETTYPE(boundto)) {
    case Unc_TString:
    case Unc_TBlob:
    case Unc_TArray:
    case Unc_TTable:
    case Unc_TObject:
    case Unc_TOpaque:
        break;
    default:
        return UNCIL_ERR_ARG_CANNOTBINDFUNC;
    }
    VIMPOSE(w, &bfn->fn, fn);
    VIMPOSE(w, &bfn->boundto, boundto);
    return 0;
}

/* function calling is in uvm.c */

void unc__dropfunc(Unc_View *w, Unc_Function *fn) {
    Unc_Size i, o = fn->argc - fn->rargc;
    for (i = 0; i < fn->refc; ++i)
        UNCIL_DECREFE(w, fn->refs[i]);
    for (i = 0; i < o; ++i)
        VDECREF(w, &fn->defaults[i]);
    unc__sunsetfunc(&w->world->alloc, fn);
}

void unc__sunsetfunc(Unc_Allocator *alloc, Unc_Function *fn) {
    if (fn->flags & UNC_FUNCTION_FLAG_CFUNC) {
        UNC_LOCKFINAF(fn->f.c.lock);
        unc__mfree(alloc, fn->f.c.name, fn->f.c.namelen);
    }
    else if (fn->f.u.program)
        unc__progdecref(fn->f.u.program, alloc);
    TMFREE(Unc_Value, alloc, fn->defaults, fn->argc - fn->rargc);
    TMFREE(Unc_Entity *, alloc, fn->refs, fn->refc);
}

void unc__dropbfunc(Unc_View *w, Unc_FunctionBound *fn) {
    VDECREF(w, &fn->boundto);
    VDECREF(w, &fn->fn);
}

void unc__sunsetbfunc(Unc_Allocator *alloc, Unc_FunctionBound *fn) {
}
