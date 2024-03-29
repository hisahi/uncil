/*******************************************************************************
 
Uncil -- garbage collector header

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

#ifndef UNCIL_UGC_H
#define UNCIL_UGC_H

struct Unc_World;
struct Unc_View;

typedef struct Unc_GC {
    int enabled;
    int entitylimit;
} Unc_GC;

void unc0_gcdefaults(Unc_GC *gc);
void unc0_gccollect(struct Unc_World *w, struct Unc_View *v);
Unc_Size unc0_suggeststacksize(Unc_Size s);

#endif /* UNCIL_UGC_H */
