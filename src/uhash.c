/*******************************************************************************
 
Uncil -- hash set and table impls

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

#include <limits.h>
#include <string.h>

#define UNCIL_DEFINES

#include "udebug.h"
#include "uerr.h"
#include "uhash.h"
#include "umem.h"
#include "uncil.h"
#include "ustr.h"
#include "uvali.h"
#include "uvop.h"

#define ROTL(x,n) (((x) << (n)) | ((x) >> (sizeof(unsigned) * CHAR_BIT - (n))))

unsigned unc__hashint(Unc_Int i) {
    return (Unc_UInt)i * 2600201173U;
}

unsigned unc__hashflt(Unc_Float f) {
    byte b[sizeof(Unc_Float)];
    unc__memcpy(b, &f, sizeof(Unc_Float));
    return unc__hashstr(sizeof(Unc_Float), b);
}

unsigned unc__hashptr(const void *p) {
    return (unsigned)((Unc_UInt)p * 2600201173U);
}

unsigned unc__hashstr(Unc_Size n, const byte *s) {
    Unc_Size i, z = ((3 * n) >> 6) + 1;
    unsigned x = 2857740885U;
    for (i = 0; i < n; i += z)
        x = ROTL(x, 11) ^ *s++;
    return x * 3690348479U;
}

Unc_HSet *unc__newhset(Unc_Allocator *alloc) {
    Unc_HSet *hset = unc__malloc(alloc, 0, sizeof(Unc_HSet));
    if (hset) unc__inithset(hset, alloc);
    return hset;
}

void unc__inithset(Unc_HSet *hset, Unc_Allocator *alloc) {
    hset->alloc = alloc;
    hset->entries = 0;
    hset->capacity = 0;
    hset->buckets = NULL;
}

int unc__puthset(Unc_HSet *hset, Unc_Size sn, const unsigned char *s,
                  Unc_Size *out, Unc_Size submit) {
    unsigned h, h1;
    Unc_HSet_V *b, **prev;
    if (!hset->buckets) {
        Unc_Size nc = 4;
        Unc_HSet_V **bb;
        if (!(bb = unc__mallocz(hset->alloc, 0, nc * sizeof(Unc_HSet_V *))))
            return UNCIL_ERR_MEM;
        hset->buckets = bb;
        hset->capacity = nc;
    }
    h = unc__hashstr(sn, s), h1 = h % hset->capacity;
    prev = &hset->buckets[h1];
    b = *prev;
    while (b) {
        if (sn == b->size && !unc__memcmp(s, b->val, sn)) {
            *out = b->index;
            return 0;
        }
        prev = &(b->next);
        b = b->next;
    }
    if (!b) {
        Unc_HSet_V *nrt;
        if (hset->entries > hset->capacity) {
            /* expand & rehash */
            Unc_Size s, oc = hset->capacity, nc = oc * 2;
            Unc_HSet_V *b, **bb, **obb;
            if (!(bb = unc__mallocz(hset->alloc, 0, nc * sizeof(Unc_HSet_V *))))
                return 1;
            obb = hset->buckets;
            for (s = 0; s < oc; ++s) {
                Unc_HSet_V **p0 = &bb[s], **p1 = &bb[s + oc];
                int hh;
                b = obb[s];
                while (b) {
                    hh = unc__hashstr(b->size, b->val) % nc;
                    if (hh < oc)
                        *p0 = b, p0 = &b->next;
                    else
                        *p1 = b, p1 = &b->next;
                    b = b->next;
                }
                *p0 = *p1 = NULL;
            }
            hset->buckets = bb;
            hset->capacity = nc;
            unc__mfree(hset->alloc, obb, oc * sizeof(Unc_HSet_V *));
            h1 = h % nc;
            b = *(prev = &hset->buckets[h1]);
            while (b)
                prev = &(b->next), b = b->next;
        }
        nrt = unc__malloc(hset->alloc, 0, sizeof(Unc_HSet_V) + sn);
        if (!nrt)
            return UNCIL_ERR_MEM;
        nrt->next = NULL;
        *out = nrt->index = submit;
        nrt->size = sn;
        ++hset->entries;
        unc__memcpy(nrt->val, s, sn);
        *prev = nrt;
    }
    return 0;
}

