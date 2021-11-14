/*******************************************************************************
 
Uncil -- definitions

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

#ifndef UNCIL_UDEF_H
#define UNCIL_UDEF_H

#include <float.h>
#include <limits.h>
#include <stddef.h>

/* major (2 digits), minor (2 digits), patch (2 digits) */
/* major -> API changes, minor -> ABI changes, but anything can happen
   until major gets to 1 */
#define UNCIL_VER_MAJOR 0
#define UNCIL_VER_MINOR 7
#define UNCIL_VER_PATCH 0
#define UNCIL_VER (((long)(UNCIL_VER_MAJOR) << 16) \
                 | ((long)(UNCIL_VER_MINOR) <<  8) \
                 | ((long)(UNCIL_VER_PATCH)      ))
#define UNCIL_PROGRAM_VER 1

#if CHAR_BIT != 8
#error "Uncil does not currently support systems with CHAR_BIT != 8"
#endif

#if -1 != ~0
#error "Uncil does not currently support systems without two's complement"
#endif

#if !('B' - 'A' ==  1 && 'K' - 'A' == 10 && 'Z' - 'A' == 25                    \
 && 'a' - 'A' == 32 && 'n' - 'N' == 32 && 'v' - 'V' == 32 && 'z' - 'Z' == 32   \
 && '3' - '0' ==  3 && '9' - '0' ==  9)
#error "Uncil does not currently support non-ASCII systems"
#endif

#define UNCIL_DEFAULT_RECURSE_LIMIT 1024

typedef int Unc_RetVal;

/* C99? */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#define UNCIL_C99 1
#else
#define UNCIL_C99 0
#endif

#if UNCIL_C99
#include <stdint.h>
#endif

/* C11? */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define UNCIL_C11 1
#define __STDC_WANT_LIB_EXT1__ 1
#else
#define UNCIL_C11 0
#endif

/* language type representations */
#if UNCIL_C99
typedef long long Unc_Int;
typedef unsigned long long Unc_UInt;
#define PRIUnc_Int "ll"
#define UNC_INT_MIN LLONG_MIN
#define UNC_INT_MAX LLONG_MAX
#define UNC_UINT_MAX ULLONG_MAX
#ifdef ULLONG_BIT
#define UNC_UINT_BIT ULLONG_BIT
#elif UNC_UINT_MAX == 18446744073709551615ULL
#define UNC_UINT_BIT 64
#endif
#else
typedef long Unc_Int;
typedef unsigned long Unc_UInt;
#define PRIUnc_Int "l"
#define UNC_INT_MIN LONG_MIN
#define UNC_INT_MAX LONG_MAX
#define UNC_UINT_MAX ULONG_MAX
#ifdef ULONG_BIT
#define UNC_UINT_BIT ULONG_BIT
#elif 18446744073709551615UL / 4294967295UL == 4294967297UL \
                  && UNC_UINT_MAX == 18446744073709551615UL
#define UNC_UINT_BIT 64
#elif UNC_UINT_MAX == 4294967295UL
#define UNC_UINT_BIT 32
#endif
#endif

#if INT_MAX / 1000 > 1000
typedef size_t Unc_Size;
#if UNCIL_C99
#define PRIUnc_Size "z"
#else
#define PRIUnc_Size "l"
#endif
#define UNC_SIZE_MAX SIZE_MAX
#else
typedef unsigned long Unc_Size;
#define PRIUnc_Size "l"
#define UNC_SIZE_CHECK 1
#define UNC_SIZE_MAX ULONG_MAX
#endif

typedef double Unc_Float;
#define PRIUnc_Float "l"
#ifndef __STDC_IEC_559__
#if FLT_RADIX != 2 || DBL_MANT_DIG != 53 || DBL_DIG != 15 \
    || DBL_MIN_EXP != -1021 || DBL_MIN_10_EXP != -307 \
    || DBL_MAX_EXP != 1024 || DBL_MAX_10_EXP != 308
#define UNCIL_BADFLOAT 1
#endif
#endif

#define UNC_FLOAT_MIN DBL_MIN
#define UNC_FLOAT_MAX DBL_MAX

#if UNCIL_BADFLOAT
#error "Uncil does not currently support platforms with nonstandard floats"
#endif

/* used for aligning the ends of structs */
typedef union Unc_MaxAlign {
    void* _p;
    Unc_Int _i;
    Unc_Size _s;
    Unc_Float _f;
} Unc_MaxAlign;

typedef unsigned char Unc_Byte;
typedef unsigned long Unc_MMask;

#ifdef UNCIL_DEFINES

#define UNCIL_STRINGIFY_RAW(x) #x
#define UNCIL_STRINGIFY(x) UNCIL_STRINGIFY_RAW(x)

#define UNCIL_VER_STRING UNCIL_STRINGIFY(UNCIL_VER_MAJOR)                      \
                     "." UNCIL_STRINGIFY(UNCIL_VER_MINOR)                      \
                     "." UNCIL_STRINGIFY(UNCIL_VER_PATCH)

typedef Unc_Byte byte;

#if UNCIL_C99
#define BOOL _Bool
#else
#define BOOL int
#endif

/* inlining */
#if UNCIL_C99
#define INLINE static inline
#define INLINEEXT inline
#define INLINEHERE extern inline
#define INLINEEXTOK 1
#if NOINLINE
#define FORCEINLINE static
#elif __GNUC__
#define FORCEINLINE INLINE __attribute__((always_inline))
#elif defined(_MSC_VER)
#define FORCEINLINE INLINE __forceinline
#else
#define FORCEINLINE INLINE
#endif
#else
#define INLINE static
#define INLINEEXT
#define INLINEHERE
#define INLINEEXTOK 0
#define FORCEINLINE static
#endif

#if UNCIL_C99
#define RESTRICT restrict
#else
#define RESTRICT
#endif

#ifndef SIZE_MAX
#define SIZE_MAX ((Unc_Size)(-1))
#endif

#ifndef UINTPTR_MAX
#if UNCIL_C99
typedef unsigned long long uintptr_t;
#define UINTPTR_MAX ULLONG_MAX
#else
typedef unsigned long uintptr_t;
#define UINTPTR_MAX ULONG_MAX
#endif
#endif

#define ASIZEOF(T) ((sizeof(T)) - (sizeof(Unc_MaxAlign)))
#define ARRAYSIZE(v) ((sizeof(v)) / (sizeof((v)[0])))

#define UNCIL_REGW 2

#if UNCIL_C99
typedef uint_least16_t Unc_Reg;
typedef uint_fast32_t Unc_RegFast;
#else
typedef unsigned Unc_Reg;
typedef unsigned Unc_RegFast;
#endif

#endif /* UNCIL_DEFINES */

#endif /* UNCIL_UDEF_H */
