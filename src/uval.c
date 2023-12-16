/*******************************************************************************
 
Uncil -- value impl

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

#define UNCIL_DEFINES

#include "uarr.h"
#include "ublob.h"
#include "udebug.h"
#include "uerr.h"
#include "ufunc.h"
#include "uhash.h"
#include "umt.h"
#include "uncil.h"
#include "uobj.h"
#include "uopaque.h"
#include "ustr.h"
#include "uval.h"
#include "uvali.h"
#include "uvop.h"

#define SLEEPING UCHAR_MAX

static void unc0_link(Unc_Entity **top, Unc_Entity *e) {   
    e->up = NULL;
    if ((e->down = *top))
        e->down->up = e;
    *top = e;
}

static void unc0_unlink(Unc_Entity **top, Unc_Entity *e) {
    if (e->up)
        e->up->down = e->down;
    else
        *top = e->down;
    if (e->down)
        e->down->up = e->up;
}

static void unc0_relink(Unc_Entity **top, Unc_Entity *e) {
    if (*top != e) {
        ASSERT(*top);
        ASSERT(e->up);
        e->up->down = e->down;
        if (e->down) e->down->up = e->up;
        e->up = NULL;
        (*top)->up = e;
        e->down = *top;
        *top = e;
    }
}

static void unc0_unbind(Unc_Entity *e, Unc_View *w) {
    Unc_ValueRef *r = LEFTOVER(Unc_ValueRef, e);
    VDECREF(w, &r->v);
    UNC_LOCKFINAL(r->lock);
}

Unc_RetVal unc0_bind(Unc_Entity *e, Unc_View *w, Unc_Value *v) {
    Unc_ValueRef *r = LEFTOVER(Unc_ValueRef, e);
    Unc_RetVal err = UNC_LOCKINITL(r->lock) ? UNCIL_ERR_MEM : 0;
    if (!err) {
        e->type = Unc_TRef;
        if (v)
            VIMPOSE(w, &r->v, v);
        else
            VINITNULL(&r->v);
    }
    return err;
}

INLINE size_t entitysize(Unc_ValueType type) {
    switch (type) {
    case Unc_TRef:
        return ASIZEOF(Unc_Entity) + sizeof(Unc_ValueRef);
    case Unc_TString:
        return ASIZEOF(Unc_Entity) + sizeof(Unc_String);
    case Unc_TArray:
        return ASIZEOF(Unc_Entity) + sizeof(Unc_Array);
    case Unc_TTable:
        return ASIZEOF(Unc_Entity) + sizeof(Unc_Dict);
    case Unc_TObject:
        return ASIZEOF(Unc_Entity) + sizeof(Unc_Object);
    case Unc_TBlob:
        return ASIZEOF(Unc_Entity) + sizeof(Unc_Blob);
    case Unc_TFunction:
        return ASIZEOF(Unc_Entity) + sizeof(Unc_Function);
    case Unc_TOpaque:
        return ASIZEOF(Unc_Entity) + sizeof(Unc_Opaque);
    case Unc_TWeakRef:
        return ASIZEOF(Unc_Entity) + sizeof(Unc_WeakCounter);
    case Unc_TBoundFunction:
        return ASIZEOF(Unc_Entity) + sizeof(Unc_FunctionBound);
    default:
        NEVER_();
    }
    return 0;
}

void unc0_scrap(Unc_Entity *e, Unc_Allocator *alloc, Unc_View *w) {
    if (IS_SLEEPING(e)) return;
    if (w) {
        if (w->recurse >= w->recurselimit * 2) return;
        ++w->recurse;
    }
    e->mark = SLEEPING;
    if (e->weaks) {
        e->weaks->entity = NULL;
        e->weaks = NULL;
    }
    switch (e->type) {
    case Unc_TRef:
        UNC_LOCKFINAL(LEFTOVER(Unc_ValueRef, e)->lock);
        if (w)
            unc0_unbind(e, w), --w->recurse;
        break;
    case Unc_TString:
        if (w) --w->recurse;
        unc0_dropstring(alloc, LEFTOVER(Unc_String, e));
        break;
    case Unc_TBlob:
        if (w) --w->recurse;
        unc0_dropblob(alloc, LEFTOVER(Unc_Blob, e));
        break;
    case Unc_TArray:
        if (w)
            unc0_droparray(w, LEFTOVER(Unc_Array, e)), --w->recurse;
        else
            unc0_sunsetarray(alloc, LEFTOVER(Unc_Array, e));
        break;
    case Unc_TTable:
        if (w)
            unc0_dropdict(w, LEFTOVER(Unc_Dict, e)), --w->recurse;
        else
            unc0_sunsetdict(alloc, LEFTOVER(Unc_Dict, e));
        break;
    case Unc_TObject:
        if (w)
            unc0_dropobj(w, LEFTOVER(Unc_Object, e)), --w->recurse;
        else
            unc0_sunsetobj(alloc, LEFTOVER(Unc_Object, e));
        break;
    case Unc_TFunction:
        if (w)
            unc0_dropfunc(w, LEFTOVER(Unc_Function, e)), --w->recurse;
        else
            unc0_sunsetfunc(alloc, LEFTOVER(Unc_Function, e));
        break;
    case Unc_TOpaque:
        if (w)
            unc0_dropopaque(w, LEFTOVER(Unc_Opaque, e)), --w->recurse;
        else
            unc0_sunsetopaque(alloc, LEFTOVER(Unc_Opaque, e));
        break;
    case Unc_TWeakRef:
        if (w) {
            Unc_WeakCounter *c = LEFTOVER(Unc_WeakCounter, e);
            UNC_LOCKF(w->world->entity_lock);
            if (c->entity)
                c->entity->weaks = NULL;
            UNC_UNLOCKF(w->world->entity_lock);
            --w->recurse;
        }
        break;
    case Unc_TBoundFunction:
        if (w)
            unc0_dropbfunc(w, LEFTOVER(Unc_FunctionBound, e)), --w->recurse;
        else
            unc0_sunsetbfunc(alloc, LEFTOVER(Unc_FunctionBound, e));
        break;
    default:
        break;
    }
}

void unc0_efree(Unc_Entity *e, Unc_Allocator *alloc) {
    unc0_mfree(alloc, e, entitysize(e->type));
}

INLINE Unc_Entity *prepent(Unc_View *w, Unc_Entity *e) {
    if (w->cfunc) {
        e->creffed = 1;
        e->vid = w->vid;
    } else
        e->creffed = 0;
    return e;
}

static Unc_Entity *unc0_draft(Unc_View *w, Unc_ValueType type) {
    Unc_Entity *e;
    (void)UNC_LOCKFP(w, w->world->entity_lock);
    if (w->world->gc.enabled && ++w->entityload >= w->world->gc.entitylimit)
        unc0_gccollect(w->world, w);
    e = unc0_malloc(&w->world->alloc, Unc_AllocEntity, entitysize(type));
    if (e) {
        unc0_link(&w->world->etop, e);
        ATOMICLSET(e->refs, 0);
        e->type = type;
        e->mark = 0;
        e->weaks = NULL;
    }
    UNC_UNLOCKF(w->world->entity_lock);
    return e;
}

Unc_Entity *unc0_wake(struct Unc_View *w, Unc_ValueType type) {
#if !DEBUG_NO_SLEEPERS
    Unc_Entity **p = &w->sleepers[0], *e;
    int s = UNC_SLEEPER_VALUES;
    while (--s) {
        e = *p;
        if (e && e->type == type) {
            if (UNC_LOCKFP(w, w->world->entity_lock) && !*p) {
                /* GC happened during a pause */
                UNC_UNLOCKF(w->world->entity_lock);
                break;
            }
            ATOMICLSET(e->refs, 0);
            e->mark = 0;
            *p = NULL;
            unc0_relink(&w->world->etop, e);
            UNC_UNLOCKF(w->world->entity_lock);
            return prepent(w, e);
        }
        ++p;
    }
