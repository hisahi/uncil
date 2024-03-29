/*******************************************************************************
 
Uncil -- module system impl

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

#include <limits.h>

#include <stdio.h>

#define UNCIL_DEFINES

#include "uarr.h"
#include "umodule.h"
#include "uobj.h"
#include "uosdef.h"
#include "ustr.h"
#include "uvali.h"
#include "uview.h"

Unc_RetVal unc0_stdlibinit(Unc_World *w, Unc_View *v);

static void unc0_reqrestorestate(Unc_View *w, Unc_ModuleFrame *sav) {
    ASSERT(w->mframes == sav);
    w->mframes = sav->nextf;
    VSETNULL(w, &w->fmain);
    unc0_drophtbls(w, &sav->temp_exports);
    unc0_drophtbls(w, &sav->temp_pubs);
    unc0_stackfree(w, &w->sreg);
    if (w->program) unc0_progdecref(w->program, &w->world->alloc);
    w->import = sav->import;
    w->sreg = sav->sreg;
    w->pubs = sav->pubs;
    w->exports = sav->exports;
    w->program = sav->program;
    w->met_str = sav->met_str;
    w->met_blob = sav->met_blob;
    w->met_arr = sav->met_arr;
    w->met_table = sav->met_table;
    w->curdir_n = sav->curdir_n;
    w->curdir = sav->curdir;
    w->fmain = sav->fmain;
}

static void unc0_reqstorestate(Unc_View *w, Unc_ModuleFrame *sav) {
    sav->import = w->import;
    sav->sreg = w->sreg;
    sav->pubs = w->pubs;
    sav->exports = w->exports;
    sav->program = w->program;
    sav->met_str = w->met_str;
    sav->met_blob = w->met_blob;
    sav->met_arr = w->met_arr;
    sav->met_table = w->met_table;
    sav->curdir_n = w->curdir_n;
    sav->curdir = w->curdir;
    sav->fmain = w->fmain;
    sav->nextf = w->mframes;
    w->mframes = sav;
    unc0_stackinit(w, &w->sreg, 0); /* cannot fail with size 0 */
    w->program = NULL;
    VINITNULL(&w->fmain);
    VINITNULL(&w->met_str);
    VINITNULL(&w->met_blob);
    VINITNULL(&w->met_arr);
    VINITNULL(&w->met_table);
    unc0_inithtbls(&w->world->alloc, w->pubs = &sav->temp_pubs);
    unc0_inithtbls(&w->world->alloc, w->exports = &sav->temp_exports);
}

static Unc_RetVal unc0_reqimposestate(Unc_View *w, Unc_ModuleFrame *sav,
                                      Unc_Object *obj) {
    Unc_RetVal e = 0;
    Unc_HTblS *exports = w->exports;
    Unc_HTblS_V *node;
    Unc_Size i, ie;
    for (i = 0, ie = exports->capacity; i < ie; ++i) {
        node = exports->buckets[i];
        while (node) {
            e = unc0_osetattrs(w, obj, node->key_n, (const byte *)(&node[1]),
                                    &node->val);
            if (e) break;
            node = node->next;
        }
    }
    return e;
}

static int unc0_isrelative(size_t n, const byte *path) {
    return (n >= 2 && path[0] == '.' && path[1] == UNCIL_DIRSEP)
        || (n >= 3 && path[0] == '.' && path[1] == '.'
                                     && path[2] == UNCIL_DIRSEP);
}

