/*******************************************************************************
 
Uncil -- memory management impl

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

#define _POSIX_C_SOURCE 200809L

#include <limits.h>
#include <stddef.h>

#include <stdlib.h>

#define UNCIL_DEFINES

#include "ucompdef.h"
#include "udebug.h"
#include "ugc.h"
#include "umem.h"
#include "umt.h"

#if !UNCIL_NOLIBC
#include <string.h>
#endif

#if !UNCIL_NOSTDALLOC
/* define UNCIL_STDALLOC_NOT_THREADSAFE if malloc/realloc/free
                                            is not thread-safe */
#if UNCIL_MT_OK && UNCIL_STDALLOC_NOT_THREADSAFE
UNC_LOCKSTATICL(unc0_stdalloc_lock)
#endif

void *unc0_stdalloc(void *udata, Unc_Alloc_Purpose purpose,
                    size_t oldsize, size_t newsize, void *ptr) {
    (void)udata; (void)purpose; (void)oldsize;
    DEBUGPRINT(ALLOC, ("%p (%lu => %lu) => ", ptr, oldsize, newsize));

#if UNCIL_MT_OK && UNCIL_STDALLOC_NOT_THREADSAFE
    UNC_LOCKL(unc0_stdalloc_lock);
#endif
    if (!newsize) {
        free(ptr);
        ptr = NULL;
    } else {
        void *p = realloc(ptr, newsize);
        if (p || newsize > oldsize) ptr = p;
    }
#if UNCIL_MT_OK && UNCIL_STDALLOC_NOT_THREADSAFE
    UNC_UNLOCKL(unc0_stdalloc_lock);
#endif

    DEBUGPRINT(ALLOC, ("%p\n", ptr));
    return ptr;
}
#endif /* !UNCIL_NOSTDALLOC */

#if UNCIL_NOLIBC

void unc0_memcpy_f(void *dst, const void *src, size_t sz) {
    const unsigned char *s = src;
    unsigned char *d = dst;
    while (sz--) *d++ = *s++;
}

void unc0_memcpy_b(void *dst, const void *src, size_t sz) {
    const unsigned char *s = src;
    unsigned char *d = dst;
    s += sz, d += sz;
    while (sz--) *--d = *--s;
}

size_t unc0_memcpy(void *dst, const void *src, size_t sz) {
    unc0_memcpy_f(dst, src, sz);
    return sz;
}

size_t unc0_memmove(void *dst, const void *src, size_t sz) {
    if (dst > src)
        unc0_memcpy_b(dst, src, sz);
    else
        unc0_memcpy_f(dst, src, sz);
    return sz;
}

void *unc0_memchr(const void *m, int c, size_t sz) {
    const unsigned char *p = m, x = (unsigned char)c;
    size_t i;
    for (i = 0; i < sz; ++i)
        if (p[i] == x)
            return (void *)&p[i];
    return NULL;
}

size_t unc0_memset(void *dst, int c, size_t sz) {
    unsigned char *a = dst, x = (unsigned char)c;
    size_t i;
    for (i = 0; i < sz; ++i)
        a[i] = x;
    return sz;
}

int unc0_memcmp(const void *dst, const void *src, size_t sz) {
    const unsigned char *a = dst, *b = src;
    unsigned char x, y;
    size_t i;
    for (i = 0; i < sz; ++i) {
        x = a[i], y = b[i];
        if (x != y) return (int)x - (int)y;
    }
    return 0;
}

size_t unc0_strlen(const char *s) {
    size_t l = 0;
    while (*s++) ++l;
    return l;
}

void unc0_strcpy(char *dest, const char *src) {
    while (*src) *dest++ = *src++;
    *dest = *src;
}

int unc0_strcmp(const char *dest, const char *src) {
    while (*src) {
        int a = *src++, b = *dest++;
        if (a != b) return a - b;
    }
    return (int)*dest;
}

#elif !UNCIL_MEMOP_INLINE /* UNCIL_NOLIBC */

size_t unc0_memcpy(void *dst, const void *src, size_t sz) {
    memcpy(dst, src, sz);
    return sz;
}

size_t unc0_memmove(void *dst, const void *src, size_t sz) {
    memmove(dst, src, sz);
    return sz;
}

size_t unc0_memset(void *dst, int c, size_t sz) {
    memset(dst, c, sz);
    return sz;
}

int unc0_memcmp(const void *dst, const void *src, size_t sz) {
    return memcmp(dst, src, sz);
}

void *unc0_memchr(const void *m, int c, size_t sz) {
    return memchr(m, c, sz);
}

size_t unc0_strlen(const char *s) {
    return strlen(s);
}

void unc0_strcpy(char *dest, const char *src) {
    strcpy(dest, src);
}

int unc0_strcmp(const char *dest, const char *src) {
    return strcmp(dest, src);
}

#endif

