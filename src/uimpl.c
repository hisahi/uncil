/*******************************************************************************
 
Uncil -- API impl

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

#include <limits.h>
#include <stdio.h>
#include <string.h>

#define UNCIL_DEFINES

#include "uarr.h"
#include "ublob.h"
#include "ucomp.h"
#include "udebug.h"
#include "uerr.h"
#include "ufunc.h"
#include "ulex.h"
#include "uncil.h"
#include "uobj.h"
#include "uopaque.h"
#include "uoptim.h"
#include "uosdef.h"
#include "uparse.h"
#include "ustr.h"
#include "uutf.h"
#include "uvali.h"
#include "uview.h"
#include "uvlq.h"
#include "uvm.h"
#include "uvop.h"
#include "uvsio.h"
#include "uxprintf.h"

#define CHECKHALT(w) if (w->flow == UNC_VIEW_FLOW_HALT) return UNCIL_ERR_HALT;

int unc__stdlibinit(Unc_World *w, Unc_View *v);

int unc_getversion_major(void) {
    return UNCIL_VER_MAJOR;
}

int unc_getversion_minor(void) {
    return UNCIL_VER_MINOR;
}

int unc_getversion_patch(void) {
    return UNCIL_VER_PATCH;
}

int unc_getversion_flags(void) {
    int flags = 0;
#if UNCIL_MT_OK
    flags |= UNC_VER_FLAG_MULTITHREADING;
#endif
    return flags;
}

Unc_View *unc_create(void) {
    return unc_createex(NULL, NULL, UNC_MMASK_DEFAULT);
}
    
Unc_View *unc_createex(Unc_Alloc alloc, void *udata, Unc_MMask mmask) {
    Unc_World *world = unc__incept(alloc, udata);
    Unc_View *view;
    if (!world) return NULL;
    world->mmask = mmask;
    
    view = unc__newview(world, Unc_ViewTypeNormal);
    if (view) {
        int e;
        e = unc__makeexception(view, "memory", "out of memory",
                               &world->exc_oom);
        if (!e) {
            e = unc__stdlibinit(world, view);
            if (!e) {    
                VIMPOSE(view, &world->met_str, &view->met_str);
                VIMPOSE(view, &world->met_blob, &view->met_blob);
                VIMPOSE(view, &world->met_arr, &view->met_arr);
                VIMPOSE(view, &world->met_dict, &view->met_dict);
                return view;
            }
        }
        unc__freeview(view);
        return NULL;
    }
    unc__doomsday(NULL, world);
    return NULL;
}

Unc_View *unc_dup(Unc_View *w) {
    Unc_View *w2;
    (void)UNC_LOCKFP(w, w->world->viewlist_lock);
    w2 = unc__newview(w->world, Unc_ViewTypeNormal);
    UNC_UNLOCKF(w->world->viewlist_lock);
    return w2;
}

Unc_View *unc_fork(Unc_View *w, int daemon) {
    Unc_View *w2;
    (void)UNC_LOCKFP(w, w->world->viewlist_lock);
    w2 = unc__newview(w->world, daemon ? Unc_ViewTypeSubDaemon
                                       : Unc_ViewTypeSub);
    UNC_UNLOCKF(w->world->viewlist_lock);
    return w2;
}

int unc_coinhabited(Unc_View *w1, Unc_View *w2) {
    return w1->world == w2->world;
}

void unc_copyprogram(Unc_View *w1, Unc_View *w2) {
    unc__wsetprogram(w1, w2->program);
    VCOPY(w1, &w1->fmain, &w2->fmain);
}

Unc_RetVal unc_compilestream(Unc_View *w, int (*getch)(void *), void *udata) {
    Unc_Allocator *alloc = &w->world->alloc;
    Unc_Context cxt = { NULL };
    Unc_LexOut lexout;
    Unc_QCode qcode;
    Unc_Program *program;
    Unc_Function newm;
    Unc_Entity *en;
    int e;

    w->comperrlineno = 0;
    if (!w->import && w->world->wmode == Unc_ModeREPL) {
        cxt = w->world->ccxt;
        program = w->program;
        ASSERT(!w->program || w->fmain.type == Unc_TFunction);
        en = VGETENT(&w->fmain);
    } else
        program = NULL;

    if (!program) {
        program = unc__newprogram(alloc);
        if (!program)
            return UNCIL_ERR_MEM;
        if (w->world->wmode == Unc_ModeREPL) {
            const char pname[] = "<repl>";
            if (!(program->pname = unc__mmalloc(alloc,
                                   Unc_AllocExternal, sizeof(pname))))
                return UNCIL_ERR_MEM;
            unc__memcpy(program->pname, pname, sizeof(pname));
        }
        en = unc__wake(w, Unc_TFunction);
        if (!en) {
            unc__freeprogram(program, alloc);
            return UNCIL_ERR_MEM;
        }
    }

    if (!cxt.alloc && (e = unc__newcontext(&cxt, alloc)))
        return e;
    if ((e = unc__lexcode(&cxt, &lexout, getch, udata))) {
        w->comperrlineno = lexout.lineno;
        goto unc_compilestream_fail;
    }
    cxt.extend = w->world->wmode;
    if ((e = unc__parsec1(&cxt, &qcode, &lexout))) {
        w->comperrlineno = qcode.lineno;
        goto unc_compilestream_fail;
    }
    if ((e = unc__optqcode(&cxt, &qcode)))
        goto unc_compilestream_fail;
    if ((e = unc__parsec2(&cxt, program, &qcode)))
        goto unc_compilestream_fail;
    
    program->main_doff = cxt.main_off;
    e = unc__initfuncu(w, &newm, program, cxt.main_off, 1);
    if (e) goto unc_compilestream_fail;

    if (!w->import && w->world->wmode == Unc_ModeREPL) {
        w->world->ccxt = cxt;
        if (w->fmain.type == Unc_TFunction)
            unc__dropfunc(w, LEFTOVER(Unc_Function, VGETENT(&w->fmain)));
        if (program != w->program)
            unc__wsetprogram(w, program);
        w->bcode = program->code;
        w->bdata = program->data;
    } else {
        unc__dropcontext(&cxt);
        unc__wsetprogram(w, program);
    }

    *LEFTOVER(Unc_Function, en) = newm;
    VINITENT(&w->fmain, Unc_TFunction, en);
    return 0;
unc_compilestream_fail:
    unc__dropcontext(&cxt);
    if (program != w->program) {
        unc__unwake(en, w);
        unc__freeprogram(program, alloc);
    }
    return e;
}

Unc_Size unc_getcompileerrorlinenumber(Unc_View *w) {
    return w->comperrlineno;
}

struct Unc_StrReadTemp {
    const byte *start;
    const byte *end;
};

static int getch_strc_(void *p) {
    struct Unc_StrReadTemp *s = p;
    int i;
    if (s->start == s->end)
        return -1;
    i = *s->start++;
    if (!i)
        s->end = s->start;
    return i;
}

static int getch_str_(void *p) {
    struct Unc_StrReadTemp *s = p;
    if (s->start == s->end)
        return -1;
    return *s->start++;
}

static int getch_file_(void *p) {
    return getc((FILE *)p);
}

Unc_RetVal unc_compilestringc(Unc_View *w, const char* text) {
    struct Unc_StrReadTemp str;
    str.start = (const byte *)text;
    str.end = NULL;
    return unc_compilestream(w, &getch_strc_, &str);
}

Unc_RetVal unc_compilestring(Unc_View *w, Unc_Size n, const char* text) {
    struct Unc_StrReadTemp str;
    str.start = (const byte *)text;
    str.end = (const byte *)text + n;
    return unc_compilestream(w, &getch_str_, &str);
}

Unc_RetVal unc_compilefile(Unc_View *w, FILE *file) {
    return unc_compilestream(w, &getch_file_, file);
}

static int unc__streamreadc(int (*getch)(void *), void *udata,
                            byte *buf, Unc_Size n) {
    Unc_Size i;
    int c;
    for (i = 0; i < n; ++i) {
        c = getch(udata);
        if (c < 0)
            return UNCIL_ERR_IO_GENERIC;
        buf[i] = c;
    }
    return 0;
}

static int unc__streamread0(int (*getch)(void *), void *udata, int n) {
    int i, c;
    for (i = 0; i < n; ++i) {
        c = getch(udata);
        if (c < 0)
            return UNCIL_ERR_IO_GENERIC;
    }
    return 0;
}

static int unc__streamreadz(int (*getch)(void *), void *udata,
                            byte *buf, Unc_Size n, Unc_Size *out) {
    Unc_Size s, p;
    int e;
    ASSERT(n > 0);
    e = unc__streamreadc(getch, udata, buf, n);
    if (e) return e;
    p = s = 0;
    while (n--) {
        s <<= CHAR_BIT;
        if (p > s) {
            /* we must have switched beyond the bounds */
            return UNCIL_ERR_PROGRAM_INCOMPATIBLE;
        }
        s |= buf[n];
        p = s;
    }
    *out = s;
    return 0;
}

