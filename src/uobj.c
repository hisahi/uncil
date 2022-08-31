/*******************************************************************************
 
Uncil -- dict & object impl

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

int unc0_initdict(Unc_View *w, Unc_Dict *o) {
    unc0_inithtblv(&w->world->alloc, &o->data);
    o->generation = 0;
    return UNC_LOCKINITL(o->lock);
}

int unc0_initobj(Unc_View *w, Unc_Object *o, Unc_Value *proto) {
    unc0_inithtblv(&w->world->alloc, &o->data);
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

int unc0_dgetindx(Unc_View *w, Unc_Dict *o,
                   Unc_Value *attr, int *found, Unc_Value *out) {
    Unc_Value *p;
    UNC_LOCKL(o->lock);
    p = unc0_gethtblv(w, &o->data, attr);
    *found = !!p;
    if (p) VCOPY(w, out, p);
    UNC_UNLOCKL(o->lock);
    return 0;
}

int unc0_dsetindx(Unc_View *w, Unc_Dict *o, Unc_Value *attr, Unc_Value *v) {
    Unc_Value *res;
    Unc_Size n;
    int e;
    UNC_LOCKL(o->lock);
    n = o->data.entries;
    e = unc0_puthtblv(w, &o->data, attr, &res);
    if (e) {
        UNC_UNLOCKL(o->lock);
        return e;
    }
    VCOPY(w, res, v);
    if (n != o->data.entries) nextgen(o);
    UNC_UNLOCKL(o->lock);
    return 0;
}

int unc0_ddelindx(Unc_View *w, Unc_Dict *o, Unc_Value *attr) {
    int e;
    UNC_LOCKL(o->lock);
    nextgen(o);
    e = unc0_delhtblv(w, &o->data, attr);
    UNC_UNLOCKL(o->lock);
    return e;
}

int unc0_dgetattrv(Unc_View *w, Unc_Dict *o,
                   Unc_Value *attr, int *found, Unc_Value *out) {
    return unc0_dgetindx(w, o, attr, found, out);
}

int unc0_dsetattrv(Unc_View *w, Unc_Dict *o,
                   Unc_Value *attr, Unc_Value *v) {
    return unc0_dsetindx(w, o, attr, v);
}

int unc0_ddelattrv(Unc_View *w, Unc_Dict *o, Unc_Value *attr) {
    return unc0_ddelindx(w, o, attr);
}

int unc0_dgetattrs(Unc_View *w, Unc_Dict *o,
                   size_t n, const byte *b, int *found, Unc_Value *out) {
    Unc_Value *p;
    UNC_LOCKL(o->lock);
    p = unc0_gethtblvs(w, &o->data, n, b);
    *found = !!p;
    if (p) VCOPY(w, out, p);
    UNC_UNLOCKL(o->lock);
    return 0;
}

int unc0_dsetattrs(Unc_View *w, Unc_Dict *o,
                   size_t n, const byte *b, Unc_Value *v) {
    Unc_Value *res;
    Unc_Size dn;
    int e;
    UNC_LOCKL(o->lock);
    dn = o->data.entries;
    e = unc0_puthtblvs(w, &o->data, n, b, &res);
    if (e) {
        UNC_UNLOCKL(o->lock);
        return e;
    }
    VCOPY(w, res, v);
    if (dn != o->data.entries) nextgen(o);
    UNC_UNLOCKL(o->lock);
    return 0;
}

int unc0_ddelattrs(Unc_View *w, Unc_Dict *o, size_t n, const byte *b) {
    int e;
    UNC_LOCKL(o->lock);
    nextgen(o);
    e = unc0_delhtblvs(w, &o->data, n, b);
    UNC_UNLOCKL(o->lock);
    return e;
}

int unc0_ogetattrv(Unc_View *w, Unc_Object *o,
                   Unc_Value *attr, int *found, Unc_Value *out) {
    Unc_Value *res;
    for (;;) {
        UNC_LOCKL(o->lock);
        res = unc0_gethtblv(w, &o->data, attr);
        if (res) {
            VCOPY(w, out, res);
            *found = 1;
            UNC_UNLOCKL(o->lock);
            return 0;
        }
        UNC_UNLOCKL(o->lock);
        switch (VGETTYPE(&o->prototype)) {
        case Unc_TTable:
            return unc0_dgetattrv(w, LEFTOVER(Unc_Dict, VGETENT(&o->prototype)),
                                  attr, found, out);
        case Unc_TObject:
            o = LEFTOVER(Unc_Object, VGETENT(&o->prototype));
            continue;
        case Unc_TOpaque:
        {
            Unc_Opaque *q = LEFTOVER(Unc_Opaque, VGETENT(&o->prototype));
            for (;;) {
                switch (VGETTYPE(&q->prototype)) {
                case Unc_TTable:
                    return unc0_dgetattrv(w,
                                LEFTOVER(Unc_Dict, VGETENT(&q->prototype)),
                                attr, found, out);
                case Unc_TObject:
                    o = LEFTOVER(Unc_Object, VGETENT(&q->prototype));
                    goto exitgetattrforv;
                case Unc_TOpaque:
                    q = LEFTOVER(Unc_Opaque, VGETENT(&q->prototype));
                    continue;
                default:
                    *found = 0;
                    return 0;
                }
            }
exitgetattrforv:
            continue;
        }
        default:
            break;
        }
        *found = 0;
        return 0;
    }
}

int unc0_ogetattrs(Unc_View *w, Unc_Object *o,
                   size_t n, const byte *b, int *found, Unc_Value *out) {
    Unc_Value *res;
    for (;;) {
        UNC_LOCKL(o->lock);
        res = unc0_gethtblvs(w, &o->data, n, b);
        if (res) {
            VCOPY(w, out, res);
            *found = 1;
            UNC_UNLOCKL(o->lock);
            return 0;
        }
        UNC_UNLOCKL(o->lock);
        /* proceed to prototype */
        switch (VGETTYPE(&o->prototype)) {
        case Unc_TTable:
            return unc0_dgetattrs(w, LEFTOVER(Unc_Dict, VGETENT(&o->prototype)),
                                  n, b, found, out);
        case Unc_TObject:
            o = LEFTOVER(Unc_Object, VGETENT(&o->prototype));
            continue;
        case Unc_TOpaque:
        {
            Unc_Opaque *q = LEFTOVER(Unc_Opaque, VGETENT(&o->prototype));
            for (;;) {
                switch (VGETTYPE(&q->prototype)) {
                case Unc_TTable:
                    return unc0_dgetattrs(w,
                                LEFTOVER(Unc_Dict, VGETENT(&q->prototype)),
                                  n, b, found, out);
                case Unc_TObject:
                    o = LEFTOVER(Unc_Object, VGETENT(&q->prototype));
                    goto exitgetattrfors;
                case Unc_TOpaque:
                    q = LEFTOVER(Unc_Opaque, VGETENT(&q->prototype));
                    continue;
                default:
                    *found = 0;
                    return 0;
                }
            }
exitgetattrfors:
            continue;
        }
        default:
            *found = 0;
            return 0;
        }
    }
}

