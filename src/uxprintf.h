/*******************************************************************************
 
Uncil -- xprintf header

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

#ifndef UNCIL_UXPRINTF_H
#define UNCIL_UXPRINTF_H

#include <limits.h>
#include <stdarg.h>
#include <stddef.h>

#include "udef.h"

#define UNC0_PRINTF_UTF8 1
#define UNC0_PRINTF_SKIPPOS 2

#define UNC_PRINTF_EOF ((size_t)(-1))

size_t unc0_vxprintf(int (*out)(char outp, void *udata),
                     void *udata, int gflags, size_t format_n,
                     const char *format, va_list arg);
size_t unc0_xprintf(int (*out)(char outp, void *udata),
                    void *udata, int gflags, size_t format_n,
                    const char *format, ...);

size_t unc0_vxsnprintf(char *out, size_t n, int gflags,
                       const char *format, va_list arg);
size_t unc0_xsnprintf(char *out, size_t n, int gflags,
                      const char *format, ...);

#define UNC_PRINTFSPEC_NULL  0x00
#define UNC_PRINTFSPEC_STAR  0x01
#define UNC_PRINTFSPEC_C     0x02 /* int */
#define UNC_PRINTFSPEC_S     0x03 /* const char* */
#define UNC_PRINTFSPEC_P     0x06 /* void* */
#define UNC_PRINTFSPEC_N     0x07 /* int* */
#define UNC_PRINTFSPEC_LF    0x08 /* double */
#define UNC_PRINTFSPEC_LLF   0x09 /* long double */
#define UNC_PRINTFSPEC_I     0x10 /* signed int */
#define UNC_PRINTFSPEC_LI    0x11 /* signed long */
#define UNC_PRINTFSPEC_LLI   0x12 /* signed long long */
#define UNC_PRINTFSPEC_JI    0x13 /* intmax_t */
#define UNC_PRINTFSPEC_TI    0x14 /* ptrdiff_t */
#define UNC_PRINTFSPEC_ZI    0x15 /* size_t */
#define UNC_PRINTFSPEC_U     0x18 /* unsigned int */
#define UNC_PRINTFSPEC_LU    0x19 /* unsigned long */
#define UNC_PRINTFSPEC_LLU   0x1A /* unsigned long long */
#define UNC_PRINTFSPEC_JU    0x1B /* uintmax_t */
#define UNC_PRINTFSPEC_TU    0x1C /* ptrdiff_t */
#define UNC_PRINTFSPEC_ZU    0x1D /* size_t */

#define UNC_PRINTFSPEC_MAX      4
size_t unc0_printf_specparse(unsigned *output, const char **p_format,
                             int gflags);

#endif /* UNCIL_UXPRINTF_H */