void unc__drophset(Unc_HSet *hset) {
    if (hset->buckets) {
        Unc_HSet_V **bb = hset->buckets, *b, *nb;
        Unc_Size s, nc = hset->capacity;
        for (s = 0; s < nc; ++s) {
            b = bb[s];
            while (b) {
                nb = b->next;
                unc__mfree(hset->alloc, b, sizeof(Unc_HSet_V) + b->size);
                b = nb;
            }
        }
        unc__mfree(hset->alloc, hset->buckets,
                   hset->capacity * sizeof(Unc_HSet_V *));
    }
}

void unc__freehset(Unc_HSet *hset) {
    unc__drophset(hset);
    unc__mfree(hset->alloc, hset, sizeof(Unc_HSet));
}

Unc_HTblS *unc__newhtbls(Unc_Allocator *alloc) {
    Unc_HTblS *h = unc__malloc(alloc, 0, sizeof(Unc_HTblS));
    if (h) unc__inithtbls(alloc, h);
    return h;
}

void unc__inithtbls(Unc_Allocator *alloc, Unc_HTblS *h) {
    h->entries = 0;
    h->capacity = 0;
    h->buckets = NULL;
}

static Unc_HTblS_V *unc__lookuphtbls(Unc_HTblS *h, unsigned hash, Unc_Size n,
                const byte *s, Unc_HTblS_V ***prev) {
    unsigned h1;
    Unc_HTblS_V *x;
    if (!h->buckets) {
        *prev = NULL;
        return NULL;
    }
    h1 = hash % h->capacity;
    x = *(*prev = &h->buckets[h1]);
    while (x) {
        if (n == x->key_n && !unc__memcmp(s, &x[1], n))
            break;
        x = *(*prev = &x->next);
    }
    return x;
}

Unc_Value *unc__gethtbls(Unc_View *w, Unc_HTblS *h, Unc_Size n, const byte *s) {
    Unc_HTblS_V **p;
    Unc_HTblS_V *o = unc__lookuphtbls(h, unc__hashstr(n, s), n, s, &p);
    return o ? &o->val : NULL;
}

int unc__puthtbls(Unc_View *w, Unc_HTblS *h, Unc_Size n,
                  const byte *s, Unc_Value **out) {
    Unc_HTblS_V **p, *o;
    Unc_Allocator *alloc;
    unsigned hash = unc__hashstr(n, s);
    o = unc__lookuphtbls(h, hash, n, s, &p);
    if (o) {
        *out = &o->val;
        return 0;
    }

    alloc = &w->world->alloc;
    if (!h->capacity || h->entries == h->capacity) {
        Unc_Size z = h->capacity, nz = z ? z * 2 : 4, q;
        Unc_HTblS_V **nb = TMALLOCZ(Unc_HTblS_V *, alloc, 0, nz), **p0, **p1,
                    *x;

        if (!nb) return UNCIL_ERR_MEM;
        hash %= nz;

        for (q = 0; q < z; ++q) {
            x = h->buckets[q];
            p0 = &nb[q], p1 = &nb[q + z];
            while (x) {
                if (unc__hashstr(x->key_n, (const byte *)&x[1]) % nz >= z)
                    *p1 = x, x = *(p1 = &x->next);
                else
                    *p0 = x, x = *(p0 = &x->next);
            }
            if (hash == q)
                p = p0;
            else if (hash == q + z)
                p = p1;
            *p0 = *p1 = NULL;
        }
        if (!p) {
            ASSERT(!z);
            p = &nb[hash];
        }
        TMFREE(Unc_HTblS_V *, alloc, h->buckets, z);
        h->buckets = nb;
        h->capacity = nz;
    } else
        hash %= h->capacity;

    o = unc__malloc(alloc, 0, sizeof(Unc_HTblS_V) + n);
    if (!o) return UNCIL_ERR_MEM;
    ASSERT(p);
    *p = o;
    o->next = NULL;
    o->key_n = n;
    VINITNULL(&o->val);
    unc__memcpy(&o[1], s, n);
    ++h->entries;
    *out = &o->val;
    return 0;
}

