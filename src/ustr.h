/*******************************************************************************
 
Uncil -- string header

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

#ifndef UNCIL_USTR_H
#define UNCIL_USTR_H

#include "udef.h"
#include "umem.h"
#include "uval.h"

typedef struct Unc_StringData {
    union {
        byte *p;
        const byte *c;
    } data;
    void *pad_;
} Unc_StringData;

#define UNC_STRING_SHORT sizeof(Unc_StringData)

#define UNC_STRING_FLAG_NOTOWNED 1

typedef struct Unc_String {
    Unc_Size size;
    union {
        byte a[UNC_STRING_SHORT];
        Unc_StringData b;
    } d;
    int flags;
} Unc_String;

int unc__initstringempty(Unc_Allocator *alloc, Unc_String *s);
int unc__initstring(Unc_Allocator *alloc, Unc_String *s,
                        Unc_Size n, const byte *b);
int unc__initstringc(Unc_Allocator *alloc, Unc_String *s, const char *b);
int unc__initstringcl(Unc_Allocator *alloc, Unc_String *s, const char *b);
int unc__initstringmove(Unc_Allocator *alloc, Unc_String *s,
                        Unc_Size n, byte *b);
int unc__initstringfromcat(Unc_Allocator *alloc, Unc_String *s,
                        Unc_Size an, const byte *a, Unc_Size bn, const byte *b);
int unc__initstringfromcatl(Unc_Allocator *alloc, Unc_String *s,
                        const Unc_String *a, Unc_Size n, const byte *b);
int unc__initstringfromcatr(Unc_Allocator *alloc, Unc_String *s,
                        Unc_Size n, const byte *a, const Unc_String *b);
int unc__initstringfromcatlr(Unc_Allocator *alloc, Unc_String *s,
                        const Unc_String *a, const Unc_String *b);
const byte *unc__getstringdata(const Unc_String *s);
void unc__dropstring(Unc_Allocator *alloc, Unc_String *s);

int unc__streq(Unc_String *a, Unc_String *b);
int unc__streqr(Unc_String *a, Unc_Size n, const byte *b);
int unc__cmpstr(Unc_String *a, Unc_String *b);
int unc__strreqr(Unc_Size an, const byte *a, Unc_Size bn, const byte *b);

int unc__strmcat(Unc_Allocator *alloc, Unc_String *s,
                 Unc_Size bn, const byte *b);

struct Unc_View;
int unc__sgetcodepat(struct Unc_View *w, Unc_String *s, Unc_Value *i,
                     int permissive, Unc_Value *v);
const byte *unc__strsearch(const byte *haystack, Unc_Size haystack_n,
                           const byte *needle, Unc_Size needle_n);
const byte *unc__strsearchr(const byte *haystack, Unc_Size haystack_n,
                            const byte *needle, Unc_Size needle_n);

#endif /* UNCIL_USTR_H */
