/*******************************************************************************
 
Uncil -- memory management impl

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

#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <string.h>

#define UNCIL_DEFINES

#include "udebug.h"
#include "ugc.h"
#include "umem.h"
#include "umt.h"

/* define UNCIL_STDALLOC_NOT_THREADSAFE if malloc/realloc/free
                                            is not thread-safe */
#if UNCIL_MT_OK && UNCIL_STDALLOC_NOT_THREADSAFE
UNC_LOCKSTATICF(unc__stdalloc_lock)
UNC_LOCKSTATICFINIT0(unc__stdalloc_lock)
#endif

void *unc__stdalloc(void *udata, Unc_Alloc_Purpose purpose,
                    size_t oldsize, size_t newsize, void *ptr) {
    (void)udata; (void)purpose; (void)oldsize;
    DEBUGPRINT(ALLOC, ("%p (%lu => %lu) => ", ptr, oldsize, newsize));
#if UNCIL_MT_OK && UNCIL_STDALLOC_NOT_THREADSAFE
    UNC_LOCKF(unc__stdalloc_lock);
#endif
    if (!newsize) {
        free(ptr);
        ptr = NULL;
    } else {
        void *p = realloc(ptr, newsize);
        if (p || newsize > oldsize)
            ptr = p;
    }
#if UNCIL_MT_OK && UNCIL_STDALLOC_NOT_THREADSAFE
    UNC_UNLOCKF(unc__stdalloc_lock);
#endif
    DEBUGPRINT(ALLOC, ("%p\n", ptr));
    return ptr;
}

void unc__initalloc(Unc_Allocator *alloc, struct Unc_World *w,
                    Unc_Alloc fn, void *data) {
    alloc->world = w;
    if (!fn) fn = unc__stdalloc;
#if UNCIL_MT_OK && UNCIL_STDALLOC_NOT_THREADSAFE
    UNC_LOCKSTATICFINIT1(unc__stdalloc_lock);
#endif
    alloc->fn = fn;
    alloc->data = data;
    alloc->total = 0;
}

void *unc__malloc(Unc_Allocator *alloc, Unc_Alloc_Purpose purpose, size_t sz) {
    return unc__mrealloc(alloc, purpose, NULL, 0, sz);
}

void *unc__mrealloc(Unc_Allocator *alloc, Unc_Alloc_Purpose purpose, void *optr,
                    size_t sz0, size_t sz1) {
    void *ptr;
#if UNC_SIZE_CHECK
    if ((Unc_Size)sz1 > (Unc_Size)(size_t)-1)
        return NULL;
#endif
    if (sz0 == sz1)
        return optr;
    ptr = alloc->fn(alloc->data, purpose, sz0, sz1, optr);
    if (!ptr) {
        if (!sz1) {
            alloc->total -= sz0;
            return ptr;
        } else if (sz1 < sz0) {
            alloc->total -= sz0 - sz1;
            return optr;
        }
        /* emergency GC call */
        unc__gccollect(alloc->world, NULL);
        ptr = alloc->fn(alloc->data, purpose, sz0, sz1, optr);
    }
    if (ptr)
        alloc->total += (Unc_Size)sz1 - (Unc_Size)sz0;
    return ptr;
}

void unc__mfree(Unc_Allocator *alloc, void *ptr, size_t sz) {
    if (ptr) {
        void *p = unc__mrealloc(alloc, Unc_AllocOther, ptr, sz, 0);
        (void)p;
    }
}

void *unc__mmalloc(Unc_Allocator *alloc, Unc_Alloc_Purpose purpose, size_t sz) {
    void *ptr = unc__malloc(alloc, purpose, sz + sizeof(Unc_MaxAlign));
    *(size_t *)ptr = sz;
    return (void *)((char *)ptr + sizeof(Unc_MaxAlign));
}

void *unc__mmrealloc(Unc_Allocator *alloc, Unc_Alloc_Purpose purpose,
                     void *ptr, size_t sz) {
    if (ptr) {
        void *optr = (char *)ptr - sizeof(Unc_MaxAlign);
        size_t osz = *(size_t *)(optr);
        void *nptr = unc__mrealloc(alloc, purpose, optr,
                        osz + sizeof(Unc_MaxAlign), sz + sizeof(Unc_MaxAlign));
        if (!nptr)
            return nptr;
        *(size_t *)nptr = sz;
        return (void *)((char *)nptr + sizeof(Unc_MaxAlign));
    } else
        return unc__mmalloc(alloc, purpose, sz);
}

void unc__mmfree(Unc_Allocator *alloc, void *ptr) {
    if (ptr) {
        void *optr = (char *)ptr - sizeof(Unc_MaxAlign);
        size_t osz = *(size_t *)(optr);
        unc__mfree(alloc, optr, osz);
    }
}

size_t unc__mmgetsize(Unc_Allocator *alloc, void *ptr) {
    void *optr = (char *)ptr - sizeof(Unc_MaxAlign);
    return *(size_t *)(optr);
}

