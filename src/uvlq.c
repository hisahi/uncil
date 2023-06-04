/*******************************************************************************
 
Uncil -- VLQ impl

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

#include "udebug.h"
#include "umem.h"
#include "uvlq.h"

#if UNCIL_C23
#include <stdbit.h>
#endif

/* Returns one of UNC_ENDIAN_OTHER, UNC_ENDIAN_LITTLE, UNC_ENDIAN_BIG */
int unc0_getendianness(void) {
#if UNCIL_C23
	if (__STDC_ENDIAN_NATIVE__ == __STDC_ENDIAN_LITTLE__)
        return UNC_ENDIAN_LITTLE;
	if (__STDC_ENDIAN_NATIVE__ == __STDC_ENDIAN_BIG__)
        return UNC_ENDIAN_BIG;
	return UNC_ENDIAN_OTHER;
#else
    int i;
    byte b[sizeof(unsigned long)];
    unsigned long u = ULONG_MAX;
    unc0_memcpy(&b, &u, sizeof(unsigned long));
    for (i = 0; i < sizeof(unsigned long); ++i)
        if (b[i] != UCHAR_MAX) return UNC_ENDIAN_OTHER;
    u = 0x00010203UL;
    unc0_memcpy(&b, &u, sizeof(unsigned long));
    for (i = 0; i < 4; ++i)
        if (b[i] != (i ^ 3)) break;
    if (i == 4)
        return UNC_ENDIAN_LITTLE;
    for (i = 0; i < 4; ++i)
        if (b[sizeof(unsigned long) - 4 + i] != i) break;
    if (i == 4)
        return UNC_ENDIAN_BIG;
    return UNC_ENDIAN_OTHER;
#endif
}

Unc_Size unc0_vlqencz(Unc_Size v, Unc_Size n, byte *out) {
    ASSERT(n);
    if (v < 0x80) {
        *out = v;
        return 1;
    } else {
        Unc_Size i = 0, l2 = 0, vv = v;
        while (vv) vv >>= CHAR_BIT, ++l2;
        ASSERT(l2 && l2 < 0x80);
        out[i++] = (1 << (CHAR_BIT - 1)) | l2;
        do {
            if (i >= n) break;
            out[i++] = (unsigned char)(v >> (CHAR_BIT * --l2));
        } while (l2);
        return i;
    }
}

Unc_Size unc0_vlqdecz(const byte **in) {
    if (!(**in & 0x80))
        return *(*in)++;
    else {
        const byte *p = *in;
        Unc_Size l2 = *p++ & ((1 << (CHAR_BIT - 1)) - 1), s = 0;
        while (l2--) s = (s << CHAR_BIT) | *p++;
        *in = p;
        return s;
    }
}

Unc_Size unc0_vlqdeczd(const byte *in) {
    return unc0_vlqdecz(&in);
}

Unc_Size unc0_vlqenczl(Unc_Size v) {
    byte b[UNC_VLQ_SIZE_MAXLEN];
    return unc0_vlqencz(v, sizeof(b), b);
}

Unc_Size unc0_vlqdeczl(const byte *in) {
    const byte *pin = in;
    (void)unc0_vlqdecz(&in);
    return in - pin;
}

#define NSMASK ((Unc_Int)(((Unc_UInt)-1 & ~(((Unc_UInt)-1) >> 7))))

INLINE Unc_Size unc0_vlqencip(Unc_Int v, Unc_Size n, byte *out) {
    Unc_Size i = 0;
    byte b;
    ASSERT(v >= 0);
    do {
        b = v & 0x7F;
        v >>= 7;
        if (v) b |= 0x80;
        *out++ = b;
    } while (i++ < n && v);
    if (i < n && (b & 0x40)) /* disambiguate */
        out[-1] |= 0x80, *out++ = 0, ++i;
    return i;
}

INLINE Unc_Size unc0_vlqencin(Unc_Int v, Unc_Size n, byte *out) {
    Unc_Size i = 0;
    byte b;
    ASSERT(v < 0);
    do {
        b = v & 0x7F;
        v >>= 7;
        if (v > 0) v |= NSMASK;
        if (~v) b |= 0x80;
        *out++ = b;
    } while (i++ < n && ~v);
    if (i < n && !(b & 0x40)) /* disambiguate */
        out[-1] |= 0x80, *out++ = 0x7F, ++i;
    return i;
}

Unc_Size unc0_vlqenci(Unc_Int v, Unc_Size n, byte *out) {
    return v < 0 ? unc0_vlqencin(v, n, out) : unc0_vlqencip(v, n, out);
}

Unc_Int unc0_vlqdeci(const byte **in) {
    Unc_Int s = 0, m = -1;
    int j = 0, n = 0;
    byte b;
    const byte *p = *in;
    do {
        b = *p++;
        n = b & 0x40;
        s |= (Unc_Int)(b & 0x7F) << j;
        m <<= 7;
        j += 7;
    } while (b & 0x80);
    *in = p;
    if (n) s |= m;
    return s;
}

Unc_Int unc0_vlqdecid(const byte *in) {
    return unc0_vlqdeci(&in);
}

Unc_Size unc0_vlqencil(Unc_Int v) {
    byte b[UNC_VLQ_UINT_MAXLEN];
    return unc0_vlqencz(v, sizeof(b), b);
}

Unc_Size unc0_vlqdecil(const byte *in) {
    const byte *pin = in;
    (void)unc0_vlqdeci(&in);
    return in - pin;
}

Unc_Size unc0_clqencz(Unc_Size v, Unc_Size width, byte *out) {
    if (!width) return 0;
    while (width--) {
        *out++ = (byte)v;
        v >>= CHAR_BIT;
    }
    ASSERT(!v);
    return width;
}

Unc_Size unc0_clqdecz(Unc_Size width, const byte **in) {
    Unc_Size v = 0;
    const byte *p = *in;
    int sh = 0;
    if (!width) return 0;
    v = *p++;
    while (--width)
        sh += CHAR_BIT, v |= *p++ << sh;
    *in = p;
    return v;
}

Unc_Size unc0_clqdeczd(Unc_Size width, const byte *in) {
    return unc0_clqdecz(width, &in);
}
