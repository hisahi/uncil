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

#ifndef UNCIL_USTACK_H
#define UNCIL_USTACK_H

#include "udef.h"
#include "uerr.h"
#include "umem.h"
#include "uval.h"

typedef struct Unc_Stack {
    Unc_Value *base;
    Unc_Value *top;
    Unc_Value *end;
} Unc_Stack;

struct Unc_View;

#define unc__stackdepth(s) ((s)->top - (s)->base)

int unc__stackinit(struct Unc_View *w, Unc_Stack *s, Unc_Size start);
int unc__stackreserve(struct Unc_View *w, Unc_Stack *s, Unc_Size n);
int unc__stackpush(struct Unc_View *w, Unc_Stack *s, Unc_Size n, Unc_Value *v);
int unc__stackpushv(struct Unc_View *w, Unc_Stack *s, Unc_Value *v);
int unc__stackpushn(struct Unc_View *w, Unc_Stack *s, Unc_Size n);
Unc_Value *unc__stackat(Unc_Stack *s, int offset);
Unc_Value *unc__stackbeyond(Unc_Stack *s, int offset);
int unc__stackinsert(struct Unc_View *w, Unc_Stack *s, Unc_Size i,
                                                    Unc_Value *v);
int unc__stackinsertm(struct Unc_View *w, Unc_Stack *s, Unc_Size i,
                                                    Unc_Size n, Unc_Value *v);
int unc__stackinsertn(struct Unc_View *w, Unc_Stack *s,
                                                    Unc_Size i, Unc_Value *v);
void unc__restoredepth(struct Unc_View *w, Unc_Stack *s, Unc_Size d);
void unc__stackwunwind(struct Unc_View *w, Unc_Stack *s, Unc_Size d, int ex);
void unc__stackpullrug(struct Unc_View *w, Unc_Stack *s, Unc_Size d,
                                                         Unc_Size e);
void unc__stackmove(struct Unc_View *w, Unc_Stack *b, Unc_Stack *a, Unc_Size n);
void unc__stackpop(struct Unc_View *w, Unc_Stack *s, Unc_Size n);
void unc__stackfree(struct Unc_View *w, Unc_Stack *s);

#endif /* UNCIL_USTACK_H */
