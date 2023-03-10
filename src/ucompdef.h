/*******************************************************************************
 
Uncil -- compiler-specific definitions

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

#ifndef UNCIL_UCOMPDEF_H
#define UNCIL_UCOMPDEF_H

#ifdef UNCIL_DEFINES

#include "udebug.h"

#if defined(__clang__) || (defined(__GNUC__) && (__GNUC__ > 4 \
                    || (__GNUC__ == 4 && __GNUC_MINOR__ >= 5)))
#define DEADCODE() ASSERT(0); __builtin_unreachable()
#elif defined(_MSC_VER)
#define DEADCODE() ASSERT(0); __assume(0)
#else
#define DEADCODE()
#endif

#if defined(__clang__) || (defined(__GNUC__) && (__GNUC__ > 3))
#define LIKELY(x) (__builtin_expect((x), 1))
#define UNLIKELY(x) (__builtin_expect((x), 0))
#else
#define LIKELY(x) (x)
#define UNLIKELY(x) (x)
#endif

#endif

#endif
