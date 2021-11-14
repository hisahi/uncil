/*******************************************************************************
 
Uncil -- string header

Copyright (c) 2021 Sampo Hippeläinen (hisahi)

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

int unc__initstringempty(Unc_Allocator *alloc, Unc_String *s) {
    (void)alloc;
    s->size = 0;
    s->flags = 0;
    return 0;
}

int unc__initstring(Unc_Allocator *alloc, Unc_String *s,
                        Unc_Size n, const byte *b) {
    if (n > UNC_INT_MAX) return UNCIL_ERR_ARG_STRINGTOOLONG;
    if ((s->size = n) >= UNC_STRING_SHORT) {
        byte *p = unc__malloc(alloc, Unc_AllocString, n + 1);
        if (!p) return UNCIL_ERR_MEM;
        unc__memcpy(p, b, n);
        p[n] = 0;
        s->d.b.data.p = p;
    } else {
        unc__memcpy(s->d.a, b, n);
        s->d.a[n] = 0;
    }
    s->flags = 0;
    return 0;
}

int unc__initstringc(Unc_Allocator *alloc, Unc_String *s, const char *b) {
    return unc__initstring(alloc, s, strlen(b), (const byte *)b);
}

/* b must be null-terminated */
static int unc__initstringnotowned(Unc_Allocator *alloc, Unc_String *s,
                        Unc_Size n, const byte *b) {
    if (n > UNC_INT_MAX) return UNCIL_ERR_ARG_STRINGTOOLONG;
    if ((s->size = n) >= UNC_STRING_SHORT) {
        s->d.b.data.c = b;
        s->flags = UNC_STRING_FLAG_NOTOWNED;
    } else {
        unc__memcpy(s->d.a, b, n);
        s->d.a[n] = 0,
        s->flags = 0;
    }
    return 0;
}

int unc__initstringcl(Unc_Allocator *alloc, Unc_String *s, const char *b) {
    return unc__initstringnotowned(alloc, s, strlen(b), (const byte *)b);
}

int unc__initstringmove(Unc_Allocator *alloc, Unc_String *s,
                        Unc_Size n, byte *b) {
    if (n > UNC_INT_MAX) return UNCIL_ERR_ARG_STRINGTOOLONG;
    if ((s->size = n) >= UNC_STRING_SHORT) {
        s->d.b.data.c = b;
    } else {
        unc__memcpy(s->d.a, b, n);
        s->d.a[n] = 0;
        unc__mfree(alloc, b, n);
    }
    s->flags = 0;
    return 0;
}

int unc__initstringfromcat(Unc_Allocator *alloc, Unc_String *s,
                        Unc_Size an, const byte *a,
                        Unc_Size bn, const byte *b) {
    Unc_Size n = an + bn;
    if (n > UNC_INT_MAX) return UNCIL_ERR_ARG_STRINGTOOLONG;
    if ((s->size = n) >= UNC_STRING_SHORT) {
        byte *p = unc__malloc(alloc, Unc_AllocString, n + 1);
        if (!p) return UNCIL_ERR_MEM;
        unc__memcpy(p, a, an);
        unc__memcpy(p + an, b, bn);
        p[n] = 0;
        s->d.b.data.p = p;
    } else {
        byte *p = s->d.a;
        unc__memcpy(p, a, an);
        unc__memcpy(p + an, b, bn);
    }
    s->flags = 0;
    return 0;
}

int unc__initstringfromcatl(Unc_Allocator *alloc, Unc_String *s,
                        const Unc_String *a, Unc_Size n, const byte *b) {
    return unc__initstringfromcat(alloc, s, a->size, unc__getstringdata(a),
                                            n, b);
}

int unc__initstringfromcatr(Unc_Allocator *alloc, Unc_String *s,
                        Unc_Size n, const byte *a, const Unc_String *b) {
    return unc__initstringfromcat(alloc, s, n, a,
                                            b->size, unc__getstringdata(b));
}

int unc__initstringfromcatlr(Unc_Allocator *alloc, Unc_String *s,
                        const Unc_String *a, const Unc_String *b) {
    return unc__initstringfromcat(alloc, s, a->size, unc__getstringdata(a),
                                            b->size, unc__getstringdata(b));
}