static void shrinks(Unc_View *w, Unc_HTblS *h) {
    Unc_Size cc = h->capacity, c = cc >> 1, i;
    for (i = 0; i < c; ++i) {
        Unc_HTblS_V **n = &h->buckets[i];
        while (*n) n = &(*n)->next;
        *n = h->buckets[i + c];
    }
    h->capacity = c;
    h->buckets = TMREALLOC(Unc_HTblS_V *, &w->world->alloc, 0,
                 h->buckets, cc, c);
}

int unc__delhtbls(Unc_View *w, Unc_HTblS *h, Unc_Size n, const byte *s) {
    Unc_HTblS_V **p, *o;
    Unc_Allocator *alloc = &w->world->alloc;
    o = unc__lookuphtbls(h, unc__hashstr(n, s), n, s, &p);
    if (!o) return 0;
    *p = o->next;
    VDECREF(w, &o->val);
    unc__mfree(alloc, o, sizeof(Unc_HTblS_V) + o->key_n);
    if (--h->entries * 4 < h->capacity)
        shrinks(w, h);
    return 0;
}

void unc__compacthtbls(Unc_View *w, Unc_HTblS *h) {
    while (--h->entries * 2 < h->capacity)
        shrinks(w, h);
}

void unc__drophtbls(Unc_View *w, Unc_HTblS *h) {
    if (h->buckets) {
        Unc_Allocator *alloc = &w->world->alloc;
        Unc_HTblS_V **bb = h->buckets, *b, *nb;
        Unc_Size s, nc = h->capacity;
        for (s = 0; s < nc; ++s) {
            b = bb[s];
            while (b) {
                nb = b->next;
                VDECREF(w, &b->val);
                unc__mfree(alloc, b, sizeof(Unc_HTblS_V) + b->key_n);
                b = nb;
            }
        }
        unc__mfree(alloc, h->buckets, h->capacity * sizeof(Unc_HTblS_V *));
    }
}

void unc__sunsethtbls(Unc_Allocator *alloc, Unc_HTblS *h) {
    if (h->buckets) {
        Unc_HTblS_V **bb = h->buckets, *b, *nb;
        Unc_Size s, nc = h->capacity;
        for (s = 0; s < nc; ++s) {
            b = bb[s];
            while (b) {
                nb = b->next;
                unc__mfree(alloc, b, sizeof(Unc_HTblS_V) + b->key_n);
                b = nb;
            }
        }
        unc__mfree(alloc, h->buckets, h->capacity * sizeof(Unc_HTblS_V *));
    }
}

void unc__freehtbls(Unc_View *w, Unc_HTblS *h) {
    unc__drophtbls(w, h);
    unc__mfree(&w->world->alloc, h, sizeof(Unc_HTblS));
}

Unc_HTblV *unc__newhtblv(Unc_Allocator *alloc) {
    Unc_HTblV *h = unc__malloc(alloc, Unc_AllocDict, sizeof(Unc_HTblV));
    if (h) unc__inithtblv(alloc, h);
    return h;
}

void unc__inithtblv(Unc_Allocator *alloc, Unc_HTblV *h) {
    h->entries = 0;
    h->capacity = 0;
    h->buckets = NULL;
}

static Unc_HTblV_V *unc__lookuphtblv(Unc_View *w, Unc_HTblV *h, unsigned hash, 
                        Unc_Value *key, Unc_HTblV_V ***prev) {
    unsigned h1;
    Unc_HTblV_V *x;
    if (!h->buckets) {
        *prev = NULL;
        return NULL;
    }
    h1 = hash % h->capacity;
    x = *(*prev = &h->buckets[h1]);
    while (x) {
        if (unc__vveq(w, &x->key, key))
            break;
        x = *(*prev = &x->next);
    }
    return x;
}

