/*******************************************************************************
 
Uncil -- dict & object header

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

#ifndef UNCIL_UOBJ_H
#define UNCIL_UOBJ_H

#include "udef.h"
#include "uhash.h"
#include "umem.h"
#include "umt.h"

typedef struct Unc_Dict {
    Unc_HTblV data;
    Unc_Size generation;
    UNC_LOCKLIGHT(lock)
} Unc_Dict;

typedef struct Unc_Object {
    Unc_HTblV data;
    Unc_Value prototype;
    int frozen;
    UNC_LOCKLIGHT(lock)
} Unc_Object;

struct Unc_View;

int unc0_initdict(struct Unc_View *w, Unc_Dict *o);
int unc0_initobj(struct Unc_View *w, Unc_Object *o, Unc_Value *proto);

int unc0_dgetattrv(struct Unc_View *w, Unc_Dict *o,
                   Unc_Value *attr, Unc_Value **out);
int unc0_dgetattrs(struct Unc_View *w, Unc_Dict *o,
                   Unc_Size n, const byte *b, Unc_Value **out);
int unc0_dsetattrv(struct Unc_View *w, Unc_Dict *o,
                   Unc_Value *attr, Unc_Value *v);
int unc0_dsetattrs(struct Unc_View *w, Unc_Dict *o,
                   Unc_Size n, const byte *b, Unc_Value *v);
int unc0_ddelattrv(struct Unc_View *w, Unc_Dict *o, Unc_Value *attr);
int unc0_ddelattrs(struct Unc_View *w, Unc_Dict *o, Unc_Size n, const byte *b);
int unc0_dgetindx(struct Unc_View *w, Unc_Dict *o,
                   Unc_Value *attr, Unc_Value **out);
int unc0_dsetindx(struct Unc_View *w, Unc_Dict *o,
                   Unc_Value *attr, Unc_Value *v);
int unc0_ddelindx(struct Unc_View *w, Unc_Dict *o, Unc_Value *attr);

int unc0_ogetattrv(struct Unc_View *w, Unc_Object *o,
                   Unc_Value *attr, Unc_Value **out);
int unc0_ogetattrs(struct Unc_View *w, Unc_Object *o,
                   Unc_Size n, const byte *b, Unc_Value **out);
int unc0_ogetattrc(struct Unc_View *w, Unc_Object *o,
                   const byte *s, Unc_Value **out);
int unc0_osetattrv(struct Unc_View *w, Unc_Object *o,
                   Unc_Value *attr, Unc_Value *v);
int unc0_osetattrs(struct Unc_View *w, Unc_Object *o,
                   Unc_Size n, const byte *b, Unc_Value *v);
int unc0_odelattrv(struct Unc_View *w, Unc_Object *o, Unc_Value *attr);
int unc0_odelattrs(struct Unc_View *w, Unc_Object *o, Unc_Size n, const byte *b);
int unc0_ogetindx(struct Unc_View *w, Unc_Object *o,
                   Unc_Value *attr, Unc_Value **out);
int unc0_osetindx(struct Unc_View *w, Unc_Object *o,
                   Unc_Value *attr, Unc_Value *v);
int unc0_osetindxraw(struct Unc_View *w, Unc_Object *o,
                   Unc_Value *attr, Unc_Value *v);
int unc0_odelindx(struct Unc_View *w, Unc_Object *o, Unc_Value *attr);

void unc0_ofreeze(struct Unc_View *w, Unc_Object *o);

int unc0_getprotomethod(struct Unc_View *w, Unc_Value *v,
                   Unc_Size n, const byte *b, Unc_Value **out);

void unc0_dropdict(struct Unc_View *w, Unc_Dict *o);
void unc0_dropobj(struct Unc_View *w, Unc_Object *o);

void unc0_sunsetdict(Unc_Allocator *alloc, Unc_Dict *o);
void unc0_sunsetobj(Unc_Allocator *alloc, Unc_Object *o);

#endif /* UNCIL_UOBJ_H */
