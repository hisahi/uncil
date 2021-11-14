/*******************************************************************************
 
Uncil -- arithmetic helper

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

#ifndef UNCIL_UARITHM_H
#define UNCIL_UARITHM_H

#include "udef.h"

Unc_Int unc__shiftl(Unc_Int a, Unc_Int b);
Unc_Int unc__shiftr(Unc_Int a, Unc_Int b);

/* can -a be represented correctly (a + (-a) = 0 and (a > 0) != ((-a) > 0))? */
int unc__negovf(Unc_Int a);
/* would a + b overflow or underflow? */
int unc__addovf(Unc_Int a, Unc_Int b);
/* would a - b overflow or underflow? */
int unc__subovf(Unc_Int a, Unc_Int b);
/* would a * b overflow or underflow? */
int unc__mulovf(Unc_Int a, Unc_Int b);

int unc__cmpint(Unc_Int a, Unc_Int b);
int unc__cmpflt(Unc_Float a, Unc_Float b);

Unc_Int unc__iidiv(Unc_Int a, Unc_Int b);
Unc_Int unc__imod(Unc_Int a, Unc_Int b);
Unc_Float unc__fidiv(Unc_Float a, Unc_Float b);
Unc_Float unc__fmod(Unc_Float a, Unc_Float b);
Unc_Float unc__ffrac(Unc_Float x);

Unc_Float unc__fnan(void);
Unc_Float unc__finfty(void);

int unc__fisfinite(Unc_Float x);

#ifdef UNCIL_DEFINES
#if defined(__GNUC__) && __GNUC__ >= 7
#define NEGOVF(x) __builtin_sub_overflow_p(0, x, (Unc_Int)0)
#define ADDOVF(x, y) __builtin_add_overflow_p(x, y, (Unc_Int)0)
#define SUBOVF(x, y) __builtin_sub_overflow_p(x, y, (Unc_Int)0)
#define MULOVF(x, y) __builtin_mul_overflow_p(x, y, (Unc_Int)0)
#else
#define NEGOVF(x) unc__negovf(x)
#define ADDOVF(x, y) unc__addovf(x, y)
#define SUBOVF(x, y) unc__subovf(x, y)
#define MULOVF(x, y) unc__mulovf(x, y)
#endif
 
#endif

#endif /* UNCIL_UARITHM_H */
