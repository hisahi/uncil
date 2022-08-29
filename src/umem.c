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

#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define UNCIL_DEFINES

#include "ucompdef.h"
#include "udebug.h"
#include "ugc.h"
#include "umem.h"
#include "umt.h"

/* define UNCIL_STDALLOC_NOT_THREADSAFE if malloc/realloc/free
                                            is not thread-safe */
#if UNCIL_MT_OK && UNCIL_STDALLOC_NOT_THREADSAFE
UNC_LOCKSTATICF(unc0_stdalloc_lock)
UNC_LOCKSTATICFINIT0(unc0_stdalloc_lock)
#endif

void *unc0_memchr(const void *m, int c, size_t sz) {
    return memchr(m, c, sz);
}

size_t unc0_memset(void *dst, int c, size_t sz) {
    memset(dst, c, sz);
    return sz;
}

size_t unc0_memsetv(void *dst, int c, size_t sz) {
#if UNCIL_C11 && __STDC_LIB_EXT1__
    memset_s(dst, sz, c, sz);
#else
    volatile unsigned char *p = (volatile unsigned char *)dst;
    unsigned char x = (unsigned char)c;
    size_t i;
    for (i = 0; i < sz; ++i) p[i] = x;
#endif
    return sz;
}

size_t unc0_memcpy(void *dst, const void *src, size_t sz) {
    memcpy(dst, src, sz);
    return sz;
}

size_t unc0_memmove(void *dst, const void *src, size_t sz) {
    memmove(dst, src, sz);
    return sz;
}

int unc0_memcmp(const void *dst, const void *src, size_t sz) {
    return memcmp(dst, src, sz);
}

void unc0_memrev(void *dst, size_t sz) {
    byte *p0 = dst, *p1 = p0 + sz - 1, tmp;
    while (p0 < p1) {
        tmp = *p0;
        *p0++ = *p1;
        *p1-- = tmp;
    }
}

void *unc0_memrchr(const void *p, int c, size_t n) {
    unsigned char *b = (unsigned char *)p + n;
    while (n--)
        if (*--b == c)
            return b;
    return NULL;
}

void *unc0_stdalloc(void *udata, Unc_Alloc_Purpose purpose,
                    size_t oldsize, size_t newsize, void *ptr) {
    (void)udata; (void)purpose; (void)oldsize;
    DEBUGPRINT(ALLOC, ("%p (%lu => %lu) => ", ptr, oldsize, newsize));
#if UNCIL_MT_OK && UNCIL_STDALLOC_NOT_THREADSAFE
    UNC_LOCKF(unc0_stdalloc_lock);
#endif
    if (!newsize) {
        free(ptr);
        ptr = NULL;
    } else {
        void *p = realloc(ptr, newsize);
        if (p || newsize > oldsize) ptr = p;
    }
#if UNCIL_MT_OK && UNCIL_STDALLOC_NOT_THREADSAFE
    UNC_UNLOCKF(unc0_stdalloc_lock);
#endif
    DEBUGPRINT(ALLOC, ("%p\n", ptr));
    return ptr;
}

void unc0_initalloc(Unc_Allocator *alloc, struct Unc_World *w,
                    Unc_Alloc fn, void *data) {
    alloc->world = w;
    if (!fn) fn = unc0_stdalloc;
#if UNCIL_MT_OK && UNCIL_STDALLOC_NOT_THREADSAFE
    UNC_LOCKSTATICFINIT1(unc0_stdalloc_lock);
#endif
    alloc->fn = fn;
    alloc->data = data;
    alloc->total = 0;
}

INLINE void *unc0_invokealloc(Unc_Allocator *alloc, Unc_Alloc_Purpose purpose,
                              void *optr, size_t sz0, size_t sz1) {
    void *ptr;
#if UNC_SIZE_CHECK
    if ((Unc_Size)sz1 > (Unc_Size)(size_t)-1)
        return NULL;
#endif
    if (sz0 == sz1) return optr;
    ptr = alloc->fn(alloc->data, purpose, sz0, sz1, optr);
    if (!sz1) {
        alloc->total -= sz0;
        return NULL;
    }
    if (!ptr) {
        if (sz0 > sz1) {
            alloc->total -= sz0 - sz1;
            return optr;
        }
        /* emergency GC call */
        unc0_gccollect(alloc->world, NULL);
        ptr = alloc->fn(alloc->data, purpose, sz0, sz1, optr);
    }
    if (ptr)
        alloc->total += (Unc_Size)sz1 - (Unc_Size)sz0;
    return ptr;
}

