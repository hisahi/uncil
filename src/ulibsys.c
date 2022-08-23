/*******************************************************************************
 
Uncil -- builtin sys library impl

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

#define UNCIL_DEFINES

#include "udef.h"
#include "ugc.h"
#include "umodule.h"
#include "uncil.h"
#include "uobj.h"
#include "uvali.h"
#include "uvlq.h"

Unc_RetVal unc0_lib_sys_forget(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    Unc_Size sn;
    const char *sp;
    Unc_Value *ov;
    Unc_Value v = UNC_BLANK;
    Unc_HTblS *cache = &w->world->modulecache;

    e = unc_getstring(w, &args.values[0], &sn, &sp);
    if (e) return e;
    
    ov = unc0_gethtbls(w, cache, sn, (const byte *)sp);
    if (ov) {
        unc0_delhtbls(w, cache, sn, (const byte *)sp);
        unc_setbool(w, &v, 1);
    } else
        unc_setbool(w, &v, 0);
    return unc_pushmove(w, &v, NULL);
}

Unc_RetVal unc0_lib_sys_loaddl(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    Unc_Size sn, fsn = 0;
    const char *sp, *fsp = NULL;
    Unc_Entity *en;
    Unc_Value v;
    (void)udata;

    e = unc_getstring(w, &args.values[0], &sn, &sp);
    if (e) return e;
    if (args.count >= 2) {
        e = unc_getstring(w, &args.values[1], &fsn, &fsp);
        if (e) return e;
    }
    en = unc0_wake(w, Unc_TObject);
    if (!en) return UNCIL_ERR_MEM;
    e = unc0_initobj(w, LEFTOVER(Unc_Object, en), NULL);
    if (e) {
        unc0_unwake(en, w);
        return e;
    }
    e = unc0_dorequirec(w, sn, (const byte *)sp,
                           fsn, (const byte *)fsp, LEFTOVER(Unc_Object, en));
    if (e) {
        unc0_hibernate(en, w);
        return e;
    }

    VINITENT(&v, Unc_TObject, en);
    return unc_push(w, 1, &v, NULL);
}

Unc_RetVal unc0_lib_sys_version(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    Unc_Value v;
    (void)udata;

    VINITINT(&v, UNCIL_VER_MAJOR);
    e = unc_push(w, 1, &v, NULL);
    if (e) return e;

    VINITINT(&v, UNCIL_VER_MINOR);
    e = unc_push(w, 1, &v, NULL);
    if (e) return e;

    VINITINT(&v, UNCIL_VER_PATCH);
    e = unc_push(w, 1, &v, NULL);
    if (e) return e;

    return 0;
}

static const char *endianstrings[] = { "other", "little", "big" };

Unc_RetVal uncilmain_sys(Unc_View *w) {
    Unc_RetVal e;
    e = unc_exportcfunction(w, "loaddl", &unc0_lib_sys_loaddl,
                            UNC_CFUNC_DEFAULT,
                            1, 0, 1, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "forget", &unc0_lib_sys_forget,
                            UNC_CFUNC_DEFAULT,
                            1, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_setpublicc(w, "path", &w->world->modulepaths);
    if (e) return e;
    e = unc_setpublicc(w, "dlpath", &w->world->moduledlpaths);
    if (e) return e;
    {
        Unc_Value v = UNC_BLANK;
        e = unc_newstringc(w, &v, UNCIL_TARGET);
        if (e) return e;
        e = unc_setpublicc(w, "platform", &v);
        if (e) return e;
    }
    {
        Unc_Value v = UNC_BLANK;
        e = unc_newstringc(w, &v, UNCIL_CPU_ARCH);
        if (e) return e;
        e = unc_setpublicc(w, "arch", &v);
        if (e) return e;
    }
    {
        Unc_Value v = UNC_BLANK;
        e = unc_newstringc(w, &v, UNCIL_COMPILED_WITH);
        if (e) return e;
        e = unc_setpublicc(w, "compiler", &v);
        if (e) return e;
    }
    {
        Unc_Value v = UNC_BLANK;
        e = unc_newstringc(w, &v, __DATE__ ", " __TIME__);
        if (e) return e;
        e = unc_setpublicc(w, "compiletime", &v);
        if (e) return e;
    }
    {
        Unc_Value v = UNC_BLANK;
        e = unc_newstringc(w, &v, endianstrings[unc0_getendianness()]);
        if (e) return e;
        e = unc_setpublicc(w, "endian", &v);
        if (e) return e;
    }
    {
        Unc_Value v = UNC_BLANK;
#if UNCIL_MT_OK
        unc_setbool(w, &v, 1);
#else
        unc_setbool(w, &v, 0);
#endif
        e = unc_setpublicc(w, "threaded", &v);
        if (e) return e;
    }
    {
        Unc_Value v = UNC_BLANK;
#if UNCIL_MT_OK
        e = unc_newstringc(w, &v, UNCIL_MT_PROVIDER);
        if (e) return e;
#else
        unc_setnull(w, &v);
#endif
        e = unc_setpublicc(w, "threader", &v);
        if (e) return e;
    }
    {
        Unc_Value v = UNC_BLANK;
#ifdef UNCIL_REQUIREC_IMPL
        unc_setbool(w, &v, 1);
#else
        unc_setbool(w, &v, 0);
#endif
        e = unc_setpublicc(w, "canloaddl", &v);
        if (e) return e;
    }
    e = unc_exportcfunction(w, "version", &unc0_lib_sys_version,
                            UNC_CFUNC_DEFAULT,
                            0, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    {
        Unc_Value v = UNC_BLANK;
        e = unc_newstringc(w, &v, UNCIL_VER_STRING);
        if (e) return e;
        e = unc_setpublicc(w, "versiontext", &v);
        if (e) return e;
        unc_clear(w, &v);
    }
    return 0;
}
