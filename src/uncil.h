/*******************************************************************************
 
Uncil -- main external API

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

#ifndef UNCIL_H
#define UNCIL_H

#include <stdio.h>

#include "ualloc.h"
#include "ucommon.h"
#include "ucxt.h"
#include "udef.h"
#include "uerr.h"
#include "ufunc.h"
#include "uhash.h"
#include "umem.h"
#include "uopaque.h"
#include "uprog.h"
#include "ustack.h"

#ifdef __cplusplus
extern "C" {
#endif 

typedef struct Unc_Tuple {
    Unc_Size count;
    Unc_Value *values;
} Unc_Tuple;

extern const Unc_Value unc_blank;
#define UNC_BLANK { 0 }
#define UNC_BLANKS { UNC_BLANK }

#define UNC_VER_FLAG_MULTITHREADING 1

#define UNC_MMASK_NONE          ((Unc_MMask)0)
#define UNC_MMASK_ALL           (~(Unc_MMask)0)
#define UNC_MMASK_M_OS          ((Unc_MMask)(1UL <<  0))
#define UNC_MMASK_M_SYS         ((Unc_MMask)(1UL <<  1))
#define UNC_MMASK_M_IO          ((Unc_MMask)(1UL <<  2))
#define UNC_MMASK_M_MATH        ((Unc_MMask)(1UL <<  3))
#define UNC_MMASK_M_CONVERT     ((Unc_MMask)(1UL <<  4))
#define UNC_MMASK_M_GC          ((Unc_MMask)(1UL <<  5))
#define UNC_MMASK_M_JSON        ((Unc_MMask)(1UL <<  6))
#define UNC_MMASK_M_CBOR        ((Unc_MMask)(1UL <<  7))
#define UNC_MMASK_M_FS          ((Unc_MMask)(1UL <<  8))
#define UNC_MMASK_M_RANDOM      ((Unc_MMask)(1UL <<  9))
#define UNC_MMASK_M_REGEX       ((Unc_MMask)(1UL << 10))
#define UNC_MMASK_M_TIME        ((Unc_MMask)(1UL << 11))
#define UNC_MMASK_M_COROUTINE   ((Unc_MMask)(1UL << 12))
#define UNC_MMASK_M_PROCESS     ((Unc_MMask)(1UL << 13))
#define UNC_MMASK_M_THREAD      ((Unc_MMask)(1UL << 14))
#define UNC_MMASK_M_UNICODE     ((Unc_MMask)(1UL << 15))

#if 1
#define UNC_MMASK_DEFAULT       (UNC_MMASK_M_MATH | UNC_MMASK_M_CONVERT        \
                                | UNC_MMASK_M_JSON | UNC_MMASK_M_CBOR          \
                                | UNC_MMASK_M_RANDOM | UNC_MMASK_M_REGEX       \
                                | UNC_MMASK_M_UNICODE | UNC_MMASK_M_TIME       \
                                | UNC_MMASK_M_COROUTINE)
#else
#define UNC_MMASK_DEFAULT       ((Unc_MMask)(0x00001FF8UL))
#endif

int unc_getversion_major(void);
int unc_getversion_minor(void);
int unc_getversion_patch(void);
int unc_getversion_flags(void);

Unc_View *unc_create(void);
Unc_View *unc_createex(Unc_Alloc alloc, void *udata, Unc_MMask mmask);
Unc_View *unc_dup(Unc_View *w);
Unc_View *unc_fork(Unc_View *w, int daemon);
int unc_coinhabited(Unc_View *w1, Unc_View *w2);
void unc_copyprogram(Unc_View *w1, Unc_View *w2);
Unc_RetVal unc_compilestring(Unc_View *w, Unc_Size n, const char *text);
Unc_RetVal unc_compilestringc(Unc_View *w, const char *text);
Unc_RetVal unc_compilefile(Unc_View *w, FILE *file);
Unc_RetVal unc_compilestream(Unc_View *w, int (*getch)(void *), void *udata);
Unc_Size unc_getcompileerrorlinenumber(Unc_View *w);
Unc_RetVal unc_loadfile(Unc_View *w, FILE *file);
Unc_RetVal unc_loadstream(Unc_View *w, int (*getch)(void *), void *udata);
Unc_RetVal unc_loadfileauto(Unc_View *w, const char *fn);

void unc_copy(Unc_View *w, Unc_Value *dst, Unc_Value *src);
void unc_move(Unc_View *w, Unc_Value *dst, Unc_Value *src);
void unc_swap(Unc_View *w, Unc_Value *va, Unc_Value *vb);
/* DANGEROUS! */ void unc_incref(Unc_View *w, Unc_Value *v);
/* DANGEROUS! */ void unc_decref(Unc_View *w, Unc_Value *v);
void unc_clear(Unc_View *w, Unc_Value *v);
void unc_clearmany(Unc_View *w, Unc_Size n, Unc_Value *v);
Unc_ValueType unc_gettype(Unc_View *w, Unc_Value *v);
Unc_RetVal unc_getpublic(Unc_View *w, Unc_Size nl, const char *name,
                            Unc_Value *value);