void *unc0_malloc(Unc_Allocator *alloc, Unc_Alloc_Purpose purpose, size_t sz) {
    return unc0_invokealloc(alloc, purpose, NULL, 0, sz);
}

void *unc0_mrealloc(Unc_Allocator *alloc, Unc_Alloc_Purpose purpose, void *optr,
                    size_t sz0, size_t sz1) {
    return unc0_invokealloc(alloc, purpose, optr, sz0, sz1);
}

void unc0_mfree(Unc_Allocator *alloc, void *ptr, size_t sz) {
    if (ptr) unc0_invokealloc(alloc, Unc_AllocOther, ptr, sz, 0);
}

void *unc0_mallocz(Unc_Allocator *alloc, Unc_Alloc_Purpose purpose, size_t sz) {
    void *p = unc0_malloc(alloc, purpose, sz);
    if (p) unc0_memset(p, 0, sz);
    return p;
}

void *unc0_mmallocz(Unc_Allocator *alloc,
                    Unc_Alloc_Purpose purpose, size_t sz) {
    void *p = unc0_mmalloc(alloc, purpose, sz);
    if (p) unc0_memset(p, 0, sz);
    return p;
}

void *unc0_mmalloc(Unc_Allocator *alloc, Unc_Alloc_Purpose purpose, size_t sz) {
    void *ptr = unc0_malloc(alloc, purpose, sz + sizeof(Unc_MaxAlign));
    *(size_t *)ptr = sz;
    return (void *)((char *)ptr + sizeof(Unc_MaxAlign));
}

void *unc0_mmrealloc(Unc_Allocator *alloc, Unc_Alloc_Purpose purpose,
                     void *ptr, size_t sz) {
    if (ptr) {
        void *optr = (char *)ptr - sizeof(Unc_MaxAlign);
        size_t osz = *(size_t *)(optr);
        void *nptr = unc0_mrealloc(alloc, purpose, optr,
                        osz + sizeof(Unc_MaxAlign), sz + sizeof(Unc_MaxAlign));
        if (!nptr)
            return nptr;
        *(size_t *)nptr = sz;
        return (void *)((char *)nptr + sizeof(Unc_MaxAlign));
    } else
        return unc0_mmalloc(alloc, purpose, sz);
}

void unc0_mmfree(Unc_Allocator *alloc, void *ptr) {
    if (ptr) {
        void *optr = (char *)ptr - sizeof(Unc_MaxAlign);
        size_t osz = *(size_t *)(optr);
        unc0_mfree(alloc, optr, osz);
    }
}

size_t unc0_mmgetsize(Unc_Allocator *alloc, void *ptr) {
    void *optr = (char *)ptr - sizeof(Unc_MaxAlign);
    return *(size_t *)(optr);
}

void *unc0_mmunwind(Unc_Allocator *alloc, void *ptr, size_t *out) {
    void *optr = (char *)ptr - sizeof(Unc_MaxAlign);
    size_t osz = *(size_t *)(optr);
    unc0_memmove(optr, ptr, osz);
    *out = osz;
    return unc0_mrealloc(alloc, Unc_AllocOther, optr,
                         osz + sizeof(Unc_MaxAlign), osz);
}

void *unc0_mmunwinds(Unc_Allocator *alloc, void *ptr, size_t *out) {
    void *optr = (char *)ptr - sizeof(Unc_MaxAlign);
    size_t osz = *(size_t *)(optr);
    unc0_memmove(optr, ptr, osz);
    ((char *)optr)[osz] = 0;
    *out = osz + 1;
    return unc0_mrealloc(alloc, Unc_AllocOther, optr,
                         osz + sizeof(Unc_MaxAlign), osz + 1);
}

void *unc0_mreallocz(Unc_Allocator *alloc, Unc_Alloc_Purpose purpose, void *ptr,
            size_t sz0, size_t sz1) {
    void *p = unc0_mrealloc(alloc, purpose, ptr, sz0, sz1);
    if (p && sz1 > sz0) unc0_memset((char *)p + sz0, 0, sz1 - sz0);
    return p;
}

