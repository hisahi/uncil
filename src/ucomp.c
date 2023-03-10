/*******************************************************************************
 
Uncil -- parser step 2 (Q-code -> P-code)

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

#include <limits.h>

#include "ucomp.h"
#include "udebug.h"
#include "ufunc.h"
#include "ulex.h"
#include "umem.h"
#include "uprog.h"
#include "uvlq.h"

#define MUST(val) do { int _e; if ((_e = (val))) return _e; } while (0)
#define PREBUFFER 2

typedef struct Unc_CompileContext {
    Unc_Context *cxt;
    Unc_Allocator *alloc;
    Unc_QFunc *fns;
    Unc_Program out;
    Unc_Size code_c, data_c;
    Unc_Size code_off;
    Unc_Size data_off;
    Unc_Size maindata_trs;
    Unc_Size maindata_off;
    Unc_Size tmpoff, locoff, inhoff;
    Unc_Size *fda;
    Unc_Size *labels, labels_n, labels_c, labels_i;
    Unc_Size *fjumps, fjumps_n, fjumps_c;
    Unc_Size dbug_c, dbug_n;
    byte *dbug;
    int forreal, jumpw;
} Unc_CompileContext;

INLINE int pushb(Unc_CompileContext *c, byte b) {
    return unc0_strput(c->alloc, &c->out.code, &c->out.code_sz, &c->code_c,
                                    7, b);   
}

INLINE int pushz(Unc_CompileContext *c, Unc_Size z) {
    byte b[UNC_VLQ_SIZE_MAXLEN];
    Unc_Size s = unc0_vlqencz(z, sizeof(b), b);;
    return unc0_strputn(c->alloc, &c->out.code, &c->out.code_sz, &c->code_c,
                                    7, s, b);   
}

INLINE int pushcz(Unc_CompileContext *c, Unc_Size z, Unc_Size w) {
    byte b[UNC_CLQ_MAXLEN];
    ASSERT(w <= UNC_CLQ_MAXLEN);
    unc0_clqencz(z, w, b);
    return unc0_strputn(c->alloc, &c->out.code, &c->out.code_sz, &c->code_c,
                                    7, w, b);   
}

INLINE int pushregn(Unc_CompileContext *c, Unc_Size z) {
    if (z >= (1UL << (CHAR_BIT * UNCIL_REGW)))
        return UNCIL_ERR_TOODEEP;
    return pushcz(c, z, UNCIL_REGW);
}

INLINE int pushi(Unc_CompileContext *c, Unc_Int i) {
    byte b[UNC_VLQ_UINT_MAXLEN];
    Unc_Size s = unc0_vlqenci(i, sizeof(b), b);
    return unc0_strputn(c->alloc, &c->out.code, &c->out.code_sz, &c->code_c,
                                    7, s, b);   
}

INLINE int pushil(Unc_CompileContext *c, Unc_Int i) {
    byte b[2];
    b[0] = (i & 0xFF);
    b[1] = ((i >> 8) & 0x7F) | (i < 0 ? 0x80 : 0x00);
    return unc0_strputn(c->alloc, &c->out.code, &c->out.code_sz, &c->code_c,
                                    7, 2, b);   
}

INLINE int pushf(Unc_CompileContext *c, Unc_Float f) {
    byte b[sizeof(Unc_Float)];
    unc0_memcpy(b, &f, sizeof(Unc_Float));
    return unc0_strputn(c->alloc, &c->out.code, &c->out.code_sz, &c->code_c,
                                    7, sizeof(b), b);   
}

static int pushregr(Unc_CompileContext *c, byte t, Unc_Dst d) {
    switch (t) {
    case UNC_QOPER_TYPE_TMP:
        return pushregn(c, c->tmpoff + d);
    case UNC_QOPER_TYPE_LOCAL:
        return pushregn(c, c->locoff + d);
    default:
        NEVER();
    }
}

INLINE int pushreg(Unc_CompileContext *c, byte t, union Unc_QInstr_Data d) {
    return pushregr(c, t, d.o);
}

INLINE int pushdst0(Unc_CompileContext *c, Unc_QInstr *q) {
    return pushregr(c, q->o0type, q->o0data);
}

INLINE Unc_Size offsetdata(Unc_CompileContext *c, Unc_Size s) {
    return s >= c->maindata_trs ? s + c->maindata_off : s + c->data_off;
}

#define INSTROP(q, d) q->o##d##type, q->o##d##data

static int pushrlop(Unc_CompileContext *c, byte t, union Unc_QInstr_Data d) {
    switch (t) {
    case UNC_QOPER_TYPE_TMP:
    case UNC_QOPER_TYPE_EXHALE:
    case UNC_QOPER_TYPE_INHALE:
    case UNC_QOPER_TYPE_LOCAL:
        return pushreg(c, t, d);
    case UNC_QOPER_TYPE_INT:
        return pushil(c, d.ui);
    default:
        NEVER();
    }
}

static int pushjump(Unc_CompileContext *c, Unc_Size j) {
    if (!c->forreal)
        return pushcz(c, 0, c->jumpw);
    if (j < c->labels_i) {
        /* backwards jump */
        return pushcz(c, c->labels[j], c->jumpw);
    }
    /* set up forward jump */
    if (c->fjumps_n == c->fjumps_c) {
        Unc_Size z = c->fjumps_c, nz = z + 4;
        Unc_Size *nfj = TMREALLOC(Unc_Size, c->alloc, 0, c->fjumps, z, nz);
        if (!nfj)
            return UNCIL_ERR_MEM;
        c->fjumps = nfj;
        c->fjumps_c = nz;
    }
    c->fjumps[c->fjumps_n++] = c->out.code_sz;
    return pushcz(c, j, c->jumpw);
}

