/*******************************************************************************
 
Uncil -- stack impl

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

#include "udebug.h"
#include "uncil.h"
#include "ustack.h"
#include "uvali.h"
#include "uvop.h"

int unc0_stackinit(Unc_View *w, Unc_Stack *s, Unc_Size start) {
    s->base = s->top = s->end = NULL;
    if (start) {
        Unc_Allocator *alloc = &w->world->alloc;
        Unc_Value *v = unc0_malloc(alloc, 0, start * sizeof(Unc_Value));
        if (!v)
            return UNCIL_ERR_MEM;
        s->base = s->top = v;
        s->end = s->base + start;
    }
    return 0;
}

int unc0_stackreserve(Unc_View *w, Unc_Stack *s, Unc_Size n) {
    Unc_Size q = s->top - s->base, c = s->end - s->base;
    if (q + n > c) {
        Unc_Allocator *alloc = &w->world->alloc;
        Unc_Size nc = c + 16;
        Unc_Value *newbase;
        if (q + n > nc)
            nc = q + n + 4;
        newbase = unc0_mrealloc(alloc, 0, s->base,
                            c * sizeof(Unc_Value), nc * sizeof(Unc_Value));
        if (!newbase)
            return UNCIL_ERR_MEM;
        s->base = newbase;
        s->top = newbase + q;
        s->end = newbase + nc;
    }
    return 0;
}

int unc0_stackpush(Unc_View *w, Unc_Stack *s, Unc_Size n, Unc_Value *v) {
    Unc_Size i;
    int e = unc0_stackreserve(w, s, n);
    if (e) return e;
    for (i = 0; i < n; ++i)
        VIMPOSE(w, &s->top[i], &v[i]);
    s->top += n;
    return 0;
}

int unc0_stackpushn(Unc_View *w, Unc_Stack *s, Unc_Size n) {
    Unc_Size i;
    int e = unc0_stackreserve(w, s, n);
    if (e) return e;
    for (i = 0; i < n; ++i)
        VINITNULL(&s->top[i]);
    s->top += n;
    return 0;
}

int unc0_stackpushv(Unc_View *w, Unc_Stack *s, Unc_Value *v) {
    int e = unc0_stackreserve(w, s, 1);
    if (e) return e;
    VIMPOSE(w, s->top++, v);
    return 0;
}

int unc0_stackinsert(Unc_View *w, Unc_Stack *s, Unc_Size i, Unc_Value *v) {
    int e = unc0_stackreserve(w, s, 1);
    if (e) return e;
    unc0_memmove(s->base + i + 1, s->base + i,
                 sizeof(Unc_Value) * (s->top - s->base - i));
    VIMPOSE(w, &s->base[i], v);
    ++s->top;
    return 0;
}

int unc0_stackinsertm(struct Unc_View *w, Unc_Stack *s, Unc_Size i,
                                                     Unc_Size n, Unc_Value *v) {
    int e = unc0_stackreserve(w, s, n);
    if (e) return e;
    unc0_memmove(s->base + i + n, s->base + i,
                 sizeof(Unc_Value) * (s->top - s->base - i));
    s->top += n;
    while (n--)
        VIMPOSE(w, &s->base[i++], v++);
    return 0;
}

int unc0_stackinsertn(struct Unc_View *w, Unc_Stack *s,
                                                    Unc_Size i, Unc_Value *v) {
    int e = unc0_stackreserve(w, s, 1);
    if (e) return e;
    unc0_memmove(s->top - i + 1, s->top - i, sizeof(Unc_Value) * i);
    VIMPOSE(w, s->top - i, v);
    ++s->top;
    return 0;
}

Unc_Value *unc0_stackat(Unc_Stack *s, int offset) {
    return s->top - 1 - offset;
}

Unc_Value *unc0_stackbeyond(Unc_Stack *s, int offset) {
    return s->top + offset;
}

INLINE void stackdecrefs(Unc_View *w, Unc_Value *v, Unc_Size n) {
    while (n--)
        VDECREF(w, --v);
}

void unc0_restoredepth(Unc_View *w, Unc_Stack *s, Unc_Size d) {
    Unc_Size n = s->top - s->base;
    ASSERT(n >= d);
    if (n > d) {
        stackdecrefs(w, s->top, n - d);
        s->top = s->base + d;
    }
}

void unc0_stackwunwind(Unc_View *w, Unc_Stack *s, Unc_Size d, int onexit) {
    Unc_Size n = s->top - s->base;
    ASSERT(n >= d);
    if (n > d) {
        if (onexit) ++w->frames.top;
        while (n > d) {
            --n;
            --s->top;
            unc0_vdowout(w, s->top);
            VDECREF(w, s->top);
        }
        if (onexit) --w->frames.top;
    }
}

void unc0_stackpullrug(Unc_View *w, Unc_Stack *s, Unc_Size d, Unc_Size e) {
    Unc_Size n = s->top - s->base;
    ASSERT(n >= d);
    ASSERT(n >= e);
    ASSERT(e >= d);
    if (e > d) {
        Unc_Size r = n - e;
        stackdecrefs(w, s->base + e, e - d);
        unc0_memmove(s->base + d, s->base + e, r * sizeof(Unc_Value));
        s->top = s->base + d + r;
    }
}

void unc0_stackpop(Unc_View *w, Unc_Stack *s, Unc_Size n) {
    stackdecrefs(w, s->top, n);
    s->top -= n;
    if (s->top < s->base)
        s->top = s->base;
}

void unc0_stackfree(Unc_View *w, Unc_Stack *s) {
    Unc_Allocator *alloc = &w->world->alloc;
    Unc_Size n = s->top - s->base, c = s->end - s->base;
    stackdecrefs(w, s->top, n);
    unc0_mfree(alloc, s->base, c * sizeof(Unc_Value));
    s->base = s->top = s->end = NULL;
}
