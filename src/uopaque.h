/*******************************************************************************
 
Uncil -- opaque header

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

#ifndef UNCIL_UOPAQUE_H
#define UNCIL_UOPAQUE_H

#include "udef.h"
#include "uhash.h"
#include "umem.h"
#include "umt.h"

struct Unc_Entity;
struct Unc_View;

typedef Unc_RetVal (*Unc_OpaqueDestructor)(struct Unc_View *, size_t, void *);

typedef struct Unc_Opaque {
    Unc_Value prototype;
    size_t size;
    void *data;
    Unc_OpaqueDestructor destructor;
    Unc_Size refc;
    struct Unc_Entity **refs;
    UNC_LOCKLIGHT(lock)
} Unc_Opaque;

Unc_RetVal unc0_initopaque(struct Unc_View *w, Unc_Opaque *o, size_t n,
                           void **data, Unc_Value *prototype,
                           Unc_OpaqueDestructor destructor,
                           Unc_Size refcount, Unc_Value *initvalues,
                           Unc_Size refcopycount, Unc_Size *refcopies);

Unc_RetVal unc0_oqgetattrv(struct Unc_View *w, Unc_Opaque *o,
                           Unc_Value *attr, int *found, Unc_Value *out);
Unc_RetVal unc0_oqgetattrs(struct Unc_View *w, Unc_Opaque *o,
                           Unc_Size n, const byte *b,
                           int *found, Unc_Value *out);
Unc_RetVal unc0_oqgetattrc(struct Unc_View *w, Unc_Opaque *o,
                           const byte *s, int *found, Unc_Value *out);

void unc0_graceopaque(struct Unc_View *w, Unc_Opaque *o);
void unc0_dropopaque(struct Unc_View *w, Unc_Opaque *o);
void unc0_sunsetopaque(Unc_Allocator *alloc, Unc_Opaque *o);

#endif /* UNCIL_UOPAQUE_H */