#endif
    return prepent(w, unc0_draft(w, type));
}

void unc0_wreck(Unc_Entity *e, struct Unc_World *w) {
    unc0_unlink(&w->etop, e);
    unc0_efree(e, &w->alloc);
}

void unc0_unwake(Unc_Entity *e, struct Unc_View *w) {
#if DEBUG_NO_SLEEPERS
    (void)UNC_LOCKFP(w, w->world->entity_lock);
    unc0_wreck(e, w->world);
    UNC_UNLOCKF(w->world->entity_lock);
#else
    int i = w->sleeper_next;
    if (w->sleepers[i]) {
        if (!UNC_LOCKFP(w, w->world->entity_lock) || w->sleepers[i]) {
            ASSERT(w->sleepers[i]->mark == SLEEPING);
            unc0_wreck(w->sleepers[i], w->world);
            --w->entityload;
        }
        UNC_UNLOCKF(w->world->entity_lock);
    }
    w->sleepers[i] = e;
    w->sleeper_next = (i + 1) % UNC_SLEEPER_VALUES;
    e->mark = SLEEPING;
#endif
}

void unc0_hibernate(Unc_Entity *e, Unc_View *w) {
    unc0_scrap(e, &w->world->alloc, w);
    unc0_unwake(e, w);
}

static const char * const unc0_valueTypeNames[] = {
    "null",
    "bool",
    "int",
    "float",
    "optr",
};

