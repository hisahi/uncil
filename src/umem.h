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

void unc__initalloc(Unc_Allocator *alloc, struct Unc_World *w,
                    Unc_Alloc fn, void *data);
UNCIL_NOIGNORE_RET
void *unc__malloc(Unc_Allocator *alloc, Unc_Alloc_Purpose purpose, size_t sz);
UNCIL_NOIGNORE_RET
void *unc__mrealloc(Unc_Allocator *alloc, Unc_Alloc_Purpose purpose, void *ptr,
                                                        size_t sz0, size_t sz1);
void unc__mfree(Unc_Allocator *alloc, void *ptr, size_t sz);

UNCIL_NOIGNORE_RET
void *unc__mmalloc(Unc_Allocator *alloc, Unc_Alloc_Purpose purpose, size_t sz);
UNCIL_NOIGNORE_RET
void *unc__mmrealloc(Unc_Allocator *alloc, Unc_Alloc_Purpose purpose, void *ptr,
                                                                    size_t sz);
void unc__mmfree(Unc_Allocator *alloc, void *ptr);

UNCIL_NOIGNORE_RET
void *unc__mallocz(Unc_Allocator *alloc, Unc_Alloc_Purpose purpose, size_t sz);
UNCIL_NOIGNORE_RET
void *unc__mreallocz(Unc_Allocator *alloc, Unc_Alloc_Purpose purpose, void *ptr,
                                                        size_t sz0, size_t sz1);
UNCIL_NOIGNORE_RET
void *unc__mmallocz(Unc_Allocator *alloc, Unc_Alloc_Purpose purpose, size_t sz);
UNCIL_NOIGNORE_RET
void *unc__mmreallocz(Unc_Allocator *alloc, Unc_Alloc_Purpose purpose,
                                                        void *ptr, size_t sz);
void unc__mmfree(Unc_Allocator *alloc, void *ptr);

size_t unc__mmgetsize(Unc_Allocator *alloc, void *ptr);
void *unc__mmunwind(Unc_Allocator *alloc, void *ptr, size_t *out);
void *unc__mmunwinds(Unc_Allocator *alloc, void *ptr, size_t *out);

void *unc__memchr(const void *m, int c, size_t sz);
size_t unc__memset(void *dst, int c, size_t sz);
size_t unc__memcpy(void *dst, const void *src, size_t sz);
size_t unc__memmove(void *dst, const void *src, size_t sz);
int unc__memcmp(const void *dst, const void *src, size_t sz);
void unc__memrev(void *dst, size_t sz);
void *unc__memrchr(const void *p, int c, size_t n);
const byte *unc__strsearch(const byte *haystack, Unc_Size haystack_n,
                           const byte *needle, Unc_Size needle_n);
const byte *unc__strsearchr(const byte *haystack, Unc_Size haystack_n,
                            const byte *needle, Unc_Size needle_n);

size_t unc__strnlen(const char *s, size_t maxlen);

int unc__strput(Unc_Allocator *alloc, Unc_Byte **o, Unc_Size *n, Unc_Size *c,
                    Unc_Size inc_log2, Unc_Byte q);
int unc__strputn(Unc_Allocator *alloc, Unc_Byte **o, Unc_Size *n, Unc_Size *c,
                    Unc_Size inc_log2, Unc_Size qn, const Unc_Byte *qq);
int unc__sstrput(Unc_Allocator *alloc, Unc_Byte **o, Unc_Size *n, Unc_Size *c,
                    Unc_Size inc_log2, Unc_Byte q);
int unc__sstrputn(Unc_Allocator *alloc, Unc_Byte **o, Unc_Size *n, Unc_Size *c,
                    Unc_Size inc_log2, Unc_Size qn, const Unc_Byte *qq);
int unc__strpushb(Unc_Allocator *alloc, Unc_Byte **o, Unc_Size *n, Unc_Size *c,
                    Unc_Size inc_log2, Unc_Size qn, const Unc_Byte *qq);
int unc__strpush(Unc_Allocator *alloc, Unc_Byte **o, Unc_Size *n, Unc_Size *c,
                    Unc_Size inc_log2, Unc_Size qn, const Unc_Byte *qq);
int unc__strpush1(Unc_Allocator *alloc, Unc_Byte **o, Unc_Size *n, Unc_Size *c,
                    Unc_Size inc_log2, Unc_Byte q);
int unc__strpushrv(Unc_Allocator *alloc, Unc_Byte **o, Unc_Size *n, Unc_Size *c,
                    Unc_Size inc_log2, Unc_Size qn, const Unc_Byte *qq);

#ifdef UNCIL_DEFINES
#define TMALLOC(T, A, purpose, n) \
    ((T *)(unc__malloc(A, purpose, (n) * sizeof(T))))
#define TMALLOCZ(T, A, purpose, n) \
    ((T *)(unc__mallocz(A, purpose, (n) * sizeof(T))))
#define TMREALLOC(T, A, purpose, p, n, z) \
    ((T *)(unc__mrealloc(A, purpose, p, (n) * sizeof(T), (z) * sizeof(T))))
#define TMREALLOCZ(T, A, purpose, p, n, z) \
    ((T *)(unc__mreallocz(A, purpose, p, (n) * sizeof(T), (z) * sizeof(T))))
#define TMFREE(T, A, ptr, n) \
    (unc__mfree(A, (T *)ptr, (n) * sizeof(T)))

#define TMMALLOC(T, A, purpose, n) \
    ((T *)(unc__mmalloc(A, purpose, (n) * sizeof(T))))
#define TMMALLOCZ(T, A, purpose, n) \
    ((T *)(unc__mmallocz(A, purpose, (n) * sizeof(T))))
#define TMMREALLOC(T, A, purpose, p, z) \
    ((T *)(unc__mmrealloc(A, purpose, p, (z) * sizeof(T))))
#define TMMREALLOCZ(T, A, purpose, p, z) \
    ((T *)(unc__mmreallocz(A, purpose, p, (z) * sizeof(T))))
#define TMMFREE(T, A, ptr) \
    (unc__mmfree(A, (T *)ptr))
#endif

#endif /* UNCIL_UMEM_H */
