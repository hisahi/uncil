/*******************************************************************************
 
Uncil -- module system header

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

#ifndef UNCIL_UMODULE_H
#define UNCIL_UMODULE_H

#include "uncil.h"
#include "uobj.h"

typedef struct Unc_ModuleFrame {
    struct Unc_ModuleFrame *nextf;
    Unc_HTblS *pubs;
    Unc_HTblS *exports;
    Unc_Program *program;
    Unc_Stack sreg;
    Unc_Value met_str;
    Unc_Value met_blob;
    Unc_Value met_arr;
    Unc_Value met_dict;
    Unc_Value fmain;
    char import;
    size_t curdir_n;
    const byte *curdir;
} Unc_ModuleFrame;

int unc0_dorequire(struct Unc_View *w, Unc_Size name_n,
                   const byte *name, Unc_Object *obj);
int unc0_dorequirec(Unc_View *w,
                    Unc_Size dname_n, const byte *dname,
                    Unc_Size fname_n, const byte *fname,
                    Unc_Object *obj);

typedef Unc_RetVal (*Unc_CMain)(struct Unc_View *w);

/* builtin modules; dump their stuff into public and meant to be used with
   require */
extern Unc_RetVal uncilmain_cbor(struct Unc_View *w);
extern Unc_RetVal uncilmain_convert(struct Unc_View *w);
extern Unc_RetVal uncilmain_coroutine(struct Unc_View *w);
extern Unc_RetVal uncilmain_fs(struct Unc_View *w);
extern Unc_RetVal uncilmain_gc(struct Unc_View *w);
extern Unc_RetVal uncilmain_io(struct Unc_View *w);
extern Unc_RetVal uncilmain_json(struct Unc_View *w);
extern Unc_RetVal uncilmain_math(struct Unc_View *w);
extern Unc_RetVal uncilmain_os(struct Unc_View *w);
extern Unc_RetVal uncilmain_process(struct Unc_View *w);
extern Unc_RetVal uncilmain_random(struct Unc_View *w);
extern Unc_RetVal uncilmain_regex(struct Unc_View *w);
extern Unc_RetVal uncilmain_sys(struct Unc_View *w);
extern Unc_RetVal uncilmain_thread(struct Unc_View *w);
extern Unc_RetVal uncilmain_time(struct Unc_View *w);
extern Unc_RetVal uncilmain_unicode(struct Unc_View *w);

#endif /* UNCIL_UMODULE_H */