void *unc0_mmreallocz(Unc_Allocator *alloc, Unc_Alloc_Purpose purpose,
            void *ptr, size_t sz1) {
    if (ptr) {
        void *optr = (char *)ptr - sizeof(Unc_MaxAlign);
        size_t sz0 = *(size_t *)(optr);
        void *p = unc0_mmrealloc(alloc, purpose, ptr, sz1);
        if (p && sz1 > sz0) unc0_memset((char *)p + sz0, 0, sz1 - sz0);
        return p;
    } else {
        return unc0_mmallocz(alloc, purpose, sz1);
    }
}

#if DONT_CARE_ABOUT_SIZET_OVERFLOW
/* why? */
#define SIZET_MUL_OVERFLOWS(s, a, b) ((s) = (a) * (b), 0)
#elif defined(__GNUC__) || defined(__clang__)
#define SIZET_MUL_OVERFLOWS(s, a, b) __builtin_mul_overflow(a, b, &(s))
#elif defined(_MSC_VER) && SIZE_MAX == ULLONG_MAX && SIZE_MAX != ULONG_MAX
#define SIZET_MUL_OVERFLOWS(s, a, b) ((s) = (a) * (b),                         \
                                 __umulh((__int64)(a), (__int64)(b)))
#else
INLINE int unc0_checkzmulof(size_t a, size_t b) {
    return b && a != (a * b) / b;
}
#define SIZET_MUL_OVERFLOWS(s, a, b) ((s) = (a) * (b), unc0_checkzmulof(a, b))
#endif

void *unc0_tmalloc(Unc_Allocator *alloc, Unc_Alloc_Purpose purpose,
                                        size_t sz, size_t n) {
    size_t s;
    if (SIZET_MUL_OVERFLOWS(s, sz, n)) return NULL;
    return unc0_malloc(alloc, purpose, s);
}

void *unc0_tmrealloc(Unc_Allocator *alloc, Unc_Alloc_Purpose purpose, void *ptr,
                                        size_t sz, size_t n0, size_t n1) {
    size_t s0, s1;
    if (SIZET_MUL_OVERFLOWS(s0, sz, n0)) return NULL;
    if (SIZET_MUL_OVERFLOWS(s1, sz, n1)) return NULL;
    return unc0_mrealloc(alloc, purpose, ptr, s0, s1);
}

void unc0_tmfree(Unc_Allocator *alloc, void *ptr, size_t sz, size_t n) {
    unc0_mfree(alloc, ptr, sz * n);   
}

void *unc0_tmmalloc(Unc_Allocator *alloc, Unc_Alloc_Purpose purpose,
                                        size_t sz, size_t n) {
    size_t s;
    if (SIZET_MUL_OVERFLOWS(s, sz, n)) return NULL;
    return unc0_mmalloc(alloc, purpose, s);
}

void *unc0_tmmrealloc(Unc_Allocator *alloc, Unc_Alloc_Purpose purpose,
                             void *ptr, size_t sz, size_t n1) {
    size_t s1;
    if (SIZET_MUL_OVERFLOWS(s1, sz, n1)) return NULL;
    return unc0_mmrealloc(alloc, purpose, ptr, s1);
}

void *unc0_tmallocz(Unc_Allocator *alloc, Unc_Alloc_Purpose purpose,
                                        size_t sz, size_t n) {
    size_t s;
    if (SIZET_MUL_OVERFLOWS(s, sz, n)) return NULL;
    return unc0_mallocz(alloc, purpose, s);
}

void *unc0_tmreallocz(Unc_Allocator *alloc, Unc_Alloc_Purpose purpose,
                             void *ptr, size_t sz, size_t n0, size_t n1) {
    size_t s0, s1;
    if (SIZET_MUL_OVERFLOWS(s0, sz, n0)) return NULL;
    if (SIZET_MUL_OVERFLOWS(s1, sz, n1)) return NULL;
    return unc0_mreallocz(alloc, purpose, ptr, s0, s1);
}

