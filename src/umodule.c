/*******************************************************************************
 
Uncil -- module system impl

Copyright (c) 2021 Sampo Hippeläinen (hisahi)

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
#include <string.h>

#define UNCIL_DEFINES

#include "uarr.h"
#include "umodule.h"
#include "uobj.h"
#include "uosdef.h"
#include "ustr.h"
#include "uvali.h"
#include "uview.h"

#define MAX_EXT_LEN 6

int unc__stdlibinit(Unc_World *w, Unc_View *v);

INLINE void unc__reqrestorestate0(Unc_View *w, Unc_ModuleFrame *sav) {
    ASSERT(w->mframes == sav);
    w->mframes = sav->nextf;
    w->import = sav->import;
    w->sreg = sav->sreg;
    w->pubs = sav->pubs;
    w->exports = sav->exports;
    unc__wsetprogram(w, sav->program);
    /* wsetprogram does progincref, we don't need it */
    unc__progdecref(sav->program, &w->world->alloc);
    w->met_str = sav->met_str;
    w->met_blob = sav->met_blob;
    w->met_arr = sav->met_arr;
    w->met_dict = sav->met_dict;
    w->curdir_n = sav->curdir_n;
    w->curdir = sav->curdir;
    w->fmain = sav->fmain;
}

static void unc__reqrestorestate1(Unc_View *w, Unc_ModuleFrame *sav) {
    VCOPY(w, &w->met_str, &sav->met_str);
    VCOPY(w, &w->met_blob, &sav->met_blob);
    VCOPY(w, &w->met_arr, &sav->met_arr);
    VCOPY(w, &w->met_dict, &sav->met_dict);
    unc__reqrestorestate0(w, sav);
}

static void unc__reqrestorestate(Unc_View *w, Unc_ModuleFrame *sav) {
    VSETNULL(w, &w->fmain);
    unc__drophtbls(w, w->exports);
    unc__drophtbls(w, w->pubs);
    unc__stackfree(w, &w->sreg);
    unc__reqrestorestate1(w, sav);
}

static int unc__reqstorestate(Unc_View *w, Unc_ModuleFrame *sav) {
    int e;
    sav->import = w->import;
    sav->sreg = w->sreg;
    sav->pubs = w->pubs;
    sav->exports = w->exports;
    sav->program = w->program;
    sav->met_str = w->met_str;
    sav->met_blob = w->met_blob;
    sav->met_arr = w->met_arr;
    sav->met_dict = w->met_dict;
    sav->nextf = w->mframes;
    sav->curdir_n = w->curdir_n;
    sav->curdir = w->curdir;
    sav->fmain = w->fmain;
    w->mframes = sav;
    e = unc__stackinit(w, &w->sreg, 16);
    if (e) {
        unc__reqrestorestate1(w, sav);
        return e;
    }
    return 0;
}

static int unc__reqimposestate(Unc_View *w, Unc_ModuleFrame *sav,
                               Unc_Object *obj) {
    int e = 0;
    Unc_HTblS *exports = w->exports;
    Unc_HTblS_V *node;
    Unc_Size i, ie;
    for (i = 0, ie = exports->capacity; i < ie; ++i) {
        node = exports->buckets[i];
        while (node) {
            e = unc__osetattrs(w, obj, node->key_n, (const byte *)(&node[1]),
                                    &node->val);
            if (e) break;
            node = node->next;
        }
    }
    return e;
}

static int unc__isrelative(size_t n, const byte *path) {
    return (n >= 2 && path[0] == '.' && path[1] == UNCIL_DIRSEP)
        || (n >= 3 && path[0] == '.' && path[1] == '.'
                                     && path[2] == UNCIL_DIRSEP);
}