static const char * const unc0_valueRefTypeNames[] = {
    "string",
    "array",
    "table",
    "object",
    "blob",
    "function",
    "opaque",
    "weakref",
    "boundfunction"
};

const char *unc0_getvaluetypename(Unc_ValueType t) {
    int i = t;
    if (i < 0) {
        i = -i - 1;
        if (i < ARRAYSIZE(unc0_valueRefTypeNames))
            return unc0_valueRefTypeNames[i];
    } else {
        if (i < ARRAYSIZE(unc0_valueTypeNames))
            return unc0_valueTypeNames[i];
    }
    NEVER_();
    return "";
}

Unc_RetVal unc0_makeweak(Unc_View *w, Unc_Value *from, Unc_Value *to) {
    if (IS_OF_REFTYPE(from)) {
        Unc_Entity *e = VGETENT(from);
        Unc_Value res;
        (void)UNC_LOCKFP(w, w->world->entity_lock);
        res.type = Unc_TWeakRef;
        if (e->weaks) {
            res.v.c = UNLEFTOVER(e->weaks);
        } else {
            Unc_WeakCounter *wc;
            res.v.c = unc0_wake(w, Unc_TWeakRef);
            if (!res.v.c) {
                UNC_UNLOCKF(w->world->entity_lock);
                return UNCIL_ERR_MEM;
            }
            wc = LEFTOVER(Unc_WeakCounter, res.v.c);
            wc->entity = e;
            e->weaks = wc;
        }
        *to = res;
        UNC_UNLOCKF(w->world->entity_lock);
        return 0;
    } else
        return UNCIL_ERR_ARG_CANNOTWEAK;
}

void unc0_fetchweak(struct Unc_View *w, Unc_Value *wp, Unc_Value *dst) {
    Unc_Entity *e;
    ASSERT(wp->type == Unc_TWeakRef);
    (void)UNC_LOCKFP(w, w->world->entity_lock);
    e = LEFTOVER(Unc_WeakCounter, VGETENT(wp))->entity;
    UNC_UNLOCKF(w->world->entity_lock);
    if (!e) {
        VINITNULL(dst);
    } else {
        ASSERT(e->type != Unc_TRef);
        VINITENT(dst, e->type, e);
    }
}

Unc_RetVal unc0_vrefnew(Unc_View *w, Unc_Value *v, Unc_ValueType type) {
    Unc_Entity *e = unc0_wake(w, type);
    if (!e) return UNCIL_ERR_MEM;
    VINITENT(v, type, e);
    return 0;
}

Unc_RetVal unc0_hashvalue(Unc_View *w, Unc_Value *v, unsigned *hash) {
    switch (VGETTYPE(v)) {
    case Unc_TNull:
        *hash = 0;
        return 0;
    case Unc_TBool:
    case Unc_TInt:
        *hash = unc0_hashint(VGETINT(v));
        return 0;
    case Unc_TFloat:
        *hash = unc0_hashflt(VGETFLT(v));
        return 0;
    case Unc_TString:
    {
        Unc_String *s = LEFTOVER(Unc_String, VGETENT(v));
        *hash = unc0_hashstr(s->size, unc0_getstringdata(s));
        return 0;
    }
    case Unc_TObject:
    case Unc_TOpaque:
    /*      this is not safe right now because the rehashing algorithm
            simply cannot accept errors, and __hash could even cause UB
            if it tried to access a dict that is being rehashed
    {
        int e;
        Unc_Value vout;
        e = unc0_vovlunary(w, v, &vout, PASSSTRL(OPOVERLOAD(hash)));
        if (e) {
            int r;
            if (UNCIL_IS_ERR(e)) return e;
            if (w->recurse >= w->recurselimit)
                return UNCIL_ERR_TOODEEP;
            ++w->recurse;
            r = unc0_hashvalue(w, vout, hash);
            --w->recurse;
            unc0_decref(w, vout);
            return r;
        }
    }*/
    case Unc_TArray:
    case Unc_TTable:
    case Unc_TBlob:
    case Unc_TFunction:
    case Unc_TBoundFunction:
    case Unc_TOpaquePtr:
    case Unc_TWeakRef:
        return UNCIL_ERR_UNHASHABLE;
    default:
        NEVER();
    }
}

int unc0_vcangetint(Unc_Value *v) {
    switch (VGETTYPE(v)) {
    case Unc_TInt:
        return 1;
    default:
        return 0;
    }
}

int unc0_vcangetfloat(Unc_Value *v) {
    switch (VGETTYPE(v)) {
    case Unc_TInt:
    case Unc_TFloat:
        return 1;
    default:
        return 0;
    }
}