int compilemov(Unc_CompileContext *c, Unc_QInstr *q) {
    switch (q->o1type) {
    case UNC_QOPER_TYPE_TMP:
    case UNC_QOPER_TYPE_LOCAL:
    case UNC_QOPER_TYPE_EXHALE:
    case UNC_QOPER_TYPE_INHALE:
        MUST(pushb(c, UNC_I_MOV));
        MUST(pushdst0(c, q));
        MUST(pushreg(c, INSTROP(q, 1)));
        break;
    case UNC_QOPER_TYPE_INT:
        if (-0x8000 <= q->o1data.ui && q->o1data.ui <= 0x7FFF) {
            MUST(pushb(c, UNC_I_LDNUM));
            MUST(pushdst0(c, q));
            MUST(pushil(c, q->o1data.ui));
        } else {
            MUST(pushb(c, UNC_I_LDINT));
            MUST(pushdst0(c, q));
            MUST(pushi(c, q->o1data.ui));
        }
        break;
    case UNC_QOPER_TYPE_FLOAT:
        MUST(pushb(c, UNC_I_LDFLT));
        MUST(pushdst0(c, q));
        MUST(pushf(c, q->o1data.uf));
        break;
    case UNC_QOPER_TYPE_NULL:
        MUST(pushb(c, UNC_I_LDNUL));
        MUST(pushdst0(c, q));
        break;
    case UNC_QOPER_TYPE_STR:
        MUST(pushb(c, UNC_I_LDSTR));
        MUST(pushdst0(c, q));
        MUST(pushz(c, offsetdata(c, q->o1data.o)));
        break;
    case UNC_QOPER_TYPE_STACK:
        MUST(pushb(c, UNC_I_LDSTK));
        MUST(pushdst0(c, q));
        MUST(pushz(c, q->o1data.o));
        break;
    case UNC_QOPER_TYPE_FALSE:
        MUST(pushb(c, UNC_I_LDBLF));
        MUST(pushdst0(c, q));
        break;
    case UNC_QOPER_TYPE_TRUE:
        MUST(pushb(c, UNC_I_LDBLT));
        MUST(pushdst0(c, q));
        break;
    case UNC_QOPER_TYPE_STACKNEG:
        MUST(pushb(c, UNC_I_LDSTKN));
        MUST(pushdst0(c, q));
        MUST(pushz(c, q->o1data.o));
        break;
    default:
        NEVER();
    }
    return 0;
}

int compileuinstr(Unc_CompileContext *c, Unc_QInstr *q) {
    int v = 0;
    switch (q->op) {
    case UNC_QINSTR_OP_LNOT:
        v = 0;
        break;
    case UNC_QINSTR_OP_UPOS:
        v = 1;
        break;
    case UNC_QINSTR_OP_UNEG:
        v = 2;
        break;
    case UNC_QINSTR_OP_UXOR:
        v = 3;
        break;
    default:
        NEVER();
    }
    
    v |= 0x80;
    if (unc0_qcode_isoplit(q->o1type)) v |= 0x10;
    MUST(pushb(c, v));
    MUST(pushdst0(c, q));
    MUST(pushrlop(c, INSTROP(q, 1)));
    return 0;
}

int compilebinstr(Unc_CompileContext *c, Unc_QInstr *q) {
    int v = 0;
    switch (q->op) {
    case UNC_QINSTR_OP_ADD:
        v = 0;
        break;
    case UNC_QINSTR_OP_SUB:
        v = 1;
        break;
    case UNC_QINSTR_OP_MUL:
        v = 2;
        break;
    case UNC_QINSTR_OP_DIV:
        v = 3;
        break;
    case UNC_QINSTR_OP_IDIV:
        v = 4;
        break;
    case UNC_QINSTR_OP_MOD:
        v = 5;
        break;
    case UNC_QINSTR_OP_AND:
        v = 6;
        break;
    case UNC_QINSTR_OP_OR:
        v = 7;
        break;
    case UNC_QINSTR_OP_XOR:
        v = 8;
        break;
    case UNC_QINSTR_OP_SHL:
        v = 9;
        break;
    case UNC_QINSTR_OP_SHR:
        v = 10;
        break;
    case UNC_QINSTR_OP_CAT:
        v = 11;
        break;
    case UNC_QINSTR_OP_CEQ:
        v = 12;
        break;
    case UNC_QINSTR_OP_CLT:
        v = 13;
        break;
    default:
        NEVER();
    }
    
    v |= 0x40;
    if (unc0_qcode_isoplit(q->o1type)) v |= 0x20;
    if (unc0_qcode_isoplit(q->o2type)) v |= 0x10;
    MUST(pushb(c, v));
    MUST(pushdst0(c, q));
    MUST(pushrlop(c, INSTROP(q, 1)));
    MUST(pushrlop(c, INSTROP(q, 2)));
    return 0;
}