const byte *unc__getstringdata(const Unc_String *s) {
    if (s->size >= UNC_STRING_SHORT)
        return s->flags & UNC_STRING_FLAG_NOTOWNED
                ? s->d.b.data.c : s->d.b.data.p;
    else
        return s->d.a;
}

void unc__dropstring(Unc_Allocator *alloc, Unc_String *s) {
    if (s->size >= UNC_STRING_SHORT && !(s->flags & UNC_STRING_FLAG_NOTOWNED))
        unc__mfree(alloc, s->d.b.data.p, s->size + 1);
}

int unc__streq(Unc_String *a, Unc_String *b) {
    if (a->size != b->size) return 0;
    if (!a->size) return 1;
    return !unc__memcmp(unc__getstringdata(a), unc__getstringdata(b), a->size);
}

int unc__streqr(Unc_String *a, Unc_Size n, const byte *b) {
    if (a->size != n) return 0;
    if (!n) return 1;
    return !unc__memcmp(unc__getstringdata(a), b, n);
}

int unc__strreqr(Unc_Size an, const byte *a, Unc_Size bn, const byte *b) {
    return an == bn && !unc__memcmp(a, b, an);
}

int unc__cmpstr(Unc_String *a, Unc_String *b) {
    Unc_Size ms = a->size < b->size ? a->size : b->size;
    int res = unc__memcmp(unc__getstringdata(a), unc__getstringdata(b), ms);
    if (res > 0) return 1;
    if (res < 0) return -1;
    if (a->size > b->size) return 1;
    if (a->size < b->size) return -1;
    return 0;
}

int unc__sgetcodepat(Unc_View *w, Unc_String *s, Unc_Value *indx,
                                 int permissive, Unc_Value *out) {
    Unc_Int i;
    int e = unc__vgetint(w, indx, &i);
    const byte *s0, *s1;
    Unc_Size n = UNC_UTF8_MAX_SIZE;
    if (e) {
        if (e == UNCIL_ERR_CONVERT_TOINT)
            e = UNCIL_ERR_ARG_INDEXNOTINTEGER;
        return e;
    }
    if (i < 0) {
        const byte *d = unc__getstringdata(s);
        const byte *p = unc__utf8scanbackw(d, d + s->size, -(i + 1));
        if (!p) {
            if (permissive) {
                VSETNULL(w, out);
                return 0;
            }
            return UNCIL_ERR_ARG_INDEXOUTOFBOUNDS;
        }
        s0 = p;
    } else {
        const byte *d = unc__getstringdata(s);
        const byte *p = unc__utf8scanforw(d, d + s->size, i);
        if (!p) {
            if (permissive) {
                VSETNULL(w, out);
                return 0;
            }
            return UNCIL_ERR_ARG_INDEXOUTOFBOUNDS;
        }
        s0 = p;
    }
    s1 = unc__utf8nextchar(s0, &n);
    return unc_newstring(w, out, s1 - s0, (const char *)s0);
}

const byte *unc__strsearch(const byte *haystack, Unc_Size haystack_n,
                           const byte *needle, Unc_Size needle_n) {
    byte s0;
    const byte *next;
    if (!needle_n) return haystack;
    s0 = needle[0];
    haystack_n -= needle_n - 1;
    while ((next = memchr(haystack, s0, haystack_n))) {
        if (!memcmp(next + 1, needle + 1, needle_n - 1))
            return next;
        haystack_n -= next - haystack + 1;
        haystack = next + 1;
    }
    return NULL;
}

void *unc__memrchr(const void *p, int c, Unc_Size n) {
    unsigned char *b = (unsigned char *)p + n;
    while (n--)
        if (*--b == c)
            return b;
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

/* modifies existing string! make sure the string you modify only has
   one reference, as strings should be immutable! */
int unc__strmcat(Unc_Allocator *alloc, Unc_String *s,
                 Unc_Size bn, const byte *b) {
    byte *p;
    Unc_Size n = s->size;
    ASSERT(n > UNC_STRING_SHORT);
    if (!bn) return 0;
    p = unc__mrealloc(alloc, Unc_AllocString, s->d.b.data.p, n, n + bn + 1);
    if (!p) return UNCIL_ERR_MEM;
    s->d.b.data.p = p;
    unc__memcpy(p + n, b, bn);
    s->size += bn;
    p[n + bn] = 0;
    return 0;
}