int unc0_vcvt2bool(Unc_View *w, Unc_Value *v) {
    switch (VGETTYPE(v)) {
    case Unc_TNull:
        return 0;
    case Unc_TBool:
        return VGETINT(v) != 0;
    case Unc_TInt:
        return VGETINT(v) != 0;
    case Unc_TFloat:
        return !!VGETFLT(v);
    case Unc_TString:
        return LEFTOVER(Unc_String, VGETENT(v))->size != 0;
    case Unc_TBlob:
        return LEFTOVER(Unc_Blob, VGETENT(v))->size != 0;
    case Unc_TArray:
        return LEFTOVER(Unc_Array, VGETENT(v))->size != 0;
    case Unc_TTable:
        return LEFTOVER(Unc_Dict, VGETENT(v))->data.entries != 0;
    case Unc_TObject:
    case Unc_TOpaque:
    {
        int e;
        Unc_Value vout;
        e = unc0_vovlunary(w, v, &vout, PASSSTRL(OPOVERLOAD(bool)));
        if (e) {
            int r;
            if (UNCIL_IS_ERR(e)) return e;
            if (w->recurse >= w->recurselimit)
                return UNCIL_ERR_TOODEEP;
            ++w->recurse;
            r = unc0_vcvt2bool(w, &vout);
            --w->recurse;
            VDECREF(w, &vout);
            return r;
        }
    }
    case Unc_TFunction:
    case Unc_TBoundFunction:
    case Unc_TOpaquePtr:
    case Unc_TWeakRef:
        return 1;
    default:
        NEVER_();
        return 0;
    }
}

Unc_RetVal unc0_vgetint(Unc_View *w, Unc_Value *v, Unc_Int *out) {
    switch (VGETTYPE(v)) {
    case Unc_TInt:
        *out = VGETINT(v);
        return 0;
    case Unc_TBool:
    case Unc_TFloat:
    case Unc_TNull:
    case Unc_TRef:
    case Unc_TString:
    case Unc_TArray:
    case Unc_TTable:
    case Unc_TObject:
    case Unc_TBlob:
    case Unc_TFunction:
    case Unc_TOpaque:
    case Unc_TOpaquePtr:
    case Unc_TWeakRef:
    case Unc_TBoundFunction:
        return UNCIL_ERR_CONVERT_TOINT;
    default:
        NEVER_();
        return UNCIL_ERR_CONVERT_TOINT;
    }
}

Unc_RetVal unc0_vgetfloat(Unc_View *w, Unc_Value *v, Unc_Float *out) {
    switch (VGETTYPE(v)) {
    case Unc_TInt:
        *out = (Unc_Float)VGETINT(v);
        return 0;
    case Unc_TFloat:
        *out = VGETFLT(v);
        return 0;
    case Unc_TBool:
    case Unc_TNull:
    case Unc_TRef:
    case Unc_TString:
    case Unc_TArray:
    case Unc_TTable:
    case Unc_TObject:
    case Unc_TBlob:
    case Unc_TFunction:
    case Unc_TOpaque:
    case Unc_TOpaquePtr:
    case Unc_TWeakRef:
    case Unc_TBoundFunction:
        return UNCIL_ERR_CONVERT_TOFLOAT;
    default:
        NEVER_();
        return UNCIL_ERR_CONVERT_TOFLOAT;
    }
}

void unc0_reversevalues(Unc_Value *v, size_t n) {
    Unc_Value *a = v, *b = v + n - 1;
    while (a < b) {
        Unc_Value tmp = *a;
        *a++ = *b;
        *b-- = tmp;
    }
}


Unc_RetVal unc0_verifyopaque(Unc_View *w, Unc_Value *v, Unc_Value *prototype,
                             Unc_Size *n, void **p, const char *emsg) {
    Unc_RetVal e = unc_verifyopaque(w, v, prototype, NULL, p);
    switch (e) {
    case UNCIL_ERR_ARG_FAILEDVERIFY:
    case UNCIL_ERR_TYPE_NOTOPAQUE:
        return unc_throwexc(w, "type", emsg);
    default:
        return e;
    }
}

#if DEBUGPRINT_REFS
Unc_AtomicLarge unc0_incref_debug(Unc_Entity *ent) {
    Unc_AtomicLarge p = ent->refs;
    Unc_AtomicLarge r = ATOMICLINC(ent->refs);
    DEBUGPRINT(REFS, ("INCREF %p (%lu => %lu) // ", (void *)ent,
                        (unsigned long)p, (unsigned long)ent->refs));
    return r;
}

Unc_AtomicLarge unc0_decref_debug(Unc_Entity *ent) {
    Unc_Size p = (Unc_Size)ent->refs;
    Unc_AtomicLarge r = ATOMICLDEC(ent->refs);
    DEBUGPRINT(REFS, ("DECREF %p (%lu => %lu) // ", (void *)ent,
                        (unsigned long)p, (unsigned long)ent->refs));
    return r;
}
#endif
