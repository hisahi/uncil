/*******************************************************************************
 
Uncil -- dict & object impl

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

#include <string.h>

#define UNCIL_DEFINES

#include "umt.h"
#include "uncil.h"
#include "ustr.h"
#include "uobj.h"
#include "uopaque.h"
#include "uval.h"
#include "uvali.h"
#include "uvm.h"
#include "uvop.h"

int unc__initdict(Unc_View *w, Unc_Dict *o) {
    unc__inithtblv(&w->world->alloc, &o->data);
    o->generation = 0;
    return UNC_LOCKINITL(o->lock);
}

int unc__initobj(Unc_View *w, Unc_Object *o, Unc_Value *proto) {
    unc__inithtblv(&w->world->alloc, &o->data);
    if (proto)
        VIMPOSE(w, &o->prototype, proto);
    else
        VINITNULL(&o->prototype);
    /* prototype cycles are impossible, you'd need to know the address
       of the new value */
    o->frozen = 0;
    return UNC_LOCKINITL(o->lock);
}

INLINE void nextgen(Unc_Dict *o) {
    if (++o->generation == UNC_INT_MAX) o->generation = 0;
}

int unc__dgetindx(Unc_View *w, Unc_Dict *o,
                   Unc_Value *attr, Unc_Value **out) {
    UNC_LOCKL(o->lock);
    *out = unc__gethtblv(w, &o->data, attr);
    UNC_UNLOCKL(o->lock);
    return 0;
}

int unc__dsetindx(Unc_View *w, Unc_Dict *o, Unc_Value *attr, Unc_Value *v) {
    Unc_Value *res;
    Unc_Size n;
    int e;
    UNC_LOCKL(o->lock);
    n = o->data.entries;
    e = unc__puthtblv(w, &o->data, attr, &res);
    if (e) {
        UNC_UNLOCKL(o->lock);
        return e;
    }
    VCOPY(w, res, v);
    if (n != o->data.entries) nextgen(o);
    UNC_UNLOCKL(o->lock);
    return 0;
}

int unc__ddelindx(Unc_View *w, Unc_Dict *o, Unc_Value *attr) {
    int e;
    UNC_LOCKL(o->lock);
    nextgen(o);
    e = unc__delhtblv(w, &o->data, attr);
    UNC_UNLOCKL(o->lock);
    return e;
}

int unc__dgetattrv(Unc_View *w, Unc_Dict *o,
                   Unc_Value *attr, Unc_Value **out) {
    return unc__dgetindx(w, o, attr, out);
}

int unc__dsetattrv(Unc_View *w, Unc_Dict *o,
                   Unc_Value *attr, Unc_Value *v) {
    return unc__dsetindx(w, o, attr, v);
}

int unc__ddelattrv(Unc_View *w, Unc_Dict *o, Unc_Value *attr) {
    return unc__ddelindx(w, o, attr);
}

int unc__dgetattrs(Unc_View *w, Unc_Dict *o,
                   size_t n, const byte *b, Unc_Value **out) {
    UNC_LOCKL(o->lock);
    *out = unc__gethtblvs(w, &o->data, n, b);
    UNC_UNLOCKL(o->lock);
    return 0;
}

int unc__dsetattrs(Unc_View *w, Unc_Dict *o,
                   size_t n, const byte *b, Unc_Value *v) {
    Unc_Value *res;
    Unc_Size dn;
    int e;
    UNC_LOCKL(o->lock);
    dn = o->data.entries;
    e = unc__puthtblvs(w, &o->data, n, b, &res);
    if (e) {
        UNC_UNLOCKL(o->lock);
        return e;
    }
    VCOPY(w, res, v);
    if (dn != o->data.entries) nextgen(o);
    UNC_UNLOCKL(o->lock);
    return 0;
}

int unc__ddelattrs(Unc_View *w, Unc_Dict *o, size_t n, const byte *b) {
    int e;
    UNC_LOCKL(o->lock);
    nextgen(o);
    e = unc__delhtblvs(w, &o->data, n, b);
    UNC_UNLOCKL(o->lock);
    return e;
}

