/*******************************************************************************
 
Uncil -- xprintf header

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

#ifndef UNCIL_UXPRINTF_H
#define UNCIL_UXPRINTF_H

#include <stdarg.h>

int unc__vxprintf(int (*out)(char outp, void *udata),
                 void *udata, const char *format, va_list arg);
int unc__xprintf(int (*out)(char outp, void *udata),
                 void *udata, const char *format, ...);

int unc__vxsnprintf(char *out, size_t n, const char *format, va_list arg);
int unc__xsnprintf(char *out, size_t n, const char *format, ...);

#endif /* UNCIL_UXPRINTF_H */
