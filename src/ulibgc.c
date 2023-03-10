/*******************************************************************************
 
Uncil -- builtin gc library impl

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

#define UNCIL_DEFINES

#include "udef.h"
#include "ugc.h"
#include "uncil.h"

Unc_RetVal unc0_lib_gc_collect(Unc_View *w, Unc_Tuple args, void *udata) {
    unc0_gccollect(w->world, w);
    return 0;
}

Unc_RetVal unc0_lib_gc_enable(Unc_View *w, Unc_Tuple args, void *udata) {
    (void)UNC_LOCKFP(w, w->world->entity_lock);
    w->world->gc.enabled = 1;
    UNC_UNLOCKF(w->world->entity_lock);
    return 0;
}

Unc_RetVal unc0_lib_gc_disable(Unc_View *w, Unc_Tuple args, void *udata) {
    (void)UNC_LOCKFP(w, w->world->entity_lock);
    w->world->gc.enabled = 0;
    UNC_UNLOCKF(w->world->entity_lock);
    return 0;
}

Unc_RetVal unc0_lib_gc_enabled(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value v = UNC_BLANK;
    (void)UNC_LOCKFP(w, w->world->entity_lock);
    unc_setbool(w, &v, w->world->gc.enabled);
    UNC_UNLOCKF(w->world->entity_lock);
    return unc_pushmove(w, &v, NULL);
}

Unc_RetVal unc0_lib_gc_getthreshold(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value v = UNC_BLANK;
    (void)UNC_LOCKFP(w, w->world->entity_lock);
    unc_setint(w, &v, w->world->gc.entitylimit);
    UNC_UNLOCKF(w->world->entity_lock);
    return unc_pushmove(w, &v, NULL);
}

Unc_RetVal unc0_lib_gc_setthreshold(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Int i;
    int e;
    e = unc_getint(w, &args.values[0], &i);
    if (e) return e;
    if (i <= 0 || i > INT_MAX)
        return unc_throwexc(w, "value", "invalid threshold value");
    (void)UNC_LOCKFP(w, w->world->entity_lock);
    w->world->gc.entitylimit = (int)i;
    UNC_UNLOCKF(w->world->entity_lock);
    return 0;
}

Unc_RetVal unc0_lib_gc_getusage(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value v = UNC_BLANK;
    (void)UNC_LOCKFP(w, w->world->entity_lock);
    unc_setint(w, &v, w->world->alloc.total);
    UNC_UNLOCKF(w->world->entity_lock);
    return unc_pushmove(w, &v, NULL);
}

Unc_RetVal uncilmain_gc(struct Unc_View *w) {
    Unc_RetVal e;
    e = unc_exportcfunction(w, "collect", &unc0_lib_gc_collect,
                            UNC_CFUNC_DEFAULT,
                            0, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "enable", &unc0_lib_gc_enable,
                            UNC_CFUNC_DEFAULT,
                            0, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "disable", &unc0_lib_gc_disable,
                            UNC_CFUNC_DEFAULT,
                            0, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "enabled", &unc0_lib_gc_enabled,
                            UNC_CFUNC_DEFAULT,
                            0, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "getthreshold", &unc0_lib_gc_getthreshold,
                            UNC_CFUNC_DEFAULT,
                            0, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "setthreshold", &unc0_lib_gc_setthreshold,
                            UNC_CFUNC_DEFAULT,
                            1, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "getusage", &unc0_lib_gc_getusage,
                            UNC_CFUNC_DEFAULT,
                            0, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    return 0;
}