#define GAMMA 0.57721566490153286060651209008240243104215933593992

Unc_RetVal unc_loadstream(Unc_View *w, int (*getch)(void *), void *udata) {
    /*    4B    "UncL"
          4B    version
          2B    CHAR_BIT
          2B    endianness (0 = other, 1 = little, 2 = big)
          4B    size of Unc_Size
          4B    size of Unc_Int
          4B    size of Unc_Float
          ?B    gamma Unc_Float
          ---pad to 4B
          4B    reserved
          4B    reserved
          4B    reserved
          8B    UNC_BYTES_IN_FCODEADDR
          8B    code size
          8B    main func offset into data
          8B    data size
          8B    debug block size
          ???   code
          ???   data                
          ???   debug block*/
    int e;
    Unc_Size z, v, sc, om, sd;
    byte buf[8];
    Unc_Float f;
    byte dbuf[sizeof(Unc_Float)];
    Unc_Program *prog;
    byte *pc, *pd;
    Unc_Function newm;
    Unc_Entity *en;

    if ((e = unc__streamreadz(getch, udata, buf, 4, &z))) return e;
    if (z != 0x636E558BUL) return UNCIL_ERR_PROGRAM_INCOMPATIBLE;
    if ((e = unc__streamreadz(getch, udata, buf, 4, &v))) return e;
    if (v > INT_MAX) return UNCIL_ERR_PROGRAM_INCOMPATIBLE;
    if ((e = unc__streamreadz(getch, udata, buf, 2, &z))) return e;
    if (z != CHAR_BIT) return UNCIL_ERR_PROGRAM_INCOMPATIBLE;
    if ((e = unc__streamreadz(getch, udata, buf, 2, &z))) return e;
    if (z != unc__getendianness()) return UNCIL_ERR_PROGRAM_INCOMPATIBLE;
    if ((e = unc__streamreadz(getch, udata, buf, 4, &z))) return e;
    if (z != sizeof(Unc_Size)) return UNCIL_ERR_PROGRAM_INCOMPATIBLE;
    if ((e = unc__streamreadz(getch, udata, buf, 4, &z))) return e;
    if (z != sizeof(Unc_Int)) return UNCIL_ERR_PROGRAM_INCOMPATIBLE;
    if ((e = unc__streamreadz(getch, udata, buf, 4, &z))) return e;
    if (z != sizeof(Unc_Float)) return UNCIL_ERR_PROGRAM_INCOMPATIBLE;
    if ((e = unc__streamreadc(getch, udata, dbuf, sizeof(Unc_Float)))) return e;
    unc__memcpy(&f, dbuf, sizeof(Unc_Float));
    if (f != (Unc_Float)GAMMA) return UNCIL_ERR_PROGRAM_INCOMPATIBLE;
    if ((e = unc__streamread0(getch, udata,
                            (4 - (sizeof(Unc_Float) & 3)) & 3))) return e;
    if ((e = unc__streamread0(getch, udata, 12))) return e;
    if ((e = unc__streamreadz(getch, udata, buf, 8, &z))) return e;
    if (z != UNC_BYTES_IN_FCODEADDR) return UNCIL_ERR_PROGRAM_INCOMPATIBLE;
    if ((e = unc__streamreadz(getch, udata, buf, 8, &sc))) return e;
    if ((e = unc__streamreadz(getch, udata, buf, 8, &om))) return e;
    if ((e = unc__streamreadz(getch, udata, buf, 8, &sd))) return e;

    prog = unc__newprogram(&w->world->alloc);
    if (!prog) {
        e = UNCIL_ERR_MEM;
        goto unc_loadstream_fail0;
    }
    pc = unc__malloc(&w->world->alloc, 0, sc);
    if (!pc) {
        e = UNCIL_ERR_MEM;
        goto unc_loadstream_fail1;
    }
    pd = unc__malloc(&w->world->alloc, 0, sd);
    if (!pd) {
        e = UNCIL_ERR_MEM;
        goto unc_loadstream_fail2;
    }

    prog->uncil_version = v;
    prog->code_sz = sc;
    prog->code = pc;
    prog->data_sz = sd;
    prog->data = pd;
    prog->main_doff = om;

    if ((e = unc__streamreadc(getch, udata, pc, sc)))
        goto unc_loadstream_fail2;
    if ((e = unc__streamreadc(getch, udata, pd, sd)))
        goto unc_loadstream_fail2;

    if ((e = unc__upgradeprogram(prog, &w->world->alloc)))
        goto unc_loadstream_fail2;
    if ((e = unc__initfuncu(w, &newm, prog, om, 1)))
        goto unc_loadstream_fail3;

    en = unc__wake(w, Unc_TFunction);
    if (!en) {
        e = UNCIL_ERR_MEM;
        goto unc_loadstream_fail3;
    }
    unc__wsetprogram(w, prog);
    *LEFTOVER(Unc_Function, en) = newm;
    VSETENT(w, &w->fmain, Unc_TFunction, en);
    return 0;

unc_loadstream_fail3:
    unc__dropfunc(w, &newm);
unc_loadstream_fail2:
    unc__mfree(&w->world->alloc, pd, sd);
unc_loadstream_fail1:
    unc__mfree(&w->world->alloc, pc, sc);
unc_loadstream_fail0:
    unc__freeprogram(prog, &w->world->alloc);
    return e;
}

Unc_RetVal unc_loadfile(Unc_View *w, FILE *file) {
    return unc_loadstream(w, &getch_file_, file);
}

static void unc__copyprogname(Unc_View *w, const char *fn) {
    size_t sl = strlen(fn);
    if ((w->program->pname =
            unc__mmalloc(&w->world->alloc, Unc_AllocExternal, sl + 1)))
        unc__memcpy(w->program->pname, fn, sl + 1);
}