int compilejump(Unc_CompileContext *c, Unc_QInstr *q) {
    switch (q->op) {
    case UNC_QINSTR_OP_JMP:
        MUST(pushb(c, UNC_I_JMP));
        break;
    case UNC_QINSTR_OP_IFT:
        MUST(pushb(c, UNC_I_IFT));
        MUST(pushdst0(c, q));
        break;
    case UNC_QINSTR_OP_IFF:
        MUST(pushb(c, UNC_I_IFF));
        MUST(pushdst0(c, q));
        break;
    case UNC_QINSTR_OP_EXPUSH:
        MUST(pushb(c, UNC_I_XPUSH));
        break;
    default:
        NEVER();
    }
    ASSERT(q->o1type == UNC_QOPER_TYPE_JUMP);
    MUST(pushjump(c, q->o1data.o));
    return 0;
}

int compilepush(Unc_CompileContext *c, Unc_QInstr *q) {
    MUST(pushb(c, UNC_I_STSTK));
    ASSERT(q->o0type == UNC_QOPER_TYPE_STACK);
    MUST(pushreg(c, INSTROP(q, 1)));
    return 0;
}

int compilewpush(Unc_CompileContext *c, Unc_QInstr *q) {
    MUST(pushb(c, UNC_I_STWITH));
    ASSERT(q->o0type == UNC_QOPER_TYPE_WSTACK);
    MUST(pushreg(c, INSTROP(q, 1)));
    return 0;
}

int compilexattr(Unc_CompileContext *c, Unc_QInstr *q) {
    switch (q->op) {
    case UNC_QINSTR_OP_GATTR:
        MUST(pushb(c, UNC_I_LDATTR));
        break;
    case UNC_QINSTR_OP_GATTRQ:
        MUST(pushb(c, UNC_I_LDATTRQ));
        break;
    case UNC_QINSTR_OP_GATTRF:
        MUST(pushb(c, UNC_I_LDATTRF));
        break;
    case UNC_QINSTR_OP_SATTR:
        MUST(pushb(c, UNC_I_STATTR));
        break;
    case UNC_QINSTR_OP_DATTR:
        MUST(pushb(c, UNC_I_DEATTR));
        break;
    default:
        NEVER();
    }
    if (q->op != UNC_QINSTR_OP_DATTR) MUST(pushdst0(c, q));
    MUST(pushreg(c, INSTROP(q, 1)));
    ASSERT(q->o2type == UNC_QOPER_TYPE_IDENT);
    MUST(pushz(c, offsetdata(c, q->o2data.o)));
    return 0;
}

int compilexindx(Unc_CompileContext *c, Unc_QInstr *q) {
    switch (q->op) {
    case UNC_QINSTR_OP_GINDX:
        MUST(pushb(c, UNC_I_LDINDX));
        break;
    case UNC_QINSTR_OP_GINDXQ:
        MUST(pushb(c, UNC_I_LDINDXQ));
        break;
    case UNC_QINSTR_OP_SINDX:
        MUST(pushb(c, UNC_I_STINDX));
        break;
    case UNC_QINSTR_OP_DINDX:
        MUST(pushb(c, UNC_I_DEINDX));
        break;
    default:
        NEVER();
    }
    if (q->op != UNC_QINSTR_OP_DINDX) MUST(pushdst0(c, q));
    MUST(pushreg(c, INSTROP(q, 1)));
    MUST(pushreg(c, INSTROP(q, 2)));
    return 0;
}

int compilexpub(Unc_CompileContext *c, Unc_QInstr *q) {
    switch (q->op) {
    case UNC_QINSTR_OP_GPUB:
        MUST(pushb(c, UNC_I_LDPUB));
        break;
    case UNC_QINSTR_OP_SPUB:
        MUST(pushb(c, UNC_I_STPUB));
        break;
    case UNC_QINSTR_OP_DPUB:
        MUST(pushb(c, UNC_I_DEPUB));
        break;
    default:
        NEVER();
    }
    if (q->op != UNC_QINSTR_OP_DPUB) MUST(pushdst0(c, q));
    ASSERT(q->o1type == UNC_QOPER_TYPE_PUBLIC);
    MUST(pushz(c, offsetdata(c, q->o1data.o)));
    return 0;
}

int compilexbind(Unc_CompileContext *c, Unc_QInstr *q) {
    switch (q->op) {
    case UNC_QINSTR_OP_GBIND:
        MUST(pushb(c, UNC_I_LDBIND));
        break;
    case UNC_QINSTR_OP_SBIND:
        MUST(pushb(c, UNC_I_STBIND));
        break;
    default:
        NEVER();
    }
    MUST(pushdst0(c, q));
    switch (q->o1type) {
    case UNC_QOPER_TYPE_EXHALE:
        MUST(pushregn(c, q->o1data.o));
        break;
    case UNC_QOPER_TYPE_INHALE:
        MUST(pushregn(c, q->o1data.o + c->inhoff));
        break;
    default:
        NEVER();
    }
    return 0;
}

int compilefcall(Unc_CompileContext *c, Unc_QInstr *q) {
    if (q->o0type == UNC_QOPER_TYPE_STACK) {
        MUST(pushb(c, UNC_I_FCALLS));
        MUST(pushreg(c, INSTROP(q, 1)));
    } else {
        MUST(pushb(c, UNC_I_FCALL));
        MUST(pushdst0(c, q));
        MUST(pushreg(c, INSTROP(q, 1)));
    }
    return 0;
}