static int unc0_ovgetattrs(Unc_View *w, Unc_Value *v,
                   size_t n, const byte *b, int *found, Unc_Value *out) {
unc0_ovgetattrs_again:
    switch (VGETTYPE(v)) {
    case Unc_TTable:
        return unc0_dgetattrs(w, LEFTOVER(Unc_Dict, VGETENT(v)),
                              n, b, found, out);
    case Unc_TObject:
        return unc0_ogetattrs(w, LEFTOVER(Unc_Object, VGETENT(v)),
                              n, b, found, out);
    case Unc_TOpaque:
        v = &LEFTOVER(Unc_Opaque, VGETENT(v))->prototype;
        goto unc0_ovgetattrs_again;
    default:
        *found = 0;
        return 0;
    }
}

int unc0_getprotomethod(Unc_View *w, Unc_Value *v,
                   size_t n, const byte *b, int *found, Unc_Value *out) {
    switch (VGETTYPE(v)) {
    case Unc_TObject:
        return unc0_ovgetattrs(w, &LEFTOVER(Unc_Object, VGETENT(v))->prototype,
                               n, b, found, out);
    case Unc_TOpaque:
        return unc0_ovgetattrs(w, &LEFTOVER(Unc_Opaque, VGETENT(v))->prototype,
                               n, b, found, out);
    default:
        *found = 0;
        return 0;
    }
}