Unc_RetVal unc_loadfileauto(Unc_View *w, const char *fn) {
    int e, c;
    FILE *file = fopen(fn, "rb");
    if (!file) return UNCIL_ERR_IO_GENERIC;
    c = getc(file);
    if (c != EOF && ungetc(c, file) == EOF) {
        fclose(file);
        return UNCIL_ERR_IO_GENERIC;
    }
    if (c != EOF && 0x80 <= c && c < 0xc0) {
        e = unc_loadfile(w, file);
        if (!e) unc__copyprogname(w, fn);
        fclose(file);
        return e;
    }
    fclose(file);
    file = fopen(fn, "r");
    if (!file) return UNCIL_ERR_IO_GENERIC;
    e = unc_compilefile(w, file);
    if (!e) unc__copyprogname(w, fn);
    fclose(file);
    return e;
}

static int unc__streamwritec(int (*putch)(int, void *), void *udata,
                             Unc_Size n, byte *buf) {
    Unc_Size i;
    for (i = 0; i < n; ++i)
        if (putch(buf[i], udata) < 0)
            return UNCIL_ERR_IO_GENERIC;
    return 0;
}

static int unc__streamwrite0(int (*putch)(int, void *), void *udata, int n) {
    Unc_Size i;
    for (i = 0; i < n; ++i)
        if (putch(0, udata) < 0)
            return UNCIL_ERR_IO_GENERIC;
    return 0;
}

static int unc__streamwritez(int (*putch)(int, void *), void *udata,
                             Unc_Size n, Unc_Size v) {
    ASSERT(n > 0);
    while (n--) {
        if (putch((unsigned char)v, udata) < 0)
            return UNCIL_ERR_IO_GENERIC;
        v >>= CHAR_BIT;
    }
    if (v)
        return UNCIL_ERR_PROGRAM_INCOMPATIBLE;
    return 0;
}

static int putch_file_(int c, void *p) {
    return putc(c, (FILE *)p);
}

Unc_RetVal unc_dumpstream(Unc_View *w, int (*putch)(int, void *), void *udata) {
    int e;
    Unc_Program *prog = w->program;
    byte buf[sizeof(Unc_Float)];
    Unc_Float g = (Unc_Float)GAMMA;
    unc__memcpy(buf, &g, sizeof(Unc_Float));

    if ((e = unc__streamwritez(putch, udata, 4, 0x636E558BUL)))
        return e;
    if ((e = unc__streamwritez(putch, udata, 4, prog->uncil_version)))
        return e;
    if ((e = unc__streamwritez(putch, udata, 2, CHAR_BIT)))
        return e;
    if ((e = unc__streamwritez(putch, udata, 2, unc__getendianness())))
        return e;
    if ((e = unc__streamwritez(putch, udata, 4, sizeof(Unc_Size))))
        return e;
    if ((e = unc__streamwritez(putch, udata, 4, sizeof(Unc_Int))))
        return e;
    if ((e = unc__streamwritez(putch, udata, 4, sizeof(Unc_Float))))
        return e;
    if ((e = unc__streamwritec(putch, udata, sizeof(buf), buf)))
        return e;
    if ((e = unc__streamwrite0(putch, udata, (4 - (sizeof(buf) & 3)) & 3)))
        return e;
    if ((e = unc__streamwrite0(putch, udata, 12)))
        return e;
    if ((e = unc__streamwritez(putch, udata, 8, UNC_BYTES_IN_FCODEADDR)))
        return e;
    if ((e = unc__streamwritez(putch, udata, 8, prog->code_sz)))
        return e;
    if ((e = unc__streamwritez(putch, udata, 8, prog->main_doff)))
        return e;
    if ((e = unc__streamwritez(putch, udata, 8, prog->data_sz)))
        return e;
    if ((e = unc__streamwritec(putch, udata, prog->code_sz, prog->code)))
        return e;
    if ((e = unc__streamwritec(putch, udata, prog->data_sz, prog->data)))
        return e;
    return 0;
}

Unc_RetVal unc_dumpfile(Unc_View *w, FILE *file) {
    return unc_dumpstream(w, &putch_file_, file);
}

void unc_copy(Unc_View *w, Unc_Value *dst, Unc_Value *src) {
    VCOPY(w, dst, src);
}

void unc_move(Unc_View *w, Unc_Value *dst, Unc_Value *src) {
    VMOVE(w, dst, src);
    VINITNULL(src);
}

void unc_swap(Unc_View *w, Unc_Value *va, Unc_Value *vb) {
    Unc_Value v = *va;
    *va = *vb;
    *vb = v;
}

Unc_RetVal unc_load(Unc_View *w, Unc_Program *p) {
    int e;
    Unc_Function newm;
    e = unc__upgradeprogram(p, &w->world->alloc);
    if (!e)
        e = unc__initfuncu(w, &newm, p, p->main_doff, 1);
    if (e)
        unc__dropfunc(w, &newm);
    else {
        Unc_Entity *en = unc__wake(w, Unc_TFunction);
        if (!en) {
            unc__dropfunc(w, &newm);
            return UNCIL_ERR_MEM;
        }
        unc__wsetprogram(w, p);
        *LEFTOVER(Unc_Function, en) = newm;
        VSETENT(w, &w->fmain, Unc_TFunction, en);
    }
    return 0;
}

Unc_RetVal unc_getpublic(Unc_View *w, Unc_Size nl, const char *name,
                         Unc_Value *value) {
    Unc_Value *v;
    (void)UNC_LOCKFP(w, w->world->public_lock);
    v = unc__gethtbls(w, w->pubs, nl, (const byte *)name);
    if (v) {
        VCOPY(w, value, v);
        UNC_UNLOCKF(w->world->public_lock);
        return 0;
    } else {
        UNC_UNLOCKF(w->world->public_lock);
        return UNCIL_ERR_ARG_NOSUCHNAME;
    }
}

Unc_RetVal unc_setpublic(Unc_View *w, Unc_Size nl, const char *name,
                         Unc_Value *value) {
    Unc_Value *ptr;
    int e;
    (void)UNC_LOCKFP(w, w->world->public_lock);
    if (w->import) {
        e = unc__puthtbls(w, w->exports, nl, (const byte *)name, &ptr);
        if (e) {
            UNC_UNLOCKF(w->world->public_lock);
            return e;
        }
        VCOPY(w, ptr, value);
    }
    e = unc__puthtbls(w, w->pubs, nl, (const byte *)name, &ptr);
    if (e) {
        UNC_UNLOCKF(w->world->public_lock);
        return e;
    }
    VCOPY(w, ptr, value);
    UNC_UNLOCKF(w->world->public_lock);
    return 0;
}

Unc_RetVal unc_getpublicc(Unc_View *w, const char *name, Unc_Value *value) {
    return unc_getpublic(w, strlen(name), name, value);
}

Unc_RetVal unc_setpublicc(Unc_View *w, const char *name, Unc_Value *value) {
    return unc_setpublic(w, strlen(name), name, value);
}

Unc_ValueType unc_gettype(Unc_View *w, Unc_Value *v) {
    ASSERT(v->type != Unc_TRef);
    return v->type;
}

int unc_issame(Unc_View *w, Unc_Value *a, Unc_Value *b) {
    if (a->type != b->type) return 0;
    switch (a->type) {
    case Unc_TNull:
        return 1;
    case Unc_TBool:
    case Unc_TInt:
        return a->v.i == b->v.i;
    case Unc_TFloat:
        return a->v.f == b->v.f;
    case Unc_TOpaquePtr:
        return a->v.p == b->v.p;
    case Unc_TString:
    case Unc_TArray:
    case Unc_TTable:
    case Unc_TObject:
    case Unc_TBlob:
    case Unc_TFunction:
    case Unc_TOpaque:
    case Unc_TWeakRef:
    case Unc_TBoundFunction:
        return a->v.c == b->v.c;
    default:
        return 0;
    }
}

