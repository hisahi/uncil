/*******************************************************************************
 
Uncil -- Unicode conversion header

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

#ifndef UNCIL_UUTF_H
#define UNCIL_UUTF_H

#include "udef.h"
#include "utxt.h"

#define UNC_UTF8_MAX_SIZE 4

#if UNCIL_C99
typedef uint_least32_t Unc_UChar;
#define UNC_UTF8_MAX_CHAR UINT32_C(0x110000)
#else
typedef unsigned long Unc_UChar;
#define UNC_UTF8_MAX_CHAR 0x110000UL
#endif

Unc_Size unc__utf8enc(Unc_UChar u, Unc_Size n, byte *out);
Unc_UChar unc__utf8dec(Unc_Size n, const byte **in);
Unc_UChar unc__utf8decx(Unc_Size *n, const byte **in);
Unc_UChar unc__utf8decd(const byte *in);

/* shorten overlong encodings and pad remaining spaces with 00, modify buffer.
   invalid bytes are ignored. */
Unc_Size unc__utf8patdown(Unc_Size n, byte *out, const byte *in);
/* return length if unc__utf8patdown. */
Unc_Size unc__utf8patdownl(Unc_Size n, const byte *in);

const byte *unc__utf8scanforw(const byte *s0, const byte *s1, Unc_Size count);
const byte *unc__utf8scanbackw(const byte *s0, const byte *s1, Unc_Size count);
const byte *unc__utf8nextchar(const byte *sb, Unc_Size *n);
const byte *unc__utf8lastchar(const byte *sb, Unc_Size n);

int unc__utf8validate(Unc_Size n, const char *s);

int unc__cconv_utf8ts(Unc_CConv_In in, void *in_data,
                      Unc_CConv_Out out, void *out_data,
                      Unc_Size n);

const byte *unc__utf8shift(const byte *s, Unc_Size *n, Unc_Size i);
const byte *unc__utf8shiftback(const byte *s, Unc_Size *n, Unc_Size i);
const byte *unc__utf8shiftaway(const byte *s, Unc_Size *n, Unc_Size i);
#define UNC_RESHIFTBAD (~(Unc_Size)0)
Unc_Size unc__utf8reshift(const byte *s, Unc_Size z, Unc_Size n);
Unc_Size unc__utf8unshift(const byte *s, Unc_Size n);
void unc__utf8rev(byte *b, const byte *s, Unc_Size n);

#endif /* UNCIL_UUTF_H */