int unc0_ogetattrc(struct Unc_View *w, Unc_Object *o,
                   const byte *s, int *found, Unc_Value *out) {
    return unc0_ogetattrs(w, o, strlen((const char *)s), s, found, out);
}

void unc0_ofreeze(struct Unc_View *w, Unc_Object *o) {
    UNC_LOCKL(o->lock);
    o->frozen = 1;
    UNC_UNLOCKL(o->lock);
}

int unc0_osetattrv(Unc_View *w, Unc_Object *o,
                   Unc_Value *attr, Unc_Value *v) {
    int e;
    UNC_LOCKL(o->lock);
    if (!o->frozen) {
        Unc_Value *res;
        e = unc0_puthtblv(w, &o->data, attr, &res);
        if (e) {
            UNC_UNLOCKL(o->lock);
            return e;
        }
        VCOPY(w, res, v);
    }
    UNC_UNLOCKL(o->lock);
    return 0;
}

int unc0_osetattrs(Unc_View *w, Unc_Object *o,
                   size_t n, const byte *b, Unc_Value *v) {
    int e;
    UNC_LOCKL(o->lock);
    if (!o->frozen) {
        Unc_Value *res;
        e = unc0_puthtblvs(w, &o->data, n, b, &res);
        if (e) {
            UNC_UNLOCKL(o->lock);
            return e;
        }
        VCOPY(w, res, v);
    }
    UNC_UNLOCKL(o->lock);
    return 0;
}

int unc0_odelattrv(Unc_View *w, Unc_Object *o, Unc_Value *attr) {
    int e;
    UNC_LOCKL(o->lock);
    e = o->frozen ? 0 : unc0_delhtblv(w, &o->data, attr);
    UNC_UNLOCKL(o->lock);
    return e;
}

int unc0_odelattrs(Unc_View *w, Unc_Object *o, size_t n, const byte *b) {
    int e;
    UNC_LOCKL(o->lock);
    e = o->frozen ? 0 : unc0_delhtblvs(w, &o->data, n, b);
    UNC_UNLOCKL(o->lock);
    return e;
}

int unc0_ogetindx(Unc_View *w, Unc_Object *o,
                   Unc_Value *attr, int *found, Unc_Value *out) {
    int e, fnf;
    Unc_Value fn = UNC_BLANK;
    e = unc0_ovgetattrs(w, &o->prototype, PASSSTRL(OPOVERLOAD(getindex)),
                        &fnf, &fn);
    if (e) return e;
    if (fnf) {
        size_t d = unc0_stackdepth(&w->sval);
        Unc_Pile ret;
        Unc_Value tmp;
        VINITENT(&tmp, Unc_TObject, UNLEFTOVER(o));
        if ((e = unc0_stackpushv(w, &w->sval, &tmp))) {
            VDECREF(w, &fn);
            return e;
        }
        if ((e = unc0_stackpush(w, &w->sval, 1, attr))) {
            VDECREF(w, &fn);
            unc0_restoredepth(w, &w->sval, d);
            return e;
        }
        if ((e = unc_newpile(w, &ret))) {
            VDECREF(w, &fn);
            unc0_restoredepth(w, &w->sval, d);
            return e;
        }
        w->region.base[ret.r] -= 2;
        e = unc0_fcallv(w, &fn, 2, 1, 1, 1, 0);
        if (!e)
            e = unc0_run(w);
        else if (!UNCIL_IS_ERR(e))
            e = 0;
        if (e) {
            VDECREF(w, &fn);
            return e;
        } else {
            Unc_Tuple tuple;
            unc_returnvalues(w, &ret, &tuple);
            if (tuple.count == 1) {
                w->tmpval = tuple.values[0];
                VMOVE(w, out, &w->tmpval);
                --w->sval.top;
                VDECREF(w, &fn);
                return 0;
            } else if (tuple.count > 1) {
                unc_discard(w, &ret);
                VDECREF(w, &fn);
                return UNCIL_ERR_LOGIC_OVLTOOMANY;
            } else {
                unc_discard(w, &ret);
                *found = 0;
                VDECREF(w, &fn);
                return 0;
            }
        }
    }
    return unc0_ogetattrv(w, o, attr, found, out);
}

