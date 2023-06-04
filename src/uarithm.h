/*******************************************************************************
 
Uncil -- arithmetic utilities header

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

#ifndef UNCIL_UARITHM_H
#define UNCIL_UARITHM_H

#include <float.h>

#include "udef.h"

Unc_Int unc0_shiftl(Unc_Int a, Unc_Int b);
Unc_Int unc0_shiftr(Unc_Int a, Unc_Int b);

/* can -a be represented correctly (a + (-a) = 0 and (a > 0) != ((-a) > 0))? */
int unc0_negovf(Unc_Int a);
/* would a + b overflow or underflow? */
int unc0_addovf(Unc_Int a, Unc_Int b);
/* would a - b overflow or underflow? */
int unc0_subovf(Unc_Int a, Unc_Int b);
/* would a * b overflow or underflow? */
int unc0_mulovf(Unc_Int a, Unc_Int b);

int unc0_cmpint(Unc_Int a, Unc_Int b);
int unc0_cmpflt(Unc_Float a, Unc_Float b);

Unc_Int unc0_iidiv(Unc_Int a, Unc_Int b);
Unc_Int unc0_imod(Unc_Int a, Unc_Int b);
Unc_Float unc0_fidiv(Unc_Float a, Unc_Float b);
Unc_Float unc0_fmod(Unc_Float a, Unc_Float b);
Unc_Float unc0_ffrac(Unc_Float x);
Unc_Float unc0_adjexp10(Unc_Float x, long p);

Unc_Float unc0_fnan(void);
Unc_Float unc0_finfty(void);

#if UNCIL_C99
#define UNC_FLOATMAX_LONG 1
typedef long double Unc_FloatMax;
#define UNCF_DIG            LDBL_DIG
#define UNCF_MANT_DIG       LDBL_MANT_DIG
#define UNCF_MIN_EXP        LDBL_MIN_EXP
#define UNCF_MIN_10_EXP     LDBL_MIN_10_EXP
#define UNCF_MAX_EXP        LDBL_MAX_EXP
#define UNCF_MAX_10_EXP     LDBL_MAX_10_EXP
#define UNCF_MAX            LDBL_MAX
#define UNCF_EPSILON        LDBL_EPSILON
#define UNCF_MIN            LDBL_MIN
#else
#define UNC_FLOATMAX_LONG 0
typedef double Unc_FloatMax;
#define UNCF_DIG            DBL_DIG
#define UNCF_MANT_DIG       DBL_MANT_DIG
#define UNCF_MIN_EXP        DBL_MIN_EXP
#define UNCF_MIN_10_EXP     DBL_MIN_10_EXP
#define UNCF_MAX_EXP        DBL_MAX_EXP
#define UNCF_MAX_10_EXP     DBL_MAX_10_EXP
#define UNCF_MAX            DBL_MAX
#define UNCF_EPSILON        DBL_EPSILON
#define UNCF_MIN            DBL_MIN
#endif

Unc_FloatMax unc0_mfrexp(Unc_FloatMax num, int *exp);
Unc_FloatMax unc0_mldexp(Unc_FloatMax num, int exp);
Unc_FloatMax unc0_mpow10(Unc_Int exp);
Unc_FloatMax unc0_mpow10n(Unc_Int exp);
Unc_FloatMax unc0_mmodf(Unc_FloatMax num, Unc_FloatMax *iptr);
intmax_t unc0_malog10f(Unc_Float num);

int unc0_fisfinite(Unc_Float x);

#ifdef UNCIL_DEFINES
#if defined(__GNUC__) && __GNUC__ >= 7
#define NEGOVF(x) __builtin_sub_overflow_p(0, x, (Unc_Int)0)
#define ADDOVF(x, y) __builtin_add_overflow_p(x, y, (Unc_Int)0)
#define SUBOVF(x, y) __builtin_sub_overflow_p(x, y, (Unc_Int)0)
#define MULOVF(x, y) __builtin_mul_overflow_p(x, y, (Unc_Int)0)
#else
#define NEGOVF(x) unc0_negovf(x)
#define ADDOVF(x, y) unc0_addovf(x, y)
#define SUBOVF(x, y) unc0_subovf(x, y)
#define MULOVF(x, y) unc0_mulovf(x, y)
#endif
 
#endif

#endif /* UNCIL_UARITHM_H */