void *unc__mmunwind(Unc_Allocator *alloc, void *ptr, size_t *out) {
    void *optr = (char *)ptr - sizeof(Unc_MaxAlign);
    size_t osz = *(size_t *)(optr);
    unc__memmove(optr, ptr, osz);
    *out = osz;
    return unc__mrealloc(alloc, Unc_AllocOther, optr,
                         osz + sizeof(Unc_MaxAlign), osz);
}

void *unc__mmunwinds(Unc_Allocator *alloc, void *ptr, size_t *out) {
    void *optr = (char *)ptr - sizeof(Unc_MaxAlign);
    size_t osz = *(size_t *)(optr);
    unc__memmove(optr, ptr, osz);
    ((char *)optr)[osz] = 0;
    *out = osz + 1;
    return unc__mrealloc(alloc, Unc_AllocOther, optr,
                         osz + sizeof(Unc_MaxAlign), osz + 1);
}

void *unc__mallocz(Unc_Allocator *alloc, Unc_Alloc_Purpose purpose, size_t sz) {
    void *p = unc__malloc(alloc, purpose, sz);
    if (p) unc__memset(p, 0, sz);
    return p;
}

void *unc__mmallocz(Unc_Allocator *alloc,
                    Unc_Alloc_Purpose purpose, size_t sz) {
    void *p = unc__mmalloc(alloc, purpose, sz);
    if (p) unc__memset(p, 0, sz);
    return p;
}

void *unc__mreallocz(Unc_Allocator *alloc, Unc_Alloc_Purpose purpose, void *ptr,
            size_t sz0, size_t sz1) {
    void *p = unc__mrealloc(alloc, purpose, ptr, sz0, sz1);
    if (p && sz1 > sz0) unc__memset((char *)p + sz0, 0, sz1 - sz0);
    return p;
}

void *unc__mmreallocz(Unc_Allocator *alloc, Unc_Alloc_Purpose purpose,
            void *ptr, size_t sz1) {
    if (ptr) {
        void *optr = (char *)ptr - sizeof(Unc_MaxAlign);
        size_t sz0 = *(size_t *)(optr);
        void *p = unc__mmrealloc(alloc, purpose, ptr, sz1);
        if (p && sz1 > sz0) unc__memset((char *)p + sz0, 0, sz1 - sz0);
        return p;
    } else {
        return unc__mmallocz(alloc, purpose, sz1);
    }
}

void *unc__memchr(const void *m, int c, size_t sz) {
    return memchr(m, c, sz);
}

size_t unc__memset(void *dst, int c, size_t sz) {
    memset(dst, c, sz);
    return sz;
}

size_t unc__memcpy(void *dst, const void *src, size_t sz) {
    memcpy(dst, src, sz);
    return sz;
}

size_t unc__memmove(void *dst, const void *src, size_t sz) {
    memmove(dst, src, sz);
    return sz;
}

int unc__memcmp(const void *dst, const void *src, size_t sz) {
    return memcmp(dst, src, sz);
}

void unc__memrev(void *dst, size_t sz) {
    byte *p0 = dst, *p1 = p0 + sz - 1, tmp;
    while (p0 < p1) {
        tmp = *p0;
        *p0++ = *p1;
        *p1-- = tmp;
    }
}

void *unc__memrchr(const void *p, int c, size_t n) {
    unsigned char *b = (unsigned char *)p + n;
    while (n--)
        if (*--b == c)
            return b;
    return NULL;
}

int unc__strput(Unc_Allocator *alloc, byte **o, Unc_Size *n, Unc_Size *c,
                    Unc_Size inc_log2, byte q) {
    if (*n >= *c) {
        Unc_Size inc = 1 << inc_log2;
        Unc_Size oc = *c, nc = oc + inc;
        byte *oo = *o, *on;
        nc = (nc + inc - 1) & ~(inc - 1);
        on = unc__mrealloc(alloc, 0, oo, oc, nc);
        if (!on)
            return UNCIL_ERR_MEM;
        *c = nc, *o = on;
    }
    (*o)[(*n)++] = q;
    return 0;
}

/* (raw) string search */
const byte *unc__strsearch(const byte *haystack, Unc_Size haystack_n,
                           const byte *needle, Unc_Size needle_n) {
    byte s0;
    const byte *next;
    if (!needle_n) return haystack;
    s0 = needle[0];
    haystack_n -= needle_n - 1;
    while ((next = unc__memchr(haystack, s0, haystack_n))) {
        if (!unc__memcmp(next + 1, needle + 1, needle_n - 1))
            return next;
        haystack_n -= next - haystack + 1;
        haystack = next + 1;
    }
    return NULL;
}

const byte *unc__strsearchr(const byte *haystack, Unc_Size haystack_n,
                            const byte *needle, Unc_Size needle_n) {
    byte s0;
    const byte *next;
    if (!needle_n) return haystack;
    s0 = needle[0];
    haystack_n -= needle_n - 1;
    while ((next = unc__memrchr(haystack, s0, haystack_n))) {
        if (!memcmp(next + 1, needle + 1, needle_n - 1))
            return next;
        haystack_n = next - haystack - 1;
    }
    return NULL;
}