size_t unc0_strnlen(const char *s, size_t maxlen) {
#if !UNCIL_NOLIBC && _POSIX_VERSION >= 200809L
    return strnlen(s, maxlen);
#else
    const char *p = unc0_memchr(s, 0, maxlen);
    return p ? p - s : maxlen;
#endif
}

size_t unc0_memsetv(void *dst, int c, size_t sz) {
#if !UNCIL_NOLIBC && UNCIL_C23
    memset_explicit(dst, c, sz);
#elif !UNCIL_NOLIBC && UNCIL_C11 && __STDC_LIB_EXT1__
    memset_s(dst, sz, c, sz);
#else
    volatile unsigned char *p = (volatile unsigned char *)dst;
    unsigned char x = (unsigned char)c;
    size_t i;
    for (i = 0; i < sz; ++i) p[i] = x;
#endif
    return sz;
}

void *unc0_memrchr(const void *p, int c, size_t n) {
    unsigned char *b = (unsigned char *)p + n;
    while (n--)
        if (*--b == c)
            return b;
    return NULL;
}

void unc0_memrev(void *dst, size_t sz) {
    byte *p0 = dst, *p1 = p0 + sz - 1, tmp;
    while (p0 < p1) {
        tmp = *p0;
        *p0++ = *p1;
        *p1-- = tmp;
    }
}

