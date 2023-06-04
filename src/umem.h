/*******************************************************************************
 
Uncil -- memory management (internal headers)

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

#ifndef UNCIL_UMEM_H
#define UNCIL_UMEM_H

#include "ualloc.h"
#include "udef.h"

struct Unc_World;

typedef struct Unc_Allocator {
    struct Unc_World *world;
    Unc_Alloc fn;
    void *data;
    Unc_Size total;
} Unc_Allocator;

#if defined(__GNUC__)
#define UNCIL_NOIGNORE_RET __attribute__((warn_unused_result))
#else
#define UNCIL_NOIGNORE_RET
#endif

#if UNCIL_LIB_MIMALLOC
#include <mimalloc-override.h>
#endif

#define UNCIL_MEMOP_INLINE 0

Unc_RetVal unc0_initalloc(Unc_Allocator *alloc, struct Unc_World *w,
                          Unc_Alloc fn, void *data);
UNCIL_NOIGNORE_RET
void *unc0_malloc(Unc_Allocator *alloc, Unc_Alloc_Purpose purpose, size_t sz);
UNCIL_NOIGNORE_RET
void *unc0_mrealloc(Unc_Allocator *alloc, Unc_Alloc_Purpose purpose, void *ptr,
                                                    size_t sz0, size_t sz1);
void unc0_mfree(Unc_Allocator *alloc, void *ptr, size_t sz);

UNCIL_NOIGNORE_RET
void *unc0_mmalloc(Unc_Allocator *alloc, Unc_Alloc_Purpose purpose, size_t sz);
UNCIL_NOIGNORE_RET
void *unc0_mmrealloc(Unc_Allocator *alloc, Unc_Alloc_Purpose purpose,
                     void *ptr, size_t sz);
void unc0_mmfree(Unc_Allocator *alloc, void *ptr);

UNCIL_NOIGNORE_RET
void *unc0_mallocz(Unc_Allocator *alloc, Unc_Alloc_Purpose purpose, size_t sz);
UNCIL_NOIGNORE_RET
void *unc0_mreallocz(Unc_Allocator *alloc, Unc_Alloc_Purpose purpose,
                     void *ptr, size_t sz0, size_t sz1);
UNCIL_NOIGNORE_RET
void *unc0_mmallocz(Unc_Allocator *alloc, Unc_Alloc_Purpose purpose,
                    size_t sz);
UNCIL_NOIGNORE_RET
void *unc0_mmreallocz(Unc_Allocator *alloc, Unc_Alloc_Purpose purpose,
                                                        void *ptr, size_t sz);
void unc0_mmfree(Unc_Allocator *alloc, void *ptr);

UNCIL_NOIGNORE_RET
void *unc0_tmalloc(Unc_Allocator *alloc, Unc_Alloc_Purpose purpose,
                                        size_t sz, size_t n);
UNCIL_NOIGNORE_RET
void *unc0_tmrealloc(Unc_Allocator *alloc, Unc_Alloc_Purpose purpose,
                     void *ptr, size_t sz, size_t n0, size_t n1);
void unc0_tmfree(Unc_Allocator *alloc, void *ptr, size_t sz, size_t n);

UNCIL_NOIGNORE_RET
void *unc0_tmmalloc(Unc_Allocator *alloc, Unc_Alloc_Purpose purpose,
                                        size_t sz, size_t n);
UNCIL_NOIGNORE_RET
void *unc0_tmmrealloc(Unc_Allocator *alloc, Unc_Alloc_Purpose purpose,
                             void *ptr, size_t sz, size_t n1);

UNCIL_NOIGNORE_RET
void *unc0_tmallocz(Unc_Allocator *alloc, Unc_Alloc_Purpose purpose,
                                        size_t sz, size_t n);
UNCIL_NOIGNORE_RET
void *unc0_tmreallocz(Unc_Allocator *alloc, Unc_Alloc_Purpose purpose,
                             void *ptr, size_t sz, size_t n0, size_t n1);
UNCIL_NOIGNORE_RET
void *unc0_tmmallocz(Unc_Allocator *alloc, Unc_Alloc_Purpose purpose,
                                        size_t sz, size_t n);
UNCIL_NOIGNORE_RET
void *unc0_tmmreallocz(Unc_Allocator *alloc, Unc_Alloc_Purpose purpose,
                             void *ptr, size_t sz, size_t n);

size_t unc0_mmgetsize(Unc_Allocator *alloc, void *ptr);
void *unc0_mmunwind(Unc_Allocator *alloc, void *ptr, size_t *out);
void *unc0_mmunwinds(Unc_Allocator *alloc, void *ptr, size_t *out);

#if UNCIL_MEMOP_INLINE
#include <string.h>
#define unc0_memset(dst, c, sz) (memset(dst, c, sz), sz)
#define unc0_memcpy(dst, src, sz) (memcpy(dst, src, sz), sz)
#define unc0_memmove(dst, src, sz) (memmove(dst, src, sz), sz)
#define unc0_memcmp(dst, src, sz) memcmp(dst, src, sz)
#define unc0_memchr(m, c, sz) memchr(m, c, sz)
#define unc0_strlen(s) strlen(s)
#define unc0_strcpy(dest, src) strcpy(dest, src)
#define unc0_strcmp(dest, src) strcmp(dest, src)
#else
size_t unc0_memset(void *dst, int c, size_t sz);
size_t unc0_memcpy(void *dst, const void *src, size_t sz);
size_t unc0_memmove(void *dst, const void *src, size_t sz);
int unc0_memcmp(const void *dst, const void *src, size_t sz);
void *unc0_memchr(const void *m, int c, size_t sz);
size_t unc0_strlen(const char *s);
void unc0_strcpy(char *dest, const char *src);
int unc0_strcmp(const char *dest, const char *src);
#endif

#define unc0_mbzero(dst, sz) unc0_memset(dst, 0, sz)
#define unc0_mbzerov(dst, sz) unc0_memsetv(dst, 0, sz)

size_t unc0_memsetv(void *dst, int c, size_t sz);
void unc0_memrev(void *dst, size_t sz);
void *unc0_memrchr(const void *p, int c, size_t n);
size_t unc0_strnlen(const char *s, size_t maxlen);

size_t unc0_tmemcpy(void *dst, const void *src, size_t sz, size_t n);
size_t unc0_tmemmove(void *dst, const void *src, size_t sz, size_t n);
size_t unc0_tmemrev(void *dst, const void *src, size_t sz, size_t n);

const byte *unc0_strsearch(const byte *haystack, Unc_Size haystack_n,
                           const byte *needle, Unc_Size needle_n);
const byte *unc0_strsearchr(const byte *haystack, Unc_Size haystack_n,
                            const byte *needle, Unc_Size needle_n);

struct unc0_strbuf {
    Unc_Byte *buffer;
    Unc_Size length;
    Unc_Size capacity;
    Unc_Allocator *alloc;
    Unc_Alloc_Purpose purpose;
};

void unc0_strbuf_init(struct unc0_strbuf *buf, Unc_Allocator *alloc,
                      Unc_Alloc_Purpose purpose);
void unc0_strbuf_init_blank(struct unc0_strbuf *buf);
void unc0_strbuf_init_fixed(struct unc0_strbuf *buf,
                            Unc_Size n, Unc_Byte *tmp);
void unc0_strbuf_init_forswap(struct unc0_strbuf *buf,
                              const struct unc0_strbuf *src);
Unc_Byte *unc0_strbuf_reserve_next(struct unc0_strbuf *buf, Unc_Size n);
Unc_Byte *unc0_strbuf_reserve_clear(struct unc0_strbuf *buf, Unc_Size n);
Unc_RetVal unc0_strbuf_put1(struct unc0_strbuf *buf, Unc_Byte c);
Unc_RetVal unc0_strbuf_putn(struct unc0_strbuf *buf, Unc_Size strn,
                            const Unc_Byte *str);
Unc_RetVal unc0_strbuf_putfill(struct unc0_strbuf *buf,
                               Unc_Size n, Unc_Byte c);
Unc_RetVal unc0_strbuf_putn_rv(struct unc0_strbuf *buf, Unc_Size strn,
                               const Unc_Byte *str);
void unc0_strbuf_swap(struct unc0_strbuf *buf, struct unc0_strbuf *src);
void unc0_strbuf_compact(struct unc0_strbuf *buf);
void unc0_strbuf_free(struct unc0_strbuf *buf);

/* do not mix managed and unmanaged calls!! */
Unc_Byte *unc0_strbuf_reserve_managed(struct unc0_strbuf *buf, Unc_Size n);
Unc_RetVal unc0_strbuf_putn_managed(struct unc0_strbuf *buf, Unc_Size strn,
                                    const Unc_Byte *str);
