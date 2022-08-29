/*******************************************************************************
 
Uncil -- types and other common definitions

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

#ifndef UNCIL_UCOMMON_H
#define UNCIL_UCOMMON_H

#include <setjmp.h>
#include <signal.h>

#include "ualloc.h"
#include "ucxt.h"
#include "udef.h"
#include "uerr.h"
#include "ufunc.h"
#include "ugc.h"
#include "uhash.h"
#include "umem.h"
#include "umt.h"
#include "uprog.h"
#include "ustack.h"
#include "utxt.h"

struct Unc_View;
struct Unc_ModuleFrame;

typedef enum Unc_FrameType {
    Unc_FrameMain,
    Unc_FrameTry,
    Unc_FrameCall,
    Unc_FrameCallSpew,
    Unc_FrameNext,
    Unc_FrameNextSpew,
    Unc_FrameCallC,
    Unc_FrameCallCSpew
} Unc_FrameType;

typedef enum Unc_Mode {
    Unc_ModeStandard,
    Unc_ModeREPL
} Unc_Mode;

#if UNCIL_MT_OK
#define UNCIL_THREAD_FLAG_DAEMON 1

typedef struct Unc_Thread {
    struct Unc_View *view;
    struct Unc_Thread *up, *down;
    int flags;
} Unc_Thread;
#endif

/* represents an Uncil global state */
typedef struct Unc_World {
    Unc_Allocator alloc;        /* allocator... */
    Unc_Size viewn;             /* number of active views ("refcount") */
    Unc_Entity *etop;           /* allocated entities */
    Unc_HTblS pubs;             /* public variables */
    Unc_Mode wmode;             /* compilation mode */
    Unc_MMask mmask;            /* module mask */
    unsigned vnid;              /* next view ID */
    unsigned viewc;             /* number of total views in list */
    struct Unc_View *view;      /* first view */
    struct Unc_View *viewlast;  /* last view */
    Unc_Context ccxt;           /* saved context for REPL mode */
    Unc_Value met_str;          /* string methods */
    Unc_Value met_blob;         /* blob methods */
    Unc_Value met_arr;          /* array methods */
    Unc_Value met_dict;         /* dict methods */
    Unc_Value io_file;          /* I/O file table */
    Unc_Value exc_oom;          /* out of memory exception */
    Unc_HTblS modulecache;      /* module cache */
    Unc_Value modulepaths;      /* paths to look for modules from */
    Unc_Value moduledlpaths;    /* paths to look for C modules from */
    Unc_GC gc;                  /* garbage collector info */
    Unc_AtomicLarge refs;       /* refs from non-subviews */
    Unc_EncodingTable encs;     /* character encoding table */
    Unc_AtomicSmall finalize;   /* finalizing? */
    UNC_LOCKFULL(viewlist_lock)
    UNC_LOCKFULL(public_lock)
    UNC_LOCKFULL(entity_lock)
} Unc_World;

/* represents an Uncil stack frame */
typedef struct Unc_Frame {
    Unc_FrameType type;
    int jumpw_r;                /* backup of jumpw */
    const byte *pc_r;           /* return or target address */
    Unc_Size sreg_r;            /* saved sizes of sreg, sval, region */
    Unc_Size sval_r;
    Unc_Size region_r;
    Unc_Size regs_r;            /* backup of regs as index */
    Unc_Entity **bounds_r;      /* backup of bounds */
    const byte *jbase_r;        /* backup of jbase */
    const byte *uncfname_r;     /* backup of uncfname */
    const byte *debugbase_r;    /* backup of debugbase */
    Unc_Size boundcount_r;      /* backup of boundcount */
    Unc_Size swith_r;           /* saved size of swith */
    Unc_Size rwith_r;           /* saved size of rwith */
    const byte *pc2_r;          /* FrameNext: place to jump to if 0 values */
    Unc_Size target;            /* target, used for Unc_FrameCall */
    Unc_FunctionC *cfunc_r;     /* previous cfunc */
    Unc_Program *program_r;     /* loaded program */
    Unc_Size tails;             /* number of tail calls */
} Unc_Frame;