Unc_Value *unc__gethtblv(Unc_View *w, Unc_HTblV *h, Unc_Value *key) {
    Unc_HTblV_V **p, *o;
    unsigned hash;
    if (unc__hashvalue(w, key, &hash)) return NULL;
    o = unc__lookuphtblv(w, h, hash, key, &p);
    return o ? &o->val : NULL;
}

INLINE unsigned hashval(Unc_View *w, Unc_Value *key) {
    unsigned u = 0;
    unc__hashvalue(w, key, &u);
    return u;
}

static int unc__inserthtblv(Unc_View *w, Unc_HTblV *h, Unc_Value *key,
                            Unc_Value **out, Unc_HTblV_V **p, unsigned hash) {
    Unc_Allocator *alloc = &w->world->alloc;
    Unc_HTblV_V *o;
    if (!h->capacity || h->entries == h->capacity) {
        Unc_Size z = h->capacity, nz = z ? z * 2 : 4, q;
        Unc_HTblV_V **nb = TMALLOCZ(Unc_HTblV_V *, alloc, Unc_AllocDict, nz),
                    **p0, **p1, *x;

        if (!nb) return UNCIL_ERR_MEM;
        hash %= nz;

        for (q = 0; q < z; ++q) {
            x = h->buckets[q];
            p0 = &nb[q], p1 = &nb[q + z];
            while (x) {
                unsigned u = 0;
                unc__hashvalue(w, &x->key, &u);
                if (u % nz >= z)
                    *p1 = x, p1 = &x->next;
                else
                    *p0 = x, p0 = &x->next;
                x = x->next;
            }
            if (hash == q)
                p = p0;
            else if (hash == q + z)
                p = p1;
            *p0 = *p1 = NULL;
        }
        if (!p) {
            ASSERT(!z);
            p = &nb[hash];
        }
        TMFREE(Unc_HTblV_V *, alloc, h->buckets, z);
        h->buckets = nb;
        h->capacity = nz;
    } else
        hash %= h->capacity;

    o = TMALLOC(Unc_HTblV_V, alloc, Unc_AllocDict, 1);
    if (!o) return UNCIL_ERR_MEM;
    ASSERT(p);
    *p = o;
    o->next = NULL;
    VIMPOSE(w, &o->key, key);
    VINITNULL(&o->val);
    ++h->entries;
    *out = &o->val;
    return 0;
}

int unc__puthtblv(Unc_View *w, Unc_HTblV *h, Unc_Value *key, Unc_Value **out) {
    Unc_HTblV_V **p, *o;
    unsigned hash;
    int e = unc__hashvalue(w, key, &hash);
    if (e) return e;
    o = unc__lookuphtblv(w, h, hash, key, &p);
    if (o) {
        *out = &o->val;
        return 0;
    }
    return unc__inserthtblv(w, h, key, out, p, hash);
}

static void shrinkv(Unc_View *w, Unc_HTblV *h) {
    Unc_Size cc = h->capacity, c = cc >> 1, i;
    for (i = 0; i < c; ++i) {
        Unc_HTblV_V **n = &h->buckets[i];
        while (*n) n = &(*n)->next;
        *n = h->buckets[i + c];
    }
    h->capacity = c;
    h->buckets = TMREALLOC(Unc_HTblV_V *, &w->world->alloc, 0,
                 h->buckets, cc, c);
}

int unc__delhtblv(Unc_View *w, Unc_HTblV *h, Unc_Value *key) {
    Unc_HTblV_V **p, *o;
    Unc_Allocator *alloc = &w->world->alloc;
    unsigned hash;
    int e = unc__hashvalue(w, key, &hash);
    if (e) return e;
    o = unc__lookuphtblv(w, h, hash, key, &p);
    if (!o) return 0;
    *p = o->next;
    VDECREF(w, &o->key);
    VDECREF(w, &o->val);
    unc__mfree(alloc, o, sizeof(Unc_HTblV_V));
    if (--h->entries * 4 < h->capacity)
        shrinkv(w, h);
    return 0;
}