static void unc__updatecurdir(Unc_View *w, const char *path) {
    const char *p = strchr(path, 0);
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

/* p must be at least name_n + MAX_EXT_LEN long */
static int unc__dorequire_path_i(Unc_View *w, const char *name,
                                 Unc_Object *obj) {
    FILE *f;
    Unc_HTblS pubs, exports;

    f = fopen(name, "r");
    if (!f)
        return UNCIL_ERR_ARG_MODULENOTFOUND;
    fclose(f);

    {
        Unc_ModuleFrame sav;
        Unc_Pile pile;
        int e;

        e = unc__reqstorestate(w, &sav);
        if (e)
            return e;
        w->program = NULL;
        w->import = 1;
        w->fmain.type = Unc_TNull;
        unc__inithtbls(&w->world->alloc, w->pubs = &pubs);
        unc__inithtbls(&w->world->alloc, w->exports = &exports);
        e = unc__stdlibinit(w->world, w);
        if (e) {
            unc__reqrestorestate(w, &sav);
            return e;
        }
        unc__updatecurdir(w, name);
        e = unc_loadfileauto(w, name);
        if (e) {
            unc__reqrestorestate(w, &sav);
            return e;
        }
        e = unc_call(w, NULL, 0, &pile);
        if (e) {
            unc__reqrestorestate(w, &sav);
            return e;
        }
        unc_discard(w, &pile);
        e = unc__reqimposestate(w, &sav, obj);
        unc__reqrestorestate(w, &sav);
        if (e) return e;
    }
    return 0;
}

static int unc__dorequire_path_fmt(Unc_Allocator *alloc, int kind,
        char **buf, size_t *buf_c,
        Unc_Size path_n, const byte *path,
        Unc_Size name_n, const byte *name) {
    size_t req = path_n + name_n + 16;
    char *d;
    if (req > (Unc_Size)(size_t)-1) return UNCIL_ERR_MEM;
    if (req > *buf_c) {
        char *p = unc__mrealloc(alloc, 0, *buf, *buf_c, req);
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
    d += unc__memcpy(d, name, name_n);
    switch (kind) {
    case 0:
        d += unc__memcpy(d, ".unc", 4);
        break;
    case 1:
        *d++ = UNCIL_DIRSEP;
        d += unc__memcpy(d, "_init.unc", 4);
        break;
    }
    *d++ = 0;
    return 0;
}

static int unc__dorequire_path(Unc_View *w, Unc_Size name_n, const byte *name,
                               Unc_Object *obj) {
    int e = UNCIL_ERR_ARG_MODULENOTFOUND, ee;
    char *buf = NULL;
    size_t buf_sz = 0;
    Unc_Value v = w->world->modulepaths;
    Unc_Allocator *alloc = &w->world->alloc;

    if (unc__isrelative(name_n, name)) {
        e = unc__dorequire_path_fmt(alloc, 0, &buf, &buf_sz,
                        w->curdir_n, w->curdir, name_n, name);
        if (e) {
            unc__mfree(alloc, buf, buf_sz);
            return e;
        }
        e = unc__dorequire_path_i(w, buf, obj);
        if (!e || e != UNCIL_ERR_ARG_MODULENOTFOUND) {
            unc__mfree(alloc, buf, buf_sz);
            return e;
        }
        e = unc__dorequire_path_fmt(alloc, 1, &buf, &buf_sz,
                        w->curdir_n, w->curdir, name_n, name);
        if (e) {
            unc__mfree(alloc, buf, buf_sz);
            return e;
        }
        e = unc__dorequire_path_i(w, buf, obj);
        unc__mfree(alloc, buf, buf_sz);
        return e;
    }

    if (VGETTYPE(&v) == Unc_TArray) {
        Unc_Array *arr = LEFTOVER(Unc_Array, VGETENT(&v));
        Unc_Size i, ik = arr->size;
        for (i = 0; i < ik; ++i) {
            if (arr->data[i].type == Unc_TString) {
                Unc_String *str = LEFTOVER(Unc_String, VGETENT(&arr->data[i]));
                ee = unc__dorequire_path_fmt(alloc, 0, &buf, &buf_sz, str->size,
                                unc__getstringdata(str), name_n, name);
                if (ee) {
                    unc__mfree(alloc, buf, buf_sz);
                    return ee;
                }
                e = unc__dorequire_path_i(w, buf, obj);
                if (!e || e != UNCIL_ERR_ARG_MODULENOTFOUND) {
                    unc__mfree(alloc, buf, buf_sz);
                    return e;
                }
                ee = unc__dorequire_path_fmt(alloc, 1, &buf, &buf_sz, str->size,
                                unc__getstringdata(str), name_n, name);
                if (ee) {
                    unc__mfree(alloc, buf, buf_sz);
                    return ee;
                }
                e = unc__dorequire_path_i(w, buf, obj);
                if (!e || e != UNCIL_ERR_ARG_MODULENOTFOUND) {
                    unc__mfree(alloc, buf, buf_sz);
                    return e;
                }
            }
        }
    }
    unc__mfree(alloc, buf, buf_sz);
    return UNCIL_ERR_ARG_MODULENOTFOUND;
}

/* p must be at least name_n + MAX_EXT_LEN long */
static int unc__dorequire_unc(Unc_View *w, Unc_Size name_n, const char *name,
                              char *p, Unc_Object *obj) {
    FILE *f;
    Unc_HTblS pubs, exports;

    if (name_n > INT_MAX) return UNCIL_ERR_ARG_MODULENOTFOUND;
    sprintf(p, "%.*s", (int)name_n, name);
    if (name_n <= 4 || strcmp(name + name_n - 4, ".unc"))
        strcat(p, ".unc");
    
    f = fopen(p, "r");
    if (!f)
        return UNCIL_ERR_ARG_MODULENOTFOUND;
    fclose(f);
    
    {
        Unc_ModuleFrame sav;
        Unc_Pile pile;
        int e;

        e = unc__reqstorestate(w, &sav);
        if (e) {
            fclose(f);
            return e;
        }
        w->program = NULL;
        w->import = 1;
        w->fmain.type = Unc_TNull;
        unc__inithtbls(&w->world->alloc, w->pubs = &pubs);
        unc__inithtbls(&w->world->alloc, w->exports = &exports);
        e = unc__stdlibinit(w->world, w);
        if (e) {
            unc__reqrestorestate(w, &sav);
            return e;
        }
        unc__updatecurdir(w, p);
        e = unc_loadfileauto(w, p);
        if (e) {
            unc__reqrestorestate(w, &sav);
            return e;
        }
        e = unc_call(w, NULL, 0, &pile);
        if (e) {
            unc__reqrestorestate(w, &sav);
            return e;
        }
        unc_discard(w, &pile);
        e = unc__reqimposestate(w, &sav, obj);
        unc__reqrestorestate(w, &sav);
        if (e) return e;
    }
    return 0;
}

int unc__dorequire_std(Unc_View *w, Unc_Size name_n, const byte *name,
                              Unc_Object *obj) {
    if (name_n) {
        Unc_CMain lib = NULL;
        Unc_MMask mask;

        switch (name[0]) {
        case 'c':
            if (unc__strreqr(PASSSTRL("bor"), name_n - 1, name + 1))
                lib = &uncilmain_cbor, mask = UNC_MMASK_M_CBOR;
            if (unc__strreqr(PASSSTRL("onvert"), name_n - 1, name + 1))
                lib = &uncilmain_convert, mask = UNC_MMASK_M_CONVERT;
            if (unc__strreqr(PASSSTRL("oroutine"), name_n - 1, name + 1))
                lib = &uncilmain_coroutine, mask = UNC_MMASK_M_COROUTINE;
            break;
        case 'f':
            if (unc__strreqr(PASSSTRL("s"), name_n - 1, name + 1))
                lib = &uncilmain_fs, mask = UNC_MMASK_M_FS;
            break;
        case 'g':
            if (unc__strreqr(PASSSTRL("c"), name_n - 1, name + 1))
                lib = &uncilmain_gc, mask = UNC_MMASK_M_GC;
            break;
        case 'i':
            if (unc__strreqr(PASSSTRL("o"), name_n - 1, name + 1))
                lib = &uncilmain_io, mask = UNC_MMASK_M_IO;
            break;
        case 'j':
            if (unc__strreqr(PASSSTRL("son"), name_n - 1, name + 1))
                lib = &uncilmain_json, mask = UNC_MMASK_M_JSON;
            break;
        case 'm':
            if (unc__strreqr(PASSSTRL("ath"), name_n - 1, name + 1))
                lib = &uncilmain_math, mask = UNC_MMASK_M_MATH;
            break;
        case 'o':
            if (unc__strreqr(PASSSTRL("s"), name_n - 1, name + 1))
                lib = &uncilmain_os, mask = UNC_MMASK_M_OS;
            break;
        case 'p':
            if (unc__strreqr(PASSSTRL("rocess"), name_n - 1, name + 1))
                lib = &uncilmain_process, mask = UNC_MMASK_M_PROCESS;
            break;
        case 'r':
            if (unc__strreqr(PASSSTRL("andom"), name_n - 1, name + 1))
                lib = &uncilmain_random, mask = UNC_MMASK_M_RANDOM;
            if (unc__strreqr(PASSSTRL("egex"), name_n - 1, name + 1))
                lib = &uncilmain_regex, mask = UNC_MMASK_M_REGEX;
            break;
        case 's':
            if (unc__strreqr(PASSSTRL("ys"), name_n - 1, name + 1))
                lib = &uncilmain_sys, mask = UNC_MMASK_M_SYS;
            break;
        case 't':
            if (unc__strreqr(PASSSTRL("hread"), name_n - 1, name + 1))
                lib = &uncilmain_thread, mask = UNC_MMASK_M_THREAD;
            if (unc__strreqr(PASSSTRL("ime"), name_n - 1, name + 1))
                lib = &uncilmain_time, mask = UNC_MMASK_M_TIME;
            break;
        case 'u':
            if (unc__strreqr(PASSSTRL("nicode"), name_n - 1, name + 1))
                lib = &uncilmain_unicode, mask = UNC_MMASK_M_UNICODE;
            break;
        }

        if (lib && (w->world->mmask & mask)) {
            int e;
            Unc_ModuleFrame sav;
            Unc_HTblS pubs, exports;

            e = unc__reqstorestate(w, &sav);
            if (e)
                return e;
            w->program = NULL;
            w->import = 1;
            w->fmain.type = Unc_TNull;
            unc__inithtbls(&w->world->alloc, w->pubs = &pubs);
            unc__inithtbls(&w->world->alloc, w->exports = &exports);
            e = unc__stdlibinit(w->world, w);
            if (e) {
                unc__reqrestorestate(w, &sav);
                return e;
            }
            e = (*lib)(w);
            if (!e) e = unc__reqimposestate(w, &sav, obj);
            unc__reqrestorestate(w, &sav);
            return e;
        }
    }
    return UNCIL_ERR_ARG_MODULENOTFOUND;
}

int unc__dorequire(Unc_View *w, Unc_Size name_n,
                   const byte *name, Unc_Object *obj) {
    int e = UNCIL_ERR_ARG_MODULENOTFOUND;
    char buf[64], *p = buf;
    if (e == UNCIL_ERR_ARG_MODULENOTFOUND)
        e = unc__dorequire_path(w, name_n, name, obj);
    if (e == UNCIL_ERR_ARG_MODULENOTFOUND) {
        if (name_n > sizeof(buf) - MAX_EXT_LEN)
            p = unc__malloc(&w->world->alloc, 0, name_n + MAX_EXT_LEN);
        if (!p) return UNCIL_ERR_MEM;
        e = unc__dorequire_unc(w, name_n, (const char *)name, p, obj);
    }
    if (e == UNCIL_ERR_ARG_MODULENOTFOUND)
        e = unc__dorequire_std(w, name_n, name, obj);
    if (p != buf)
        unc__mfree(&w->world->alloc, p, name_n + MAX_EXT_LEN);
    return e;
}

#if UNCIL_REQUIREC_IMPL_DLOPEN
#include <dlfcn.h>
#define LOADEDLIB void *

int unc__dorequirec_impl_open(Unc_Allocator *alloc, const char *path,
                              LOADEDLIB *handle) {
    LOADEDLIB p = dlopen(path, RTLD_LAZY);
    if (p) {
        *handle = p;
        return 0;
    }
    return UNCIL_ERR_ARG_MODULENOTFOUND;
}

int unc__dorequirec_impl_call(Unc_View *w, LOADEDLIB handle, const char *fn) {
    char *e;
    Unc_CMain f;
    dlerror();
    *(void **)&f = dlsym(handle, fn);
    if ((e = dlerror()) != NULL)
        return UNCIL_ERR_ARG_MODULENOTFOUND;
    return (*f)(w);
}

void unc__dorequirec_impl_close(LOADEDLIB handle) {
    dlclose(handle);
}
#elif UNCIL_REQUIREC_IMPL
#error "requirec implementation missing"
#endif

static int unc__dorequirec_path_fmt(Unc_Allocator *alloc,
        char **buf, size_t *buf_c,
        Unc_Size path_n, const byte *path,
        Unc_Size name_n, const byte *name) {
    size_t req = path_n + name_n + 4;
    char *d;
    if (req > (Unc_Size)(size_t)-1) return UNCIL_ERR_MEM;
    if (req > *buf_c) {
        char *p = unc__mrealloc(alloc, 0, *buf, *buf_c, req);
        if (!p) return UNCIL_ERR_MEM;
        *buf = p;
        *buf_c = req;
    }
    d = *buf;
    if (path_n) {
        d += unc__memcpy(d, path, path_n);
        if (path[-1] != UNCIL_DIRSEP)
            *d++ = UNCIL_DIRSEP;
    }
    d += unc__memcpy(d, name, name_n);
    *d++ = 0;
    return 0;
}

int unc__dorequirec_i(Unc_View *w, const char *name,
                      const char *fn, Unc_Object *obj) {
#if !UNCIL_REQUIREC_IMPL
    return UNCIL_ERR_ARG_MODULENOTFOUND;
#else
    Unc_Allocator *alloc = &w->world->alloc;
    Unc_HTblS pubs, exports;
    LOADEDLIB lib;
    
    {
        Unc_ModuleFrame sav;
        int e;

        e = unc__dorequirec_impl_open(alloc, name, &lib);
        if (e) return e;

        e = unc__reqstorestate(w, &sav);
        if (e) {
            unc__dorequirec_impl_close(lib);
            return e;
        }
        w->program = NULL;
        w->import = 1;
        w->fmain.type = Unc_TNull;
        unc__inithtbls(alloc, w->pubs = &pubs);
        unc__inithtbls(alloc, w->exports = &exports);
        e = unc__stdlibinit(w->world, w);
        if (e) {
            unc__dorequirec_impl_close(lib);
            unc__reqrestorestate(w, &sav);
            return e;
        }
        unc__updatecurdir(w, name);
        e = unc__dorequirec_impl_call(w, lib, fn);
        if (e) {
            unc__dorequirec_impl_close(lib);
            unc__reqrestorestate(w, &sav);
            return e;
        }
        e = unc__reqimposestate(w, &sav, obj);
        unc__reqrestorestate(w, &sav);
        if (e) {
            unc__dorequirec_impl_close(lib);
            return e;
        }
    }
    return 0;
#endif
}

int unc__dorequirec(Unc_View *w,
                    Unc_Size dname_n, const byte *dname,
                    Unc_Size fname_n, const byte *fname,
                    Unc_Object *obj) {
#if !UNCIL_REQUIREC_IMPL
    return UNCIL_ERR_ARG_MODULENOTFOUND;
#else
    Unc_Allocator *alloc = &w->world->alloc;
    Unc_Value v = w->world->moduledlpaths;
    char *n;
    int e = UNCIL_ERR_ARG_MODULENOTFOUND, ee;
    char *buf = NULL;
    size_t buf_sz = 0;

    if (dname_n > INT_MAX) return UNCIL_ERR_ARG_MODULENOTFOUND;
    if (fname_n > INT_MAX) return UNCIL_ERR_ARG_MODULENOTFOUND;

    if (fname) {
        n = unc__malloc(alloc, 0, fname_n + 1);
        if (!n) return UNCIL_ERR_MEM;
        unc__memcpy(n, fname, fname_n);
        n[fname_n] = 0;
    } else
        n = "uncilmain";

    if (unc__isrelative(dname_n, dname)) {
        e = unc__dorequirec_path_fmt(alloc, &buf, &buf_sz,
                        w->curdir_n, w->curdir, dname_n, dname);
        if (!e) e = unc__dorequirec_i(w, buf, n, obj);
        unc__mfree(alloc, buf, buf_sz);
        return e;
    }

    if (VGETTYPE(&v) == Unc_TArray) {
        Unc_Array *arr = LEFTOVER(Unc_Array, VGETENT(&v));
        Unc_Size i, ik = arr->size;
        for (i = 0; i < ik; ++i) {
            if (arr->data[i].type == Unc_TString) {
                Unc_String *str = LEFTOVER(Unc_String, VGETENT(&arr->data[i]));
                ee = unc__dorequirec_path_fmt(alloc, &buf, &buf_sz, str->size,
                                unc__getstringdata(str), dname_n, dname);
                if (ee) {
                    unc__mfree(alloc, buf, buf_sz);
                    return ee;
                }
                e = unc__dorequirec_i(w, buf, n, obj);
                if (e != UNCIL_ERR_ARG_MODULENOTFOUND) {
                    unc__mfree(alloc, buf, buf_sz);
                    return e;
                }
            }
        }
    } else {
        ee = unc__dorequirec_path_fmt(alloc, &buf, &buf_sz, 0, NULL,
                                      dname_n, dname);
        if (ee) {
            unc__mfree(alloc, buf, buf_sz);
            return ee;
        }
        e = unc__dorequirec_i(w, buf, n, obj);
        if (e != UNCIL_ERR_ARG_MODULENOTFOUND) {
            unc__mfree(alloc, buf, buf_sz);
            return e;
        } 
    }
    unc__mfree(alloc, buf, buf_sz);
    return UNCIL_ERR_ARG_MODULENOTFOUND;
#endif
}