void *unc0_tmmallocz(Unc_Allocator *alloc, Unc_Alloc_Purpose purpose,
                                        size_t sz, size_t n) {
    size_t s;
    if (SIZET_MUL_OVERFLOWS(s, sz, n)) return NULL;
    return unc0_mmallocz(alloc, purpose, s);
}

void *unc0_tmmreallocz(Unc_Allocator *alloc, Unc_Alloc_Purpose purpose,
                             void *ptr, size_t sz, size_t n) {
    size_t s1;
    if (SIZET_MUL_OVERFLOWS(s1, sz, n)) return NULL;
    return unc0_mmreallocz(alloc, purpose, ptr, s1);
}

size_t unc0_tmemcpy(void *dst, const void *src, size_t sz, size_t n) {
    size_t s;
    if (LIKELY(!SIZET_MUL_OVERFLOWS(s, sz, n))) {
        unc0_memcpy(dst, src, s);
    } else {
        size_t i;
        char *d = (char *)dst;
        const char *s = (const char *)src;
        for (i = 0; i < n; ++i) {
            unc0_memcpy(d, s, sz);
            d += sz, s += sz;
        }
    }
    return n;
}

size_t unc0_tmemmove(void *dst, const void *src, size_t sz, size_t n) {
    size_t s;
    if (LIKELY(!SIZET_MUL_OVERFLOWS(s, sz, n))) {
        unc0_memmove(dst, src, s);
    } else {
        size_t i;
        char *d = (char *)dst;
        const char *s = (const char *)src;
        for (i = 0; i < n; ++i) {
            unc0_memmove(d, s, sz);
            d += sz, s += sz;
        }
    }
    return n;
}

size_t unc0_strnlen(const char *s, size_t maxlen) {
#if _POSIX_C_SOURCE >= 200809L
    return strnlen(s, maxlen);
#else
    const char *p = unc0_memchr(s, 0, maxlen);
    return p ? p - s : maxlen;
#endif
}

int unc0_strput(Unc_Allocator *alloc, byte **o, Unc_Size *n, Unc_Size *c,
                    Unc_Size inc_log2, byte q) {
    if (*n >= *c) {
        Unc_Size inc = 1 << inc_log2;
        Unc_Size oc = *c, nc = oc + inc;
        byte *oo = *o, *on;
        nc = (nc + inc - 1) & ~(inc - 1);
        on = unc0_mrealloc(alloc, 0, oo, oc, nc);
        if (!on)
            return UNCIL_ERR_MEM;
        *c = nc, *o = on;
    }
    (*o)[(*n)++] = q;
    return 0;
}

/* (raw) string search */
const byte *unc0_strsearch(const byte *haystack, Unc_Size haystack_n,
                           const byte *needle, Unc_Size needle_n) {
    byte s0;
    const byte *next;
    if (!needle_n) return haystack;
    s0 = needle[0];
    haystack_n -= needle_n - 1;
    while ((next = unc0_memchr(haystack, s0, haystack_n))) {
        if (!unc0_memcmp(next + 1, needle + 1, needle_n - 1))
            return next;
        haystack_n -= next - haystack + 1;
        haystack = next + 1;
    }
    return NULL;
}

const byte *unc0_strsearchr(const byte *haystack, Unc_Size haystack_n,
                            const byte *needle, Unc_Size needle_n) {
    byte s0;
    const byte *next;
    if (!needle_n) return haystack;
    s0 = needle[0];
    haystack_n -= needle_n - 1;
    while ((next = unc0_memrchr(haystack, s0, haystack_n))) {
        if (!memcmp(next + 1, needle + 1, needle_n - 1))
            return next;
        haystack_n = next - haystack - 1;
    }
    return NULL;
}

int unc0_strputn(Unc_Allocator *alloc, byte **o, Unc_Size *n, Unc_Size *c,
                    Unc_Size inc_log2, Unc_Size qn, const byte *qq) {
    if (*n + qn > *c) {
        Unc_Size inc = 1 << inc_log2;
        Unc_Size oc = *c, nc = oc + (inc > qn ? inc : qn);
        byte *oo = *o, *on;
        nc = (nc + inc - 1) & ~(inc - 1);
        on = unc0_mrealloc(alloc, 0, oo, oc, nc);
        if (!on)
            return UNCIL_ERR_MEM;
        *c = nc, *o = on;
    }
    unc0_memcpy(*o + *n, qq, qn);
    *n += qn;
    return 0;
}