Unc_RetVal unc_setpublic(Unc_View *w, Unc_Size nl, const char *name,
                            Unc_Value *value);
Unc_RetVal unc_getpublicc(Unc_View *w, const char *name, Unc_Value *value);
Unc_RetVal unc_setpublicc(Unc_View *w, const char *name, Unc_Value *value);

int unc_issame(Unc_View *w, Unc_Value *a, Unc_Value *b);

Unc_RetVal unc_getbool(Unc_View *w, Unc_Value *v, Unc_RetVal nul);
Unc_RetVal unc_converttobool(Unc_View *w, Unc_Value *v);
Unc_RetVal unc_getint(Unc_View *w, Unc_Value *v, Unc_Int *ret);
Unc_RetVal unc_getfloat(Unc_View *w, Unc_Value *v, Unc_Float *ret);
Unc_RetVal unc_getstring(Unc_View *w, Unc_Value *v, Unc_Size *n,
                                                    const char **p);
Unc_RetVal unc_getstringc(Unc_View *w, Unc_Value *v, const char **p);
Unc_RetVal unc_resizeblob(Unc_View *w, Unc_Value *v,
                                Unc_Size n, Unc_Byte **p);
Unc_RetVal unc_resizearray(Unc_View *w, Unc_Value *v,
                           Unc_Size n, Unc_Value **p);
Unc_RetVal unc_getopaqueptr(Unc_View *w, Unc_Value *v, void **p);
Unc_RetVal unc_getblobsize(Unc_View *w, Unc_Value *v, Unc_Size *ret);
Unc_RetVal unc_getarraysize(Unc_View *w, Unc_Value *v, Unc_Size *ret);

Unc_RetVal unc_getindex(Unc_View *w, Unc_Value *v, Unc_Value *i,
                        Unc_Value *out);
Unc_RetVal unc_setindex(Unc_View *w, Unc_Value *v, Unc_Value *i,
                        Unc_Value *in);
Unc_RetVal unc_getattrv(Unc_View *w, Unc_Value *v, Unc_Value *a,
                        Unc_Value *out);
Unc_RetVal unc_setattrv(Unc_View *w, Unc_Value *v, Unc_Value *a,
                        Unc_Value *in);
Unc_RetVal unc_getattrs(Unc_View *w, Unc_Value *v, Unc_Size an, const char *as,
                        Unc_Value *out);
Unc_RetVal unc_setattrs(Unc_View *w, Unc_Value *v, Unc_Size an, const char *as,
                        Unc_Value *in);
Unc_RetVal unc_getattrc(Unc_View *w, Unc_Value *v, const char *as,
                        Unc_Value *out);
