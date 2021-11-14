/*******************************************************************************
 
Uncil -- VLQ header

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

#ifndef UNCIL_UVLQ_H
#define UNCIL_UVLQ_H

#include <limits.h>
#include "udef.h"

#define UNC_VLQ_SIZE_MAXLEN (sizeof(Unc_Size) + 1)
#define UNC_VLQ_UINT_MAXLEN ((sizeof(Unc_Int) * CHAR_BIT + CHAR_BIT - 1) \
                                                       / (CHAR_BIT - 1))
#define UNC_CLQ_MAXLEN (sizeof(Unc_Size))

#define UNC_ENDIAN_OTHER 0
#define UNC_ENDIAN_LITTLE 1
#define UNC_ENDIAN_BIG 2

int unc__getendianness(void);

Unc_Size unc__vlqencz(Unc_Size v, Unc_Size n, byte *out);
Unc_Size unc__vlqdecz(const byte **in);
Unc_Size unc__vlqdeczd(const byte *in);
Unc_Size unc__vlqenczl(Unc_Size v);
Unc_Size unc__vlqdeczl(const byte *in);

Unc_Size unc__vlqenci(Unc_Int v, Unc_Size n, byte *out);
Unc_Int unc__vlqdeci(const byte **in);
Unc_Int unc__vlqdecid(const byte *in);
Unc_Size unc__vlqencil(Unc_Int v);
Unc_Size unc__vlqdecil(const byte *in);

Unc_Size unc__clqencz(Unc_Size v, Unc_Size width, byte *out);
Unc_Size unc__clqdecz(Unc_Size width, const byte **in);
Unc_Size unc__clqdeczd(Unc_Size width, const byte *in);

#endif /* UNCIL_UVLQ_H */