Unc_RetVal unc_getbool(Unc_View *w, Unc_Value *v, Unc_RetVal nul) {
    switch (VGETTYPE(v)) {
    case Unc_TNull:
        return nul;
    case Unc_TBool:
        return VGETBOOL(v);
    default:
        return UNCIL_ERR_TYPE_NOTBOOL;
    }
}

Unc_RetVal unc_converttobool(Unc_View *w, Unc_Value *v) {
    return unc__vcvt2bool(w, v);
}

Unc_RetVal unc_getint(Unc_View *w, Unc_Value *v, Unc_Int *ret) {
    return unc__vgetint(w, v, ret);
}

Unc_RetVal unc_getfloat(Unc_View *w, Unc_Value *v, Unc_Float *ret) {
    return unc__vgetfloat(w, v, ret);
}

Unc_RetVal unc_getstring(Unc_View *w, Unc_Value *v,
                         Unc_Size *n, const char **p) {
    Unc_String *s;
    if (v->type != Unc_TString)
        return UNCIL_ERR_TYPE_NOTSTR;
    s = LEFTOVER(Unc_String, VGETENT(v));
    *n = s->size;
    *p = (const char *)unc__getstringdata(s);
    return 0;
}

Unc_RetVal unc_getstringc(Unc_View *w, Unc_Value *v, const char **p) {
    Unc_String *s;
    const char *sp;
    if (v->type != Unc_TString)
        return UNCIL_ERR_TYPE_NOTSTR;
    s = LEFTOVER(Unc_String, VGETENT(v));
    sp = (const char *)unc__getstringdata(s);
    if (memchr(sp, 0, s->size))
        return UNCIL_ERR_ARG_NULLCHAR;
    *p = sp;
    return 0;
}

Unc_RetVal unc_resizeblob(Unc_View *w, Unc_Value *v, Unc_Size n, byte **p) {
    Unc_Blob *s;
    Unc_Size z;
    if (v->type != Unc_TBlob)
        return UNCIL_ERR_TYPE_NOTBLOB;
    s = LEFTOVER(Unc_Blob, VGETENT(v));
    z = s->size;
    if (n > z) {
        int e = unc__blobaddn(&w->world->alloc, s, n - z);
        if (e)
            return e;
    } else if (n < z) {
        int e = unc__blobdel(&w->world->alloc, s, n, z - n);
        if (e)
            return e;
    }
    *p = s->data;
    return 0;
}

Unc_RetVal unc_resizearray(Unc_View *w, Unc_Value *v,
                           Unc_Size n, Unc_Value **p) {
    Unc_Array *s;
    Unc_Size z;
    if (v->type != Unc_TArray)
        return UNCIL_ERR_TYPE_NOTARRAY;
    s = LEFTOVER(Unc_Array, VGETENT(v));
    z = s->size;
    if (n > z) {
        int e = unc__arraypushn(w, s, n - z);
        if (e)
            return e;
    } else if (n < z) {
        int e = unc__arraydel(w, s, n, z - n);
        if (e)
            return e;
    }
    *p = s->data;
    return 0;
}

Unc_RetVal unc_getopaqueptr(Unc_View *w, Unc_Value *v, void **p) {
    if (v->type != Unc_TOpaquePtr)
        return UNCIL_ERR_TYPE_NOTOPAQUEPTR;
    *p = v->v.p;
    return 0;
}

Unc_RetVal unc_getblobsize(Unc_View *w, Unc_Value *v, Unc_Size *ret) {
    if (v->type == Unc_TBlob) {
        UNC_LOCKL(LEFTOVER(Unc_Blob, VGETENT(v))->lock);
        *ret = LEFTOVER(Unc_Blob, VGETENT(v))->size;
        UNC_UNLOCKL(LEFTOVER(Unc_Blob, VGETENT(v))->lock);
        return 0;
    } else
        return UNCIL_ERR_TYPE_NOTBLOB;
}

Unc_RetVal unc_getarraysize(Unc_View *w, Unc_Value *v, Unc_Size *ret) {
    if (v->type == Unc_TArray) {
        UNC_LOCKL(LEFTOVER(Unc_Array, VGETENT(v))->lock);
        *ret = LEFTOVER(Unc_Array, VGETENT(v))->size;
        UNC_UNLOCKL(LEFTOVER(Unc_Array, VGETENT(v))->lock);
        return 0;
    } else
        return UNCIL_ERR_TYPE_NOTARRAY;
}

Unc_RetVal unc_lockblob(Unc_View *w, Unc_Value *v,
                        Unc_Size *n, byte **p) {
    if (v->type == Unc_TBlob) {
        Unc_Blob *s = LEFTOVER(Unc_Blob, VGETENT(v));
        if (!w->cfunc || !(w->cfunc->cflags & UNC_CFUNC_EXCLUSIVE))
            UNC_LOCKL(s->lock);
        *n = s->size;
        *p = s->data;
        return 0;
    } else
        return UNCIL_ERR_TYPE_NOTBLOB;
}

Unc_RetVal unc_lockarray(Unc_View *w, Unc_Value *v,
                         Unc_Size *n, Unc_Value **p) {
    if (v->type == Unc_TArray) {
        Unc_Array *s = LEFTOVER(Unc_Array, VGETENT(v));
        if (!w->cfunc || !(w->cfunc->cflags & UNC_CFUNC_EXCLUSIVE))
            UNC_LOCKL(s->lock);
        *n = s->size;
        *p = s->data;
        return 0;
    } else
        return UNCIL_ERR_TYPE_NOTARRAY;
}

Unc_RetVal unc_lockopaque(Unc_View *w, Unc_Value *v,
                          Unc_Size *n, void **p) {
    if (v->type == Unc_TOpaque) {
        Unc_Opaque *s = LEFTOVER(Unc_Opaque, VGETENT(v));
        if (!w->cfunc || !(w->cfunc->cflags & UNC_CFUNC_EXCLUSIVE))
            UNC_LOCKL(s->lock);
        if (n) *n = s->size;
        *p = s->data;
        return 0;
    } else
        return UNCIL_ERR_TYPE_NOTOPAQUE;
}

Unc_RetVal unc_trylockopaque(Unc_View *w, Unc_Value *v, Unc_Size *n, void **p) {
    if (v->type == Unc_TOpaque) {
        Unc_Opaque *s = LEFTOVER(Unc_Opaque, VGETENT(v));
        if (!w->cfunc || !(w->cfunc->cflags & UNC_CFUNC_EXCLUSIVE)) {
            int success = UNC_LOCKLQ(s->lock);
            if (!success) success = UNC_LOCKLQ(s->lock);
            if (!success) return UNCIL_ERR_LOGIC_CANNOTLOCK;
        }
        if (n) *n = s->size;
        *p = s->data;
        return 0;
    } else
        return UNCIL_ERR_TYPE_NOTOPAQUE;
}

void unc_unlock(Unc_View *w, Unc_Value *v) {
    switch (v->type) {
    case Unc_TBlob:
        UNC_UNLOCKL(LEFTOVER(Unc_Blob, VGETENT(v))->lock);
        break;
    case Unc_TArray:
        UNC_UNLOCKL(LEFTOVER(Unc_Array, VGETENT(v))->lock);
        break;
    case Unc_TOpaque:
        UNC_UNLOCKL(LEFTOVER(Unc_Opaque, VGETENT(v))->lock);
        break;
    default:
        break;
    }
}

