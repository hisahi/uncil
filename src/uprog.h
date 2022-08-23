/*******************************************************************************
 
Uncil -- program header

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

#ifndef UNCIL_UPROG_H
#define UNCIL_UPROG_H

#include "ualloc.h"
#include "ufunc.h"
#include "umem.h"
#include "umt.h"

typedef struct Unc_Program {
    Unc_AtomicLarge refcount;
    int uncil_version;
    Unc_Size code_sz;
    byte *code;
    Unc_Size data_sz;
    byte *data;
    Unc_Size main_doff;
    struct Unc_Program *next;
    char *pname; /* unc__mmalloc */
} Unc_Program;

Unc_Program *unc__newprogram(Unc_Allocator *alloc);
void unc__initprogram(Unc_Program *program);
int unc__upgradeprogram(Unc_Program *program, Unc_Allocator *alloc);
void unc__dropprogram(Unc_Program *program, Unc_Allocator *alloc);
void unc__freeprogram(Unc_Program *program, Unc_Allocator *alloc);

Unc_Program *unc__progincref(Unc_Program *program);
Unc_Program *unc__progdecref(Unc_Program *program, Unc_Allocator *alloc);

#endif /* UNCIL_UVAL_H */