#define UNC_VIEW_FLOW_RUN 0
#define UNC_VIEW_FLOW_PAUSE 1
#define UNC_VIEW_FLOW_HALT 2

#define UNC_SLEEPER_VALUES 8

typedef enum Unc_ViewType {
    Unc_ViewTypeNormal,
    Unc_ViewTypeSub,
    Unc_ViewTypeSubDaemon,
    Unc_ViewTypeFinalized = -1
} Unc_ViewType;

typedef signed char Unc_ViewTypeSmall;

/* represents an Uncil thread of execution */
typedef struct Unc_View {
    Unc_World *world;           /* world */
    Unc_Value *regs;            /* register file */
    Unc_Entity **bounds;        /* bound variable file */
    const byte *jbase;          /* jump base */
    int jumpw;                  /* jump width */
    Unc_AtomicSmall flow;       /* flow control */
    Unc_AtomicSmall paused;     /* in pause loop? */
    const byte *pc;             /* program counter, when yielding etc. */
    Unc_Stack sval;             /* value stack */
    Unc_Stack sreg;             /* register stack */
    struct {                    /* expression/output stack regions */
        Unc_Size *base;
        Unc_Size *top;
        Unc_Size *end;
    } region;
    struct {                    /* call/try stack frames */
        Unc_Frame *base;
        Unc_Frame *top;
        Unc_Frame *end;
    } frames;
    const byte *bcode;          /* beginning of code pointer */
    const byte *bdata;          /* beginning of data pointer */
    Unc_FunctionC *cfunc;       /* current C func */
    Unc_Size boundcount;        /* number of bound variables. only used
                                   for C functions and is 0 otherwise */
    Unc_Size recurse;           /* overload / C function call recursion count */
    Unc_Size recurselimit;      /* overload / C function call recursion limit */
    Unc_Size vid;               /* view ID */
    Unc_Stack swith;            /* with stack values */
    struct {                    /* with stack regions */
        Unc_Size *base;
        Unc_Size *top;
        Unc_Size *end;
    } rwith;
    Unc_Program *program;       /* loaded program */
    Unc_Value fmain;            /* main function for loaded program */
    Unc_Size comperrlineno;     /* compile error line number */
    Unc_HTblS *pubs;            /* public variables */
    Unc_HTblS *exports;         /* "exported" public variables */
    struct Unc_View *nextview;  /* next view */
    struct Unc_View *prevview;  /* previous view */
    struct Unc_ModuleFrame *mframes;
    Unc_Value met_str;          /* string methods */
    Unc_Value met_blob;         /* blob methods */
    Unc_Value met_arr;          /* array methods */
    Unc_Value met_dict;         /* dict methods */
    Unc_Value exc;              /* exception */
    Unc_Value tmpval;
    /* import stuff */
    size_t curdir_n;            /* length of current directory */
    const byte *curdir;         /* current directory */
    /* flags etc. */
    char import;                /* whether we are importing */
    char has_lasterr;           /* whether we have lasterr info */
    char corotail;              /* tail call in coroutine trampoline? */
    Unc_ViewTypeSmall vtype;    /* subview? */
    /* last error info */
    struct {
        int i1;
        int i2;
        Unc_Size s;
        union {
            void *p;
            const void *c;
        } p;
    } lasterr;
    const byte *uncfname;       /* current Uncil function name */
    const byte *debugbase;      /* debug information base */
    struct Unc_View *trampoline;/* trampoline */
    Unc_Value coroutine;        /* active coroutine */
#if UNCIL_MT_OK
    Unc_Value threadme;         /* this as a thread */
    UNC_LOCKFULL(runlock)       /* running lock */
#endif
    /* Unc_Entity cache to avoid repetitive dealloc/alloc */
    Unc_Entity *sleepers[UNC_SLEEPER_VALUES];
    int sleeper_next;
    int entityload;
} Unc_View;

typedef struct Unc_Pile {
    Unc_Size r;
} Unc_Pile;

#endif /* UNCIL_UCOMMON_H */