Unc_RetVal unc_getindex(Unc_View *w, Unc_Value *v, Unc_Value *i,
                        Unc_Value *out) {
    Unc_Value o = UNC_BLANK;
    int e = unc__vgetindx(w, v, i, 0, &o);
    if (!e) VCOPY(w, out, &o);
    return e;
}

Unc_RetVal unc_setindex(Unc_View *w, Unc_Value *v, Unc_Value *i,
                        Unc_Value *in) {
    return unc__vsetindx(w, v, i, in);
}

Unc_RetVal unc_getattrv(Unc_View *w, Unc_Value *v, Unc_Value *a,
                        Unc_Value *out) {
    Unc_Value o = UNC_BLANK;
    int e = unc__vgetattrv(w, v, a, 0, &o);
    if (!e) VCOPY(w, out, &o);
    return e;
}

Unc_RetVal unc_setattrv(Unc_View *w, Unc_Value *v, Unc_Value *a,
                        Unc_Value *in) {
    return unc__vsetattrv(w, v, a, in);
}

Unc_RetVal unc_getattrs(Unc_View *w, Unc_Value *v, Unc_Size an, const char *as,
                        Unc_Value *out) {
    Unc_Value o = UNC_BLANK;
    int e = unc__vgetattr(w, v, an, (const byte *)as, 0, &o);
    if (!e) VCOPY(w, out, &o);
    return e;
}

Unc_RetVal unc_setattrs(Unc_View *w, Unc_Value *v, Unc_Size an, const char *as,
                        Unc_Value *in) {
    return unc__vsetattr(w, v, an, (const byte *)as, in);
}

Unc_RetVal unc_getattrc(Unc_View *w, Unc_Value *v, const char *as,
                        Unc_Value *out) {
    return unc_getattrs(w, v, strlen(as), as, out);
}

Unc_RetVal unc_setattrc(Unc_View *w, Unc_Value *v, const char *as,
                        Unc_Value *in) {
    return unc_setattrs(w, v, strlen(as), as, in);
}

Unc_Size unc_getopaquesize(Unc_View *w, Unc_Value *v) {
    Unc_Opaque *o;
    if (v->type != Unc_TOpaque) return 0;
    o = LEFTOVER(Unc_Opaque, VGETENT(v));
    return o->size;
}

void unc_getprototype(Unc_View *w, Unc_Value *v, Unc_Value *p) {
    switch (v->type) {
    case Unc_TObject:
        VCOPY(w, p, &LEFTOVER(Unc_Object, VGETENT(v))->prototype);
        return;
    case Unc_TOpaque:
        VCOPY(w, p, &LEFTOVER(Unc_Opaque, VGETENT(v))->prototype);
        return;
    default:
        unc_clear(w, p);
    }
}

Unc_Size unc_getopaqueboundcount(Unc_View *w, Unc_Value *v) {
    Unc_Opaque *o;
    if (v->type != Unc_TOpaque)
        return UNCIL_ERR_TYPE_NOTOPAQUE;
    o = LEFTOVER(Unc_Opaque, VGETENT(v));
    return o->refc;
}

Unc_Value *unc_opaqueboundvalue(Unc_View *w, Unc_Value *v, Unc_Size i) {
    Unc_Opaque *o;
    if (v->type != Unc_TOpaque)
        return NULL;
    o = LEFTOVER(Unc_Opaque, VGETENT(v));
    return LEFTOVER(Unc_Value, o->refs[i]);
}

const Unc_Value unc_blank = UNC_BLANK;

void unc_setnull(Unc_View *w, Unc_Value *v) {
    VSETNULL(w, v);
}

void unc_setbool(Unc_View *w, Unc_Value *v, int b) {
    VSETBOOL(w, v, b);
}

void unc_setint(Unc_View *w, Unc_Value *v, Unc_Int i) {
    VSETINT(w, v, i);
}

void unc_setfloat(Unc_View *w, Unc_Value *v, Unc_Float f) {
    VSETFLT(w, v, f);
}

Unc_RetVal unc_newstring(Unc_View *w, Unc_Value *v, Unc_Size n, const char *c) {
    int e;
    Unc_Value tmp;
    if (unc__utf8validate(n, c))
        return UNCIL_ERR_IO_INVALIDENCODING;
    e = unc__vrefnew(w, &tmp, Unc_TString);
    if (e) return e;
    e = unc__initstring(&w->world->alloc, LEFTOVER(Unc_String, VGETENT(&tmp)),
                            n, (const byte *)c);
    if (e)
        unc__unwake(VGETENT(&tmp), w);
    else
        VCOPY(w, v, &tmp);
    return e;
}

Unc_RetVal unc_newstringc(Unc_View *w, Unc_Value *v, const char *c) {
    return unc_newstring(w, v, strlen(c), c);
}

Unc_RetVal unc_newstringmove(Unc_View *w, Unc_Value *v, Unc_Size n, char *c) {
    int e;
    size_t an;
    Unc_Value tmp;
    e = unc__vrefnew(w, &tmp, Unc_TString);
    if (e) return e;
    an = unc__mmgetsize(&w->world->alloc, c);
    if (unc__utf8validate(n, c))
        return UNCIL_ERR_IO_INVALIDENCODING;
    c = unc__mmunwinds(&w->world->alloc, c, &an);
    if (n >= an)
        return UNCIL_ERR_ARG_INDEXOUTOFBOUNDS;
    if (n + 1 < an) {
        c = unc__mrealloc(&w->world->alloc, Unc_AllocString, c, an, n + 1);
        c[n] = 0;
        an = n + 1;
    }
    e = unc__initstringmove(&w->world->alloc,
                            LEFTOVER(Unc_String, VGETENT(&tmp)),
                            n, (byte *)c);
    if (e)
        unc__unwake(VGETENT(&tmp), w);
    else
        VCOPY(w, v, &tmp);
    return e;
}

Unc_RetVal unc_newstringcmove(Unc_View *w, Unc_Value *v, char *c) {
    size_t an, sn; 
    an = unc__mmgetsize(&w->world->alloc, c);
    sn = unc__strnlen(c, an);
    if (sn >= an)
        return UNCIL_ERR_ARG_INDEXOUTOFBOUNDS;
    return unc_newstringmove(w, v, sn, c);
}

Unc_RetVal unc_newblob(Unc_View *w, Unc_Value *v, Unc_Size n, byte **data) {
    int e;
    Unc_Value tmp;
    e = unc__vrefnew(w, &tmp, Unc_TBlob);
    if (e) return e;
    e = unc__initblobraw(&w->world->alloc,
                        LEFTOVER(Unc_Blob, VGETENT(&tmp)), n, data);
    if (e)
        unc__unwake(VGETENT(&tmp), w);
    else {
        VCOPY(w, v, &tmp);
        if (!w->cfunc || !(w->cfunc->cflags & UNC_CFUNC_EXCLUSIVE))
            UNC_LOCKL(LEFTOVER(Unc_Blob, VGETENT(&tmp))->lock);
    }
    return e;
}

Unc_RetVal unc_newblobfrom(Unc_View *w, Unc_Value *v, Unc_Size n, byte *data) {
    byte *p;
    int e = unc_newblob(w, v, n, &p);
    if (e) return e;
    unc__memcpy(p, data, n);
    UNC_UNLOCKL(LEFTOVER(Unc_Blob, VGETENT(v))->lock);
    return 0;
}