int unc__ogetattrv(Unc_View *w, Unc_Object *o,
                   Unc_Value *attr, Unc_Value **out) {
    Unc_Value *res;
    for (;;) {
        UNC_LOCKL(o->lock);
        res = unc__gethtblv(w, &o->data, attr);
        if (res) {
            *out = res;
            UNC_UNLOCKL(o->lock);
            return 0;
        }
        UNC_UNLOCKL(o->lock);
        switch (VGETTYPE(&o->prototype)) {
        case Unc_TTable:
            return unc__dgetattrv(w, LEFTOVER(Unc_Dict, VGETENT(&o->prototype)),
                                  attr, out);
        case Unc_TObject:
            o = LEFTOVER(Unc_Object, VGETENT(&o->prototype));
            continue;
        case Unc_TOpaque:
        {
            Unc_Opaque *q = LEFTOVER(Unc_Opaque, VGETENT(&o->prototype));
            for (;;) {
                switch (VGETTYPE(&q->prototype)) {
                case Unc_TTable:
                    return unc__dgetattrv(w,
                                LEFTOVER(Unc_Dict, VGETENT(&q->prototype)),
                                attr, out);
                case Unc_TObject:
                    o = LEFTOVER(Unc_Object, VGETENT(&q->prototype));
                    goto exitgetattrforv;
                case Unc_TOpaque:
                    q = LEFTOVER(Unc_Opaque, VGETENT(&q->prototype));
                    continue;
                default:
                    return 0;
                }
            }
exitgetattrforv:
            continue;
        }
        default:
            break;
        }
        return 0;
    }
}

int unc__ogetattrs(Unc_View *w, Unc_Object *o,
                   size_t n, const byte *b, Unc_Value **out) {
    Unc_Value *res;
    for (;;) {
        UNC_LOCKL(o->lock);
        res = unc__gethtblvs(w, &o->data, n, b);
        if (res) {
            *out = res;
            UNC_UNLOCKL(o->lock);
            return 0;
        }
        UNC_UNLOCKL(o->lock);
        /* proceed to prototype */
        switch (VGETTYPE(&o->prototype)) {
        case Unc_TTable:
            return unc__dgetattrs(w, LEFTOVER(Unc_Dict, VGETENT(&o->prototype)),
                                  n, b, out);
        case Unc_TObject:
            o = LEFTOVER(Unc_Object, VGETENT(&o->prototype));
            continue;
        case Unc_TOpaque:
        {
            Unc_Opaque *q = LEFTOVER(Unc_Opaque, VGETENT(&o->prototype));
            for (;;) {
                switch (VGETTYPE(&q->prototype)) {
                case Unc_TTable:
                    return unc__dgetattrs(w,
                                LEFTOVER(Unc_Dict, VGETENT(&q->prototype)),
                                  n, b, out);
                case Unc_TObject:
                    o = LEFTOVER(Unc_Object, VGETENT(&q->prototype));
                    goto exitgetattrfors;
                case Unc_TOpaque:
                    q = LEFTOVER(Unc_Opaque, VGETENT(&q->prototype));
                    continue;
                default:
                    *out = NULL;
                    return 0;
                }
            }
exitgetattrfors:
            continue;
        }
        default:
            *out = NULL;
            return 0;
        }
    }
}

static int unc__ovgetattrs(Unc_View *w, Unc_Value *v,
                   size_t n, const byte *b, Unc_Value **out) {
unc__ovgetattrs_again:
    switch (VGETTYPE(v)) {
    case Unc_TTable:
        return unc__dgetattrs(w, LEFTOVER(Unc_Dict, VGETENT(v)), n, b, out);
    case Unc_TObject:
        return unc__ogetattrs(w, LEFTOVER(Unc_Object, VGETENT(v)), n, b, out);
    case Unc_TOpaque:
        v = &LEFTOVER(Unc_Opaque, VGETENT(v))->prototype;
        goto unc__ovgetattrs_again;
    default:
        *out = NULL;
        return 0;
    }
}

