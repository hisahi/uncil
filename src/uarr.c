/*******************************************************************************
 
Uncil -- array impl

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

#define UNCIL_DEFINES

#include "uarr.h"
#include "uerr.h"
#include "umem.h"
#include "uncil.h"
#include "uval.h"
#include "uvali.h"
#include "uvop.h"

/* init array and copy n values from v */
int unc__initarray(Unc_View *w, Unc_Array *a, Unc_Size n, Unc_Value *v) {
    a->size = a->capacity = n;
    if (!n) {
        a->data = NULL;
    } else {
        Unc_Size i;
        a->data = TMALLOC(Unc_Value, &w->world->alloc, Unc_AllocArray, n);
        if (!a->data) return UNCIL_ERR_MEM;
        for (i = 0; i < n; ++i)
            VIMPOSE(w, &a->data[i], &v[i]);
    }
    return UNC_LOCKINITL(a->lock);
}

/* init array and fill with n null values */
int unc__initarrayn(Unc_View *w, Unc_Array *a, Unc_Size n) {
    a->size = a->capacity = n;
    if (!n) {
        a->data = NULL;
    } else {
        Unc_Size i;
        a->data = TMALLOC(Unc_Value, &w->world->alloc, Unc_AllocArray, n);
        if (!a->data) return UNCIL_ERR_MEM;
        for (i = 0; i < n; ++i)
            VINITNULL(&a->data[i]);
    }
    return UNC_LOCKINITL(a->lock);
}

/* init array by concatenating two arrays */
int unc__initarrayfromcat(Unc_View *w, Unc_Array *s,
                          Unc_Size an, Unc_Value *av,
                          Unc_Size bn, Unc_Value *bv) {
    Unc_Size n = an + bn;
    s->size = s->capacity = n;
    if (!n) {
        s->data = NULL;
    } else {
        Unc_Size i;
        s->data = TMALLOC(Unc_Value, &w->world->alloc, Unc_AllocArray, n);
        if (!s->data) return UNCIL_ERR_MEM;
        for (i = 0; i < an; ++i)
            VCOPY(w, &s->data[i], &av[i]);
        for (i = 0; i < bn; ++i)
            VCOPY(w, &s->data[an + i], &bv[i]);
    }
    return UNC_LOCKINITL(s->lock);
}

/* init array and move n values from v */
int unc__initarrayraw(Unc_View *w, Unc_Array *a, Unc_Size n, Unc_Value *v) {
    a->size = a->capacity = n;
    if (!n) {
        a->data = NULL;
    } else {
        a->data = TMALLOC(Unc_Value, &w->world->alloc, Unc_AllocArray, n);
        if (!a->data) return UNCIL_ERR_MEM;
        unc__memcpy(a->data, v, n * sizeof(Unc_Value));
    }
    return UNC_LOCKINITL(a->lock);
}

/* concatenate n values from v onto a */
int unc__arraycatr(Unc_View *w, Unc_Array *a, Unc_Size n, Unc_Value *v) {
    Unc_Size c = a->capacity, s = a->size, ns = s + n, i;
    if (ns > c) {
        Unc_Size nc = (3 * c) / 2;
        Unc_Value *p;
        if (nc < ns) nc = ns;
        p = TMREALLOCZ(Unc_Value, &w->world->alloc, Unc_AllocArray,
                        a->data, c, nc);
        if (!p) return UNCIL_ERR_MEM;
        a->data = p;
        a->capacity = nc;
    }
    for (i = 0; i < n; ++i)
        VCOPY(w, &a->data[s + i], &v[i]);
    a->size = ns;
    return 0;
}

/* append/push one value to a */
int unc__arraypush(Unc_View *w, Unc_Array *a, Unc_Value *v) {
    return unc__arraycatr(w, a, 1, v);
}

/* concatenate array a2 to a */
int unc__arraycat(Unc_View *w, Unc_Array *a, const Unc_Array *a2) {
    return unc__arraycatr(w, a, a2->size, a2->data);
}

