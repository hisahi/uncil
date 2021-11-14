/*******************************************************************************
 
Uncil -- function impl header

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

#ifndef UNCIL_UFUNC_H
#define UNCIL_UFUNC_H

#include "udef.h"
#include "umem.h"
#include "umt.h"
#include "uval.h"

struct Unc_View;
struct Unc_Tuple;
struct Unc_Program;

typedef Unc_RetVal (*Unc_CFunc)(struct Unc_View *w,
                                struct Unc_Tuple args,
                                void *udata);

#define UNC_FUNCTION_FLAG_NAMED 1
#define UNC_FUNCTION_FLAG_ELLIPSIS 2
#define UNC_FUNCTION_FLAG_CFUNC 4
#define UNC_FUNCTION_FLAG_MAIN 8

#define UNC_CFUNC_DEFAULT 0
#define UNC_CFUNC_CONCURRENT 1
#define UNC_CFUNC_EXCLUSIVE 2

typedef struct Unc_FunctionUnc {
    struct Unc_Program *program;
    Unc_Size pc;
    int jumpw;
    Unc_Reg regc, floc;
    Unc_Size nameoff; /* offset into string table for function name
                       only defined if flags | UNC_FUNCTION_FLAG_NAMED */
    /* Unc_Size lineno;   line number */
    Unc_Size dbugoff; /* debug data offset */
} Unc_FunctionUnc;

typedef struct Unc_FunctionC {
    Unc_CFunc fc;
    Unc_Size namelen;
    char *name;       /* name or NULL */
    void *udata;
    int cflags;
    UNC_LOCKFULL(lock)
} Unc_FunctionC;

typedef struct Unc_Function {
    int flags;
    Unc_Size argc;
    Unc_Size rargc;
    Unc_Value *defaults;
    Unc_Size refc;
    Unc_Entity **refs;
    union {
        Unc_FunctionUnc u;
        Unc_FunctionC c;
    } f;
} Unc_Function;

typedef struct Unc_FunctionBound {
    Unc_Value boundto;
    Unc_Value fn;
} Unc_FunctionBound;

int unc__initfuncu(struct Unc_View *w, Unc_Function *fn,
            struct Unc_Program *program, Unc_Size in_off, int fromc);
int unc__initfuncc(struct Unc_View *w, Unc_Function *fn, Unc_CFunc fc,
            Unc_Size argcount, int flags, int cflags,
            Unc_Size optcount, Unc_Value *defaults,
            Unc_Size refcount, Unc_Value *initvalues,
            Unc_Size refcopycount, Unc_Size *refcopies,
            const char *fname, void *udata);
int unc__initbfunc(struct Unc_View *w, Unc_FunctionBound *bfn,
            Unc_Value *fn, Unc_Value *boundto);

/* function calling is in uvm.c */

void unc__dropfunc(struct Unc_View *w, Unc_Function *fn);
void unc__sunsetfunc(Unc_Allocator *alloc, Unc_Function *fn);

void unc__dropbfunc(struct Unc_View *w, Unc_FunctionBound *fn);
void unc__sunsetbfunc(Unc_Allocator *alloc, Unc_FunctionBound *fn);

#endif /* UNCIL_UFUNC_H */
