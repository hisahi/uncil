/*******************************************************************************
 
Uncil -- blob impl

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

#define UNCIL_DEFINES

#include "ublob.h"
#include "ucommon.h"
#include "udebug.h"

/* how much to expand blob by if we add one byte and it's out of bounds */
#define UNC_BLOB_BUFFER 64

/* create new blob with size n and data copied from b */
int unc0_initblob(Unc_Allocator *alloc, Unc_Blob *s, Unc_Size n,
                                                     const byte *b) {
    s->capacity = s->size = n;
    if (!n) {
        s->data = NULL;
        return 0;
    }
    if (!(s->data = unc0_malloc(alloc, Unc_AllocBlob, n)))
        return UNCIL_ERR_MEM;
    unc0_memcpy(s->data, b, n);
    return UNC_LOCKINITL(s->lock);
}

/* create new blob with size n, allocate and return pointer as b */
int unc0_initblobraw(Unc_Allocator *alloc, Unc_Blob *s, Unc_Size n, byte **b) {
    s->capacity = s->size = n;
    if (!n) {
        s->data = NULL;
        return 0;
    }
    if (!(s->data = unc0_malloc(alloc, Unc_AllocBlob, n)))
        return UNCIL_ERR_MEM;
    *b = s->data;
    return UNC_LOCKINITL(s->lock);
}

/* create new blob with size n, usurp b as pointer */
int unc0_initblobmove(Unc_Allocator *alloc, Unc_Blob *s, Unc_Size n, byte *b) {
    s->capacity = s->size = n;
    s->data = b;
    return UNC_LOCKINITL(s->lock);
}

/* append n bytes form b to blob q */
int unc0_blobadd(Unc_Allocator *alloc, Unc_Blob *q,
                 Unc_Size n, const byte *b) {
    Unc_Size c = q->capacity, s = q->size, ns = s + n;
    if (ns > c) {
        Unc_Size nc = ns + UNC_BLOB_BUFFER - 1;
        byte *p;
        nc -= nc % UNC_BLOB_BUFFER;
        p = unc0_mrealloc(alloc, Unc_AllocBlob, q->data, c, nc);
        if (!p) return UNCIL_ERR_MEM;
        q->data = p;
        q->capacity = nc;
    }
    unc0_memcpy(q->data + s, b, n);
    q->size = ns;
    return 0;
}

/* append n bytes from b to blob q */
int unc0_blobaddb(Unc_Allocator *alloc, Unc_Blob *q, byte b) {
    return unc0_blobadd(alloc, q, 1, &b);
}

/* append blob q2 to blob q */
int unc0_blobaddf(Unc_Allocator *alloc, Unc_Blob *q, const Unc_Blob *q2) {
    return unc0_blobadd(alloc, q, q2->size, q2->data);
}

/* append n zeroes to blob q */
int unc0_blobaddn(Unc_Allocator *alloc, Unc_Blob *q, Unc_Size n) {
    Unc_Size c = q->capacity, s = q->size, ns = s + n;
    if (ns > c) {
        Unc_Size nc = ns + UNC_BLOB_BUFFER - 1;
        byte *p;
        nc -= nc % UNC_BLOB_BUFFER;
        p = unc0_mrealloc(alloc, Unc_AllocBlob, q->data, c, nc);
        if (!p) return UNCIL_ERR_MEM;
        q->data = p;
        q->capacity = nc;
    }
    unc0_mbzero(q->data + s, n);
    q->size = ns;
    return 0;
}

/* insert n bytes from b to blob q at index i */
int unc0_blobins(Unc_Allocator *alloc, Unc_Blob *q, Unc_Size i,
                                    Unc_Size n, const byte *b) {
    Unc_Size c = q->capacity, s = q->size, ns = s + n;
    if (ns > c) {
        Unc_Size nc = ns + UNC_BLOB_BUFFER - 1;
        byte *p;
        nc -= nc % UNC_BLOB_BUFFER;
        p = unc0_mrealloc(alloc, Unc_AllocBlob, q->data, c, nc);
        if (!p) return UNCIL_ERR_MEM;
        q->data = p;
        q->capacity = nc;
    }
    if (i + n < s)
        unc0_memmove(q->data + i + n, q->data + i, s - n - i);
    unc0_memcpy(q->data + i, b, n);
    q->size = ns;
    return 0;
}

/* insert blob q2 to blob q at index i */
int unc0_blobinsf(Unc_Allocator *alloc, Unc_Blob *q, Unc_Size i,
                  const Unc_Blob *q2) {
    return unc0_blobins(alloc, q, i, q2->size, q2->data);
}

/* insert n zeroes to blob q at index i */
int unc0_blobinsn(Unc_Allocator *alloc, Unc_Blob *q, Unc_Size i, Unc_Size n) {
    Unc_Size c = q->capacity, s = q->size, ns = s + n;
    if (ns > c) {
        Unc_Size nc = ns + UNC_BLOB_BUFFER - 1;
        byte *p;
        nc -= nc % UNC_BLOB_BUFFER;
        p = unc0_mrealloc(alloc, Unc_AllocBlob, q->data, c, nc);
        if (!p) return UNCIL_ERR_MEM;
        q->data = p;
        q->capacity = nc;
    }
    if (i + n < s)
        unc0_memmove(q->data + i + n, q->data + i, s - n - i);
    unc0_mbzero(q->data + i, n);
    q->size = ns;
    return 0;
}