/* append/push N null values to a */
int unc__arraypushn(Unc_View *w, Unc_Array *a, Unc_Size n) {
    Unc_Size c = a->capacity, s = a->size, ns = s + n, i;
    Unc_Value nl = UNC_BLANK;
    if (ns > c) {
        Unc_Size nc = (3 * c) / 2;
        Unc_Value *p;
        if (nc < ns) nc = ns;
        p = TMREALLOCZ(Unc_Value, &w->world->alloc, Unc_AllocArray,
                        a->data, c, nc);
        if (!p) return UNCIL_ERR_MEM;
        a->data = p;
        a->capacity = nc;
    }
    for (i = 0; i < n; ++i)
        a->data[s + i] = nl;
    a->size = ns;
    return 0;
}

/* insert n values from v into a at index i */
int unc__arrayinsr(Unc_View *w, Unc_Array *a, Unc_Size i,
                    Unc_Size n, Unc_Value *v) {
    Unc_Size c = a->capacity, s = a->size, ns = s + n, j;
    if (i > a->size)
        return UNCIL_ERR_ARG_OUTOFBOUNDS;
    if (ns > c) {
        Unc_Size nc = (3 * c) / 2;
        Unc_Value *p;
        if (nc < ns) nc = ns;
        p = TMREALLOC(Unc_Value, &w->world->alloc, Unc_AllocArray,
                    a->data, c, nc);
        if (!p) return UNCIL_ERR_MEM;
        a->data = p;
        a->capacity = nc;
    }
    if (i < a->size)
        unc__memmove(a->data + i + n, a->data + i,
                    (a->size - i) * sizeof(Unc_Value));
    for (j = 0; j < n; ++j)
        VCOPY(w, &a->data[i + j], &v[j]);
    a->size = ns;
    return 0;
}

/* insert value v into a at index i */
int unc__arrayinsv(Unc_View *w, Unc_Array *a, Unc_Size i, Unc_Value *v) {
    return unc__arrayinsr(w, a, i, 1, v);
}

/* insert array a2 into a at index i */
int unc__arrayins(Unc_View *w, Unc_Array *a, Unc_Size i, const Unc_Array *a2) {
    return unc__arrayinsr(w, a, i, a2->size, a2->data);
}

/* delete n items from array a at index i */
int unc__arraydel(Unc_View *w, Unc_Array *a, Unc_Size i, Unc_Size n) {
    if (i + n > a->size)
        return UNCIL_ERR_ARG_OUTOFBOUNDS;
    if (n) {
        Unc_Size j, e = i + n;
        for (j = i; j < e; ++j)
            VDECREF(w, &a->data[j]);
        unc__memmove(a->data + i, a->data + i + n,
                    (a->size - i - n) * sizeof(Unc_Value));
        a->size -= n;
    }
    return 0;
}

/* drop/delete array, destructor */
void unc__droparray(Unc_View *w, Unc_Array *a) {
    Unc_Size s = a->size, i;
    UNC_LOCKFINAL(a->lock);
    for (i = 0; i < s; ++i)
        VDECREF(w, &a->data[i]);
    unc__sunsetarray(&w->world->alloc, a);
}

/* sunset array (free without destructor) */
void unc__sunsetarray(Unc_Allocator *alloc, Unc_Array *a) {
    TMFREE(Unc_Value, alloc, a->data, a->capacity);
}

/* array index getter */
int unc__agetindx(Unc_View *w, Unc_Array *a,
                    Unc_Value *indx, int permissive, Unc_Value *out) {
    Unc_Int i;
    int e = unc__vgetint(w, indx, &i);
    if (e) {
        if (e == UNCIL_ERR_CONVERT_TOINT)
            e = UNCIL_ERR_ARG_INDEXNOTINTEGER;
        return e;
    }
    if (i < 0)
        i += a->size;
    if (i < 0 || i >= a->size) {
        if (permissive) {
            VSETNULL(w, out);
            return 0;
        }
        return UNCIL_ERR_ARG_INDEXOUTOFBOUNDS;
    }
    VCOPY(w, out, &a->data[i]);
    return 0;
}

/* array index setter */
int unc__asetindx(Unc_View *w, Unc_Array *a, Unc_Value *indx, Unc_Value *v) {
    Unc_Int i;
    int e = unc__vgetint(w, indx, &i);
    if (e) {
        if (e == UNCIL_ERR_CONVERT_TOINT)
            e = UNCIL_ERR_ARG_INDEXNOTINTEGER;
        return e;
    }
    if (i < 0)
        i += a->size;
    if (i < 0 || i >= a->size)
        return UNCIL_ERR_ARG_INDEXOUTOFBOUNDS;
    VCOPY(w, &a->data[i], v);
    return 0;
}
