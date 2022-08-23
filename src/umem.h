/*******************************************************************************
 
Uncil -- memory management (internal headers)

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

#ifndef UNCIL_UMEM_H
#define UNCIL_UMEM_H

#include <signal.h>

#include "ualloc.h"
#include "udef.h"

#define UNCIL_UMEM_USEMEMCPY 1

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

void unc0_initalloc(Unc_Allocator *alloc, struct Unc_World *w,
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
void *unc0_mmrealloc(Unc_Allocator *alloc, Unc_Alloc_Purpose purpose, void *ptr,
                                                                    size_t sz);
void unc0_mmfree(Unc_Allocator *alloc, void *ptr);

UNCIL_NOIGNORE_RET
void *unc0_mallocz(Unc_Allocator *alloc, Unc_Alloc_Purpose purpose, size_t sz);
UNCIL_NOIGNORE_RET
void *unc0_mreallocz(Unc_Allocator *alloc, Unc_Alloc_Purpose purpose, void *ptr,
                                                        size_t sz0, size_t sz1);
UNCIL_NOIGNORE_RET
void *unc0_mmallocz(Unc_Allocator *alloc, Unc_Alloc_Purpose purpose, size_t sz);
UNCIL_NOIGNORE_RET
void *unc0_mmreallocz(Unc_Allocator *alloc, Unc_Alloc_Purpose purpose,
                                                        void *ptr, size_t sz);
void unc0_mmfree(Unc_Allocator *alloc, void *ptr);

size_t unc0_mmgetsize(Unc_Allocator *alloc, void *ptr);
void *unc0_mmunwind(Unc_Allocator *alloc, void *ptr, size_t *out);
void *unc0_mmunwinds(Unc_Allocator *alloc, void *ptr, size_t *out);

void *unc0_memchr(const void *m, int c, size_t sz);
size_t unc0_memset(void *dst, int c, size_t sz);
size_t unc0_memcpy(void *dst, const void *src, size_t sz);
size_t unc0_memmove(void *dst, const void *src, size_t sz);
int unc0_memcmp(const void *dst, const void *src, size_t sz);
void unc0_memrev(void *dst, size_t sz);
void *unc0_memrchr(const void *p, int c, size_t n);
const byte *unc0_strsearch(const byte *haystack, Unc_Size haystack_n,
                           const byte *needle, Unc_Size needle_n);
const byte *unc0_strsearchr(const byte *haystack, Unc_Size haystack_n,
                            const byte *needle, Unc_Size needle_n);

size_t unc0_strnlen(const char *s, size_t maxlen);

int unc0_strput(Unc_Allocator *alloc, Unc_Byte **o, Unc_Size *n, Unc_Size *c,
                    Unc_Size inc_log2, Unc_Byte q);
int unc0_strputn(Unc_Allocator *alloc, Unc_Byte **o, Unc_Size *n, Unc_Size *c,
                    Unc_Size inc_log2, Unc_Size qn, const Unc_Byte *qq);
int unc0_sstrput(Unc_Allocator *alloc, Unc_Byte **o, Unc_Size *n, Unc_Size *c,
                    Unc_Size inc_log2, Unc_Byte q);
int unc0_sstrputn(Unc_Allocator *alloc, Unc_Byte **o, Unc_Size *n, Unc_Size *c,
                    Unc_Size inc_log2, Unc_Size qn, const Unc_Byte *qq);
int unc0_strpushb(Unc_Allocator *alloc, Unc_Byte **o, Unc_Size *n, Unc_Size *c,
                    Unc_Size inc_log2, Unc_Size qn, const Unc_Byte *qq);
int unc0_strpush(Unc_Allocator *alloc, Unc_Byte **o, Unc_Size *n, Unc_Size *c,
                    Unc_Size inc_log2, Unc_Size qn, const Unc_Byte *qq);
int unc0_strpush1(Unc_Allocator *alloc, Unc_Byte **o, Unc_Size *n, Unc_Size *c,
                    Unc_Size inc_log2, Unc_Byte q);
int unc0_strpushrv(Unc_Allocator *alloc, Unc_Byte **o, Unc_Size *n, Unc_Size *c,
                    Unc_Size inc_log2, Unc_Size qn, const Unc_Byte *qq);

#ifdef UNCIL_DEFINES
#define TMALLOC(T, A, purpose, n) \
    ((T *)(unc0_malloc(A, purpose, (n) * sizeof(T))))
#define TMALLOCZ(T, A, purpose, n) \
    ((T *)(unc0_mallocz(A, purpose, (n) * sizeof(T))))
#define TMREALLOC(T, A, purpose, p, n, z) \
    ((T *)(unc0_mrealloc(A, purpose, p, (n) * sizeof(T), (z) * sizeof(T))))
#define TMREALLOCZ(T, A, purpose, p, n, z) \
    ((T *)(unc0_mreallocz(A, purpose, p, (n) * sizeof(T), (z) * sizeof(T))))
#define TMFREE(T, A, ptr, n) \
    (unc0_mfree(A, (T *)ptr, (n) * sizeof(T)))

#define TMMALLOC(T, A, purpose, n) \
    ((T *)(unc0_mmalloc(A, purpose, (n) * sizeof(T))))
#define TMMALLOCZ(T, A, purpose, n) \
    ((T *)(unc0_mmallocz(A, purpose, (n) * sizeof(T))))
#define TMMREALLOC(T, A, purpose, p, z) \
    ((T *)(unc0_mmrealloc(A, purpose, p, (z) * sizeof(T))))
#define TMMREALLOCZ(T, A, purpose, p, z) \
    ((T *)(unc0_mmreallocz(A, purpose, p, (z) * sizeof(T))))
#define TMMFREE(T, A, ptr) \
    (unc0_mmfree(A, (T *)ptr))
#endif

#endif /* UNCIL_UMEM_H */
