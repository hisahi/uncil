/*******************************************************************************
 
Uncil -- value string I/O header

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

#ifndef UNCIL_UVSIO_H
#define UNCIL_UVSIO_H

#include <stdarg.h>

#include "umem.h"
#include "uval.h"

struct Unc_View;

Unc_RetVal unc0_vcvt2str(struct Unc_View *w, Unc_Value *in,
            int (*out)(Unc_Size n, const byte *s, void *udata), void *udata);

size_t unc0_savxprintf(Unc_Allocator *alloc, byte **s, const char *fmt,
                    va_list arg);
size_t unc0_saxprintf(Unc_Allocator *alloc, byte **s, const char *fmt, ...);

Unc_RetVal unc0_usvxprintf(struct Unc_View *w, Unc_Value *out,
                           const char *fmt, va_list arg);
Unc_RetVal unc0_usxprintf(struct Unc_View *w, Unc_Value *out,
                          const char *fmt, ...);

size_t unc0_sacvxprintf(struct unc0_strbuf *buf,
                     int gflags, size_t fmt_n, const char *fmt, va_list arg);
size_t unc0_sacxprintf(struct unc0_strbuf *buf,
                    int gflags, size_t fmt_n, const char *fmt, ...);

Unc_RetVal unc0_std_makeerr(Unc_View *w, const char *mt,
                            const char *prefix, Unc_RetVal err);
size_t unc0_sxscanf(Unc_Size sn, const byte *bn, const char *format, ...);

Unc_RetVal unc0_buftostring(struct Unc_View *w, Unc_Value *out,
                            struct unc0_strbuf *buf);
Unc_RetVal unc0_buftoblob(struct Unc_View *w, Unc_Value *out,
                          struct unc0_strbuf *buf);

#endif /* UNCIL_UVSIO_H */