Unc_RetVal unc_setattrc(Unc_View *w, Unc_Value *v, const char *as,
                        Unc_Value *in);

Unc_RetVal unc_lockblob(Unc_View *w, Unc_Value *v, Unc_Size *n, Unc_Byte **p);
Unc_RetVal unc_lockarray(Unc_View *w, Unc_Value *v, Unc_Size *n, Unc_Value **p);
Unc_RetVal unc_lockopaque(Unc_View *w, Unc_Value *v, Unc_Size *n, void **p);
Unc_RetVal unc_trylockopaque(Unc_View *w, Unc_Value *v, Unc_Size *n, void **p);
void unc_unlock(Unc_View *w, Unc_Value *v);

void unc_lockthisfunc(Unc_View *w);
void unc_unlockthisfunc(Unc_View *w);

Unc_Size unc_getopaquesize(Unc_View *w, Unc_Value *v);
void unc_getprototype(Unc_View *w, Unc_Value *v, Unc_Value *p);
Unc_Size unc_getopaqueboundcount(Unc_View *w, Unc_Value *v);
Unc_Value *unc_opaqueboundvalue(Unc_View *w, Unc_Value *v, Unc_Size i);

void unc_setnull(Unc_View *w, Unc_Value *v);
void unc_setbool(Unc_View *w, Unc_Value *v, int b);
void unc_setint(Unc_View *w, Unc_Value *v, Unc_Int i);
void unc_setfloat(Unc_View *w, Unc_Value *v, Unc_Float f);
Unc_RetVal unc_newstring(Unc_View *w, Unc_Value *v, Unc_Size n, const char *c);
Unc_RetVal unc_newstringc(Unc_View *w, Unc_Value *v, const char *c);
Unc_RetVal unc_newstringmove(Unc_View *w, Unc_Value *v, Unc_Size n, char *c);
Unc_RetVal unc_newstringcmove(Unc_View *w, Unc_Value *v, char *c);
Unc_RetVal unc_newarrayempty(Unc_View *w, Unc_Value *v);
Unc_RetVal unc_newarray(Unc_View *w, Unc_Value *v, Unc_Size n, Unc_Value **p);
Unc_RetVal unc_newarrayfrom(Unc_View *w, Unc_Value *v,
                            Unc_Size n, Unc_Value *a);
Unc_RetVal unc_newtable(Unc_View *w, Unc_Value *v);
Unc_RetVal unc_newobject(Unc_View *w, Unc_Value *v, Unc_Value *prototype);
Unc_RetVal unc_newblob(Unc_View *w, Unc_Value *v, Unc_Size n, Unc_Byte **data);
Unc_RetVal unc_newblobfrom(Unc_View *w, Unc_Value *v,
                           Unc_Size n, Unc_Byte *data);
Unc_RetVal unc_newblobmove(Unc_View *w, Unc_Value *v, Unc_Byte *data);
Unc_RetVal unc_newopaque(Unc_View *w, Unc_Value *v, Unc_Value *prototype,
                    Unc_Size n, void **data,
                    Unc_OpaqueDestructor destructor,
                    Unc_Size refcount, Unc_Value *initvalues,
                    Unc_Size refcopycount, Unc_Size *refcopies);
Unc_RetVal unc_newcfunction(Unc_View *w, Unc_Value *v, Unc_CFunc func,
                    int cflags, Unc_Size argcount, int ellipsis,
                    Unc_Size optcount, Unc_Value *defaults,
                    Unc_Size refcount, Unc_Value *initvalues,
                    Unc_Size refcopycount, Unc_Size *refcopies,
                    const char *fname, void *udata);
Unc_RetVal unc_exportcfunction(Unc_View *w, const char *name, Unc_CFunc func,
                    int cflags, Unc_Size argcount, int ellipsis,
                    Unc_Size optcount, Unc_Value *defaults,
                    Unc_Size refcount, Unc_Value *initvalues,
                    Unc_Size refcopycount, Unc_Size *refcopies, void *udata);
