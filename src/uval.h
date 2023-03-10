/*******************************************************************************
 
Uncil -- value header

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

int unc0_bind(Unc_Entity *e, struct Unc_View *w, Unc_Value *v);
Unc_Entity *unc0_wake(struct Unc_View *w, Unc_ValueType type);
void unc0_scrap(Unc_Entity *e, Unc_Allocator *alloc, struct Unc_View *w);
void unc0_efree(Unc_Entity *e, Unc_Allocator *alloc);
void unc0_hibernate(Unc_Entity *e, struct Unc_View *w);
void unc0_unwake(Unc_Entity *e, struct Unc_View *w);
void unc0_fetchweak(struct Unc_View *w, Unc_Value *wp, Unc_Value *dst);

/* these functions DO NOT lock! */
void unc0_wreck(Unc_Entity *e, struct Unc_World *w);
int unc0_makeweak(struct Unc_View *w, Unc_Value *from, Unc_Value *to);

#define UNCIL_OF_REFTYPE(V) (((V)->type) < 0)
#define UNCIL_GETENT(V) (V)->v.c
#define UNCIL_INCREFE(w, E) ATOMICLINC((E)->refs)
#define UNCIL_DECREFEX(w, E) ATOMICLDEC((E)->refs)
#define UNCIL_DECREFE(w, E) do { register Unc_Entity *tX_ = (E);               \
                            if (!UNCIL_DECREFEX(w, tX_))                       \
                                unc0_hibernate(tX_, w); } while (0)
#define UNCIL_INCREF(w, V) do { if (UNCIL_OF_REFTYPE(V)) {                     \
                                UNCIL_INCREFE(w, UNCIL_GETENT(V)); } } while (0)
#define UNCIL_DECREF(w, V) do { if (UNCIL_OF_REFTYPE(V)) {                     \
                                UNCIL_DECREFE(w, UNCIL_GETENT(V)); } } while (0)

int unc0_vrefnew(struct Unc_View *w, Unc_Value *v, Unc_ValueType type);

int unc0_vcangetint(Unc_Value *v);
int unc0_vcangetfloat(Unc_Value *v);

int unc0_vcvt2bool(struct Unc_View *w, Unc_Value *v);
int unc0_vgetint(struct Unc_View *w, Unc_Value *v, Unc_Int *out);
int unc0_vgetfloat(struct Unc_View *w, Unc_Value *v, Unc_Float *out);

int unc0_hashvalue(struct Unc_View *w, Unc_Value *v, unsigned *hash);

const char *unc0_getvaluetypename(Unc_ValueType t);

/* uerr.c */
/* make sure type is a string literal */
int unc0_makeexception(struct Unc_View *w, const char *type,
                       const char *msg, Unc_Value *out);
int unc0_makeexceptiona(struct Unc_View *w, const char *type, Unc_Size msg_n,
                        byte *msg, Unc_Value *out);
void unc0_makeexceptionoroom(struct Unc_View *w, Unc_Value *out,
                                const char *type, const char *msg);
void unc0_makeexceptionaoroom(struct Unc_View *w, Unc_Value *out,
                                const char *type, Unc_Size msg_n, byte *msg);
void unc0_makeexceptiontoroom(struct Unc_View *w, Unc_Value *out,
                                const char *type, Unc_Value *msg);
void unc0_makeexceptionvoroom(struct Unc_View *w, Unc_Value *out,
                                Unc_Value *type, Unc_Value *msg);
void unc0_errtoexcept(struct Unc_View *w, int e, Unc_Value *out);
int unc0_throwexc(struct Unc_View *w, const char *type, const char *msg);

#ifdef UNCIL_DEFINES
#include "uvali.h"
#endif

#endif /* UNCIL_UVAL_H */