Unc_RetVal unc0_strbuf_put1_managed(struct unc0_strbuf *buf, Unc_Byte c);
void unc0_strbuf_compact_managed(struct unc0_strbuf *buf);

#ifdef UNCIL_DEFINES
#define TMALLOC(T, A, purpose, n) \
    ((T *)(unc0_tmalloc(A, purpose, sizeof(T), n)))
#define TMALLOCZ(T, A, purpose, n) \
    ((T *)(unc0_tmallocz(A, purpose, sizeof(T), n)))
#define TMREALLOC(T, A, purpose, p, n, z) \
    ((T *)(unc0_tmrealloc(A, purpose, p, sizeof(T), n, z)))
#define TMREALLOCZ(T, A, purpose, p, n, z) \
    ((T *)(unc0_tmreallocz(A, purpose, p, sizeof(T), n, z)))
#define TMFREE(T, A, ptr, n) \
    (unc0_tmfree(A, (T *)ptr, sizeof(T), n))

#define TMMALLOC(T, A, purpose, n) \
    ((T *)(unc0_tmmalloc(A, purpose, sizeof(T), n)))
#define TMMALLOCZ(T, A, purpose, n) \
    ((T *)(unc0_tmmallocz(A, purpose, sizeof(T), n)))
#define TMMREALLOC(T, A, purpose, p, z) \
    ((T *)(unc0_tmmrealloc(A, purpose, sizeof(T), p, z)))
#define TMMREALLOCZ(T, A, purpose, p, z) \
    ((T *)(unc0_tmmreallocz(A, purpose, sizeof(T), p, z)))
#define TMMFREE(T, A, ptr) \
    (unc0_mmfree(A, (T *)ptr))
    
#define TMEMCPY(T, dst, src, n) (unc0_tmemcpy(dst, src, sizeof(T), n))
#define TMEMMOVE(T, dst, src, n) (unc0_tmemmove(dst, src, sizeof(T), n))
#endif

#endif /* UNCIL_UMEM_H */
