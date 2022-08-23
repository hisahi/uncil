/*******************************************************************************
 
Uncil -- value header

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

#ifndef UNCIL_UVAL_H
#define UNCIL_UVAL_H

#include "udebug.h"
#include "udef.h"
#include "umem.h"
#include "umt.h"

typedef enum Unc_ValueType {
    /* value types */
    Unc_TNull           = 0,
    Unc_TBool           = 1,
    Unc_TInt            = 2,
    Unc_TFloat          = 3,
    Unc_TOpaquePtr      = 4,
    /* reference types */
    Unc_TString         = -1,
    Unc_TArray          = -2,
    Unc_TTable          = -3,
    Unc_TObject         = -4,
    Unc_TBlob           = -5,
    Unc_TFunction       = -6,
    Unc_TOpaque         = -7,
    Unc_TWeakRef        = -8,
    Unc_TBoundFunction  = -9,
    /* types below are not externally visible, internal use only */
    Unc_TRef            = -10
} Unc_ValueType;
typedef signed short Unc_ValueTypeSmall;

struct Unc_Entity;

typedef struct Unc_WeakCounter {
    struct Unc_Entity *entity;
} Unc_WeakCounter;

typedef struct Unc_Entity {
    Unc_AtomicLarge refs;
    Unc_ValueTypeSmall type;
    unsigned char mark; /* 0-127 for GC generations (cyclical)
                           128-255 means "dead" value */
    unsigned char creffed;
    unsigned vid;       /* owner view ID, used for creffed */
    Unc_WeakCounter *weaks;
    struct Unc_Entity *up, *down;
    /* only for alignment; does not actually exist in this form */
    Unc_MaxAlign _align;
} Unc_Entity;

typedef struct Unc_Value {
    Unc_ValueType type;
    union {
        void *p;
        const void *pc;
        Unc_Int i;
        Unc_Float f;
        Unc_Entity *c;
    } v;
} Unc_Value;

typedef struct Unc_ValueRef {
    Unc_Value v;
    UNC_LOCKLIGHT(lock)
} Unc_ValueRef;

struct Unc_View;

int unc__bind(Unc_Entity *e, struct Unc_View *w, Unc_Value *v);
Unc_Entity *unc__wake(struct Unc_View *w, Unc_ValueType type);
void unc__scrap(Unc_Entity *e, Unc_Allocator *alloc, struct Unc_View *w);
void unc__efree(Unc_Entity *e, Unc_Allocator *alloc);
void unc__hibernate(Unc_Entity *e, struct Unc_View *w);
void unc__unwake(Unc_Entity *e, struct Unc_View *w);
void unc__fetchweak(struct Unc_View *w, Unc_Value *wp, Unc_Value *dst);

/* these functions DO NOT lock! */
void unc__wreck(Unc_Entity *e, struct Unc_World *w);
int unc__makeweak(struct Unc_View *w, Unc_Value *from, Unc_Value *to);

#define UNCIL_OF_REFTYPE(V) (((V)->type) < 0)
#define UNCIL_GETENT(V) (V)->v.c
#define UNCIL_INCREFE(w, E) ATOMICLINC((E)->refs)
#define UNCIL_DECREFEX(w, E) ATOMICLDEC((E)->refs)
#define UNCIL_DECREFE(w, E) do { register Unc_Entity *tX_ = (E);               \
                            if (!UNCIL_DECREFEX(w, tX_))                       \
                                unc__hibernate(tX_, w); } while (0)
#define UNCIL_INCREF(w, V) do { if (UNCIL_OF_REFTYPE(V)) {                     \
                                UNCIL_INCREFE(w, UNCIL_GETENT(V)); } } while (0)
#define UNCIL_DECREF(w, V) do { if (UNCIL_OF_REFTYPE(V)) {                     \
                                UNCIL_DECREFE(w, UNCIL_GETENT(V)); } } while (0)

int unc__vrefnew(struct Unc_View *w, Unc_Value *v, Unc_ValueType type);

int unc__vcangetint(Unc_Value *v);
int unc__vcangetfloat(Unc_Value *v);

int unc__vcvt2bool(struct Unc_View *w, Unc_Value *v);
int unc__vgetint(struct Unc_View *w, Unc_Value *v, Unc_Int *out);
int unc__vgetfloat(struct Unc_View *w, Unc_Value *v, Unc_Float *out);

int unc__hashvalue(struct Unc_View *w, Unc_Value *v, unsigned *hash);

const char *unc__getvaluetypename(Unc_ValueType t);

/* uerr.c */
/* make sure type is a string literal */
int unc__makeexception(struct Unc_View *w, const char *type,
                       const char *msg, Unc_Value *out);
int unc__makeexceptiona(struct Unc_View *w, const char *type, Unc_Size msg_n,
                        byte *msg, Unc_Value *out);
void unc__makeexceptionoroom(struct Unc_View *w, Unc_Value *out,
                                const char *type, const char *msg);
void unc__makeexceptionaoroom(struct Unc_View *w, Unc_Value *out,
                                const char *type, Unc_Size msg_n, byte *msg);
void unc__makeexceptiontoroom(struct Unc_View *w, Unc_Value *out,
                                const char *type, Unc_Value *msg);
void unc__makeexceptionvoroom(struct Unc_View *w, Unc_Value *out,
                                Unc_Value *type, Unc_Value *msg);
void unc__errtoexcept(struct Unc_View *w, int e, Unc_Value *out);
int unc__throwexc(struct Unc_View *w, const char *type, const char *msg);

#ifdef UNCIL_DEFINES
#include "uvali.h"
#endif

#endif /* UNCIL_UVAL_H */
