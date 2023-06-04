/*******************************************************************************
 
Uncil -- C standard detection definitions

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

#ifndef UNCIL_UCSTD_H
#define UNCIL_UCSTD_H

#if !defined(__STDC__) && !defined(_MSC_VER)
#error Uncil requires compliance to the ANSI C standard
#endif

#if defined(__STDC_VERSION__)

/* C99? */
#if __STDC_VERSION__ >= 199901L
#define UNCIL_C99 1
#else
#define UNCIL_C99 0
#endif

/* C11? */
#if __STDC_VERSION__ >= 201112L
#define UNCIL_C11 1
#define __STDC_WANT_LIB_EXT1__ 1
#else
#define UNCIL_C11 0
#endif

/* C23? */
#if __STDC_VERSION__ >= 202311L
#define UNCIL_C23 1
#else
#define UNCIL_C23 0
#endif

#endif /* defined(__STDC_VERSION__) */

#if defined(__STDC_HOSTED__) && __STDC_HOSTED__ == 0
#define UNCIL_NOLIBC 1
#endif

#endif /* UNCIL_UCSTD_H */