Unc_RetVal unc_newblobmove(Unc_View *w, Unc_Value *v, byte *data) {
    int e;
    size_t n;
    Unc_Value tmp;
    e = unc__vrefnew(w, &tmp, Unc_TBlob);
    if (e) return e;
    data = unc__mmunwind(&w->world->alloc, data, &n);
    e = unc__initblobmove(&w->world->alloc,
                        LEFTOVER(Unc_Blob, VGETENT(&tmp)), n, data);
    if (e)
        unc__unwake(VGETENT(&tmp), w);
    else
        VCOPY(w, v, &tmp);
    return e;
}

Unc_RetVal unc_newarrayempty(Unc_View *w, Unc_Value *v) {
    int e;
    Unc_Value tmp;
    e = unc__vrefnew(w, &tmp, Unc_TArray);
    if (e) return e;
    e = unc__initarray(w, LEFTOVER(Unc_Array, VGETENT(&tmp)), 0, NULL);
    if (e)
        unc__unwake(VGETENT(&tmp), w);
    else
        VCOPY(w, v, &tmp);
    return e;
}

Unc_RetVal unc_newarray(Unc_View *w, Unc_Value *v, Unc_Size n, Unc_Value **p) {
    int e;
    Unc_Value tmp;
    Unc_Array *s;
    e = unc__vrefnew(w, &tmp, Unc_TArray);
    if (e) return e;
    s = LEFTOVER(Unc_Array, VGETENT(&tmp));
    e = unc__initarrayn(w, LEFTOVER(Unc_Array, VGETENT(&tmp)), n);
    if (e)
        unc__unwake(VGETENT(&tmp), w);
    else {
        if (!w->cfunc || !(w->cfunc->cflags & UNC_CFUNC_EXCLUSIVE))
            UNC_LOCKL(s->lock);
        *p = s->data;
        VCOPY(w, v, &tmp);
    }
    return e;
}

Unc_RetVal unc_newarrayfrom(Unc_View *w, Unc_Value *v,
                            Unc_Size n, Unc_Value *a) {
    int e;
    Unc_Value tmp;
    e = unc__vrefnew(w, &tmp, Unc_TArray);
    if (e) return e;
    e = unc__initarray(w, LEFTOVER(Unc_Array, VGETENT(&tmp)), n, a);
    if (e)
        unc__unwake(VGETENT(&tmp), w);
    else
        VCOPY(w, v, &tmp);
    return e;
}

Unc_RetVal unc_newtable(Unc_View *w, Unc_Value *v) {
    int e;
    Unc_Value tmp;
    e = unc__vrefnew(w, &tmp, Unc_TTable);
    if (e) return e;
    e = unc__initdict(w, LEFTOVER(Unc_Dict, VGETENT(&tmp)));
    if (e)
        unc__unwake(VGETENT(&tmp), w);
    else
        VCOPY(w, v, &tmp);
    return e;
}

Unc_RetVal unc_newobject(Unc_View *w, Unc_Value *v, Unc_Value *prototype) {
    int e;
    Unc_Value tmp;
    e = unc__vrefnew(w, &tmp, Unc_TObject);
    if (e) return e;
    e = unc__initobj(w, LEFTOVER(Unc_Object, VGETENT(&tmp)), prototype);
    if (e)
        unc__unwake(VGETENT(&tmp), w);
    else
        VCOPY(w, v, &tmp);
    return e;
}

Unc_RetVal unc_newopaque(Unc_View *w, Unc_Value *v, Unc_Value *prototype,
                    Unc_Size n, void **data,
                    Unc_OpaqueDestructor destructor,
                    Unc_Size refcount, Unc_Value *initvalues,
                    Unc_Size refcopycount, Unc_Size *refcopies) {
    int e;
    Unc_Value tmp;
    e = unc__vrefnew(w, &tmp, Unc_TOpaque);
    if (e) return e;
    e = unc__initopaque(w, LEFTOVER(Unc_Opaque, VGETENT(&tmp)), 
                        n, data, prototype,
                        destructor, refcount, initvalues,
                        refcopycount, refcopies);
    if (e)
        unc__unwake(VGETENT(&tmp), w);
    else {
        VCOPY(w, v, &tmp);
        if (!w->cfunc || !(w->cfunc->cflags & UNC_CFUNC_EXCLUSIVE))
            UNC_LOCKL(LEFTOVER(Unc_Opaque, VGETENT(&tmp))->lock);
    }
    return e;
}

Unc_RetVal unc_newcfunction(Unc_View *w, Unc_Value *v, Unc_CFunc func,
                    int cflags, Unc_Size argcount, int ellipsis,
                    Unc_Size optcount, Unc_Value *defaults,
                    Unc_Size refcount, Unc_Value *initvalues,
                    Unc_Size refcopycount, Unc_Size *refcopies,
                    const char *fname, void *udata) {
    int e;
    Unc_Value tmp;
    e = unc__vrefnew(w, &tmp, Unc_TFunction);
    if (e) return e;
    e = unc__initfuncc(w, LEFTOVER(Unc_Function, VGETENT(&tmp)), func, argcount,
                        ellipsis ? UNC_FUNCTION_FLAG_ELLIPSIS : 0, cflags,
                        optcount, defaults, refcount, initvalues,
                        refcopycount, refcopies, fname, udata);
    if (e)
        unc__unwake(VGETENT(&tmp), w);
    else
        VCOPY(w, v, &tmp);
    return e;
}

void unc_setopaqueptr(Unc_View *w, Unc_Value *v, void *data) {
    VSETPTR(w, v, data);
}

Unc_RetVal unc_freezeobject(Unc_View *w, Unc_Value *v) {
    if (VGETTYPE(v) != Unc_TObject)
        return UNCIL_ERR_TYPE_NOTOBJECT;
    
    return 0;
}

int unc_yield(Unc_View *w) {
    if (w->cfunc) {
        if (w->cfunc->cflags & UNC_CFUNC_EXCLUSIVE)
            return 0;
    }
    return unc__vmcheckpause(w);
}

int unc_yieldfull(Unc_View *w) {
    int e = 0;
    if (w->cfunc) {
        if (!(w->cfunc->cflags & UNC_CFUNC_CONCURRENT))
            UNC_UNLOCKF(w->cfunc->lock);
        e = unc__vmcheckpause(w);
        if (!(w->cfunc->cflags & UNC_CFUNC_CONCURRENT))
            UNC_LOCKF(w->cfunc->lock);
    }
    return e;
}

void unc_vmpause(Unc_View *w) {
    /* do not call unless you know what you are doing! */
#if UNCIL_MT_OK
    ATOMICSSET(w->paused, 1);
#endif
}

int unc_vmresume(Unc_View *w) {
    /* do not call unless you know what you are doing! */
#if UNCIL_MT_OK
    ATOMICSSET(w->paused, 0);
    return unc__vmcheckpause(w);
#else
    return 0;
#endif
}

Unc_RetVal unc_exportcfunction(Unc_View *w, const char *name, Unc_CFunc func,
                    int cflags, Unc_Size argcount, int ellipsis,
                    Unc_Size optcount, Unc_Value *defaults,
                    Unc_Size refcount, Unc_Value *initvalues,
                    Unc_Size refcopycount, Unc_Size *refcopies, void *udata) {
    int e;
    Unc_Value v = UNC_BLANK;
    e = unc_newcfunction(w, &v, func, cflags, argcount, ellipsis,
            optcount, defaults, refcount, initvalues, refcopycount, refcopies,
            name, udata);
    if (e) return e;
    e = unc_setpublicc(w, name, &v);
    unc_decref(w, &v);
    return e;
}

