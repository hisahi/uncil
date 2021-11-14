/*******************************************************************************
 
Uncil -- value operation header

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

#ifndef UNCIL_UVOP_H
#define UNCIL_UVOP_H

#include "uval.h"

int unc__vveq(Unc_View *w, Unc_Value *a, Unc_Value *b);
int unc__vvclt(Unc_View *w, Unc_Value *a, Unc_Value *b);
int unc__vvcmp(Unc_View *w, Unc_Value *a, Unc_Value *b);

int unc__vcvt2int(struct Unc_View *w, Unc_Value *out, Unc_Value *in);
int unc__vcvt2flt(struct Unc_View *w, Unc_Value *out, Unc_Value *in);

struct Unc_View;

int unc__vgetattr(struct Unc_View *w, Unc_Value *a,
                                Unc_Size sl, const byte *sb,
                                int q, Unc_Value *v);
int unc__vgetattrf(struct Unc_View *w, Unc_Value *a,
                                Unc_Size sl, const byte *sb,
                                int q, Unc_Value *v);
int unc__vsetattr(struct Unc_View *w, Unc_Value *a,
                                Unc_Size sl, const byte *sb,
                                Unc_Value *v);
int unc__vdelattr(struct Unc_View *w, Unc_Value *a, Unc_Size sl, const byte *sb);
int unc__vgetattrv(struct Unc_View *w, Unc_Value *a, Unc_Value *i, int q,
                                       Unc_Value *v);
int unc__vsetattrv(struct Unc_View *w, Unc_Value *a, Unc_Value *i, Unc_Value *v);
int unc__vdelattrv(struct Unc_View *w, Unc_Value *a, Unc_Value *i);
int unc__vgetindx(struct Unc_View *w, Unc_Value *a, Unc_Value *i, int q,
                                       Unc_Value *v);
int unc__vsetindx(struct Unc_View *w, Unc_Value *a, Unc_Value *i, Unc_Value *v);
int unc__vdelindx(struct Unc_View *w, Unc_Value *a, Unc_Value *i);

int unc__vgetiter(struct Unc_View *w, Unc_Value *out, Unc_Value *in);

/* these return a value with refs=1 */
int unc__vovlunary(struct Unc_View *w, Unc_Value *in,
                        Unc_Value *out, Unc_Size bn, const byte *bb);
int unc__vovlbinary(struct Unc_View *w, Unc_Value *a, Unc_Value *b,
                        Unc_Value *out, Unc_Size bn, const byte *bb,
                                        Unc_Size b2n, const byte *b2b);

int unc__vdowith(struct Unc_View *w, Unc_Value *v);
void unc__vdowout(struct Unc_View *w, Unc_Value *v);

#endif /* UNCIL_UVOP_H */