/* delete n bytes from blob q at index i */
int unc0_blobdel(Unc_Allocator *alloc, Unc_Blob *q, Unc_Size i, Unc_Size n) {
    if (i + n > q->size)
        return UNCIL_ERR_ARG_OUTOFBOUNDS;
    if (i + n < q->size)
        unc0_memmove(q->data + i, q->data + i + n, q->size - i - n);
    q->size -= n;
    if (q->size * 2 < q->capacity) {
        Unc_Size nz = q->capacity / 2;
        q->data = unc0_mrealloc(alloc, Unc_AllocBlob,
                                q->data, q->capacity, nz);
        q->capacity = nz;
    }
    return 0;
}

/* drop/delete blob, destructor */
void unc0_dropblob(Unc_Allocator *alloc, Unc_Blob *s) {
    UNC_LOCKFINAL(s->lock);
    unc0_mfree(alloc, s->data, s->capacity);
}

/* blob-blob equality check, <>0 = equal, 0 = not equal */
int unc0_blobeq(Unc_Blob *a, Unc_Blob *b) {
    int e;
    if (a == b) return 1;
    UNC_LOCKL(a->lock);
    UNC_LOCKL(b->lock);
    if (a->size != b->size)
        e = 0;
    else if (!a->size)
        e = 1;
    else
        e = !unc0_memcmp(a->data, b->data, a->size);
    UNC_UNLOCKL(b->lock);
    UNC_UNLOCKL(a->lock);
    return e;
}

/* blob-byte block equality check, <>0 = equal, 0 = not equal */
int unc0_blobeqr(Unc_Blob *a, Unc_Size n, const byte *b) {
    int e;
    UNC_LOCKL(a->lock);
    if (a->size != n)
        e = 0;
    else if (!n)
        e = 1;
    else
        e = !unc0_memcmp(a->data, b, n);
    UNC_UNLOCKL(a->lock);
    return e;
}

/* <0, 0, >0 for comparing two blobs a, b */
int unc0_cmpblob(Unc_Blob *a, Unc_Blob *b) {
    Unc_Size ms;
    int e, res;
    UNC_LOCKL(a->lock);
    UNC_LOCKL(b->lock);
    ms = a->size < b->size ? a->size : b->size;
    res = unc0_memcmp(a->data, b->data, ms);
    if (res > 0) e = 1;
    else if (res < 0) e = -1;
    else if (a->size > b->size) e = 1;
    else if (a->size < b->size) e = -1;
    else e = 0;
    UNC_UNLOCKL(b->lock);
    UNC_UNLOCKL(a->lock);
    return e;
}

/* blob index getter */
int unc0_blgetbyte(Unc_View *w, Unc_Blob *a, Unc_Value *indx, int permissive,
                                             Unc_Value *out) {
    Unc_Int i;
    int e = unc0_vgetint(w, indx, &i);
    if (e) {
        if (e == UNCIL_ERR_CONVERT_TOINT)
            e = UNCIL_ERR_ARG_INDEXNOTINTEGER;
        return e;
    }
    UNC_LOCKL(a->lock);
    if (i < 0)
        i += a->size;
    if (i < 0 || i >= a->size) {
        UNC_UNLOCKL(a->lock);
        if (permissive) {
            VSETNULL(w, out);
            return 0;
        }
        return UNCIL_ERR_ARG_INDEXOUTOFBOUNDS;
    }
    VSETINT(w, out, a->data[i]);
    UNC_UNLOCKL(a->lock);
    return 0;
}

/* blob index setter */
int unc0_blsetbyte(Unc_View *w, Unc_Blob *a, Unc_Value *indx, Unc_Value *v) {
    Unc_Int i, i2;
    int e = unc0_vgetint(w, indx, &i);
    if (e)
        return e != UNCIL_ERR_CONVERT_TOINT ? e
            : UNCIL_ERR_ARG_INDEXNOTINTEGER;
    UNC_LOCKL(a->lock);
    if (i < 0) {
        UNC_UNLOCKL(a->lock);
        i += a->size;
    }
    if (i < 0 || i >= a->size) {
        UNC_UNLOCKL(a->lock);
        return UNCIL_ERR_ARG_INDEXOUTOFBOUNDS;
    }
    e = unc0_vgetint(w, v, &i2);
    if (e) {
        UNC_UNLOCKL(a->lock);
        return e;
    }
    if (i2 < -128 || i2 > 255) {
        UNC_UNLOCKL(a->lock);
        return UNCIL_ERR_CONVERT_TOINT;
    }
    a->data[i] = (byte)i2;
    UNC_UNLOCKL(a->lock);
    return 0;
}