int compiledcall(Unc_CompileContext *c, Unc_QInstr *q) {
    if (q->o0type == UNC_QOPER_TYPE_STACK) {
        MUST(pushb(c, UNC_I_DCALLS));
        MUST(pushb(c, q->o2data.o));
        MUST(pushreg(c, INSTROP(q, 1)));
    } else {
        MUST(pushb(c, UNC_I_DCALL));
        MUST(pushb(c, q->o2data.o));
        MUST(pushdst0(c, q));
        MUST(pushreg(c, INSTROP(q, 1)));
    }
    return 0;
}

int compileftail(Unc_CompileContext *c, Unc_QInstr *q) {
    ASSERT(q->o0type == UNC_QOPER_TYPE_STACK);
    MUST(pushb(c, UNC_I_FTAIL));
    MUST(pushreg(c, INSTROP(q, 1)));
    return 0;
}

int compiledtail(Unc_CompileContext *c, Unc_QInstr *q) {
    ASSERT(q->o0type == UNC_QOPER_TYPE_STACK);
    MUST(pushb(c, UNC_I_DTAIL));
    MUST(pushb(c, q->o2data.o));
    MUST(pushreg(c, INSTROP(q, 1)));
    return 0;
}

int compilespread(Unc_CompileContext *c, Unc_QInstr *q) {
    if (q->o0type == UNC_QOPER_TYPE_STACK) {
        MUST(pushb(c, UNC_I_LSPRS));
        MUST(pushreg(c, INSTROP(q, 1)));
    } else {
        MUST(pushb(c, UNC_I_LSPR));
        MUST(pushdst0(c, q));
        MUST(pushreg(c, INSTROP(q, 1)));
    }
    return 0;
}

int compileiiter(Unc_CompileContext *c, Unc_QInstr *q) {
    MUST(pushb(c, UNC_I_IITER));
    MUST(pushdst0(c, q));
    MUST(pushreg(c, INSTROP(q, 1)));
    return 0;
}

int compileinext(Unc_CompileContext *c, Unc_QInstr *q) {
    MUST(pushb(c, UNC_I_INEXT));
    MUST(pushdst0(c, q));
    MUST(pushreg(c, INSTROP(q, 1)));
    ASSERT(q->o2type == UNC_QOPER_TYPE_JUMP);
    MUST(pushjump(c, q->o2data.o));
    return 0;
}

int compileinexts(Unc_CompileContext *c, Unc_QInstr *q) {
    MUST(pushb(c, UNC_I_INEXTS));
    MUST(pushreg(c, INSTROP(q, 1)));
    ASSERT(q->o2type == UNC_QOPER_TYPE_JUMP);
    MUST(pushjump(c, q->o2data.o));
    return 0;
}

int compilefmake(Unc_CompileContext *c, Unc_QInstr *q) {
    MUST(pushb(c, UNC_I_FMAKE));
    MUST(pushdst0(c, q));
    ASSERT(q->o1type == UNC_QOPER_TYPE_FUNCTION);
    MUST(pushz(c, c->fda[q->o1data.o]));
    return 0;
}

int compile1reg(Unc_CompileContext *c, Unc_QInstr *q, byte b) {
    MUST(pushb(c, b));
    MUST(pushdst0(c, q));
    return 0;
}

int compile1us(Unc_CompileContext *c, Unc_QInstr *q, byte b) {
    MUST(pushb(c, b));
    ASSERT(q->o1type == UNC_QOPER_TYPE_UNSIGN);
    MUST(pushz(c, q->o1data.o));
    return 0;
}

int compilemlistp(Unc_CompileContext *c, Unc_QInstr *q) {
    MUST(pushb(c, UNC_I_MLISTP));
    MUST(pushdst0(c, q));
    ASSERT(q->o1type == UNC_QOPER_TYPE_UNSIGN);
    MUST(pushz(c, q->o1data.o));
    ASSERT(q->o2type == UNC_QOPER_TYPE_UNSIGN);
    MUST(pushz(c, q->o2data.o));
    return 0;
}

int compilefbind(Unc_CompileContext *c, Unc_QInstr *q) {
    MUST(pushb(c, UNC_I_FBIND));
    MUST(pushdst0(c, q));
    MUST(pushreg(c, INSTROP(q, 1)));
    MUST(pushreg(c, INSTROP(q, 2)));
    return 0;
}

