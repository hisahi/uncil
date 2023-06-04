/*******************************************************************************
 
Uncil -- blob header

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

#ifndef UNCIL_UBLOB_H
#define UNCIL_UBLOB_H

#include "udef.h"
#include "umem.h"
#include "umt.h"
#include "uval.h"

typedef struct Unc_Blob {
    Unc_Size size;
    Unc_Size capacity;
    byte *data;
    UNC_LOCKLIGHT(lock)
} Unc_Blob;

int unc0_initblob(Unc_Allocator *alloc, Unc_Blob *q,
                  Unc_Size n, const byte *b);
int unc0_initblobraw(Unc_Allocator *alloc, Unc_Blob *q, Unc_Size n, byte **b);
int unc0_initblobmove(Unc_Allocator *alloc, Unc_Blob *q, Unc_Size n, byte *b);
int unc0_blobadd(Unc_Allocator *alloc, Unc_Blob *q, Unc_Size n, const byte *b);
int unc0_blobaddb(Unc_Allocator *alloc, Unc_Blob *q, byte b);
int unc0_blobaddf(Unc_Allocator *alloc, Unc_Blob *q, const Unc_Blob *q2);
int unc0_blobaddn(Unc_Allocator *alloc, Unc_Blob *q, Unc_Size n);
int unc0_blobins(Unc_Allocator *alloc, Unc_Blob *q, Unc_Size i,
                                    Unc_Size n, const byte *b);
int unc0_blobinsf(Unc_Allocator *alloc, Unc_Blob *q, Unc_Size i,
                                    const Unc_Blob *q2);
int unc0_blobinsn(Unc_Allocator *alloc, Unc_Blob *q, Unc_Size i, Unc_Size n);
int unc0_blobdel(Unc_Allocator *alloc, Unc_Blob *q, Unc_Size i, Unc_Size n);
void unc0_dropblob(Unc_Allocator *alloc, Unc_Blob *q);

/* the functions above do not lock, the ones below do */
int unc0_blobeq(Unc_Blob *a, Unc_Blob *b);
int unc0_blobeqr(Unc_Blob *a, Unc_Size n, const byte *b);
int unc0_cmpblob(Unc_Blob *a, Unc_Blob *b);

int unc0_blgetbyte(struct Unc_View *w, Unc_Blob *bl, Unc_Value *i,
                        int permissive, Unc_Value *v);
int unc0_blsetbyte(struct Unc_View *w, Unc_Blob *bl, Unc_Value *i,
                        Unc_Value *v);

#endif /* UNCIL_UBLOB_H */