int unc__strputn(Unc_Allocator *alloc, byte **o, Unc_Size *n, Unc_Size *c,
                    Unc_Size inc_log2, Unc_Size qn, const byte *qq) {
    if (*n + qn > *c) {
        Unc_Size inc = 1 << inc_log2;
        Unc_Size oc = *c, nc = oc + (inc > qn ? inc : qn);
        byte *oo = *o, *on;
        nc = (nc + inc - 1) & ~(inc - 1);
        on = unc__mrealloc(alloc, 0, oo, oc, nc);
        if (!on)
            return UNCIL_ERR_MEM;
        *c = nc, *o = on;
    }
    unc__memcpy(*o + *n, qq, qn);
    *n += qn;
    return 0;
}

int unc__sstrput(Unc_Allocator *alloc, byte **o, Unc_Size *n, Unc_Size *c,
                    Unc_Size inc_log2, byte q) {
    if (*n >= *c) {
        Unc_Size inc = 1 << inc_log2;
        Unc_Size oc = *c, nc = oc + inc;
        byte *oo = *o, *on;
        nc = (nc + inc - 1) & ~(inc - 1);
        on = unc__mrealloc(alloc, Unc_AllocString, oo, oc, nc);
        if (!on)
            return UNCIL_ERR_MEM;
        *c = nc, *o = on;
    }
    (*o)[(*n)++] = q;
    return 0;
}

int unc__sstrputn(Unc_Allocator *alloc, byte **o, Unc_Size *n, Unc_Size *c,
                    Unc_Size inc_log2, Unc_Size qn, const byte *qq) {
    if (*n + qn > *c) {
        Unc_Size inc = 1 << inc_log2;
        Unc_Size oc = *c, nc = oc + (inc > qn ? inc : qn);
        byte *oo = *o, *on;
        nc = (nc + inc - 1) & ~(inc - 1);
        on = unc__mrealloc(alloc, Unc_AllocString, oo, oc, nc);
        if (!on)
            return UNCIL_ERR_MEM;
        *c = nc, *o = on;
    }
    unc__memcpy(*o + *n, qq, qn);
    *n += qn;
    return 0;
}

int unc__strpushb(Unc_Allocator *alloc, byte **o, Unc_Size *n, Unc_Size *c,
                    Unc_Size inc_log2, Unc_Size qn, const byte *qq) {
    if (!qn) return 0;
    if (*n + qn > *c) {
        Unc_Size inc = 1 << inc_log2;
        Unc_Size oc = *c, nc = oc + (inc > qn ? inc : qn);
        byte *oo = *o, *on;
        nc = (nc + inc - 1) & ~(inc - 1);
        on = unc__mmrealloc(alloc, Unc_AllocBlob, oo, nc);
        if (!on)
            return UNCIL_ERR_MEM;
        *c = nc, *o = on;
    }
    unc__memcpy(*o + *n, qq, qn);
    *n += qn;
    return 0;
}

int unc__strpush(Unc_Allocator *alloc, byte **o, Unc_Size *n, Unc_Size *c,
                    Unc_Size inc_log2, Unc_Size qn, const byte *qq) {
    if (!qn) return 0;
    if (*n + qn > *c) {
        Unc_Size inc = 1 << inc_log2;
        Unc_Size oc = *c, nc = oc + (inc > qn ? inc : qn);
        byte *oo = *o, *on;
        nc = (nc + inc - 1) & ~(inc - 1);
        on = unc__mmrealloc(alloc, Unc_AllocString, oo, nc);
        if (!on)
            return UNCIL_ERR_MEM;
        *c = nc, *o = on;
    }
    unc__memcpy(*o + *n, qq, qn);
    *n += qn;
    return 0;
}

int unc__strpush1(Unc_Allocator *alloc, byte **o, Unc_Size *n, Unc_Size *c,
                    Unc_Size inc_log2, byte q) {
    return unc__strpush(alloc, o, n, c, inc_log2, 1, &q);
}

int unc__strpushrv(Unc_Allocator *alloc, byte **o, Unc_Size *n, Unc_Size *c,
                   Unc_Size inc_log2, Unc_Size qn, const byte *qq) {
    if (!qn) return 0;
    if (*n + qn > *c) {
        Unc_Size inc = 1 << inc_log2;
        Unc_Size oc = *c, nc = oc + (inc > qn ? inc : qn);
        byte *oo = *o, *on;
        nc = (nc + inc - 1) & ~(inc - 1);
        on = unc__mmrealloc(alloc, Unc_AllocString, oo, nc);
        if (!on)
            return UNCIL_ERR_MEM;
        *c = nc, *o = on;
    }
    unc__memcpy(*o + *n, qq, qn);
    unc__memrev(*o + *n, qn);
    *n += qn;
    return 0;
}

size_t unc__strnlen(const char *s, size_t maxlen) {
#if _POSIX_C_SOURCE >= 200809L
    return strnlen(s, maxlen);
#else
    const char *p = memchr(s, 0, maxlen);
    return p ? p - s : maxlen;
#endif
}