int compileinstr(Unc_CompileContext *c, Unc_QInstr *q) {
    switch (q->op) {
    case UNC_QINSTR_OP_DELETE:
        return 0;
    case UNC_QINSTR_OP_MOV:
        return compilemov(c, q);
    case UNC_QINSTR_OP_ADD:
    case UNC_QINSTR_OP_SUB:
    case UNC_QINSTR_OP_MUL:
    case UNC_QINSTR_OP_DIV:
    case UNC_QINSTR_OP_IDIV:
    case UNC_QINSTR_OP_MOD:
    case UNC_QINSTR_OP_SHL:
    case UNC_QINSTR_OP_SHR:
    case UNC_QINSTR_OP_CAT:
    case UNC_QINSTR_OP_AND:
    case UNC_QINSTR_OP_OR:
    case UNC_QINSTR_OP_XOR:
    case UNC_QINSTR_OP_CEQ:
    case UNC_QINSTR_OP_CLT:
        return compilebinstr(c, q);
    case UNC_QINSTR_OP_JMP:
    case UNC_QINSTR_OP_IFT:
    case UNC_QINSTR_OP_IFF:
        return compilejump(c, q);
    case UNC_QINSTR_OP_PUSH:
        return compilepush(c, q);
    case UNC_QINSTR_OP_LNOT:
    case UNC_QINSTR_OP_UPOS:
    case UNC_QINSTR_OP_UNEG:
    case UNC_QINSTR_OP_UXOR:
        return compileuinstr(c, q);
    case UNC_QINSTR_OP_EXPUSH:
        return compilejump(c, q);
    case UNC_QINSTR_OP_EXPOP:
        return pushb(c, UNC_I_XPOP);
    case UNC_QINSTR_OP_GATTR:
    case UNC_QINSTR_OP_GATTRQ:
    case UNC_QINSTR_OP_GATTRF:
    case UNC_QINSTR_OP_SATTR:
    case UNC_QINSTR_OP_DATTR:
        return compilexattr(c, q);
    case UNC_QINSTR_OP_GINDX:
    case UNC_QINSTR_OP_GINDXQ:
    case UNC_QINSTR_OP_SINDX:
    case UNC_QINSTR_OP_DINDX:
        return compilexindx(c, q);
    case UNC_QINSTR_OP_PUSHF:
        return pushb(c, UNC_I_RPUSH);
    case UNC_QINSTR_OP_POPF:
        return pushb(c, UNC_I_RPOP);
    case UNC_QINSTR_OP_FCALL:
        return compilefcall(c, q);
    case UNC_QINSTR_OP_GPUB:
    case UNC_QINSTR_OP_SPUB:
    case UNC_QINSTR_OP_DPUB:
        return compilexpub(c, q);
    case UNC_QINSTR_OP_IITER:
        return compileiiter(c, q);
    case UNC_QINSTR_OP_INEXT:
        return compileinext(c, q);
    case UNC_QINSTR_OP_INEXTS:
        return compileinexts(c, q);
    case UNC_QINSTR_OP_FMAKE:
        return compilefmake(c, q);
    case UNC_QINSTR_OP_NOP:
        return pushb(c, UNC_I_NOP);
    case UNC_QINSTR_OP_END:
        return pushb(c, UNC_I_EXIT);
    case UNC_QINSTR_OP_MLIST:
        return compile1reg(c, q, UNC_I_MLIST);
    case UNC_QINSTR_OP_NDICT:
        return compile1reg(c, q, UNC_I_NDICT);
    case UNC_QINSTR_OP_GBIND:
    case UNC_QINSTR_OP_SBIND:
        return compilexbind(c, q);
    case UNC_QINSTR_OP_SPREAD:
        return compilespread(c, q);
    case UNC_QINSTR_OP_MLISTP:
        return compilemlistp(c, q);
    case UNC_QINSTR_OP_STKEQ:
        return compile1us(c, q, UNC_I_CSTK);
    case UNC_QINSTR_OP_STKGE:
        return compile1us(c, q, UNC_I_CSTKG);
    case UNC_QINSTR_OP_FBIND:
        return compilefbind(c, q);
    case UNC_QINSTR_OP_WPUSH:
        return pushb(c, UNC_I_WPUSH);
    case UNC_QINSTR_OP_WPOP:
        return pushb(c, UNC_I_WPOP);
    case UNC_QINSTR_OP_PUSHW:
        return compilewpush(c, q);
    case UNC_QINSTR_OP_FTAIL:
        return compileftail(c, q);
    case UNC_QINSTR_OP_DCALL:
        return compiledcall(c, q);
    case UNC_QINSTR_OP_DTAIL:
        return compiledtail(c, q);
    case UNC_QINSTR_OP_EXIT0:
        return pushb(c, UNC_I_EXIT0);
    case UNC_QINSTR_OP_EXIT1:
        return compile1reg(c, q, UNC_I_EXIT1);
    default:
        NEVER();
    }
}

int compilefunc_i(Unc_CompileContext *c, Unc_QFunc *f, Unc_Size base) {
    Unc_Size i, n = f->cd_sz, cn = base;
    Unc_QInstr *q = f->cd;
    Unc_Size lineno = f->lineno;

    c->tmpoff = 0;
    c->locoff = c->tmpoff + f->cnt_tmp;
    c->inhoff = f->cnt_exh;
    c->fjumps_n = 0;
    
    if (c->forreal) {
        /* put line number into debug data */
        Unc_Size o;
        byte b[UNC_VLQ_SIZE_MAXLEN];
        o = unc0_vlqencz(lineno, sizeof(b), b);
        MUST(unc0_strputn(c->alloc,
                        &c->dbug, &c->dbug_n, &c->dbug_c, 6, o, b));
    }

    /* compile, backward jumps are immediately set up,
                forward jumps are recorded */
    for (i = 0; i < n; ++i) {
        if (c->forreal && c->labels_i < c->labels_n
                                && i == c->labels[c->labels_i])
            c->labels[c->labels_i++] = c->out.code_sz - base;
        if (q[i].lineno != lineno) {
            if (c->forreal) {
                /* add entry to line number offset table */
                Unc_Int l = q[i].lineno - lineno;
                Unc_Dst off = c->out.code_sz - cn;
                Unc_Size o;
                {
                    byte b[UNC_VLQ_SIZE_MAXLEN];
                    o = unc0_vlqencz(off, sizeof(b), b);
                    MUST(unc0_strputn(c->alloc,
                                    &c->dbug, &c->dbug_n, &c->dbug_c, 6, o, b));
                }
                {
                    byte b[UNC_VLQ_UINT_MAXLEN];
                    o = unc0_vlqenci(l, sizeof(b), b);
                    MUST(unc0_strputn(c->alloc,
                                    &c->dbug, &c->dbug_n, &c->dbug_c, 6, o, b));
                }
                cn = c->out.code_sz;
            }
            
            lineno = q[i].lineno;
        }
        MUST(compileinstr(c, &q[i]));
    }
    if (c->forreal) {
        Unc_Size o;
        byte b[UNC_VLQ_SIZE_MAXLEN];
        o = unc0_vlqencz(39, sizeof(b), b);
        MUST(unc0_strputn(c->alloc,
                        &c->dbug, &c->dbug_n, &c->dbug_c, 6, o, b));
    }
    return 0;
}

