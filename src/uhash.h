/*******************************************************************************
 
Uncil -- hash set & table headers

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

#ifndef UNCIL_UHASH_H
#define UNCIL_UHASH_H

#include "udef.h"
#include "umem.h"
#include "uval.h"

typedef struct Unc_HSet_V {
    struct Unc_HSet_V *next;
    Unc_Size index;
    Unc_Size size;
    unsigned char val[1];
} Unc_HSet_V;

typedef struct Unc_HSet {
    Unc_Allocator *alloc;
    Unc_Size entries;
    Unc_Size capacity;
    Unc_HSet_V **buckets;
} Unc_HSet;

typedef struct Unc_HTblV_V {
    struct Unc_HTblV_V *next;
    Unc_Value key;
    Unc_Value val;
} Unc_HTblV_V;

typedef struct Unc_HTblV {
    Unc_Size entries;
    Unc_Size capacity;
    Unc_HTblV_V **buckets;
} Unc_HTblV;

typedef struct Unc_HTblS_V {
    struct Unc_HTblS_V *next;
    Unc_Size key_n;
    Unc_Value val;
} Unc_HTblS_V;

typedef struct Unc_HTblS {
    Unc_Size entries;
    Unc_Size capacity;
    Unc_HTblS_V **buckets;
} Unc_HTblS;

unsigned unc0_hashint(Unc_Int i);
unsigned unc0_hashflt(Unc_Float f);
unsigned unc0_hashptr(const void *p);
unsigned unc0_hashstr(Unc_Size n, const byte *s);

Unc_HSet *unc0_newhset(Unc_Allocator *alloc);
void unc0_inithset(Unc_HSet *hset, Unc_Allocator *alloc);
Unc_RetVal unc0_puthset(Unc_HSet *hset, Unc_Size n, const byte *s,
                        Unc_Size *out, Unc_Size submit);
void unc0_drophset(Unc_HSet *hset);
void unc0_freehset(Unc_HSet *hset);

struct Unc_View;

Unc_HTblS *unc0_newhtbls(Unc_Allocator *alloc);
void unc0_inithtbls(Unc_Allocator *alloc, Unc_HTblS *h);
Unc_Value *unc0_gethtbls(struct Unc_View *w, Unc_HTblS *h,
                         Unc_Size n, const byte *s);
Unc_RetVal unc0_puthtbls(struct Unc_View *w, Unc_HTblS *h, 
                         Unc_Size n, const byte *s, Unc_Value **out);
Unc_RetVal unc0_delhtbls(struct Unc_View *w, Unc_HTblS *h,
                         Unc_Size n, const byte *s);
void unc0_compacthtbls(struct Unc_View *w, Unc_HTblS *h);
void unc0_drophtbls(struct Unc_View *w, Unc_HTblS *h);
void unc0_sunsethtbls(Unc_Allocator *alloc, Unc_HTblS *h);
void unc0_freehtbls(struct Unc_View *w, Unc_HTblS *h);

Unc_HTblV *unc0_newhtblv(Unc_Allocator *alloc);
void unc0_inithtblv(Unc_Allocator *alloc, Unc_HTblV *h);
Unc_Value *unc0_gethtblv(struct Unc_View *w, Unc_HTblV *h, Unc_Value *key);
Unc_RetVal unc0_puthtblv(struct Unc_View *w, Unc_HTblV *h,
                         Unc_Value *key, Unc_Value **out);
Unc_RetVal unc0_delhtblv(struct Unc_View *w, Unc_HTblV *h, Unc_Value *key);
void unc0_compacthtblv(struct Unc_View *w, Unc_HTblV *h);
Unc_Value *unc0_gethtblvs(struct Unc_View *w, Unc_HTblV *h,
                          Unc_Size n, const byte *s);
Unc_RetVal unc0_puthtblvs(struct Unc_View *w, Unc_HTblV *h,
                          Unc_Size n, const byte *s, Unc_Value **out);
Unc_RetVal unc0_delhtblvs(struct Unc_View *w, Unc_HTblV *h,
                          Unc_Size n, const byte *s);
void unc0_drophtblv(struct Unc_View *w, Unc_HTblV *h);
void unc0_sunsethtblv(Unc_Allocator *alloc, Unc_HTblV *h);
void unc0_freehtblv(struct Unc_View *w, Unc_HTblV *h);

#endif /* UNCIL_UHASH_H */