int unc0_sstrput(Unc_Allocator *alloc, byte **o, Unc_Size *n, Unc_Size *c,
                    Unc_Size inc_log2, byte q) {
    if (*n >= *c) {
        Unc_Size inc = 1 << inc_log2;
        Unc_Size oc = *c, nc = oc + inc;
        byte *oo = *o, *on;
        nc = (nc + inc - 1) & ~(inc - 1);
        on = unc0_mrealloc(alloc, Unc_AllocString, oo, oc, nc);
        if (!on)
            return UNCIL_ERR_MEM;
        *c = nc, *o = on;
    }
    (*o)[(*n)++] = q;
    return 0;
}

int unc0_sstrputn(Unc_Allocator *alloc, byte **o, Unc_Size *n, Unc_Size *c,
                    Unc_Size inc_log2, Unc_Size qn, const byte *qq) {
    if (*n + qn > *c) {
        Unc_Size inc = 1 << inc_log2;
        Unc_Size oc = *c, nc = oc + (inc > qn ? inc : qn);
        byte *oo = *o, *on;
        nc = (nc + inc - 1) & ~(inc - 1);
        on = unc0_mrealloc(alloc, Unc_AllocString, oo, oc, nc);
        if (!on)
            return UNCIL_ERR_MEM;
        *c = nc, *o = on;
    }
    unc0_memcpy(*o + *n, qq, qn);
    *n += qn;
    return 0;
}

int unc0_strpushb(Unc_Allocator *alloc, byte **o, Unc_Size *n, Unc_Size *c,
                    Unc_Size inc_log2, Unc_Size qn, const byte *qq) {
    if (!qn) return 0;
    if (*n + qn > *c) {
        Unc_Size inc = 1 << inc_log2;
        Unc_Size oc = *c, nc = oc + (inc > qn ? inc : qn);
        byte *oo = *o, *on;
        nc = (nc + inc - 1) & ~(inc - 1);
        on = unc0_mmrealloc(alloc, Unc_AllocBlob, oo, nc);
        if (!on)
            return UNCIL_ERR_MEM;
        *c = nc, *o = on;
    }
    unc0_memcpy(*o + *n, qq, qn);
    *n += qn;
    return 0;
}

int unc0_strpush(Unc_Allocator *alloc, byte **o, Unc_Size *n, Unc_Size *c,
                    Unc_Size inc_log2, Unc_Size qn, const byte *qq) {
    if (!qn) return 0;
    if (*n + qn > *c) {
        Unc_Size inc = 1 << inc_log2;
        Unc_Size oc = *c, nc = oc + (inc > qn ? inc : qn);
        byte *oo = *o, *on;
        nc = (nc + inc - 1) & ~(inc - 1);
        on = unc0_mmrealloc(alloc, Unc_AllocString, oo, nc);
        if (!on)
            return UNCIL_ERR_MEM;
        *c = nc, *o = on;
    }
    unc0_memcpy(*o + *n, qq, qn);
    *n += qn;
    return 0;
}

int unc0_strpush1(Unc_Allocator *alloc, byte **o, Unc_Size *n, Unc_Size *c,
                    Unc_Size inc_log2, byte q) {
    return unc0_strpush(alloc, o, n, c, inc_log2, 1, &q);
}

int unc0_strpushrv(Unc_Allocator *alloc, byte **o, Unc_Size *n, Unc_Size *c,
                   Unc_Size inc_log2, Unc_Size qn, const byte *qq) {
    if (!qn) return 0;
    if (*n + qn > *c) {
        Unc_Size inc = 1 << inc_log2;
        Unc_Size oc = *c, nc = oc + (inc > qn ? inc : qn);
        byte *oo = *o, *on;
        nc = (nc + inc - 1) & ~(inc - 1);
        on = unc0_mmrealloc(alloc, Unc_AllocString, oo, nc);
        if (!on)
            return UNCIL_ERR_MEM;
        *c = nc, *o = on;
    }
    unc0_memcpy(*o + *n, qq, qn);
    unc0_memrev(*o + *n, qn);
    *n += qn;
    return 0;
}
