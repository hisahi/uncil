/*******************************************************************************
 
Uncil -- view header

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

#ifndef UNCIL_UVIEW_H
#define UNCIL_UVIEW_H

#include "uncil.h"

Unc_World *unc0_launch(Unc_Alloc alloc, void *udata);
Unc_View *unc0_newview(Unc_World *world, Unc_ViewType vtype);
void unc0_haltview(Unc_View *view);
void unc0_freeview(Unc_View *view);
void unc0_scuttle(Unc_View *v, Unc_World *w);

void unc0_wsetprogram(Unc_View *w, Unc_Program *p);

#ifdef UNCIL_DEFINES
/* in ulibthrd.c */
#if UNCIL_MT_OK
void unc0_waitonviewthread(Unc_View *w);
#endif
#endif

#endif /* UNCIL_UVIEW_H */