int unc0_osetindx(Unc_View *w, Unc_Object *o,
                   Unc_Value *attr, Unc_Value *v) {
    int e, fnf;
    Unc_Value fn = UNC_BLANK;
    e = unc0_ovgetattrs(w, &o->prototype, PASSSTRL(OPOVERLOAD(setindex)),
                        &fnf, &fn);
    if (e) return e;
    if (fnf) {
        size_t d = unc0_stackdepth(&w->sval);
        Unc_Pile ret;
        Unc_Value tmp;
        VINITENT(&tmp, Unc_TObject, UNLEFTOVER(o));
        if ((e = unc0_stackpushv(w, &w->sval, &tmp))) {
            VDECREF(w, &fn);
            return e;
        }
        if ((e = unc0_stackpush(w, &w->sval, 1, attr))) {
            unc0_restoredepth(w, &w->sval, d);
            VDECREF(w, &fn);
            return e;
        }
        if ((e = unc_newpile(w, &ret))) {
            unc0_restoredepth(w, &w->sval, d);
            VDECREF(w, &fn);
            return e;
        }
        w->region.base[ret.r] -= 2;
        e = unc0_fcallv(w, &fn, 2, 1, 1, 1, 0);
        if (!e)
            e = unc0_run(w);
        else if (!UNCIL_IS_ERR(e))
            e = 0;
        if (!e)
            unc_discard(w, &ret);
        VDECREF(w, &fn);
        return e;
    }
    return unc0_osetattrv(w, o, attr, v);
}

int unc0_osetindxraw(Unc_View *w, Unc_Object *o,
                   Unc_Value *attr, Unc_Value *v) {
    return unc0_osetattrv(w, o, attr, v);
}

int unc0_odelindx(Unc_View *w, Unc_Object *o, Unc_Value *attr) {
    int e, fnf;
    Unc_Value fn = UNC_BLANK;
    e = unc0_ovgetattrs(w, &o->prototype, PASSSTRL(OPOVERLOAD(delindex)),
                        &fnf, &fn);
    if (e) return e;
    if (fnf) {
        size_t d = unc0_stackdepth(&w->sval);
        Unc_Pile ret;
        Unc_Value tmp;
        VINITENT(&tmp, Unc_TObject, UNLEFTOVER(o));
        if ((e = unc0_stackpushv(w, &w->sval, &tmp))) {
            VDECREF(w, &fn);
            return e;
        }
        if ((e = unc0_stackpush(w, &w->sval, 1, attr))) {
            unc0_restoredepth(w, &w->sval, d);
            VDECREF(w, &fn);
            return e;
        }
        if ((e = unc_newpile(w, &ret))) {
            unc0_restoredepth(w, &w->sval, d);
            VDECREF(w, &fn);
            return e;
        }
        w->region.base[ret.r] -= 2;
        e = unc0_fcallv(w, &fn, 2, 1, 1, 1, 0);
        if (!e)
            e = unc0_run(w);
        else if (!UNCIL_IS_ERR(e))
            e = 0;
        if (!e)
            unc_discard(w, &ret);
        VDECREF(w, &fn);
        return e;
    }
    return unc0_odelattrv(w, o, attr);
}

void unc0_dropdict(Unc_View *w, Unc_Dict *o) {
    UNC_LOCKFINAL(o->lock);
    unc0_drophtblv(w, &o->data);
}

void unc0_sunsetdict(Unc_Allocator *alloc, Unc_Dict *o) {
    unc0_sunsethtblv(alloc, &o->data);
}

void unc0_dropobj(Unc_View *w, Unc_Object *o) {
    UNC_LOCKFINAL(o->lock);
    unc0_drophtblv(w, &o->data);
    VDECREF(w, &o->prototype);
}

void unc0_sunsetobj(Unc_Allocator *alloc, Unc_Object *o) {
    unc0_sunsethtblv(alloc, &o->data);
}
