/*******************************************************************************
 
Uncil -- lexer/parser context header

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

#ifndef UNCIL_UCXT_H
#define UNCIL_UCXT_H

#include "ubtree.h"
#include "uhash.h"
#include "umem.h"

typedef struct Unc_Context {
    Unc_Allocator *alloc;
    Unc_Size regbase;
    Unc_Size main_off;
    Unc_Size main_dta;
    Unc_Size next_aux;
    /* extensible? used for "REPL mode" */
    char extend;
} Unc_Context;

int unc__newcontext(Unc_Context *cxt, Unc_Allocator *alloc);
void unc__dropcontext(Unc_Context *cxt);

#endif /* UNCIL_UCXT_H */
