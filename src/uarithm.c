/*******************************************************************************
 
Uncil -- arithmetic utilities

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

#include <float.h>
#include <limits.h>

#define UNCIL_DEFINES

#include "uarithm.h"
#include "udef.h"
#include "uerr.h"

#if UNCIL_C99
#include <tgmath.h>
#else
#include <math.h>
#endif

/* figure out how many bits we can shift by, since shifting too much
   leads to UB in C */
#ifdef UNC_UINT_BIT
#define BIT_WIDTH UNC_UINT_BIT
#define INIT_BIT_WIDTH()
#else
#define BIT_WIDTH bitWidth
#define INIT_BIT_WIDTH() if (!bitWidth) bitWidth = unc0_init_bitwidth()
/* sizeof(Unc_UInt) * CHAR_BIT means type is 32 bits wide, and since
   UNC_UINT_MAX >= 2^32-1 (this works because it's unsigned long (long)) */
#define FULL_BITS (sizeof(Unc_UInt) * CHAR_BIT)
#ifndef UINT32_MAX
#define UINT32_MAX 4294967295
#endif
/* acceptable global state... probably */
static int bitWidth = 0;
/* maximum shift count */

/* count number of bits in int type */
static int unc0_init_bitwidth(void) {
    Unc_UInt i = (Unc_UInt)UNC_UINT_MAX;
    int s = 0;
    while (i)
        i <<= 1, ++s;
    return s;
}

#endif

Unc_Int unc0_shiftl2(Unc_Int a, Unc_UInt b) {
    INIT_BIT_WIDTH();
    if (b >= BIT_WIDTH)
        return 0; /* too much shifting, stop here */
    else
        return a << b;
}

Unc_Int unc0_shiftr2(Unc_Int a, Unc_UInt b) {
    /* check if our right shift is already arithmetic */
    CONSTEXPR int asr = (-9) >> 1 == -5;
    INIT_BIT_WIDTH();
    if (b >= BIT_WIDTH)
        return a < 0 ? -1 : 0; /* too much shifting, stop here */
    else if (asr || a >= 0)
        return a >> b;
    else /* define right shift on negative numbers (arithmetic) */
        return (a >> b) | (~((Unc_UInt)0) << (BIT_WIDTH - b));
}

Unc_Int unc0_shiftl(Unc_Int a, Unc_Int b) {
    if (b < 0)
        return unc0_shiftr2(a, (Unc_UInt)-b);
    else
        return unc0_shiftl2(a, b);
}

Unc_Int unc0_shiftr(Unc_Int a, Unc_Int b) {
    if (b < 0)
        return unc0_shiftl2(a, (Unc_UInt)-b);
    else
        return unc0_shiftr2(a, b);
}

/* can -a be represented correctly (a + (-a) = 0
    and (a > 0) != ((-a) > 0) for nonzero a)? */
int unc0_negovf(Unc_Int a) {
    return (a >= 0) != (-a < 0);
}

/* would a + b overflow or underflow? */
int unc0_addovf(Unc_Int a, Unc_Int b) {
    return b < 0 ? a < UNC_INT_MIN - b : a > UNC_INT_MAX - b;
}

/* would a - b overflow or underflow? */
int unc0_subovf(Unc_Int a, Unc_Int b) {
    return b < 0 ? a > UNC_INT_MAX + b : a < UNC_INT_MIN + b;
}

/* would a * b overflow or underflow? */
int unc0_mulovf(Unc_Int a, Unc_Int b) {
    int negative, h;
    Unc_UInt x, y, q, r, m;
    INIT_BIT_WIDTH();
    /* bunch of special cases */
    if (!a || !b) return 0;
    /* is a/b the minimum value that cannot be represented as positive? */
    if (a == -a) return b != 1;
    if (b == -b) return a != 1;
    /* BIT_WIDTH = 2N -> m = 2^N */
    h = BIT_WIDTH / 2;
    m = (Unc_UInt)1 << h;
    if (a >= (Unc_Int)m && b >= (Unc_Int)m)
        return 1;
    --m;
    negative = (a < 0) != (b < 0);
    /* partial multiplication */
    x = (Unc_UInt)(a < 0 ? -a : a);
    y = (Unc_UInt)(b < 0 ? -b : b);
    q = (x >> h) * (y & m);
    r = (x & m) * (y >> h);
    if (q >> h || r >> h)
        return 1;
    q = x * y;
    return q > (negative ? -(Unc_UInt)UNC_INT_MIN : (Unc_UInt)-UNC_INT_MAX);
}

int unc0_cmpint(Unc_Int a, Unc_Int b) {
    return a == b ? 0 : a < b ? -1 : 1;
}

int unc0_cmpflt(Unc_Float a, Unc_Float b) {
    if (a != a || b != b) /* NAN */
        return UNCIL_ERR_LOGIC_CMPNAN;
    return a == b ? 0 : a < b ? -1 : 1;
}

/* divide a / b and round down */
Unc_Int unc0_iidiv(Unc_Int a, Unc_Int b) {
    Unc_Int q = a / b;
    return q - ((q * b != a) & (a < 0));
}

/* divide a / b and get remainder, use the sign of the divisor */
Unc_Int unc0_imod(Unc_Int a, Unc_Int b) {
    return (a % b + b) % b;
}

Unc_Float unc0_adjexp10(Unc_Float x, long p) {
#if UNCIL_C99
    return x * powl(10.L, p);
#else
    return x * pow(10., p);
#endif
}

Unc_Float unc0_fnan(void) {
#if UNCIL_C99
    return NAN;
#elif UNCIL_IEEE754
    return (Unc_Float)0.0 / (Unc_Float)0.0;
#else
    return HUGE_VAL - HUGE_VAL + HUGE_VAL;
#endif
}

Unc_Float unc0_finfty(void) {
#if UNCIL_C99
    return INFINITY;
#elif UNCIL_IEEE754
    return (Unc_Float)1.0 / (Unc_Float)0.0;
#else
    return HUGE_VAL;
#endif
}

/* floored integer division */
Unc_Float unc0_fidiv(Unc_Float a, Unc_Float b) {
    return floor(a / b);
}

/* float modulo */
Unc_Float unc0_fmod(Unc_Float a, Unc_Float b) {
    return a - floor(a / b) * b;
}

/* get fractional portion */
Unc_Float unc0_ffrac(Unc_Float x) {
    return x - floor(x);
}

int unc0_fisfinite(Unc_Float x) {
#if UNCIL_C99
    return isfinite(x);
#else
/* not perfect */
    return x == x && fabs(x) * 0.5 < HUGE_VAL;
#endif
}

Unc_FloatMax unc0_mfrexp(Unc_FloatMax num, int *exp) {
    return frexp(num, exp);
}

Unc_FloatMax unc0_mldexp(Unc_FloatMax num, int exp) {
    return ldexp(num, exp);
}

Unc_FloatMax unc0_mpow10(Unc_Int exp) {
    return pow((Unc_FloatMax)10, exp);
}

Unc_FloatMax unc0_mpow10n(Unc_Int exp) {
    return pow((Unc_FloatMax)10, -exp);
}

Unc_FloatMax unc0_mmodf(Unc_FloatMax num, Unc_FloatMax *iptr) {
#if UNC_FLOATMAX_LONG
    return modfl(num, iptr);
#else
    return modf(num, iptr);
#endif
}

intmax_t unc0_malog10f(Unc_Float num) {
    return (intmax_t)floor(log10(fabs(num)));
}
