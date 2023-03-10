/*******************************************************************************
 
Uncil -- value operation header

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

#ifndef UNCIL_UVOP_H
#define UNCIL_UVOP_H

#include "uval.h"

int unc0_vveq(Unc_View *w, Unc_Value *a, Unc_Value *b);
int unc0_vvclt(Unc_View *w, Unc_Value *a, Unc_Value *b);
int unc0_vvcmp(Unc_View *w, Unc_Value *a, Unc_Value *b);
int unc0_vvcmpe(Unc_View *w, Unc_Value *a, Unc_Value *b, int e);

int unc0_vcvt2int(struct Unc_View *w, Unc_Value *out, Unc_Value *in);
int unc0_vcvt2flt(struct Unc_View *w, Unc_Value *out, Unc_Value *in);

struct Unc_View;

int unc0_vgetattr(struct Unc_View *w, Unc_Value *a,
                                Unc_Size sl, const byte *sb,
                                int q, Unc_Value *v);
int unc0_vgetattrf(struct Unc_View *w, Unc_Value *a,
                                Unc_Size sl, const byte *sb,
                                int q, Unc_Value *v);
int unc0_vsetattr(struct Unc_View *w, Unc_Value *a,
                                Unc_Size sl, const byte *sb,
                                Unc_Value *v);
int unc0_vdelattr(struct Unc_View *w, Unc_Value *a, Unc_Size sl, const byte *sb);
int unc0_vgetattrv(struct Unc_View *w, Unc_Value *a, Unc_Value *i, int q,
                                       Unc_Value *v);
int unc0_vsetattrv(struct Unc_View *w, Unc_Value *a, Unc_Value *i, Unc_Value *v);
int unc0_vdelattrv(struct Unc_View *w, Unc_Value *a, Unc_Value *i);
int unc0_vgetindx(struct Unc_View *w, Unc_Value *a, Unc_Value *i, int q,
                                       Unc_Value *v);
int unc0_vsetindx(struct Unc_View *w, Unc_Value *a, Unc_Value *i, Unc_Value *v);
int unc0_vdelindx(struct Unc_View *w, Unc_Value *a, Unc_Value *i);

int unc0_vgetiter(struct Unc_View *w, Unc_Value *out, Unc_Value *in);

/* these return a value with refs=1 */
int unc0_vovlunary(struct Unc_View *w, Unc_Value *in,
                        Unc_Value *out, Unc_Size bn, const byte *bb);
int unc0_vovlbinary(struct Unc_View *w, Unc_Value *a, Unc_Value *b,
                        Unc_Value *out, Unc_Size bn, const byte *bb,
                                        Unc_Size b2n, const byte *b2b);

int unc0_vdowith(struct Unc_View *w, Unc_Value *v);
void unc0_vdowout(struct Unc_View *w, Unc_Value *v);

#endif /* UNCIL_UVOP_H */
