/*******************************************************************************
 
Uncil -- array header

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

#ifndef UNCIL_UARR_H
#define UNCIL_UARR_H

#include "udef.h"
#include "umem.h"
#include "umt.h"
#include "uval.h"

typedef struct Unc_Array {
    Unc_Size size;
    Unc_Size capacity;
    Unc_Value *data;
    UNC_LOCKLIGHT(lock)
} Unc_Array;

struct Unc_View;

int unc0_initarray(struct Unc_View *w, Unc_Array *a, Unc_Size n, Unc_Value *v);
int unc0_initarrayn(struct Unc_View *w, Unc_Array *a, Unc_Size n);
int unc0_initarrayfromcat(struct Unc_View *w, Unc_Array *s,
                          Unc_Size an, Unc_Value *av,
                          Unc_Size bn, Unc_Value *bv);
int unc0_initarrayraw(struct Unc_View *w, Unc_Array *a, Unc_Size n,
                            Unc_Value *v);
int unc0_arraycatr(struct Unc_View *w, Unc_Array *a, Unc_Size n, Unc_Value *v);
int unc0_arraypush(struct Unc_View *w, Unc_Array *a, Unc_Value *v);
int unc0_arraypushn(struct Unc_View *w, Unc_Array *a, Unc_Size n);
int unc0_arraycat(struct Unc_View *w, Unc_Array *a, const Unc_Array *a2);
int unc0_arrayinsr(struct Unc_View *w, Unc_Array *a, Unc_Size i,
                    Unc_Size n, Unc_Value *v);
int unc0_arrayinsv(struct Unc_View *w, Unc_Array *a, Unc_Size i,
                    Unc_Value *v);
int unc0_arrayins(struct Unc_View *w, Unc_Array *a, Unc_Size i,
                    const Unc_Array *a2);
int unc0_arraydel(struct Unc_View *w, Unc_Array *a, Unc_Size i, Unc_Size n);
void unc0_droparray(struct Unc_View *w, Unc_Array *a);
void unc0_sunsetarray(Unc_Allocator *alloc, Unc_Array *a);

/* the functions above do not lock, the ones below do */
int unc0_agetindx(struct Unc_View *w, Unc_Array *a,
                    Unc_Value *indx, int permissive, Unc_Value *out);
int unc0_asetindx(struct Unc_View *w, Unc_Array *a,
                    Unc_Value *indx, Unc_Value *v);

#endif /* UNCIL_UARR_H */