static int unc0_module_fileexists(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

static void unc0_updatecurdir(Unc_View *w, const char *path) {
    const char *p = path + unc0_strlen(path);
    while (p >= path)
        if (*--p == UNCIL_DIRSEP)
            break;
    if (*p == UNCIL_DIRSEP) {
        w->curdir_n = path - p + 1;
        w->curdir = (const byte *)path;
    } else {
        w->curdir_n = 0;
        w->curdir = NULL;
    }
}

static Unc_RetVal unc0_dorequire_path_i(Unc_View *w, const char *name,
                                        Unc_Object *obj) {
    Unc_ModuleFrame sav;
    Unc_Pile pile;
    Unc_RetVal e;

    if (!unc0_module_fileexists(name))
        return UNCIL_ERR_ARG_MODULENOTFOUND;

    unc0_reqstorestate(w, &sav);
    e = unc0_stdlibinit(w->world, w);
    if (e) goto fail;
    unc0_updatecurdir(w, name);
    w->import = 1;
    e = unc_loadfileauto(w, name);
    if (e) goto fail;
    e = unc_call(w, NULL, 0, &pile);
    if (e) goto fail;
    unc_discard(w, &pile);
    e = unc0_reqimposestate(w, &sav, obj);
fail:
    unc0_reqrestorestate(w, &sav);
    return e;
}

static Unc_RetVal unc0_dorequire_path_fmt(Unc_Allocator *alloc, int kind,
                                          char **buf, size_t *buf_c,
                                          Unc_Size path_n, const byte *path,
                                          Unc_Size name_n, const byte *name) {
    size_t req = path_n + name_n + 16;
    char *d;
    if (req > (Unc_Size)(size_t)-1) return UNCIL_ERR_MEM;
    if (req > *buf_c) {
        char *p = unc0_mrealloc(alloc, 0, *buf, *buf_c, req);
        if (!p) return UNCIL_ERR_MEM;
        *buf = p;
        *buf_c = req;
    }
    d = *buf;
    if (path_n) {
        const byte *path_e = path + path_n;
        char c;
        while (path < path_e) {
            c = *path++;
            if (c == '/')
                c = UNCIL_DIRSEP;
            *d++ = c;
        }
        if (d[-1] != UNCIL_DIRSEP)
            *d++ = UNCIL_DIRSEP;
    }
    d += unc0_memcpy(d, name, name_n);
    switch (kind) {
    case 0:
        d += unc0_memcpy(d, ".unc", 4);
        break;
    case 1:
        *d++ = UNCIL_DIRSEP;
        d += unc0_memcpy(d, "_init.unc", 4);
        break;
    }
    *d++ = 0;
    return 0;
}

static Unc_RetVal unc0_dorequire_path(Unc_View *w,
                                      Unc_Size name_n, const byte *name,
                                      Unc_Object *obj) {
    Unc_RetVal e = UNCIL_ERR_ARG_MODULENOTFOUND;
    char *buf = NULL;
    size_t buf_sz = 0;
    Unc_Value v = w->world->modulepaths;
    Unc_Allocator *alloc = &w->world->alloc;

    if (unc0_isrelative(name_n, name)) {
        e = unc0_dorequire_path_fmt(alloc, 0, &buf, &buf_sz,
                        w->curdir_n, w->curdir, name_n, name);
        if (e) goto fail0;
        e = unc0_dorequire_path_i(w, buf, obj);
        if (!e || e != UNCIL_ERR_ARG_MODULENOTFOUND) goto fail0;
        e = unc0_dorequire_path_fmt(alloc, 1, &buf, &buf_sz,
                        w->curdir_n, w->curdir, name_n, name);
        if (e) goto fail0;
        e = unc0_dorequire_path_i(w, buf, obj);
fail0:
        unc0_mfree(alloc, buf, buf_sz);
        return e;
    }

    e = UNCIL_ERR_ARG_MODULENOTFOUND;
    if (VGETTYPE(&v) == Unc_TArray) {
        Unc_Array *arr = LEFTOVER(Unc_Array, VGETENT(&v));
        Unc_Size i, ik = arr->size;
        for (i = 0; i < ik; ++i) {
            if (arr->data[i].type == Unc_TString) {
                Unc_String *str = LEFTOVER(Unc_String, VGETENT(&arr->data[i]));
                e = unc0_dorequire_path_fmt(alloc, 0, &buf, &buf_sz, str->size,
                                unc0_getstringdata(str), name_n, name);
                if (e) break;
                e = unc0_dorequire_path_i(w, buf, obj);
                if (e != UNCIL_ERR_ARG_MODULENOTFOUND) break;
                e = unc0_dorequire_path_fmt(alloc, 1, &buf, &buf_sz, str->size,
                                unc0_getstringdata(str), name_n, name);
                if (e) break;
                e = unc0_dorequire_path_i(w, buf, obj);
                if (e != UNCIL_ERR_ARG_MODULENOTFOUND) break;
            }
        }
    }

    unc0_mfree(alloc, buf, buf_sz);
    return e;
}

static const char *unc_file_ext = ".unc";
#define MAX_EXT_LEN sizeof(unc_file_ext)

/* p must be at least name_n + MAX_EXT_LEN long */
static Unc_RetVal unc0_dorequire_unc(Unc_View *w,
                                     Unc_Size name_n, const char *name,
                                     char *p, Unc_Object *obj) {
    Unc_ModuleFrame sav;
    Unc_Pile pile;
    Unc_RetVal e;

    if (name_n > INT_MAX) return UNCIL_ERR_ARG_MODULENOTFOUND;
    unc0_memcpy(p, name, name_n);
    if (name_n <= sizeof(unc_file_ext) ||
        /* - 1 for null terminator */
            !unc0_memcmp(name + name_n - (sizeof(unc_file_ext) - 1),
                    unc_file_ext, sizeof(unc_file_ext) - 1))
        unc0_memcpy(p + name_n, unc_file_ext, sizeof(unc_file_ext));
    else
        p[name_n] = 0;
    
    if (!unc0_module_fileexists(name))
        return UNCIL_ERR_ARG_MODULENOTFOUND;
    
    unc0_reqstorestate(w, &sav);
    e = unc0_stdlibinit(w->world, w);
    if (e) goto fail;
    unc0_updatecurdir(w, p);
    w->import = 1;
    e = unc_loadfileauto(w, p);
    if (e) goto fail;
    e = unc_call(w, NULL, 0, &pile);
    if (e) goto fail;
    unc_discard(w, &pile);
    e = unc0_reqimposestate(w, &sav, obj);
fail:
    unc0_reqrestorestate(w, &sav);
    return e;
}

Unc_RetVal unc0_dorequire_std(Unc_View *w, Unc_Size name_n, const byte *name,
                              Unc_Object *obj) {
    if (name_n) {
        Unc_CMain lib = NULL;
        Unc_MMask mask;

        switch (name[0]) {
        case 'c':
            if (unc0_strreqr(PASSSTRL("bor"), name_n - 1, name + 1))
                lib = &uncilmain_cbor, mask = UNC_MMASK_M_CBOR;
            if (unc0_strreqr(PASSSTRL("onvert"), name_n - 1, name + 1))
                lib = &uncilmain_convert, mask = UNC_MMASK_M_CONVERT;
            if (unc0_strreqr(PASSSTRL("oroutine"), name_n - 1, name + 1))
                lib = &uncilmain_coroutine, mask = UNC_MMASK_M_COROUTINE;
            break;
        case 'f':
#if !UNCIL_SANDBOXED
            if (unc0_strreqr(PASSSTRL("s"), name_n - 1, name + 1))
                lib = &uncilmain_fs, mask = UNC_MMASK_M_FS;
#endif
            break;
        case 'g':
#if !UNCIL_SANDBOXED
            if (unc0_strreqr(PASSSTRL("c"), name_n - 1, name + 1))
                lib = &uncilmain_gc, mask = UNC_MMASK_M_GC;
#endif
            break;
        case 'i':
#if !UNCIL_SANDBOXED
            if (unc0_strreqr(PASSSTRL("o"), name_n - 1, name + 1))
                lib = &uncilmain_io, mask = UNC_MMASK_M_IO;
#endif
            break;
        case 'j':
            if (unc0_strreqr(PASSSTRL("son"), name_n - 1, name + 1))
                lib = &uncilmain_json, mask = UNC_MMASK_M_JSON;
            break;
        case 'm':
            if (unc0_strreqr(PASSSTRL("ath"), name_n - 1, name + 1))
                lib = &uncilmain_math, mask = UNC_MMASK_M_MATH;
            break;
        case 'o':
#if !UNCIL_SANDBOXED
            if (unc0_strreqr(PASSSTRL("s"), name_n - 1, name + 1))
                lib = &uncilmain_os, mask = UNC_MMASK_M_OS;
#endif
            break;
        case 'p':
#if !UNCIL_SANDBOXED
            if (unc0_strreqr(PASSSTRL("rocess"), name_n - 1, name + 1))
                lib = &uncilmain_process, mask = UNC_MMASK_M_PROCESS;
#endif
            break;
        case 'r':
            if (unc0_strreqr(PASSSTRL("andom"), name_n - 1, name + 1))
                lib = &uncilmain_random, mask = UNC_MMASK_M_RANDOM;
            if (unc0_strreqr(PASSSTRL("egex"), name_n - 1, name + 1))
                lib = &uncilmain_regex, mask = UNC_MMASK_M_REGEX;
            break;
        case 's':
#if !UNCIL_SANDBOXED
            if (unc0_strreqr(PASSSTRL("ys"), name_n - 1, name + 1))
                lib = &uncilmain_sys, mask = UNC_MMASK_M_SYS;
#endif
            break;
        case 't':
#if !UNCIL_SANDBOXED
            if (unc0_strreqr(PASSSTRL("hread"), name_n - 1, name + 1))
                lib = &uncilmain_thread, mask = UNC_MMASK_M_THREAD;
#endif
            if (unc0_strreqr(PASSSTRL("ime"), name_n - 1, name + 1))
                lib = &uncilmain_time, mask = UNC_MMASK_M_TIME;
            break;
        case 'u':
            if (unc0_strreqr(PASSSTRL("nicode"), name_n - 1, name + 1))
                lib = &uncilmain_unicode, mask = UNC_MMASK_M_UNICODE;
            break;
        }

        if (lib && (w->world->mmask & mask)) {
            Unc_RetVal e;
            Unc_ModuleFrame sav;

            unc0_reqstorestate(w, &sav);
            e = unc0_stdlibinit(w->world, w);
            if (e) goto fail;
            w->import = 1;
            e = (*lib)(w);
            if (!e) e = unc0_reqimposestate(w, &sav, obj);
fail:
            unc0_reqrestorestate(w, &sav);
            return e;
        }
    }
    return UNCIL_ERR_ARG_MODULENOTFOUND;
}

Unc_RetVal unc0_dorequire(Unc_View *w, Unc_Size name_n,
                          const byte *name, Unc_Object *obj) {
    Unc_RetVal e = UNCIL_ERR_ARG_MODULENOTFOUND;
    char buf[64], *p = buf;
    if (e == UNCIL_ERR_ARG_MODULENOTFOUND)
        e = unc0_dorequire_path(w, name_n, name, obj);
    if (e == UNCIL_ERR_ARG_MODULENOTFOUND) {
        if (name_n > sizeof(buf) - MAX_EXT_LEN)
            p = unc0_malloc(&w->world->alloc, 0, name_n + MAX_EXT_LEN);
        if (!p) return UNCIL_ERR_MEM;
        e = unc0_dorequire_unc(w, name_n, (const char *)name, p, obj);
    }
    if (e == UNCIL_ERR_ARG_MODULENOTFOUND)
        e = unc0_dorequire_std(w, name_n, name, obj);
    if (p != buf)
        unc0_mfree(&w->world->alloc, p, name_n + MAX_EXT_LEN);
    return e;
}

#if UNCIL_REQUIREC_IMPL_DLOPEN
#include <dlfcn.h>
#define LOADEDLIB void *

Unc_RetVal unc0_dorequirec_impl_open(Unc_Allocator *alloc,
                                     const char *path, LOADEDLIB *handle) {
    LOADEDLIB p = dlopen(path, RTLD_LAZY);
    if (p) {
        *handle = p;
        return 0;
    }
    return UNCIL_ERR_ARG_MODULENOTFOUND;
}

Unc_RetVal unc0_dorequirec_impl_call(Unc_View *w, LOADEDLIB handle,
                                     const char *fn) {
    char *e;
    Unc_CMain f;
    dlerror();
    *(void **)&f = dlsym(handle, fn);
    if ((e = dlerror()) != NULL)
        return UNCIL_ERR_ARG_MODULENOTFOUND;
    return (*f)(w);
}

void unc0_dorequirec_impl_close(LOADEDLIB handle) {
    dlclose(handle);
}
#elif UNCIL_REQUIREC_IMPL
#error "requirec implementation missing"
#endif

static Unc_RetVal unc0_dorequirec_path_fmt(Unc_Allocator *alloc,
                                           char **buf, size_t *buf_c,
                                           Unc_Size path_n, const byte *path,
                                           Unc_Size name_n, const byte *name) {
    size_t req = path_n + name_n + 4;
    char *d;
    if (req > (Unc_Size)(size_t)-1) return UNCIL_ERR_MEM;
    if (req > *buf_c) {
        char *p = unc0_mrealloc(alloc, 0, *buf, *buf_c, req);
        if (!p) return UNCIL_ERR_MEM;
        *buf = p;
        *buf_c = req;
    }
    d = *buf;
    if (path_n) {
        d += unc0_memcpy(d, path, path_n);
        if (path[-1] != UNCIL_DIRSEP)
            *d++ = UNCIL_DIRSEP;
    }
    d += unc0_memcpy(d, name, name_n);
    *d++ = 0;
    return 0;
}

Unc_RetVal unc0_dorequirec_i(Unc_View *w, const char *name,
                             const char *fn, Unc_Object *obj) {
#if !UNCIL_REQUIREC_IMPL
    return UNCIL_ERR_ARG_MODULENOTFOUND;
#else
    Unc_Allocator *alloc = &w->world->alloc;
    LOADEDLIB lib;
    
    {
        Unc_ModuleFrame sav;
        Unc_RetVal e;

        e = unc0_dorequirec_impl_open(alloc, name, &lib);
        if (e) return e;

        unc0_reqstorestate(w, &sav);
        e = unc0_stdlibinit(w->world, w);
        if (e) goto fail;
        unc0_updatecurdir(w, name);
        w->import = 1;
        e = unc0_dorequirec_impl_call(w, lib, fn);
        if (e) goto fail;
        e = unc0_reqimposestate(w, &sav, obj);
fail:
        unc0_reqrestorestate(w, &sav);
        if (e) {
            unc0_dorequirec_impl_close(lib);
            return e;
        }
    }
    return 0;
#endif
}

Unc_RetVal unc0_dorequirec(Unc_View *w,
                           Unc_Size dname_n, const byte *dname,
                           Unc_Size fname_n, const byte *fname,
                           Unc_Object *obj) {
#if !UNCIL_REQUIREC_IMPL
    return UNCIL_ERR_ARG_MODULENOTFOUND;
#else
    Unc_Allocator *alloc = &w->world->alloc;
    Unc_Value v = w->world->moduledlpaths;
    char *n;
    Unc_RetVal e = UNCIL_ERR_ARG_MODULENOTFOUND;
    char *buf = NULL;
    size_t buf_sz = 0;

    if (dname_n > INT_MAX) return UNCIL_ERR_ARG_MODULENOTFOUND;
    if (fname_n > INT_MAX) return UNCIL_ERR_ARG_MODULENOTFOUND;

    if (fname) {
        n = unc0_malloc(alloc, 0, fname_n + 1);
        if (!n) return UNCIL_ERR_MEM;
        unc0_memcpy(n, fname, fname_n);
        n[fname_n] = 0;
    } else
        n = "uncilmain";

    e = UNCIL_ERR_ARG_MODULENOTFOUND;
    if (unc0_isrelative(dname_n, dname)) {
        e = unc0_dorequirec_path_fmt(alloc, &buf, &buf_sz,
                        w->curdir_n, w->curdir, dname_n, dname);
        if (!e) e = unc0_dorequirec_i(w, buf, n, obj);
    } else if (VGETTYPE(&v) == Unc_TArray) {
        Unc_Array *arr = LEFTOVER(Unc_Array, VGETENT(&v));
        Unc_Size i, ik = arr->size;
        for (i = 0; i < ik; ++i) {
            if (arr->data[i].type == Unc_TString) {
                Unc_String *str = LEFTOVER(Unc_String, VGETENT(&arr->data[i]));
                e = unc0_dorequirec_path_fmt(alloc, &buf, &buf_sz, str->size,
                                unc0_getstringdata(str), dname_n, dname);
                if (e) break;
                e = unc0_dorequirec_i(w, buf, n, obj);
                if (e != UNCIL_ERR_ARG_MODULENOTFOUND) break;
            }
        }
    } else {
        e = unc0_dorequirec_path_fmt(alloc, &buf, &buf_sz, 0, NULL,
                                      dname_n, dname);
        if (!e) e = unc0_dorequirec_i(w, buf, n, obj);
    }

    unc0_mfree(alloc, buf, buf_sz);
    return e;
#endif
}
