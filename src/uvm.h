/*******************************************************************************
 
Uncil -- VM header

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

#ifndef UNCIL_UVM_H
#define UNCIL_UVM_H

#include "udef.h"
#include "ufunc.h"
#include "uncil.h"

int unc0_fcall(Unc_View *w, Unc_Function *fn, Unc_Size argc,
               int spew, int fromc, int allowc, Unc_RegFast target);
int unc0_fcallv(Unc_View *w, Unc_Value *v, Unc_Size argc,
                int spew, int fromc, int allowc, Unc_RegFast target);
int unc0_vmrpush(Unc_View *w);
int unc0_vmcheckpause(Unc_View *w);
int unc0_run(Unc_View *w);

#ifdef UNCIL_DEFINES
void unc0_loadstrpx(const byte *offp, Unc_Size *l, const byte **b);
int unc0_vveq_j(Unc_View *w, Unc_Value *a, Unc_Value *b);
int unc0_vvclt_j(Unc_View *w, Unc_Value *a, Unc_Value *b);
#endif

#endif /* UNCIL_UVM_H */