int unc__getprotomethod(Unc_View *w, Unc_Value *v,
                   size_t n, const byte *b, Unc_Value **out) {
    switch (VGETTYPE(v)) {
    case Unc_TObject:
        return unc__ovgetattrs(w, &LEFTOVER(Unc_Object, VGETENT(v))->prototype,
                               n, b, out);
    case Unc_TOpaque:
        return unc__ovgetattrs(w, &LEFTOVER(Unc_Opaque, VGETENT(v))->prototype,
                               n, b, out);
    default:
        *out = NULL;
        return 0;
    }
}

int unc__ogetattrc(struct Unc_View *w, Unc_Object *o,
                   const byte *s, Unc_Value **out) {
    return unc__ogetattrs(w, o, strlen((const char *)s), s, out);
}

void unc__ofreeze(struct Unc_View *w, Unc_Object *o) {
    UNC_LOCKL(o->lock);
    o->frozen = 1;
    UNC_UNLOCKL(o->lock);
}

int unc__osetattrv(Unc_View *w, Unc_Object *o,
                   Unc_Value *attr, Unc_Value *v) {
    int e;
    UNC_LOCKL(o->lock);
    if (!o->frozen) {
        Unc_Value *res;
        e = unc__puthtblv(w, &o->data, attr, &res);
        if (e) {
            UNC_UNLOCKL(o->lock);
            return e;
        }
        VCOPY(w, res, v);
    }
    UNC_UNLOCKL(o->lock);
    return 0;
}

int unc__osetattrs(Unc_View *w, Unc_Object *o,
                   size_t n, const byte *b, Unc_Value *v) {
    int e;
    UNC_LOCKL(o->lock);
    if (!o->frozen) {
        Unc_Value *res;
        e = unc__puthtblvs(w, &o->data, n, b, &res);
        if (e) {
            UNC_UNLOCKL(o->lock);
            return e;
        }
        VCOPY(w, res, v);
    }
    UNC_UNLOCKL(o->lock);
    return 0;
}

int unc__odelattrv(Unc_View *w, Unc_Object *o, Unc_Value *attr) {
    int e;
    UNC_LOCKL(o->lock);
    e = o->frozen ? 0 : unc__delhtblv(w, &o->data, attr);
    UNC_UNLOCKL(o->lock);
    return e;
}

int unc__odelattrs(Unc_View *w, Unc_Object *o, size_t n, const byte *b) {
    int e;
    UNC_LOCKL(o->lock);
    e = o->frozen ? 0 : unc__delhtblvs(w, &o->data, n, b);
    UNC_UNLOCKL(o->lock);
    return e;
}

int unc__ogetindx(Unc_View *w, Unc_Object *o,
                   Unc_Value *attr, Unc_Value **out) {
    int e;
    Unc_Value *fn;
    e = unc__ovgetattrs(w, &o->prototype, PASSSTRL(OPOVERLOAD(getindex)), &fn);
    if (e) return e;
    if (fn) {
        size_t d = unc__stackdepth(&w->sval);
        Unc_Pile ret;
        Unc_Value tmp;
        VINITENT(&tmp, Unc_TObject, UNLEFTOVER(o));
        if ((e = unc__stackpushv(w, &w->sval, &tmp))) {
            return e;
        }
        if ((e = unc__stackpush(w, &w->sval, 1, attr))) {
            unc__restoredepth(w, &w->sval, d);
            return e;
        }
        if ((e = unc_newpile(w, &ret))) {
            unc__restoredepth(w, &w->sval, d);
            return e;
        }
        w->region.base[ret.r] -= 2;
        e = unc__fcallv(w, fn, 2, 1, 1, 1, 0);
        if (!e)
            e = unc__run(w);
        else if (!UNCIL_IS_ERR(e))
            e = 0;
        if (e) {
            return e;
        } else {
            Unc_Tuple tuple;
            unc_returnvalues(w, &ret, &tuple);
            if (tuple.count == 1) {
                w->tmpval = tuple.values[0];
                *out = &w->tmpval;
                --w->sval.top;
                /* decrease reference without killing; the assumption is that
                   this will be assigned again anyway. if not, GC will clean
                   it up */
                if (UNCIL_OF_REFTYPE(&w->tmpval))
                    UNCIL_DECREFEX(w, VGETENT(&w->tmpval));
                return 0;
            } else if (tuple.count > 1) {
                unc_discard(w, &ret);
                return UNCIL_ERR_LOGIC_OVLTOOMANY;
            } else {
                unc_discard(w, &ret);
                *out = NULL;
                return 0;
            }
        }
    }
    return unc__ogetattrv(w, o, attr, out);
}

