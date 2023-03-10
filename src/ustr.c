/*******************************************************************************
 
Uncil -- string impl

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

#include "uerr.h"
#include "uncil.h"
#include "ustr.h"
#include "uutf.h"

/* create empty string */
int unc0_initstringempty(Unc_Allocator *alloc, Unc_String *s) {
    (void)alloc;
    s->size = 0;
    s->flags = 0;
    return 0;
}

/* create string by copying n bytes from b (no validity check!) */
int unc0_initstring(Unc_Allocator *alloc, Unc_String *s,
                        Unc_Size n, const byte *b) {
    if (n > UNC_INT_MAX) return UNCIL_ERR_ARG_STRINGTOOLONG;
    if ((s->size = n) >= UNC_STRING_SHORT) {
        byte *p = unc0_malloc(alloc, Unc_AllocString, n + 1);
        if (!p) return UNCIL_ERR_MEM;
        unc0_memcpy(p, b, n);
        p[n] = 0;
        s->d.b.data.p = p;
    } else {
        unc0_memcpy(s->d.a, b, n);
        s->d.a[n] = 0;
    }
    s->flags = 0;
    return 0;
}

/* create string by copying a C string (no validity check!) */
int unc0_initstringc(Unc_Allocator *alloc, Unc_String *s, const char *b) {
    return unc0_initstring(alloc, s, strlen(b), (const byte *)b);
}

/* create string with external buffer that is not owned by us */
/* b must be null-terminated */
static int unc0_initstringnotowned(Unc_Allocator *alloc, Unc_String *s,
                        Unc_Size n, const byte *b) {
    if (n > UNC_INT_MAX) return UNCIL_ERR_ARG_STRINGTOOLONG;
    if ((s->size = n) >= UNC_STRING_SHORT) {
        s->d.b.data.c = b;
        s->flags = UNC_STRING_FLAG_NOTOWNED;
    } else {
        unc0_memcpy(s->d.a, b, n);
        s->d.a[n] = 0,
        s->flags = 0;
    }
    return 0;
}

/* create string mirroring an external C string (no validity check!) */
int unc0_initstringcl(Unc_Allocator *alloc, Unc_String *s, const char *b) {
    return unc0_initstringnotowned(alloc, s, strlen(b), (const byte *)b);
}

/* create string by usurping b */
int unc0_initstringmove(Unc_Allocator *alloc, Unc_String *s,
                        Unc_Size n, byte *b) {
    if (n > UNC_INT_MAX) return UNCIL_ERR_ARG_STRINGTOOLONG;
    if ((s->size = n) >= UNC_STRING_SHORT) {
        s->d.b.data.c = b;
    } else {
        unc0_memcpy(s->d.a, b, n);
        s->d.a[n] = 0;
        unc0_mfree(alloc, b, n);
    }
    s->flags = 0;
    return 0;
}

/* create string by concatenating a and b (byte blocks) */
int unc0_initstringfromcat(Unc_Allocator *alloc, Unc_String *s,
                        Unc_Size an, const byte *a,
                        Unc_Size bn, const byte *b) {
    Unc_Size n = an + bn;
    if (n > UNC_INT_MAX) return UNCIL_ERR_ARG_STRINGTOOLONG;
    if ((s->size = n) >= UNC_STRING_SHORT) {
        byte *p = unc0_malloc(alloc, Unc_AllocString, n + 1);
        if (!p) return UNCIL_ERR_MEM;
        unc0_memcpy(p, a, an);
        unc0_memcpy(p + an, b, bn);
        p[n] = 0;
        s->d.b.data.p = p;
    } else {
        byte *p = s->d.a;
        unc0_memcpy(p, a, an);
        unc0_memcpy(p + an, b, bn);
    }
    s->flags = 0;
    return 0;
}

/* create string by concatenating a (Uncil string) and b */
int unc0_initstringfromcatl(Unc_Allocator *alloc, Unc_String *s,
                        const Unc_String *a, Unc_Size n, const byte *b) {
    return unc0_initstringfromcat(alloc, s, a->size, unc0_getstringdata(a),
                                            n, b);
}

/* create string by concatenating a and b (Uncil string) */
int unc0_initstringfromcatr(Unc_Allocator *alloc, Unc_String *s,
                        Unc_Size n, const byte *a, const Unc_String *b) {
    return unc0_initstringfromcat(alloc, s, n, a,
                                            b->size, unc0_getstringdata(b));
}