void unc_incref(Unc_View *w, Unc_Value *v) {
    if (v) VINCREF(w, v);
}

void unc_decref(Unc_View *w, Unc_Value *v) {
    if (v) VDECREF(w, v);
}

void unc_clear(Unc_View *w, Unc_Value *v) {
    if (v) VSETNULL(w, v);
}

void unc_clearmany(Unc_View *w, Unc_Size n, Unc_Value *v) {
    Unc_Size i;
    for (i = 0; i < n; ++i)
        unc_clear(w, &v[i]);
}

Unc_RetVal unc_reserve(Unc_View *w, Unc_Size n) {
    return unc__stackreserve(w, &w->sval, n);
}

Unc_RetVal unc_push(Unc_View *w, Unc_Size n, Unc_Value *v, Unc_Size *counter) {
    int e = n ? unc__stackpush(w, &w->sval, n, v) : 0;
    if (e) return e;
    if (counter) *counter += n;
    return 0;
}

Unc_RetVal unc_pushmove(Unc_View *w, Unc_Value *v, Unc_Size *counter) {
    int e = unc__stackpushn(w, &w->sval, 1);
    if (e)
        return e;
    w->sval.top[-1] = *v;
    VINITNULL(v);
    if (counter) ++*counter;
    return 0;
}

void unc_pop(Unc_View *w, Unc_Size n, Unc_Size *counter) {
    if (counter) {
        if (n > *counter)
            n = *counter;
        *counter -= n;
    }
    if (n)
        unc__stackpop(w, &w->sval, n);
}

Unc_RetVal unc_shove(Unc_View *w, Unc_Size d, Unc_Size n,
                     Unc_Value *v, Unc_Size *counter) {
    Unc_Size m = unc__stackdepth(&w->sval);
    int e;
    m = m < d ? 0 : m - d;
    e = n ? unc__stackinsertm(w, &w->sval, m, n, v) : 0;
    if (e) return e;
    if (counter) *counter += n;
    return 0;
}

void unc_yank(Unc_View *w, Unc_Size d, Unc_Size n, Unc_Size *counter) {
    if (counter) {
        if (n > *counter)
            n = *counter;
        *counter -= n;
    }
    if (n) {
        Unc_Size m = unc__stackdepth(&w->sval);
        if (n > d) n = d;
        unc__stackpullrug(w, &w->sval, m - d, m - n);
    }
}

Unc_RetVal unc_throw(Unc_View *w, Unc_Value *vr) {
    CHECKHALT(w);
    VCOPY(w, &w->exc, vr);
    return UNCIL_ERR_UNCIL;
}

Unc_RetVal unc_throwex(Unc_View *w, Unc_Value *type, Unc_Value *message) {
    CHECKHALT(w);
    unc__makeexceptionvoroom(w, &w->exc, type, message);
    return UNCIL_ERR_UNCIL;
}

Unc_RetVal unc_throwext(Unc_View *w, const char *type, Unc_Value *message) {
    CHECKHALT(w);
    unc__makeexceptiontoroom(w, &w->exc, type, message);
    return UNCIL_ERR_UNCIL;
}

Unc_RetVal unc_throwexc(Unc_View *w, const char *type, const char *message) {
    CHECKHALT(w);
    unc__makeexceptionoroom(w, &w->exc, type, message);
    return UNCIL_ERR_UNCIL;
}

Unc_Size unc_boundcount(Unc_View *w) {
    if (!w->cfunc) return 0;
    return w->boundcount;
}

Unc_Value *unc_boundvalue(Unc_View *w, Unc_Size index) {
    if (!w->cfunc) return NULL;
    return LEFTOVER(Unc_Value, w->bounds[index]);
}

Unc_Size unc_recurselimit(Unc_View *w) {
    return w->recurselimit - w->recurse;
}

int unc_iscallable(Unc_View *w, Unc_Value *v) {
    switch (VGETTYPE(v)) {
    case Unc_TFunction:
    case Unc_TBoundFunction:
        return 1;
    case Unc_TObject:
    case Unc_TOpaque:
    {
        Unc_Value *o;
        int e;
unc_iscallable_again:
        e = unc__getprotomethod(w, v, PASSSTRL(OPOVERLOAD(call)), &o);
        if (e) return e;
        if (o) {
            switch (o->type) {
            case Unc_TFunction:
            case Unc_TBoundFunction:
                return 1;
            case Unc_TObject:
            case Unc_TOpaque:
                v = o;
                goto unc_iscallable_again;
            default:
                ;
            }
        }
    }
    default:
        return 0;   
    }
}

void unc_lockthisfunc(Unc_View *w) {
    if (w->cfunc) UNC_LOCKF(w->cfunc->lock);
}

void unc_unlockthisfunc(Unc_View *w) {
    if (w->cfunc) UNC_UNLOCKF(w->cfunc->lock);
}

Unc_RetVal unc_newpile(Unc_View *w, Unc_Pile *pile) {
    pile->r = w->region.top - w->region.base;
    return unc__vmrpush(w);
}

Unc_RetVal unc_callex(Unc_View *w, Unc_Value *func, Unc_Size argn,
                      Unc_Pile *ret) {
    int e;
    if (!w->program)
        return UNCIL_ERR_ARG_NOPROGRAMLOADED;
    if (ret->r + 1 != w->region.top - w->region.base)
        return UNCIL_ERR_ARG_NOTMOSTRECENT;
    e = func ? unc__fcallv(w, func, argn, 1, 1, 1, 0)
             : unc__fcall(w, NULL, argn, 1, 1, 1, 0);
    if (e)
        return UNCIL_IS_ERR(e) ? e : 0;
    return unc__run(w);
}

Unc_RetVal unc_call(Unc_View *w, Unc_Value *func,
                    Unc_Size argn, Unc_Pile *ret) {
    Unc_RetVal e;
    if ((e = unc_newpile(w, ret)))
        return e;
    w->region.base[ret->r] -= argn;
    e = unc_callex(w, func, argn, ret);
    if (e) unc_discard(w, ret);
    return e;
}

void unc_getexception(Unc_View *w, Unc_Value *out) {
    VCOPY(w, out, &w->exc);
}

void unc_getexceptionfromcode(Unc_View *w, Unc_Value *out, int e) {
    if (e != UNCIL_ERR_UNCIL)
        unc__errtoexcept(w, e, out);
    else
        VCOPY(w, out, &w->exc);
}

struct valuetostring_buffer {
    byte *c;
    Unc_Size q;
    Unc_Size n;
};

static int valuetostring_wrapper(Unc_Size n, const byte *s, void *udata) {
    struct valuetostring_buffer *buffer = udata;
    Unc_Size p = buffer->n, r = buffer->q - p;
    if (r) {
        if (n > r)
            n = r;
        unc__memcpy(buffer->c + p, s, n);
        buffer->n = p + n;
    }
    return 0;
}

Unc_RetVal unc_valuetostring(Unc_View *w, Unc_Value *v, Unc_Size *n, char *c) {
    struct valuetostring_buffer buffer;
    int e;
    buffer.c = (byte *)c;
    buffer.n = 0;
    if (!*n) return 0;
    buffer.q = *n - 1;
    e = unc__vcvt2str(w, v, &valuetostring_wrapper, &buffer);
    buffer.c[buffer.n] = 0;
    *n = buffer.n + 1;
    return e;
}

struct valuetostringn_buffer {
    Unc_Allocator *alloc;
    byte *c;
    Unc_Size q;
    Unc_Size n;
};

