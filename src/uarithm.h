/*******************************************************************************
 
Uncil -- arithmetic utilities header

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

#ifndef UNCIL_UARITHM_H
#define UNCIL_UARITHM_H

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