static int lblbsearch(Unc_CompileContext *c, Unc_Size s, Unc_Size *out) {
    Unc_Size a = 0, b = c->labels_n, r;
    while (a < b) {
        r = a + (b - a) / 2;
        if (s > c->labels[r]) {
            a = r + 1;
        } else if (s < c->labels[r]) {
            b = r;
        } else { /* equal */
            *out = r;
            return 1;
        }
    }
    *out = a;
    return 0;
}

int compilefunc(Unc_CompileContext *c, Unc_QFunc *f) {
    Unc_Size i, n = f->cd_sz, base = c->out.code_sz, size;
    Unc_QInstr *q = f->cd;
    c->fjumps_n = 0;
    c->labels_n = 0;
    /* scan all jumps */
    for (i = 0; i < n; ++i) {
        Unc_Size s, r;
        switch (unc0_qcode_getjumpd(q[i].op)) {
        case 1:
            ASSERT(q[i].o1type == UNC_QOPER_TYPE_JUMP);
            s = q[i].o1data.o;
            break;
        case 2:
            ASSERT(q[i].o2type == UNC_QOPER_TYPE_JUMP);
            s = q[i].o2data.o;
            break;
        default:
            continue;
        }

        /* binary search time */
        if (lblbsearch(c, s, &r))
            goto labelexists;

        if (c->labels_n == c->labels_c) {
            Unc_Size z = c->labels_c, nz = z + 4;
            Unc_Size *nlb = TMREALLOC(Unc_Size, c->alloc, 0, c->labels, z, nz);
            if (!nlb)
                return UNCIL_ERR_MEM;
            c->labels = nlb;
            c->labels_c = nz;
        }

        if (r < c->labels_n)
            unc0_memmove(&c->labels[r + 1], &c->labels[r],
                                    sizeof(Unc_Size) * (c->labels_n - r));
        c->labels[r] = s;
        ++c->labels_n;
labelexists:;
    }

    /* replace all jumps */
    for (i = 0; i < n; ++i) {
        union Unc_QInstr_Data u;
        int e;
        switch (unc0_qcode_getjumpd(q[i].op)) {
        case 1:
            e = lblbsearch(c, q[i].o1data.o, &u.o);
            ASSERT(e);
            q[i].o1data = u;
            break;
        case 2:
            e = lblbsearch(c, q[i].o2data.o, &u.o);
            ASSERT(e);
            q[i].o2data = u;
            break;
        default:
            continue;
        }
    }

    c->forreal = 0;
    c->jumpw = UNC_CLQ_MAXLEN;
    MUST(compilefunc_i(c, f, base));
    size = c->out.code_sz - base;
    c->out.code_sz = base;
    c->jumpw = 0;
    do {
        size >>= CHAR_BIT;
        ++c->jumpw;
    } while (size);
    ASSERT(c->jumpw <= UCHAR_MAX);
    c->labels_i = 0;
    c->forreal = 1;
    MUST(pushb(c, UNC_I_DEL));
    MUST(pushb(c, c->jumpw));
    base = c->code_off = c->out.code_sz;
    MUST(compilefunc_i(c, f, base));
    ASSERT(c->labels_i == c->labels_n);
    /* patch forward jumps */
    for (i = 0; i < c->fjumps_n; ++i) {
        Unc_Size off = c->fjumps[i];
        Unc_Size dst = unc0_clqdeczd(c->jumpw, c->out.code + off);
        unc0_clqencz(c->labels[dst], c->jumpw, c->out.code + off);
    }
    return 0;
}

INLINE void enc_ca(byte *b, Unc_Size s) {
    unc0_clqencz(s, UNC_BYTES_IN_FCODEADDR, b);
}

INLINE Unc_Size dec_ca(const byte *b) {
    return unc0_clqdeczd(UNC_BYTES_IN_FCODEADDR, b);
}