Unc_RetVal unc0_initalloc(Unc_Allocator *alloc, struct Unc_World *w,
                          Unc_Alloc fn, void *data) {
    alloc->world = w;
    if (!fn) {
#if UNCIL_NOSTDALLOC
        return UNCIL_ERR_MEM;
#else
#if UNCIL_MT_OK && UNCIL_STDALLOC_NOT_THREADSAFE
        if (UNC_LOCKINITL(unc0_stdalloc_lock))
            return UNCIL_ERR_MEM;
#endif
        fn = unc0_stdalloc;
#endif
    }
    alloc->fn = fn;
    alloc->data = data;
    alloc->total = 0;
    return 0;
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

void *unc0_mrealloc(Unc_Allocator *alloc, Unc_Alloc_Purpose purpose,
                    void *optr, size_t sz0, size_t sz1) {
    return unc0_invokealloc(alloc, purpose, optr, sz0, sz1);
}

void unc0_mfree(Unc_Allocator *alloc, void *ptr, size_t sz) {
    if (ptr) unc0_invokealloc(alloc, Unc_AllocOther, ptr, sz, 0);
}

void *unc0_mallocz(Unc_Allocator *alloc, Unc_Alloc_Purpose purpose,
                   size_t sz) {
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

void *unc0_mmalloc(Unc_Allocator *alloc, Unc_Alloc_Purpose purpose,
                   size_t sz) {
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

void *unc0_mreallocz(Unc_Allocator *alloc, Unc_Alloc_Purpose purpose,
                     void *ptr, size_t sz0, size_t sz1) {
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
#elif defined(_MSC_VER) && defined(_WIN64)
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

void *unc0_tmrealloc(Unc_Allocator *alloc, Unc_Alloc_Purpose purpose,
                     void *ptr, size_t sz, size_t n0, size_t n1) {
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
        if (!unc0_memcmp(next + 1, needle + 1, needle_n - 1))
            return next;
        haystack_n = next - haystack - 1;
    }
    return NULL;
}

void unc0_strbuf_init(struct unc0_strbuf *buf, Unc_Allocator *alloc,
                      Unc_Alloc_Purpose purpose) {
    buf->buffer = NULL;
    buf->length = buf->capacity = 0;
    buf->alloc = alloc;
    buf->purpose = purpose;
}

void unc0_strbuf_init_blank(struct unc0_strbuf *buf) {
    buf->buffer = NULL;
    buf->length = 0;
    buf->alloc = NULL;
    buf->capacity = 0;
    buf->purpose = 0;
}

void unc0_strbuf_init_fixed(struct unc0_strbuf *buf,
                            Unc_Size n, Unc_Byte *tmp) {
    buf->buffer = tmp;
    buf->length = 0;
    buf->alloc = NULL;
    buf->capacity = n;
    buf->purpose = 0;
}

void unc0_strbuf_init_forswap(struct unc0_strbuf *buf,
                              const struct unc0_strbuf *src) {
    buf->buffer = NULL;
    buf->length = buf->capacity = 0;
    buf->alloc = src->alloc;
    buf->purpose = src->purpose;
}

void unc0_strbuf_swap(struct unc0_strbuf *buf, struct unc0_strbuf *src) {
    if (buf->buffer != src->buffer) {
        unc0_strbuf_free(buf);
        unc0_memcpy(buf, src, sizeof(*buf));
        src->buffer = NULL;
        src->length = src->capacity = 0;
    }
}

INLINE Unc_Byte *unc0_strbuf_reserve(struct unc0_strbuf *buf, Unc_Size n) {
    if (n && buf->length + n > buf->capacity) {
        Unc_Size inc = 1 << 6;
        Unc_Size oc = buf->capacity, nc = oc + (inc > n ? inc : n);
        byte *oo = buf->buffer, *on;
        nc = (nc + inc - 1) & ~(inc - 1);
        on = unc0_mrealloc(buf->alloc, buf->purpose, oo, oc, nc);
        if (!on) return NULL;
        buf->capacity = nc;
        return (buf->buffer = on);
    } else {
        return buf->buffer;
    }
}

INLINE Unc_Byte *unc0_strbuf_do_reserve_managed(struct unc0_strbuf *buf,
                                             Unc_Size n) {
    if (n && buf->length + n > buf->capacity) {
        Unc_Size inc = 1 << 6;
        Unc_Size oc = buf->capacity, nc = oc + (inc > n ? inc : n);
        byte *oo = buf->buffer, *on;
        nc = (nc + inc - 1) & ~(inc - 1);
        on = unc0_mmrealloc(buf->alloc, buf->purpose, oo, nc);
        if (!on) return NULL;
        buf->capacity = nc;
        return (buf->buffer = on);
    } else {
        return buf->buffer;
    }
}

Unc_Byte *unc0_strbuf_reserve_managed(struct unc0_strbuf *buf, Unc_Size n) {
    Unc_Byte *out = unc0_strbuf_do_reserve_managed(buf, n);
    Unc_Size length;
    if (UNLIKELY(!out)) return NULL;
    length = buf->length;
    buf->length += n;
    return &buf->buffer[length];
}

Unc_Byte *unc0_strbuf_reserve_next(struct unc0_strbuf *buf, Unc_Size n) {
    Unc_Byte *out = unc0_strbuf_reserve(buf, n);
    Unc_Size length;
    if (UNLIKELY(!out)) return NULL;
    length = buf->length;
    buf->length += n;
    return &buf->buffer[length];
}

Unc_Byte *unc0_strbuf_reserve_clear(struct unc0_strbuf *buf, Unc_Size n) {
    buf->length = 0;
    return unc0_strbuf_reserve_next(buf, n);
}

Unc_RetVal unc0_strbuf_putn(struct unc0_strbuf *buf,
                            Unc_Size strn, const Unc_Byte *str) {
    if (LIKELY(strn)) {
        Unc_Byte *out = unc0_strbuf_reserve_next(buf, strn);
        if (UNLIKELY(!out)) return UNCIL_ERR_MEM;
        unc0_memcpy(out, str, strn);
    }
    return 0;
}

Unc_RetVal unc0_strbuf_putn_rv(struct unc0_strbuf *buf,
                               Unc_Size strn, const Unc_Byte *str) {
    if (LIKELY(strn)) {
        Unc_Byte *out = unc0_strbuf_reserve_next(buf, strn);
        if (UNLIKELY(!out)) return UNCIL_ERR_MEM;
        unc0_memcpy(out, str, strn);
        unc0_memrev(out, strn);
    }
    return 0;
}

Unc_RetVal unc0_strbuf_putfill(struct unc0_strbuf *buf,
                               Unc_Size n, Unc_Byte c) {
    if (LIKELY(n)) {
        Unc_Byte *out = unc0_strbuf_reserve_next(buf, n);
        if (UNLIKELY(!out)) return UNCIL_ERR_MEM;
        unc0_memset(out, c, n);
    }
    return 0;
}

Unc_RetVal unc0_strbuf_put1(struct unc0_strbuf *buf, Unc_Byte c) {
    return unc0_strbuf_putfill(buf, 1, c);
}

Unc_RetVal unc0_strbuf_putn_managed(struct unc0_strbuf *buf, Unc_Size strn,
                                    const Unc_Byte *str) {
    if (LIKELY(strn)) {
        Unc_Byte *out = unc0_strbuf_reserve_managed(buf, strn);
        if (UNLIKELY(!out)) return UNCIL_ERR_MEM;
        unc0_memcpy(out, str, strn);
    }
    return 0;
}

Unc_RetVal unc0_strbuf_put1_managed(struct unc0_strbuf *buf, Unc_Byte c) {
    return unc0_strbuf_putn_managed(buf, 1, &c);
}

void unc0_strbuf_compact(struct unc0_strbuf *buf) {
    if (buf->buffer && buf->length < buf->capacity) {
        buf->buffer = unc0_mrealloc(buf->alloc, buf->purpose,
                        buf->buffer, buf->capacity, buf->length);
        buf->capacity = buf->length;
    }
}

void unc0_strbuf_compact_managed(struct unc0_strbuf *buf) {
    if (buf->buffer && buf->length < buf->capacity) {
        buf->buffer = unc0_mmrealloc(buf->alloc, buf->purpose,
                        buf->buffer, buf->length);
        buf->capacity = buf->length;
    }
}

void unc0_strbuf_free(struct unc0_strbuf *buf) {
    if (buf->buffer) {
        unc0_mfree(buf->alloc, buf->buffer, buf->capacity);
        buf->buffer = NULL;
        buf->length = buf->capacity = 0;
    }
}
