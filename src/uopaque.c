/*******************************************************************************
 
Uncil -- opaque impl

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

#include <string.h>

#define UNCIL_DEFINES

#include "udef.h"
#include "uhash.h"
#include "umem.h"
#include "uncil.h"
#include "uobj.h"
#include "uopaque.h"
#include "uvali.h"

int unc0_initopaque(Unc_View *w, Unc_Opaque *o, size_t n, void **data,
                    Unc_Value *prototype, Unc_OpaqueDestructor destructor,
                    Unc_Size refcount, Unc_Value *initvalues,
                    Unc_Size refcopycount, Unc_Size *refcopies) {
    int e;
    Unc_Allocator *alloc = &w->world->alloc;
    o->data = unc0_malloc(alloc, Unc_AllocOpaque, n);
    if (!o->data)
        return UNCIL_ERR_MEM;
    o->size = n;
    *data = o->data;
    o->refc = refcount + refcopycount;
    if (o->refc) {
        Unc_Size i, bc = w->boundcount;
        e = (o->refs = TMALLOC(Unc_Entity *, alloc, 0, o->refc)) != NULL
                ? 0 : UNCIL_ERR_MEM;
        
        if (!e) {
            for (i = 0; i < refcount; ++i) {
                Unc_Entity *x = unc0_wake(w, Unc_TRef);
                if (!x) {
                    e = UNCIL_ERR_MEM;
                    break;
                }
                e = initvalues
                        ? unc0_bind(x, w, &initvalues[i])
                        : unc0_bind(x, w, NULL);
                if (e) break;
                o->refs[i] = x;
            }

            if (e) {
                while (i--)
                    unc0_hibernate(o->refs[i], w);
            }
        }

        if (!e)
            for (i = 0; i < refcopycount; ++i) {
                if (refcopies[i] >= bc) {
                    e = UNCIL_ERR_ARG_REFCOPYOUTOFBOUNDS;
                    break;
                }
            }

        if (!e)
            for (i = 0; i < refcopycount; ++i) {
                o->refs[refcount + i] = w->bounds[refcopies[i]];
                UNCIL_INCREFE(w, o->refs[refcount + i]);
            }

        if (e)
            TMFREE(Unc_Entity *, alloc, o->refs, o->refc);
    } else
        o->refs = NULL, e = 0;

    if (e)
        unc0_mfree(alloc, o->data, n);
    o->destructor = destructor;
    if (prototype)
        VIMPOSE(w, &o->prototype, prototype);
    else
        VINITNULL(&o->prototype);
    if (!e)
        e = UNC_LOCKINITL(o->lock);
    return e;
}

int unc0_oqgetattrv(Unc_View *w, Unc_Opaque *o,
                   Unc_Value *attr, int *found, Unc_Value *out) {
    for (;;) {
        switch (VGETTYPE(&o->prototype)) {
        case Unc_TTable:
            return unc0_dgetattrv(w,
                                  LEFTOVER(Unc_Dict, VGETENT(&o->prototype)),
                                  attr, found, out);
        case Unc_TObject:
            return unc0_ogetattrv(w,
                                  LEFTOVER(Unc_Object, VGETENT(&o->prototype)),
                                  attr, found, out);
        case Unc_TOpaque:
            o = LEFTOVER(Unc_Opaque, VGETENT(&o->prototype));
            continue;
        default:
            *found = 0;
            return 0;
        }
    }
}

int unc0_oqgetattrs(Unc_View *w, Unc_Opaque *o,
                   Unc_Size n, const byte *b, int *found, Unc_Value *out) {
    for (;;) {
        switch (VGETTYPE(&o->prototype)) {
        case Unc_TTable:
            return unc0_dgetattrs(w,
                                  LEFTOVER(Unc_Dict, VGETENT(&o->prototype)),
                                  n, b, found, out);
        case Unc_TObject:
            return unc0_ogetattrs(w,
                                  LEFTOVER(Unc_Object, VGETENT(&o->prototype)),
                                  n, b, found, out);
        case Unc_TOpaque:
            o = LEFTOVER(Unc_Opaque, VGETENT(&o->prototype));
            continue;
        default:
            *found = 0;
            return 0;
        }
    }
}

int unc0_oqgetattrc(Unc_View *w, Unc_Opaque *o,
                   const byte *s, int *found, Unc_Value *out) {
    return unc0_oqgetattrs(w, o, strlen((const char *)s), s, found, out);
}

/* call destructor if specified */
void unc0_graceopaque(Unc_View *w, Unc_Opaque *o) {
    if (o->destructor) {
        (void)(*o->destructor)(w, o->size, o->data);
        o->destructor = NULL;
    }
}

/* opaque destructor */
void unc0_dropopaque(Unc_View *w, Unc_Opaque *o) {
    Unc_Size i = 0, n = o->refc;
    unc0_graceopaque(w, o);
    for (i = 0; i < n; ++i)
        UNCIL_DECREFE(w, o->refs[i]);
    unc0_sunsetopaque(&w->world->alloc, o);
}

/* opaque free without destructor */
void unc0_sunsetopaque(Unc_Allocator *alloc, Unc_Opaque *o) {
    TMFREE(Unc_Entity *, alloc, o->refs, o->refc);
    unc0_mfree(alloc, o->data, o->size);
}