int pushfunc(Unc_CompileContext *c, Unc_QFunc *f, int main) {
    /* function format:
        unsigned LE 64-bit CLQ (NOT VLQ!!) of code offset
        unsigned LE 64-bit CLQ (NOT VLQ!!) of debug data offset
            (or 0 if none)
        unsigned 8-bit number for jump width
        unsigned VLQ of flags (1 = has name, 2 = ellipsis, 4 = invalid)
        unsigned VLQ of register count
        unsigned VLQ of register index of first local variable
        unsigned VLQ of exhale count
        unsigned VLQ of required argument count
        unsigned VLQ of optional argument count
        unsigned VLQ of inhale count
        for every inhale:
            unsigned VLQ of inhaled register
        unsigned VLQ of name offset
    */
    Unc_Allocator *alc = c->alloc;
    byte b[UNC_VLQ_SIZE_MAXLEN];
    Unc_Size s, i;
    c->tmpoff = 0;
    c->locoff = c->tmpoff + f->cnt_tmp;
    c->inhoff = f->cnt_exh;

    ASSERT(!main || !f->cnt_inh);

    {
        byte b2[UNC_BYTES_IN_FCODEADDR] = { 0 };
        MUST(unc0_strputn(alc, &c->out.data, &c->out.data_sz, &c->data_c, 6,
                                                        sizeof(b2), b2));
        MUST(unc0_strputn(alc, &c->out.data, &c->out.data_sz, &c->data_c, 6,
                                                        sizeof(b2), b2));
        MUST(unc0_strputn(alc, &c->out.data, &c->out.data_sz, &c->data_c, 6,
                                                        1, b2));
    }
    s = unc0_vlqencz(f->flags | (main ? UNC_FUNCTION_FLAG_MAIN : 0),
                     sizeof(b), b);
    MUST(unc0_strputn(alc, &c->out.data, &c->out.data_sz, &c->data_c, 6, s, b));
    s = unc0_vlqencz(c->locoff + f->cnt_loc, sizeof(b), b);
    MUST(unc0_strputn(alc, &c->out.data, &c->out.data_sz, &c->data_c, 6, s, b));
    s = unc0_vlqencz(c->locoff, sizeof(b), b);
    MUST(unc0_strputn(alc, &c->out.data, &c->out.data_sz, &c->data_c, 6, s, b));
    s = unc0_vlqencz(f->cnt_exh, sizeof(b), b);
    MUST(unc0_strputn(alc, &c->out.data, &c->out.data_sz, &c->data_c, 6, s, b));
    s = unc0_vlqencz(f->cnt_arg - f->cnt_opt
            - !!(f->flags & UNC_FUNCTION_FLAG_ELLIPSIS), sizeof(b), b);
    MUST(unc0_strputn(alc, &c->out.data, &c->out.data_sz, &c->data_c, 6, s, b));
    s = unc0_vlqencz(f->cnt_opt, sizeof(b), b);
    MUST(unc0_strputn(alc, &c->out.data, &c->out.data_sz, &c->data_c, 6, s, b));
    s = unc0_vlqencz(f->cnt_inh, sizeof(b), b);
    MUST(unc0_strputn(alc, &c->out.data, &c->out.data_sz, &c->data_c, 6, s, b));

    if (!main) {
        Unc_QFunc *pf = &c->fns[f->parent];
        Unc_Size parentinhoff = pf->cnt_exh;
        for (i = 0; i < f->cnt_inh; ++i) {
            Unc_QOperand *po = &f->inhales[i];
            switch (po->type) {
            case UNC_QOPER_TYPE_EXHALE:
                s = unc0_vlqencz(po->data.o, sizeof(b), b);
                MUST(unc0_strputn(alc, &c->out.data, &c->out.data_sz,
                                       &c->data_c, 6, s, b));
                break;
            case UNC_QOPER_TYPE_INHALE:
                s = unc0_vlqencz(parentinhoff + po->data.o, sizeof(b), b);
                MUST(unc0_strputn(alc, &c->out.data, &c->out.data_sz,
                                       &c->data_c, 6, s, b));
                break;
            default:
                ASSERT(0);
            }
        }
    }

    /*s = unc0_vlqencz(f->lineno, sizeof(b), b); now in debug data
    MUST(unc0_strputn(alc, &c->out.data, &c->out.data_sz, &c->data_c, 6, s, b)); */
    if (f->flags & UNC_FUNCTION_FLAG_NAMED)
        s = unc0_vlqencz(offsetdata(c, f->name), sizeof(b), b);
    else
        s = unc0_vlqencz(0, sizeof(b), b);
    MUST(unc0_strputn(alc, &c->out.data, &c->out.data_sz, &c->data_c, 6, s, b));

    return 0;
}