void unc_setopaqueptr(Unc_View *w, Unc_Value *v, void *data);
Unc_RetVal unc_freezeobject(Unc_View *w, Unc_Value *v);

int unc_yield(Unc_View *w);
int unc_yieldfull(Unc_View *w);
/* DANGEROUS! */ void unc_vmpause(Unc_View *w);
/* DANGEROUS! */ int unc_vmresume(Unc_View *w);

Unc_RetVal unc_reserve(Unc_View *w, Unc_Size n);
Unc_RetVal unc_push(Unc_View *w, Unc_Size n, Unc_Value *v, Unc_Size *counter);
Unc_RetVal unc_pushmove(Unc_View *w, Unc_Value *v, Unc_Size *counter);
void unc_pop(Unc_View *w, Unc_Size n, Unc_Size *counter);
Unc_RetVal unc_shove(Unc_View *w, Unc_Size d, Unc_Size n,
                     Unc_Value *v, Unc_Size *counter);
void unc_yank(Unc_View *w, Unc_Size d, Unc_Size n, Unc_Size *counter);
/* a C function may not call any Uncil API functions after calling unc_throw*
   you should probably just return unc_throw(...); */
Unc_RetVal unc_throw(Unc_View *w, Unc_Value *v);
Unc_RetVal unc_throwex(Unc_View *w, Unc_Value *type, Unc_Value *message);
Unc_RetVal unc_throwext(Unc_View *w, const char *type, Unc_Value *message);
Unc_RetVal unc_throwexc(Unc_View *w, const char *type, const char *message);

void *unc_malloc(Unc_View *w, size_t n);
void *unc_mrealloc(Unc_View *w, void *p, size_t n);
void unc_mfree(Unc_View *w, void *p);

Unc_Size unc_boundcount(Unc_View *w);
Unc_Value *unc_boundvalue(Unc_View *w, Unc_Size index);
Unc_Size unc_recurselimit(Unc_View *w);
int unc_iscallable(Unc_View *w, Unc_Value *v);
Unc_RetVal unc_newpile(Unc_View *w, Unc_Pile *pile);
Unc_RetVal unc_call(Unc_View *w, Unc_Value *func, Unc_Size argn,
                                                  Unc_Pile *ret);
Unc_RetVal unc_callex(Unc_View *w, Unc_Value *func, Unc_Size argn,
                                                    Unc_Pile *ret);
void unc_getexception(Unc_View *w, Unc_Value *out);
void unc_getexceptionfromcode(Unc_View *w, Unc_Value *out, int e);
Unc_RetVal unc_valuetostring(Unc_View *w, Unc_Value *v,
                             Unc_Size *n, char *c);
Unc_RetVal unc_valuetostringn(Unc_View *w, Unc_Value *v,
                              Unc_Size *n, char **c);
Unc_RetVal unc_exceptiontostring(Unc_View *w, Unc_Value *exc,
                                 Unc_Size *n, char *c);
Unc_RetVal unc_exceptiontostringn(Unc_View *w, Unc_Value *exc,
                                  Unc_Size *n, char **c);
void unc_returnvalues(Unc_View *w, Unc_Pile *pile, Unc_Tuple *tuple);
Unc_RetVal unc_getiterator(Unc_View *w, Unc_Value *v, Unc_Value *res);
Unc_RetVal unc_dumpfile(Unc_View *w, FILE *file);
Unc_RetVal unc_dumpstream(Unc_View *w, int (*putch)(int, void *), void *udata);
Unc_RetVal unc_discard(Unc_View *w, Unc_Pile *pile);
void unc_halt(Unc_View *w);
void unc_unload(Unc_View *w);
void unc_destroy(Unc_View *w);

#ifdef __cplusplus
}
#endif

#endif /* UNCIL_H */
