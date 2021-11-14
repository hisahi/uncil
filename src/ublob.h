/*******************************************************************************
 
Uncil -- blob header

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

int unc__initblob(Unc_Allocator *alloc, Unc_Blob *s, Unc_Size n, const byte *b);
int unc__initblobraw(Unc_Allocator *alloc, Unc_Blob *s, Unc_Size n, byte **b);
int unc__initblobmove(Unc_Allocator *alloc, Unc_Blob *s, Unc_Size n, byte *b);
int unc__blobadd(Unc_Allocator *alloc, Unc_Blob *s, Unc_Size n, const byte *b);
int unc__blobaddb(Unc_Allocator *alloc, Unc_Blob *s, byte b);
int unc__blobaddf(Unc_Allocator *alloc, Unc_Blob *s, Unc_Blob *s2);
int unc__blobaddn(Unc_Allocator *alloc, Unc_Blob *s, Unc_Size n);
int unc__blobins(Unc_Allocator *alloc, Unc_Blob *s, Unc_Size i,
                                    Unc_Size n, const byte *b);
int unc__blobinsf(Unc_Allocator *alloc, Unc_Blob *s, Unc_Size i, Unc_Blob *s2);
int unc__blobinsn(Unc_Allocator *alloc, Unc_Blob *s, Unc_Size i, Unc_Size n);
int unc__blobdel(Unc_Allocator *alloc, Unc_Blob *s, Unc_Size i, Unc_Size n);
void unc__dropblob(Unc_Allocator *alloc, Unc_Blob *s);

/* the functions above do not lock, the ones below do */
int unc__blobeq(Unc_Blob *a, Unc_Blob *b);
int unc__blobeqr(Unc_Blob *a, Unc_Size n, const byte *b);
int unc__cmpblob(Unc_Blob *a, Unc_Blob *b);

int unc__blgetbyte(struct Unc_View *w, Unc_Blob *bl, Unc_Value *i,
                        int permissive, Unc_Value *v);
int unc__blsetbyte(struct Unc_View *w, Unc_Blob *bl, Unc_Value *i,
                        Unc_Value *v);

#endif /* UNCIL_UBLOB_H */