/* create string by concatenating a and b (Uncil strings) */
int unc0_initstringfromcatlr(Unc_Allocator *alloc, Unc_String *s,
                        const Unc_String *a, const Unc_String *b) {
    return unc0_initstringfromcat(alloc, s, a->size, unc0_getstringdata(a),
                                            b->size, unc0_getstringdata(b));
}

/* pointer to string data */
const byte *unc0_getstringdata(const Unc_String *s) {
    if (s->size >= UNC_STRING_SHORT)
        return s->flags & UNC_STRING_FLAG_NOTOWNED
                ? s->d.b.data.c : s->d.b.data.p;
    else
        return s->d.a;
}

/* drop/delete string */
void unc0_dropstring(Unc_Allocator *alloc, Unc_String *s) {
    if (s->size >= UNC_STRING_SHORT && !(s->flags & UNC_STRING_FLAG_NOTOWNED))
        unc0_mfree(alloc, s->d.b.data.p, s->size + 1);
}

/* string-string equality check */
int unc0_streq(Unc_String *a, Unc_String *b) {
    if (a->size != b->size) return 0;
    if (!a->size) return 1;
    return !unc0_memcmp(unc0_getstringdata(a), unc0_getstringdata(b), a->size);
}

/* string-byte block equality check */
int unc0_streqr(Unc_String *a, Unc_Size n, const byte *b) {
    if (a->size != n) return 0;
    if (!n) return 1;
    return !unc0_memcmp(unc0_getstringdata(a), b, n);
}

/* byte block-byte block equality check */
int unc0_strreqr(Unc_Size an, const byte *a, Unc_Size bn, const byte *b) {
    return an == bn && !unc0_memcmp(a, b, an);
}

/* <0, 0, >0 for comparing two strings a, b */
int unc0_cmpstr(Unc_String *a, Unc_String *b) {
    Unc_Size ms = a->size < b->size ? a->size : b->size;
    int res = unc0_memcmp(unc0_getstringdata(a), unc0_getstringdata(b), ms);
    if (res > 0) return 1;
    if (res < 0) return -1;
    if (a->size > b->size) return 1;
    if (a->size < b->size) return -1;
    return 0;
}

/* string code point index getter */
int unc0_sgetcodepat(Unc_View *w, Unc_String *s, Unc_Value *indx,
                                 int permissive, Unc_Value *out) {
    Unc_Int i;
    int e = unc0_vgetint(w, indx, &i);
    const byte *s0, *s1;
    Unc_Size n = UNC_UTF8_MAX_SIZE;
    if (e) {
        if (e == UNCIL_ERR_CONVERT_TOINT)
            e = UNCIL_ERR_ARG_INDEXNOTINTEGER;
        return e;
    }
    if (i < 0) {
        const byte *d = unc0_getstringdata(s);
        const byte *p = unc0_utf8scanbackw(d, d + s->size, -(i + 1));
        if (!p) {
            if (permissive) {
                VSETNULL(w, out);
                return 0;
            }
            return UNCIL_ERR_ARG_INDEXOUTOFBOUNDS;
        }
        s0 = p;
    } else {
        const byte *d = unc0_getstringdata(s);
        const byte *p = unc0_utf8scanforw(d, d + s->size, i);
        if (!p) {
            if (permissive) {
                VSETNULL(w, out);
                return 0;
            }
            return UNCIL_ERR_ARG_INDEXOUTOFBOUNDS;
        }
        s0 = p;
    }
    s1 = unc0_utf8nextchar(s0, &n);
    return unc_newstring(w, out, s1 - s0, (const char *)s0);
}

/* modifies existing string! make sure the string you modify only has
   one reference, as strings should be immutable! */
int unc0_strmcat(Unc_Allocator *alloc, Unc_String *s,
                 Unc_Size bn, const byte *b) {
    byte *p;
    Unc_Size n = s->size;
    ASSERT(n > UNC_STRING_SHORT);
    if (!bn) return 0;
    p = unc0_mrealloc(alloc, Unc_AllocString, s->d.b.data.p, n, n + bn + 1);
    if (!p) return UNCIL_ERR_MEM;
    s->d.b.data.p = p;
    unc0_memcpy(p + n, b, bn);
    s->size += bn;
    p[n + bn] = 0;
    return 0;
}