static int valuetostringn_wrapper(Unc_Size n, const byte *s, void *udata) {
    struct valuetostringn_buffer *buffer = udata;
    return unc__strpush(buffer->alloc, &buffer->c, &buffer->n, &buffer->q,
                                            6, n, s);
}

Unc_RetVal unc_valuetostringn(Unc_View *w, Unc_Value *v, Unc_Size *n, char **c) {
    struct valuetostringn_buffer buffer;
    int e;
    buffer.alloc = &w->world->alloc;
    buffer.c = NULL;
    buffer.q = 0;
    buffer.n = 0;
    e = unc__vcvt2str(w, v, &valuetostringn_wrapper, &buffer);
    if (!e)
        e = unc__strpush1(buffer.alloc, &buffer.c, &buffer.n, &buffer.q, 6, 0);
    *c = unc__mmrealloc(&w->world->alloc, Unc_AllocString, buffer.c, buffer.n);
    *n = buffer.n - 1;
    return e;
}

Unc_RetVal unc__exceptiontostring(Unc_View *w, Unc_Value *exc,
                        int (*out)(char outp, void *udata), void *udata) {
    int e, nomsg = 0;
    Unc_Value etype = UNC_BLANK, emsg = UNC_BLANK;
    Unc_Size n1, n2;
    const char *p1, *p2;
    e = unc_getattrs(w, exc, PASSSTRLC("type"), &etype);
    if (!e) {
        e = unc_getattrs(w, exc, PASSSTRLC("message"), &emsg);
        if (e) nomsg = 1, e = 0;
    }
    if (!e) e = unc_getstring(w, &etype, &n1, &p1);
    if (!e && !nomsg) {
        e = unc_getstring(w, &emsg, &n2, &p2);
        if (e && !unc_gettype(w, &emsg))
            nomsg = 1, e = 0;
    }
    VDECREF(w, &etype);
    if (!nomsg) VDECREF(w, &emsg);
    if (e) return e;
    if (!nomsg) {
        if (n1 > INT_MAX / 4) n1 = INT_MAX / 4;
        if (n2 > INT_MAX / 4) n2 = INT_MAX / 4;
        unc__xprintf(out, udata, "%.*S error: %.*S", n1, p1, n2, p2);
    } else {
        unc__xprintf(out, udata, "%.*S error", n1, p1);
    }
    {
        Unc_Value estack = UNC_BLANK;
        e = unc_getattrs(w, exc, PASSSTRLC("stack"), &estack);
        if (!e && unc_gettype(w, &estack) == Unc_TArray) {
            Unc_Size sfn;
            Unc_Value *sfv;
            e = unc_lockarray(w, &estack, &sfn, &sfv);
            if (!e) {
                Unc_Size i, sfsn;
                const char *sfss;
                for (i = 0; i < sfn; ++i) {
                    e = unc_getstring(w, &sfv[i], &sfsn, &sfss);
                    if (!e)
                        unc__xprintf(out, udata, "\n    in %.*S", sfsn, sfss);
                }
            }
            unc_unlock(w, &estack);
        }
        unc_clear(w, &estack);
    }
    return 0;
}

struct exceptiontostring_buffer {
    byte *c;
    Unc_Size q;
    Unc_Size n;
};

static int exceptiontostring_wrapper(char outp, void *udata) {
    struct exceptiontostring_buffer *buf = udata;
    if (!buf->q) return 0;
    --buf->q;
    ++buf->n;
    *buf->c++ = (byte)outp;
    return 0;
}

Unc_RetVal unc_exceptiontostring(Unc_View *w, Unc_Value *exc, Unc_Size *n,
                                 char *c) {
    int e;
    Unc_Size nr = *n;
    struct exceptiontostring_buffer buf;
    if (exc->type != Unc_TObject)
        return unc_valuetostring(w, exc, n, c);
    if (!nr) return 0;
    if (nr > INT_MAX) nr = INT_MAX;
    buf.c = (byte *)c;
    buf.q = *n - 1;
    buf.n = 0;
    e = unc__exceptiontostring(w, exc, &exceptiontostring_wrapper, &buf);
    if (!e) {
        c[buf.n] = 0;
        *n = buf.n;
    }
    return e;
}

struct exceptiontostringn_buffer {
    Unc_Allocator *alloc;
    byte *c;
    Unc_Size q;
    Unc_Size n;
};

static int exceptiontostringn_wrapper(char outp, void *udata) {
    struct exceptiontostringn_buffer *buf = udata;
    return unc__strpush1(buf->alloc, &buf->c, &buf->n, &buf->q, 6, outp);
}

Unc_RetVal unc_exceptiontostringn(Unc_View *w, Unc_Value *exc, Unc_Size *n,
                                  char **c) {
    int e;
    struct exceptiontostringn_buffer buf;
    if (exc->type != Unc_TObject)
        return unc_valuetostringn(w, exc, n, c);
    buf.alloc = &w->world->alloc;
    buf.c = NULL;
    buf.q = 0;
    buf.n = 0;
    e = unc__exceptiontostring(w, exc, &exceptiontostringn_wrapper, &buf);
    if (!e)
        e = unc__strpush1(buf.alloc, &buf.c, &buf.n, &buf.q, 6, 0);
    if (!e) {
        *n = buf.n - 1;
        *c = unc__mmrealloc(&w->world->alloc, Unc_AllocString, buf.c, buf.n);
    } else {
        unc__mmfree(&w->world->alloc, buf.c);
    }
    return e;
}

void unc_returnvalues(Unc_View *w, Unc_Pile *pile, Unc_Tuple *tuple) {
    Unc_Size r = pile->r;
    Unc_Size otop = (r + 1 == w->region.top - w->region.base
                    ? unc__stackdepth(&w->sval)
                    : w->region.base[r + 1]);
    tuple->values = &w->sval.base[w->region.base[r]];
    ASSERT(otop >= w->region.base[r]);
    tuple->count = otop - w->region.base[r];
}

Unc_RetVal unc_discard(Unc_View *w, Unc_Pile *pile) {
    Unc_Size r = pile->r;
    if (r + 1 != w->region.top - w->region.base)
        return UNCIL_ERR_ARG_NOTMOSTRECENT;
    unc__restoredepth(w, &w->sval, *--w->region.top);
    return 0;
}

Unc_RetVal unc_getiterator(Unc_View *w, Unc_Value *v, Unc_Value *res) {
    return unc__vgetiter(w, res, v);
}

const Unc_Program *unc_getprogram(Unc_View *w) {
    return w->program;
}

void unc_halt(Unc_View *w) {
    unc__haltview(w);
}

void unc_unload(Unc_View *w) {
    if (w->program) w->program = unc__progdecref(w->program, &w->world->alloc);
    VSETNULL(w, &w->fmain);
}

void unc_destroy(Unc_View *w) {
    if (w->world->finalize == 1) {
        /* will get destroyed anyway */
        ASSERT(w->vtype != Unc_ViewTypeNormal);
        w->vtype = Unc_ViewTypeFinalized;
        return;
    }
    unc__freeview(w);
}

void *unc_malloc(Unc_View *w, size_t n) {
    return unc__mmallocz(&w->world->alloc, Unc_AllocExternal, n);
}

void *unc_mrealloc(Unc_View *w, void *p, size_t n) {
    return unc__mmreallocz(&w->world->alloc, Unc_AllocExternal, p, n);
}

void unc_mfree(Unc_View *w, void *p) {
    unc__mmfree(&w->world->alloc, p);
}