static Unc_HTblV_V *unc__lookuphtblvs(Unc_HTblV *h, unsigned hash, 
                        Unc_Size sn, const byte *s, Unc_HTblV_V ***prev) {
    unsigned h1;
    Unc_HTblV_V *x;
    if (!h->buckets) {
        *prev = NULL;
        return NULL;
    }
    h1 = hash % h->capacity;
    x = *(*prev = &h->buckets[h1]);
    while (x) {
        if (x->key.type == Unc_TString &&
                    unc__streqr(LEFTOVER(Unc_String, VGETENT(&x->key)), sn, s))
            break;
        x = *(*prev = &x->next);
    }
    return x;
}

Unc_Value *unc__gethtblvs(Unc_View *w, Unc_HTblV *h,
                          Unc_Size n, const byte *s) {
    Unc_HTblV_V **p;
    Unc_HTblV_V *o = unc__lookuphtblvs(h, unc__hashstr(n, s), n, s, &p);
    return o ? &o->val : NULL;
}

int unc__puthtblvs(Unc_View *w, Unc_HTblV *h,
                  Unc_Size n, const byte *s, Unc_Value **out) {
    Unc_HTblV_V **p;
    unsigned hash = unc__hashstr(n, s);
    Unc_HTblV_V *o = unc__lookuphtblvs(h, hash, n, s, &p);
    if (o) {
        *out = &o->val;
        return 0;
    } else {
        int e;
        Unc_Value tmp;
        Unc_Entity *en = unc__wake(w, Unc_TString);
        if (!en) return UNCIL_ERR_MEM;
        e = unc__initstring(&w->world->alloc, LEFTOVER(Unc_String, en), n, s);
        if (e) return e;
        VINITENT(&tmp, Unc_TString, en);
        return unc__inserthtblv(w, h, &tmp, out, p, hash);
    }
}

int unc__delhtblvs(Unc_View *w, Unc_HTblV *h, Unc_Size n, const byte *s) {
    Unc_HTblV_V **p, *o;
    Unc_Allocator *alloc = &w->world->alloc;
    o = unc__lookuphtblvs(h, unc__hashstr(n, s), n, s, &p);
    if (!o) return 0;
    *p = o->next;
    VDECREF(w, &o->key);
    VDECREF(w, &o->val);
    unc__mfree(alloc, o, sizeof(Unc_HTblV_V));
    if (--h->entries * 4 < h->capacity)
        shrinkv(w, h);
    return 0;
}

void unc__compacthtblv(Unc_View *w, Unc_HTblV *h) {
    while (--h->entries * 2 < h->capacity)
        shrinkv(w, h);
}

void unc__drophtblv(Unc_View *w, Unc_HTblV *h) {
    if (h->buckets) {
        Unc_Allocator *alloc = &w->world->alloc;
        Unc_HTblV_V **bb = h->buckets, *b, *nb;
        Unc_Size s, nc = h->capacity;
        for (s = 0; s < nc; ++s) {
            b = bb[s];
            while (b) {
                nb = b->next;
                VDECREF(w, &b->key);
                VDECREF(w, &b->val);
                unc__mfree(alloc, b, sizeof(Unc_HTblV_V));
                b = nb;
            }
        }
        unc__mfree(alloc, h->buckets, h->capacity * sizeof(Unc_HTblV_V *));
    }
}

void unc__sunsethtblv(Unc_Allocator *alloc, Unc_HTblV *h) {
    if (h->buckets) {
        Unc_HTblV_V **bb = h->buckets, *b, *nb;
        Unc_Size s, nc = h->capacity;
        for (s = 0; s < nc; ++s) {
            b = bb[s];
            while (b) {
                nb = b->next;
                unc__mfree(alloc, b, sizeof(Unc_HTblV_V));
                b = nb;
            }
        }
        unc__mfree(alloc, h->buckets, h->capacity * sizeof(Unc_HTblV_V *));
    }
}

void unc__freehtblv(Unc_View *w, Unc_HTblV *h) {
    unc__drophtblv(w, h);
    unc__mfree(&w->world->alloc, h, sizeof(Unc_HTblV));
}