int unc__osetindx(Unc_View *w, Unc_Object *o,
                   Unc_Value *attr, Unc_Value *v) {
    int e;
    Unc_Value *fn;
    e = unc__ovgetattrs(w, &o->prototype, PASSSTRL(OPOVERLOAD(setindex)), &fn);
    if (e) return e;
    if (fn) {
        size_t d = unc__stackdepth(&w->sval);
        Unc_Pile ret;
        Unc_Value tmp;
        VINITENT(&tmp, Unc_TObject, UNLEFTOVER(o));
        if ((e = unc__stackpushv(w, &w->sval, &tmp))) {
            return e;
        }
        if ((e = unc__stackpush(w, &w->sval, 1, attr))) {
            unc__restoredepth(w, &w->sval, d);
            return e;
        }
        if ((e = unc_newpile(w, &ret))) {
            unc__restoredepth(w, &w->sval, d);
            return e;
        }
        w->region.base[ret.r] -= 2;
        e = unc__fcallv(w, fn, 2, 1, 1, 1, 0);
        if (!e)
            e = unc__run(w);
        else if (!UNCIL_IS_ERR(e))
            e = 0;
        if (!e)
            unc_discard(w, &ret);
        return e;
    }
    return unc__osetattrv(w, o, attr, v);
}

int unc__osetindxraw(Unc_View *w, Unc_Object *o,
                   Unc_Value *attr, Unc_Value *v) {
    return unc__osetattrv(w, o, attr, v);
}

int unc__odelindx(Unc_View *w, Unc_Object *o, Unc_Value *attr) {
    int e;
    Unc_Value *fn;
    e = unc__ovgetattrs(w, &o->prototype, PASSSTRL(OPOVERLOAD(delindex)), &fn);
    if (e) return e;
    if (fn) {
        size_t d = unc__stackdepth(&w->sval);
        Unc_Pile ret;
        Unc_Value tmp;
        VINITENT(&tmp, Unc_TObject, UNLEFTOVER(o));
        if ((e = unc__stackpushv(w, &w->sval, &tmp))) {
            return e;
        }
        if ((e = unc__stackpush(w, &w->sval, 1, attr))) {
            unc__restoredepth(w, &w->sval, d);
            return e;
        }
        if ((e = unc_newpile(w, &ret))) {
            unc__restoredepth(w, &w->sval, d);
            return e;
        }
        w->region.base[ret.r] -= 2;
        e = unc__fcallv(w, fn, 2, 1, 1, 1, 0);
        if (!e)
            e = unc__run(w);
        else if (!UNCIL_IS_ERR(e))
            e = 0;
        if (!e)
            unc_discard(w, &ret);
        return e;
    }
    return unc__odelattrv(w, o, attr);
}

void unc__dropdict(Unc_View *w, Unc_Dict *o) {
    UNC_LOCKFINAL(o->lock);
    unc__drophtblv(w, &o->data);
}

void unc__sunsetdict(Unc_Allocator *alloc, Unc_Dict *o) {
    unc__sunsethtblv(alloc, &o->data);
}

void unc__dropobj(Unc_View *w, Unc_Object *o) {
    UNC_LOCKFINAL(o->lock);
    unc__drophtblv(w, &o->data);
    VDECREF(w, &o->prototype);
}

void unc__sunsetobj(Unc_Allocator *alloc, Unc_Object *o) {
    unc__sunsethtblv(alloc, &o->data);
}
