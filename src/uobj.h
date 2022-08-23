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

int unc__initdict(struct Unc_View *w, Unc_Dict *o);
int unc__initobj(struct Unc_View *w, Unc_Object *o, Unc_Value *proto);

int unc__dgetattrv(struct Unc_View *w, Unc_Dict *o,
                   Unc_Value *attr, Unc_Value **out);
int unc__dgetattrs(struct Unc_View *w, Unc_Dict *o,
                   Unc_Size n, const byte *b, Unc_Value **out);
int unc__dsetattrv(struct Unc_View *w, Unc_Dict *o,
                   Unc_Value *attr, Unc_Value *v);
int unc__dsetattrs(struct Unc_View *w, Unc_Dict *o,
                   Unc_Size n, const byte *b, Unc_Value *v);
int unc__ddelattrv(struct Unc_View *w, Unc_Dict *o, Unc_Value *attr);
int unc__ddelattrs(struct Unc_View *w, Unc_Dict *o, Unc_Size n, const byte *b);
int unc__dgetindx(struct Unc_View *w, Unc_Dict *o,
                   Unc_Value *attr, Unc_Value **out);
int unc__dsetindx(struct Unc_View *w, Unc_Dict *o,
                   Unc_Value *attr, Unc_Value *v);
int unc__ddelindx(struct Unc_View *w, Unc_Dict *o, Unc_Value *attr);

int unc__ogetattrv(struct Unc_View *w, Unc_Object *o,
                   Unc_Value *attr, Unc_Value **out);
int unc__ogetattrs(struct Unc_View *w, Unc_Object *o,
                   Unc_Size n, const byte *b, Unc_Value **out);
int unc__ogetattrc(struct Unc_View *w, Unc_Object *o,
                   const byte *s, Unc_Value **out);
int unc__osetattrv(struct Unc_View *w, Unc_Object *o,
                   Unc_Value *attr, Unc_Value *v);
int unc__osetattrs(struct Unc_View *w, Unc_Object *o,
                   Unc_Size n, const byte *b, Unc_Value *v);
int unc__odelattrv(struct Unc_View *w, Unc_Object *o, Unc_Value *attr);
int unc__odelattrs(struct Unc_View *w, Unc_Object *o, Unc_Size n, const byte *b);
int unc__ogetindx(struct Unc_View *w, Unc_Object *o,
                   Unc_Value *attr, Unc_Value **out);
int unc__osetindx(struct Unc_View *w, Unc_Object *o,
                   Unc_Value *attr, Unc_Value *v);
int unc__osetindxraw(struct Unc_View *w, Unc_Object *o,
                   Unc_Value *attr, Unc_Value *v);
int unc__odelindx(struct Unc_View *w, Unc_Object *o, Unc_Value *attr);

void unc__ofreeze(struct Unc_View *w, Unc_Object *o);

int unc__getprotomethod(struct Unc_View *w, Unc_Value *v,
                   Unc_Size n, const byte *b, Unc_Value **out);

void unc__dropdict(struct Unc_View *w, Unc_Dict *o);
void unc__dropobj(struct Unc_View *w, Unc_Object *o);

void unc__sunsetdict(Unc_Allocator *alloc, Unc_Dict *o);
void unc__sunsetobj(Unc_Allocator *alloc, Unc_Object *o);

#endif /* UNCIL_UOBJ_H */