int unc0_parsec2(Unc_Context *cxt, Unc_Program *out, Unc_QCode *qc) {
    Unc_Allocator *alloc = cxt->alloc;
    Unc_CompileContext c;
    int e;
    Unc_Size fn = qc->fn_sz;

    c.cxt = cxt;
    c.alloc = cxt->alloc;
    c.labels = NULL;
    c.labels_c = c.labels_n = 0;
    c.fjumps = NULL;
    c.fjumps_c = c.fjumps_n = 0;
    if ((e = unc0_upgradeprogram(out, alloc)))
        goto failure0;
    c.fda = TMALLOC(Unc_Size, alloc, 0, fn);
    if (!c.fda) {
        e = UNCIL_ERR_MEM;
        goto failure0;
    }
    c.out = *out;
    c.fns = qc->fn;
    c.maindata_trs = cxt->main_dta;

    c.code_c = c.out.code_sz;
    c.data_c = c.out.data_sz;

    if (c.out.data_sz > c.cxt->main_off)
        c.out.code_sz = dec_ca(c.out.data + c.cxt->main_off) - PREBUFFER;
    c.out.data_sz = c.cxt->main_off;
    c.maindata_off = c.data_off = c.out.data_sz;
    
    if ((e = unc0_strputn(alloc, &c.out.data, &c.out.data_sz, &c.data_c,
                            6, cxt->main_dta, qc->st)))
        goto failure2;

    c.dbug_c = 0;
    c.dbug_n = 0;
    c.dbug = NULL;

    ASSERT(fn > 0);
    {
        Unc_Size n;
        /* function data */
        for (n = 1; n < fn; ++n) {
            Unc_QFunc *f = &qc->fn[n];
            c.fda[n] = c.out.data_sz;
            if ((e = pushfunc(&c, f, 0)))
                goto failure2;
            TMFREE(Unc_QOperand, alloc, f->inhales, f->cnt_inh);
            f->inhales = NULL;
        }
        
        /* function code */
        for (n = 1; n < fn; ++n) {
            Unc_QFunc *f = &qc->fn[n];
            { /* repatch debug address */
                byte b[UNC_BYTES_IN_FCODEADDR];
                enc_ca(b, c.dbug_n);
                unc0_memcpy(c.out.data + c.fda[n] + sizeof(b), b, sizeof(b));
            }
            if ((e = compilefunc(&c, f)))
                goto failure2;
            TMFREE(Unc_QInstr, alloc, f->cd, f->cd_sz);
            f->cd = NULL;
            { /* repatch code address and jump width */
                byte b[UNC_BYTES_IN_FCODEADDR];
                enc_ca(b, c.code_off);
                unc0_memcpy(c.out.data + c.fda[n], b, sizeof(b));
                *(c.out.data + c.fda[n] + UNC_BYTES_IN_FCODEADDR * 2) = c.jumpw;
            }
        }
        
        {
            Unc_QFunc *f = &qc->fn[0];
            c.fda[0] = c.out.data_sz;
            c.cxt->main_off = c.out.data_sz;
            if ((e = pushfunc(&c, f, 1)))
                goto failure2;
            TMFREE(Unc_QOperand, alloc, f->inhales, f->cnt_inh);
            f->inhales = NULL;
        }
        c.maindata_off = c.out.data_sz - cxt->main_dta;
        if ((e = unc0_strputn(alloc, &c.out.data, &c.out.data_sz, &c.data_c,
                        6, qc->st_sz - cxt->main_dta, qc->st + cxt->main_dta)))
            goto failure2;
        unc0_mfree(alloc, qc->st, qc->st_sz);
        {
            Unc_QFunc *f = &qc->fn[0];
            { /* repatch debug address */
                byte b[UNC_BYTES_IN_FCODEADDR];
                enc_ca(b, c.dbug_n);
                unc0_memcpy(c.out.data + c.fda[0] + sizeof(b), b, sizeof(b));
            }
            if ((e = compilefunc(&c, f)))
                goto failure1;
            TMFREE(Unc_QInstr, alloc, f->cd, f->cd_sz);
            f->cd = NULL;
            /* repatch address and jump width */
            {
                byte b[UNC_BYTES_IN_FCODEADDR];
                enc_ca(b, c.code_off);
                unc0_memcpy(c.out.data + c.fda[0], b, sizeof(b));
                *(c.out.data + c.fda[0] + UNC_BYTES_IN_FCODEADDR * 2) = c.jumpw;
            }
        }
    
        /* adjust debug offsets to be correct */
        for (n = 0; n < fn; ++n) {
            Unc_Size qa;
            byte b[UNC_BYTES_IN_FCODEADDR];
            unc0_memcpy(b, c.out.data + c.fda[n] + sizeof(b), sizeof(b));
            qa = dec_ca(b);
            qa += c.out.data_sz;
            enc_ca(b, qa);
            unc0_memcpy(c.out.data + c.fda[n] + sizeof(b), b, sizeof(b));
        }

        if (c.dbug) {
            if ((e = unc0_strputn(alloc, &c.out.data, &c.out.data_sz, &c.data_c,
                        6, c.dbug_n, c.dbug)))
                goto failure1;
        }
    }

    if (c.out.code)
        c.out.code = unc0_mrealloc(alloc, 0, c.out.code,
                                    c.code_c, c.out.code_sz);
    if (c.out.data)
        c.out.data = unc0_mrealloc(alloc, 0, c.out.data,
                                    c.data_c, c.out.data_sz);
    TMFREE(Unc_Size, alloc, c.fda, fn);
    *out = c.out;
    goto success;
failure2:
    unc0_mfree(alloc, qc->st, qc->st_sz);
failure1:
    TMFREE(Unc_Size, alloc, c.fda, fn);
failure0:
    {
        Unc_Size n;
        for (n = 0; n < fn; ++n) {
            Unc_QFunc *f = &qc->fn[n];
            TMFREE(Unc_QInstr, alloc, f->cd, f->cd_sz);
            TMFREE(Unc_QOperand, alloc, f->inhales, f->cnt_inh);
        }
    }
success:
    TMFREE(Unc_QFunc, alloc, qc->fn, fn);
    TMFREE(Unc_Size, alloc, c.labels, c.labels_c);
    TMFREE(Unc_Size, alloc, c.fjumps, c.fjumps_c);
    unc0_mfree(alloc, c.dbug, c.dbug_c);
    return e;
}
