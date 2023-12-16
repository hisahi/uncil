/*******************************************************************************
 
Uncil -- parser step 1 (L-code -> Q-code) impl

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

#include "uarithm.h"
#include "udebug.h"
#include "uerr.h"
#include "ufunc.h"
#include "uparse.h"
#include "uutf.h"
#include "uvlq.h"

#define UNCIL_ERR_KIND_SYNTAXL 4
#define UNCIL_ERR_SYNTAXL_ASSIGNOP 0x0401

#define MUST(val) do { Unc_RetVal _e; if ((_e = (val))) return _e; } while (0)
#define UNCIL_ERR(x) UNCIL_ERR_##x

#if DEBUGPRINT_PARSE0
static int report(int l, int e) { printf("%d:%04x\n", l, e); return e; }
#undef UNCIL_ERR
#define UNCIL_ERR(x) report(__LINE__, UNCIL_ERR_##x)
#endif

typedef struct Unc_ParserParent {
    Unc_BTree *book;
    Unc_QInstr **cd;
    Unc_Size *cd_n, *cd_c;
    Unc_Dst *locnext, *tmphigh, *inhnext, *exhnext;
    Unc_QOperand **inhl;
    Unc_Size *inhl_c;
    Unc_Size argc, *arg_exh;
} Unc_ParserParent;

typedef struct Unc_ParserContext {
    Unc_Context c;
    Unc_QCode out;
    Unc_LexOut lex;
    Unc_QInstr next;
    Unc_BTree *book;
    Unc_QInstr *cd;
    byte *st;
    int valtype;
    unsigned quiet;
    unsigned allownl;
    int fence, xfence;
    int compoundop;
    Unc_Size pushfs;
    Unc_Size cd_n, cd_c;
    Unc_Dst locnext, tmpnext, exhnext, inhnext, tmphigh;
    Unc_Size *lb;
    Unc_Size lb_n, lb_c;
    /* aux is used for storing operands for attribute/index expressions */
    Unc_QOperand *aux;
    Unc_Size aux_n, aux_c;
    /* plb is used to store positions for break/continue jumps */
    Unc_Size *plb;
    Unc_Size plb_n, plb_c;
    Unc_QOperand *inhl;
    Unc_Size inhl_c;
    Unc_ParserParent *pframes;
    Unc_Size pframes_n, pframes_c;
    Unc_Size curfunc;
    byte *id_status;
    Unc_Size *id_offset;
    byte *st_status;
    Unc_Size *st_offset;
    Unc_Size argc;
    Unc_Size *arg_exh;
    Unc_Size withbs;
    Unc_Size withbanchor;
} Unc_ParserContext;

#define STFLAG_SAVED 0x80
#define STFLAG_USED_SUB 2
#define STFLAG_USED_MAIN 1

#define VALTYPE_NONE 0
#define VALTYPE_STACK 1
#define VALTYPE_HOLD 2
#define VALTYPE_FSTACK 3

#define FUNCTYPE_EXPR 0
#define FUNCTYPE_LOCAL 1
#define FUNCTYPE_PUBLIC 2
#define FUNCTYPE_DICT 3

#define MARK_STR_USED(c, z) (c->st_status[z] |= (c->pframes_n ? \
                                    STFLAG_USED_SUB : STFLAG_USED_MAIN))
#define MARK_ID_USED(c, z) (c->id_status[z] |= (c->pframes_n ? \
                                    STFLAG_USED_SUB : STFLAG_USED_MAIN))
#define MARK_ID_USED_BOTH(c, z) (c->id_status[z] |= \
                                    STFLAG_USED_SUB | STFLAG_USED_MAIN)

typedef struct Unc_Save {
    byte *ptr;
    Unc_Size lineno;
} Unc_Save;

INLINE void skipnl(Unc_ParserContext *c, int consuming) {
    while (c->lex.lc_sz && *c->lex.lc == ULT_NL)
        --c->lex.lc_sz, ++c->lex.lc, c->out.lineno += consuming;
}

INLINE int peek(Unc_ParserContext *c) {
    if (c->allownl) skipnl(c, 0);
    return !c->lex.lc_sz ? ULT_END : *c->lex.lc;
}

INLINE byte consume(Unc_ParserContext *c) {
    byte b;
    if (c->allownl) skipnl(c, !c->quiet);
    if (!c->lex.lc_sz) return 0;
    --c->lex.lc_sz;
    b = *c->lex.lc++;
    if (b == ULT_NL && !c->quiet) ++c->out.lineno;
    return b;
}

INLINE byte rconsume(Unc_ParserContext *c) {
    byte b;
    if (!c->lex.lc_sz) return 0;
    --c->lex.lc_sz;
    b = *c->lex.lc++;
    return b;
}

static Unc_Int consumei(Unc_ParserContext *c) {
    Unc_Int s;
    byte b[sizeof(Unc_Int)];
    int n = 0;
    for (n = 0; n < sizeof(b); ++n)
        b[n] = rconsume(c);
    unc0_memcpy(&s, b, sizeof(b));
    return s;
}

static Unc_Float consumef(Unc_ParserContext *c) {
    Unc_Float s;
    byte b[sizeof(Unc_Float)];
    int n = 0;
    for (n = 0; n < sizeof(b); ++n)
        b[n] = rconsume(c);
    unc0_memcpy(&s, b, sizeof(b));
    return s;
}

static Unc_Size consumez(Unc_ParserContext *c) {
    Unc_Size s;
    byte b[sizeof(Unc_Size)];
    int n = 0;
    for (n = 0; n < sizeof(b); ++n)
        b[n] = rconsume(c);
    unc0_memcpy(&s, b, sizeof(b));
    return s;
}

/* stops/resumes instructions from emitted */
#define qpause(c) do { if (!++c->quiet) return UNCIL_ERR(SYNTAX_TOODEEP); \
                                            } while (0)
#define qresume(c) (void)(c->quiet && --c->quiet)

/* save/restore lexer position */
static void lsave(Unc_ParserContext *c, Unc_Save *s) {
    s->ptr = c->lex.lc;
    s->lineno = c->out.lineno;
}

static void lrestore(Unc_ParserContext *c, Unc_Save *s) {
    byte *b = s->ptr;
    c->lex.lc_sz += c->lex.lc - b;
    c->lex.lc = b;
    c->out.lineno = s->lineno;
}

INLINE union Unc_QInstr_Data qdatanone(void) {
    union Unc_QInstr_Data u = { 0 };
    return u;
}

INLINE union Unc_QInstr_Data qdataz(Unc_Size o) {
    union Unc_QInstr_Data u;
    u.o = o;
    return u;
}

INLINE union Unc_QInstr_Data qdatai(Unc_Int ui) {
    union Unc_QInstr_Data u;
    u.ui = ui;
    return u;
}

INLINE union Unc_QInstr_Data qdataf(Unc_Float uf) {
    union Unc_QInstr_Data u;
    u.uf = uf;
    return u;
}

#define CHECK_UNSIGNED_OVERFLOW(x) if (!~(x)) return UNCIL_ERR(SYNTAX_TOODEEP);

INLINE Unc_RetVal tmpalloc(Unc_ParserContext *c, Unc_Dst *out) {
    Unc_Dst u = c->tmpnext;
    CHECK_UNSIGNED_OVERFLOW(u);
    if (!c->quiet) {
        if (c->tmphigh < ++c->tmpnext)
            c->tmphigh = c->tmpnext;
    }
    *out = u;
    return 0;
}

INLINE Unc_RetVal localloc(Unc_ParserContext *c, Unc_Dst *out) {
    Unc_Dst u = c->locnext;
    CHECK_UNSIGNED_OVERFLOW(u);
    if (!c->quiet) ++c->locnext;
    *out = u;
    return 0;
}

INLINE Unc_RetVal lblalloc(Unc_ParserContext *c, Unc_Size *out, int z) {
    if (c->lb_n + z > c->lb_c) {
        Unc_Size z = c->lb_c, nz = z + 16;
        Unc_Size *nlb = TMREALLOC(Unc_Size, c->c.alloc, 0, c->lb, z, nz);
        if (!nlb) return UNCIL_ERR(MEM);
        c->lb = nlb;
        c->lb_c = nz;
    }
    *out = c->lb_n;
    c->lb_n += z;
    return 0;
}

/*INLINE Unc_RetVal rauxalloc(Unc_ParserContext *c, Unc_Dst *out) {
    Unc_Dst u = c->c.next_aux;
    CHECK_UNSIGNED_OVERFLOW(u);
    if (!c->quiet) ++c->c.next_aux;
    *out = u;
    return 0;
}*/

INLINE Unc_RetVal auxpush(Unc_ParserContext *c, Unc_QOperand o) {
    if (c->aux_n == c->aux_c) {
        Unc_Size z = c->aux_c, nz = c->aux_c + 8;
        Unc_QOperand *naux =
                TMREALLOC(Unc_QOperand, c->c.alloc, 0, c->aux, z, nz);
        if (!naux) return UNCIL_ERR(MEM);
        c->aux = naux;
        c->aux_c = nz;
    }
    c->aux[c->aux_n++] = o;
    return 0;
}

INLINE void auxpop(Unc_ParserContext *c, Unc_QOperand *o) {
    ASSERT(c->aux_n > 0);
    *o = c->aux[--c->aux_n];
}

INLINE Unc_RetVal plbpush(Unc_ParserContext *c, Unc_Size s) {
    if (c->plb_n == c->plb_c) {
        Unc_Size z = c->plb_c, nz = c->plb_c + 8;
        Unc_Size *nplb = TMREALLOC(Unc_Size, c->c.alloc, 0, c->plb, z, nz);
        if (!nplb) return UNCIL_ERR(MEM);
        c->plb = nplb;
        c->plb_c = nz;
    }
    c->plb[c->plb_n++] = s;
    return 0;
}

INLINE int plbhas(Unc_ParserContext *c) {
    return c->plb_n > 0;
}

INLINE Unc_Size plbpeek(Unc_ParserContext *c) {
    ASSERT(c->plb_n > 0);
    return c->plb[c->plb_n - 1];
}

INLINE void plbpop(Unc_ParserContext *c) {
    ASSERT(c->plb_n > 0);
    --c->plb_n;
}

/* actually reads op 0 rather than writing it? */
int unc0_qcode_isread0op(byte op) {
    switch (op) {
    case UNC_QINSTR_OP_SATTR:
    case UNC_QINSTR_OP_SINDX:
    case UNC_QINSTR_OP_SPUB:
    case UNC_QINSTR_OP_SBIND:
    case UNC_QINSTR_OP_IFF:
    case UNC_QINSTR_OP_IFT:
    case UNC_QINSTR_OP_EXIT1:
        return 1;
    default:
        return 0;
    }
}

/* writes op 0? */
int unc0_qcode_ismov(byte op) {
    if (unc0_qcode_isread0op(op)) return 0;
    switch (op) {
    case UNC_QINSTR_OP_MOV:
    case UNC_QINSTR_OP_GPUB:
    case UNC_QINSTR_OP_GBIND:
    case UNC_QINSTR_OP_MLIST:
    case UNC_QINSTR_OP_NDICT:
        return 1;
    default:
        return 0;
    }
}

/* writes op 0? */
int unc0_qcode_iswrite0op(byte op) {
    if (unc0_qcode_isread0op(op)) return 0;
    switch (op) {
    case UNC_QINSTR_OP_DELETE:
    case UNC_QINSTR_OP_EXPUSH:
    case UNC_QINSTR_OP_EXPOP:
    case UNC_QINSTR_OP_PUSHF:
    case UNC_QINSTR_OP_POPF:
        return 0;
    default:
        return 1;
    }
}

int unc0_qcode_isjump(byte op) {
    switch (op) {
    case UNC_QINSTR_OP_JMP:
    case UNC_QINSTR_OP_IFF:
    case UNC_QINSTR_OP_IFT:
    case UNC_QINSTR_OP_EXPUSH:
        return 1;
    default:
        return 0;
    }
}

int unc0_qcode_isexit(byte op) {
    switch (op) {
    case UNC_QINSTR_OP_END:
    case UNC_QINSTR_OP_EXIT0:
    case UNC_QINSTR_OP_EXIT1:
        return 1;
    default:
        return 0;
    }
}

int unc0_qcode_getjumpd(byte op) {
    switch (op) {
    case UNC_QINSTR_OP_JMP:
    case UNC_QINSTR_OP_IFF:
    case UNC_QINSTR_OP_IFT:
    case UNC_QINSTR_OP_EXPUSH:
        return 1;
    case UNC_QINSTR_OP_INEXT:
    case UNC_QINSTR_OP_INEXTS:
        return 2;
    default:
        return 0;
    }
}

static int isfstackop(byte op) {
    switch (op) {
    case UNC_QINSTR_OP_FCALL:
    case UNC_QINSTR_OP_DCALL:
    case UNC_QINSTR_OP_SPREAD:
    case UNC_QINSTR_OP_INEXTS:
        return 1;
    default:
        return 0;
    }
}

static int isexitop(byte op) {
    switch (op) {
    case UNC_QINSTR_OP_END:
    case UNC_QINSTR_OP_EXIT0:
    case UNC_QINSTR_OP_EXIT1:
        return 1;
    default:
        return 0;
    }
}

int unc0_qcode_operandcount(byte op) {
    switch (op) {
    case UNC_QINSTR_OP_DELETE:     return 0;
    case UNC_QINSTR_OP_MOV:        return 2;
    case UNC_QINSTR_OP_ADD:        return 3;
    case UNC_QINSTR_OP_SUB:        return 3;
    case UNC_QINSTR_OP_MUL:        return 3;
    case UNC_QINSTR_OP_DIV:        return 3;
    case UNC_QINSTR_OP_IDIV:       return 3;
    case UNC_QINSTR_OP_MOD:        return 3;
    case UNC_QINSTR_OP_SHL:        return 3;
    case UNC_QINSTR_OP_SHR:        return 3;
    case UNC_QINSTR_OP_AND:        return 3;
    case UNC_QINSTR_OP_OR:         return 3;
    case UNC_QINSTR_OP_XOR:        return 3;
    case UNC_QINSTR_OP_CEQ:        return 3;
    case UNC_QINSTR_OP_CLT:        return 3;
    case UNC_QINSTR_OP_JMP:        return -2;
    case UNC_QINSTR_OP_IFT:        return 2;
    case UNC_QINSTR_OP_IFF:        return 2;
    case UNC_QINSTR_OP_LNOT:       return 2;
    case UNC_QINSTR_OP_PUSH:       return 2;
    case UNC_QINSTR_OP_UPOS:       return 2;
    case UNC_QINSTR_OP_UNEG:       return 2;
    case UNC_QINSTR_OP_UXOR:       return 2;
    case UNC_QINSTR_OP_EXPUSH:     return -2;
    case UNC_QINSTR_OP_EXPOP:      return 0;
    case UNC_QINSTR_OP_GATTR:      return 3;
    case UNC_QINSTR_OP_GATTRQ:     return 3;
    case UNC_QINSTR_OP_SATTR:      return 3;
    case UNC_QINSTR_OP_GINDX:      return 3;
    case UNC_QINSTR_OP_GINDXQ:     return 3;
    case UNC_QINSTR_OP_SINDX:      return 3;
    case UNC_QINSTR_OP_PUSHF:      return 0;
    case UNC_QINSTR_OP_POPF:       return 0;
    case UNC_QINSTR_OP_FCALL:      return 2;
    case UNC_QINSTR_OP_GPUB:       return 2;
    case UNC_QINSTR_OP_SPUB:       return 2;
    case UNC_QINSTR_OP_CAT:        return 3;
    case UNC_QINSTR_OP_IITER:      return 2;
    case UNC_QINSTR_OP_INEXT:      return 3;
    case UNC_QINSTR_OP_FMAKE:      return 2;
    case UNC_QINSTR_OP_MLIST:      return 1;
    case UNC_QINSTR_OP_NDICT:      return 1;
    case UNC_QINSTR_OP_GBIND:      return 2;
    case UNC_QINSTR_OP_SBIND:      return 2;
    case UNC_QINSTR_OP_SPREAD:     return 2;
    case UNC_QINSTR_OP_MLISTP:     return 3;
    case UNC_QINSTR_OP_STKEQ:      return -2;
    case UNC_QINSTR_OP_STKGE:      return -2;
    case UNC_QINSTR_OP_GATTRF:     return 3;
    case UNC_QINSTR_OP_INEXTS:     return -3;
    case UNC_QINSTR_OP_FBIND:      return 3;
    case UNC_QINSTR_OP_NOP:        return 0;
    case UNC_QINSTR_OP_DPUB:       return -2;
    case UNC_QINSTR_OP_DATTR:      return -3;
    case UNC_QINSTR_OP_DINDX:      return -3;
    case UNC_QINSTR_OP_WPUSH:      return 0;
    case UNC_QINSTR_OP_WPOP:       return 0;
    case UNC_QINSTR_OP_PUSHW:      return 2;
    case UNC_QINSTR_OP_DCALL:      return 3;
    case UNC_QINSTR_OP_DTAIL:      return 3;
    case UNC_QINSTR_OP_EXIT0:      return 0;
    case UNC_QINSTR_OP_EXIT1:      return 1;
    case UNC_QINSTR_OP_END:        return 0;
    }
    return 0;
}

#define SETLABEL(c, s) (c->lb[s] = c->cd_n)

#define QOPER_NONE() UNC_QOPER_TYPE_NONE, qdatanone()
#define QOPER_TMP(r) UNC_QOPER_TYPE_TMP, qdataz(r)
#define QOPER_LOCAL(r) UNC_QOPER_TYPE_LOCAL, qdataz(r)
#define QOPER_EXHALE(r) UNC_QOPER_TYPE_EXHALE, qdataz(r)
#define QOPER_INHALE(r) UNC_QOPER_TYPE_INHALE, qdataz(r)
#define QOPER_PUBLIC(r) UNC_QOPER_TYPE_PUBLIC, qdataz(r)
#define QOPER_INT(r) UNC_QOPER_TYPE_INT, qdatai(r)
#define QOPER_FLOAT(r) UNC_QOPER_TYPE_FLOAT, qdataf(r)
#define QOPER_NULL() UNC_QOPER_TYPE_NULL, qdatanone()
#define QOPER_STR(r) UNC_QOPER_TYPE_STR, qdataz(r)
#define QOPER_JUMP(r) UNC_QOPER_TYPE_JUMP, qdataz(r)
#define QOPER_STACK(r) UNC_QOPER_TYPE_STACK, qdataz(r)
#define QOPER_IDENT(r) UNC_QOPER_TYPE_IDENT, qdataz(r)
#define QOPER_TRUE() UNC_QOPER_TYPE_TRUE, qdatanone()
#define QOPER_FALSE() UNC_QOPER_TYPE_FALSE, qdatanone()
#define QOPER_FUNCTION(r) UNC_QOPER_TYPE_FUNCTION, qdataz(r)
#define QOPER_UNSIGN(r) UNC_QOPER_TYPE_UNSIGN, qdataz(r)
#define QOPER_WSTACK() UNC_QOPER_TYPE_WSTACK, qdatanone()

#define QOPER_STRIDENT(r) UNC_QOPER_TYPE_STRIDENT, qdataz(r)

#define QOPER_STACK_TO() QOPER_STACK(0)

INLINE Unc_QOperand makeoperand(byte t, union Unc_QInstr_Data d) {
    Unc_QOperand q;
    q.data = d;
    q.type = t;
    return q;
}

INLINE void configure2x(Unc_ParserContext *c, byte op,
        byte t1, union Unc_QInstr_Data d1) {
    c->next.op = op;
    c->next.o0type = 0, c->next.o1type = t1, c->next.o2type = 0;
    c->next.o0data = 0;
    c->next.o1data = d1;
    c->next.o2data = qdatanone();
}

INLINE void configure3x(Unc_ParserContext *c, byte op,
        byte t1, union Unc_QInstr_Data d1, byte t2, union Unc_QInstr_Data d2) {
    c->next.op = op;
    c->next.o0type = 0, c->next.o1type = t1, c->next.o2type = t2;
    c->next.o0data = 0;
    c->next.o1data = d1;
    c->next.o2data = d2;
}

INLINE Unc_RetVal pushins(Unc_ParserContext *c, Unc_QInstr *instr) {
    if (!c->quiet) {
        if (c->cd_n >= c->cd_c) {
            Unc_Size z = c->cd_c, nz = z + 32;
            Unc_QInstr *np = TMREALLOC(Unc_QInstr, c->c.alloc, 0,
                                       c->cd, z, nz);
            if (!np)
                return UNCIL_ERR(MEM);
            c->cd_c = nz;
            c->cd = np;
        }
        c->cd[c->cd_n] = *instr;
        c->cd[c->cd_n].lineno = c->out.lineno;
        ++c->cd_n;
        c->fence = 0;
        c->xfence = 0;
    }
    return 0;
}

int unc0_qcode_isopreg(int t) {
    switch (t) {
    case UNC_QOPER_TYPE_TMP:
    case UNC_QOPER_TYPE_LOCAL:
        return 1;
    default:
        return 0;
    }
}

int unc0_qcode_isoplit(int t) {
    switch (t) {
    case UNC_QOPER_TYPE_NULL:
    case UNC_QOPER_TYPE_INT:
    case UNC_QOPER_TYPE_FLOAT:
    case UNC_QOPER_TYPE_STR:
        return 1;
    default:
        return 0;
    }
}

INLINE int isintern(int t) {
    switch (t) {
    case UNC_QOPER_TYPE_TMP:
    case UNC_QOPER_TYPE_LOCAL:
    case UNC_QOPER_TYPE_INT:
    case UNC_QOPER_TYPE_FLOAT:
    case UNC_QOPER_TYPE_NULL:
    case UNC_QOPER_TYPE_STR:
        return 1;
    default:
        return 0;
    }
}

INLINE int fitsliteral(int t, union Unc_QInstr_Data data) {
    return t == UNC_QOPER_TYPE_INT && -32768 <= data.ui && data.ui <= 32767;
}

INLINE Unc_RetVal bewvar(Unc_ParserContext *c, Unc_QOperand *dst) {
    switch (dst->type) {
    case UNC_QOPER_TYPE_TMP:
    case UNC_QOPER_TYPE_LOCAL:
    case UNC_QOPER_TYPE_PUBLIC:
    case UNC_QOPER_TYPE_EXHALE:
    case UNC_QOPER_TYPE_INHALE:
        return 0;
    default:
        return UNCIL_ERR(SYNTAX);
    }
}

INLINE Unc_RetVal emitre(Unc_ParserContext *c, byte t0,
                         union Unc_QInstr_Data d0);

static Unc_RetVal push(Unc_ParserContext *c);

static Unc_RetVal retarget(Unc_ParserContext *c, byte t0,
                           union Unc_QInstr_Data d0) {
    if (!c->next.op)
        return 0;

    switch (t0) {
    case UNC_QOPER_TYPE_STACK:
        ASSERT(d0.o == 0);
        if (unc0_qcode_isread0op(c->next.op)) {
            NEVER();
        } else if (c->next.op == UNC_QINSTR_OP_MOV
                            && unc0_qcode_isopreg(c->next.o0type)) {
            c->next.op = UNC_QINSTR_OP_PUSH;
        } else if (!isfstackop(c->next.op)) {
            Unc_Dst u;
            MUST(tmpalloc(c, &u));
            MUST(emitre(c, QOPER_TMP(u)));
            c->next.op = UNC_QINSTR_OP_PUSH;
            c->next.o1type = c->next.o0type;
            c->next.o2type = 0;
            c->next.o1data = qdataz(c->next.o0data);
            c->next.o2data = qdatanone();
            c->next.o0type = UNC_QOPER_TYPE_STACK;
            c->next.o0data = 0;
            return 0;
        }
    case UNC_QOPER_TYPE_NONE:
        c->next.o0type = t0;
        c->next.o0data = 0;
        break;
    case UNC_QOPER_TYPE_TMP:
    case UNC_QOPER_TYPE_LOCAL:
        c->next.o0type = t0;
        if (d0.o > UNC_DST_MAX)
            return UNCIL_ERR(SYNTAX_TOODEEP);
        c->next.o0data = (Unc_Dst)d0.o;
        break;
    case UNC_QOPER_TYPE_INT:
    case UNC_QOPER_TYPE_FLOAT:
    case UNC_QOPER_TYPE_NULL:
    case UNC_QOPER_TYPE_STR:
    case UNC_QOPER_TYPE_FALSE:
    case UNC_QOPER_TYPE_TRUE:
    case UNC_QOPER_TYPE_UNSIGN:
        /* always illegal */
        NEVER_();
        return UNCIL_ERR(SYNTAX);
    case UNC_QOPER_TYPE_PUBLIC:
        /* split it up! */
        if (c->next.op != UNC_QINSTR_OP_MOV ||
                        !unc0_qcode_isopreg(c->next.o1type)) {
            Unc_Dst u;
            MUST(tmpalloc(c, &u));
            if (unc0_qcode_isread0op(c->next.op)) {
                Unc_QInstr instr;
                instr.op = UNC_QINSTR_OP_GPUB;
                instr.o0type = UNC_QOPER_TYPE_TMP;
                instr.o1type = t0;
                instr.o2type = 0;
                instr.o0data = u;
                instr.o1data = d0;
                instr.o2data = qdatanone();
                MUST(pushins(c, &instr));
                c->next.o0type = UNC_QOPER_TYPE_TMP;
                c->next.o0data = u;
            } else {
                c->next.o0type = UNC_QOPER_TYPE_TMP;
                c->next.o0data = u;
                MUST(push(c));
                c->next.op = UNC_QINSTR_OP_SPUB;
                c->next.o1type = t0;
                c->next.o2type = 0;
                c->next.o1data = d0;
                c->next.o2data = qdatanone();
            }
        } else {
            c->next.op = UNC_QINSTR_OP_SPUB;
            c->next.o0type = c->next.o1type;
            c->next.o0data = c->next.o1data.o;
            c->next.o1type = t0;
            c->next.o1data = d0;
        }
        break;
    case UNC_QOPER_TYPE_EXHALE:
    case UNC_QOPER_TYPE_INHALE:
        /* split it up! */
        if (c->next.op != UNC_QINSTR_OP_MOV ||
                        !unc0_qcode_isopreg(c->next.o1type)) {
            Unc_Dst u;
            MUST(tmpalloc(c, &u));
            if (unc0_qcode_isread0op(c->next.op)) {
                Unc_QInstr instr;
                instr.op = UNC_QINSTR_OP_GBIND;
                instr.o0type = UNC_QOPER_TYPE_TMP;
                instr.o1type = t0;
                instr.o2type = 0;
                instr.o0data = u;
                instr.o1data = d0;
                instr.o2data = qdatanone();
                MUST(pushins(c, &instr));
                c->next.o0type = UNC_QOPER_TYPE_TMP;
                c->next.o0data = u;
            } else {
                c->next.o0type = UNC_QOPER_TYPE_TMP;
                c->next.o0data = u;
                MUST(push(c));
                c->next.op = UNC_QINSTR_OP_SBIND;
                c->next.o1type = t0;
                c->next.o2type = 0;
                c->next.o1data = d0;
                c->next.o2data = qdatanone();
            }
        } else {
            c->next.op = UNC_QINSTR_OP_SBIND;
            c->next.o0type = c->next.o1type;
            c->next.o0data = c->next.o1data.o;
            c->next.o1type = t0;
            c->next.o1data = d0;
        }
        break;
    case UNC_QOPER_TYPE_WSTACK:
        ASSERT(c->next.op == UNC_QINSTR_OP_PUSHW);
        c->next.o0type = t0;
        c->next.o0data = 0;
        break;
    }
    return 0;
}

/* wrap source operand in temp register, temporary index */
static Unc_RetVal wraptreg(Unc_ParserContext *c, Unc_QOperand *op,
                           Unc_Dst tr) {
    switch (op->type) {
    case UNC_QOPER_TYPE_TMP:
        break;
    case UNC_QOPER_TYPE_LOCAL:
    {
        Unc_QInstr instr;
        instr.op = UNC_QINSTR_OP_MOV;
        instr.o0type = UNC_QOPER_TYPE_TMP;
        instr.o1type = op->type;
        instr.o2type = UNC_QOPER_TYPE_NONE;
        instr.o0data = tr;
        instr.o1data = op->data;
        instr.o2data = qdatanone();
        MUST(pushins(c, &instr));
        *op = makeoperand(QOPER_TMP(tr));
        break;
    }
    case UNC_QOPER_TYPE_PUBLIC:
    {
        Unc_QInstr instr;
        instr.op = UNC_QINSTR_OP_GPUB;
        instr.o0type = UNC_QOPER_TYPE_TMP;
        instr.o1type = op->type;
        instr.o2type = UNC_QOPER_TYPE_NONE;
        instr.o0data = tr;
        instr.o1data = op->data;
        instr.o2data = qdatanone();
        MUST(pushins(c, &instr));
        *op = makeoperand(QOPER_TMP(tr));
        break;
    }
    case UNC_QOPER_TYPE_EXHALE:
    case UNC_QOPER_TYPE_INHALE:
    {
        Unc_QInstr instr;
        instr.op = UNC_QINSTR_OP_GBIND;
        instr.o0type = UNC_QOPER_TYPE_TMP;
        instr.o1type = op->type;
        instr.o2type = UNC_QOPER_TYPE_NONE;
        instr.o0data = tr;
        instr.o1data = op->data;
        instr.o2data = qdatanone();
        MUST(pushins(c, &instr));
        *op = makeoperand(QOPER_TMP(tr));
        break;
    }
    case UNC_QOPER_TYPE_INT:
    case UNC_QOPER_TYPE_FLOAT:
    case UNC_QOPER_TYPE_NULL:
    case UNC_QOPER_TYPE_STR:
    case UNC_QOPER_TYPE_STRIDENT:
    case UNC_QOPER_TYPE_STACK:
    case UNC_QOPER_TYPE_FALSE:
    case UNC_QOPER_TYPE_TRUE:
    {
        Unc_QInstr instr;
        instr.op = UNC_QINSTR_OP_MOV;
        instr.o0type = UNC_QOPER_TYPE_TMP;
        instr.o1type = op->type;
        instr.o2type = UNC_QOPER_TYPE_NONE;
        instr.o0data = tr;
        instr.o1data = op->data;
        instr.o2data = qdatanone();
        MUST(pushins(c, &instr));
        *op = makeoperand(QOPER_TMP(tr));
        break;
    }
    case UNC_QOPER_TYPE_BINDABLE:
        /* bindables should be bound, but in quiet mode it's fine if
           we don't do that yet */
        ASSERT(c->quiet);
        break;
    default:
        NEVER();
    }
    return 0;
}

/* wrap source operand in register, temporary index */
static Unc_RetVal wrapreg(Unc_ParserContext *c, Unc_QOperand *op, Unc_Dst tr) {
    switch (op->type) {
    case UNC_QOPER_TYPE_TMP:
    case UNC_QOPER_TYPE_LOCAL:
        return 0;
    default:
        return wraptreg(c, op, tr);
    }
}

INLINE void seto1(Unc_ParserContext *c, Unc_QOperand o) {
    c->next.o1type = o.type;
    c->next.o1data = o.data;
}

INLINE void seto2(Unc_ParserContext *c, Unc_QOperand o) {
    c->next.o2type = o.type;
    c->next.o2data = o.data;
}

static Unc_RetVal push(Unc_ParserContext *c) {
    switch (c->next.op) {
    case UNC_QINSTR_OP_DELETE:
        break;
    case UNC_QINSTR_OP_MOV:
        if (c->next.o1type == UNC_QOPER_TYPE_PUBLIC) {
            ASSERT(unc0_qcode_isopreg(c->next.o0type));
            c->next.op = UNC_QINSTR_OP_GPUB;
            break;
        } else if (c->next.o1type == UNC_QOPER_TYPE_INHALE
                || c->next.o1type == UNC_QOPER_TYPE_EXHALE) {
            ASSERT(unc0_qcode_isopreg(c->next.o0type));
            c->next.op = UNC_QINSTR_OP_GBIND;
            break;
        } else if (!unc0_qcode_isopreg(c->next.o0type)) {
            if (!unc0_qcode_isopreg(c->next.o1type)) {
                Unc_Dst tr;
                Unc_QOperand o = makeoperand(c->next.o1type, c->next.o1data);
                MUST(tmpalloc(c, &tr));
                MUST(wrapreg(c, &o, tr));
                seto1(c, o);
            }
        }
        /* try merging into previous operation if we have moving TMP=>TMP */
        if (!c->fence && c->cd_n && c->next.o0type == UNC_QOPER_TYPE_TMP
                && c->next.o1type == UNC_QOPER_TYPE_TMP
                && unc0_qcode_iswrite0op(c->cd[c->cd_n - 1].op)
                && c->cd[c->cd_n - 1].o0type == UNC_QOPER_TYPE_TMP
                && c->cd[c->cd_n - 1].o0data == c->next.o1data.o) {
            c->cd[c->cd_n - 1].o0data = c->next.o0data;
            return 0;
        }
        break;
    /* binary OP */
    case UNC_QINSTR_OP_ADD:
    case UNC_QINSTR_OP_SUB:
    case UNC_QINSTR_OP_MUL:
    case UNC_QINSTR_OP_DIV:
    case UNC_QINSTR_OP_IDIV:
    case UNC_QINSTR_OP_MOD:
    case UNC_QINSTR_OP_SHL:
    case UNC_QINSTR_OP_SHR:
    case UNC_QINSTR_OP_AND:
    case UNC_QINSTR_OP_OR:
    case UNC_QINSTR_OP_XOR:
    case UNC_QINSTR_OP_CEQ:
    case UNC_QINSTR_OP_CLT:
        {
            Unc_QOperand o;
            if (!unc0_qcode_isopreg(c->next.o1type)
                       && !fitsliteral(c->next.o1type, c->next.o1data)) {
                Unc_Dst tr;
                o = makeoperand(c->next.o1type, c->next.o1data);
                MUST(tmpalloc(c, &tr));
                MUST(wrapreg(c, &o, tr));
                seto1(c, o);
            }
            if (!unc0_qcode_isopreg(c->next.o2type)
                       && !fitsliteral(c->next.o2type, c->next.o2data)) {
                Unc_Dst tr;
                o = makeoperand(c->next.o2type, c->next.o2data);
                MUST(tmpalloc(c, &tr));
                MUST(wrapreg(c, &o, tr));
                seto2(c, o);
            }
        }
        break;
    case UNC_QINSTR_OP_PUSH:
        ASSERT(unc0_qcode_isopreg(c->next.o1type));
        /* try merging into previous operation if we were moving reg=>tmp
                                                     and now pushing tmp */
        if (!c->fence && c->cd_n
                && c->cd[c->cd_n - 1].op == UNC_QINSTR_OP_MOV
                && c->cd[c->cd_n - 1].o0type == c->next.o1type
                && c->cd[c->cd_n - 1].o0data == c->next.o1data.o
                && unc0_qcode_isopreg(c->cd[c->cd_n - 1].o1type)) {
            c->next.o1type = c->cd[c->cd_n - 1].o1type;
            c->next.o1data = c->cd[c->cd_n - 1].o1data;
            --c->cd_n;
        }
        break;
    case UNC_QINSTR_OP_UPOS:
    case UNC_QINSTR_OP_UNEG:
    case UNC_QINSTR_OP_UXOR:
        {
            Unc_QOperand o;
            if (!unc0_qcode_isopreg(c->next.o1type)
                       && !fitsliteral(c->next.o1type, c->next.o1data)) {
                Unc_Dst tr;
                o = makeoperand(c->next.o1type, c->next.o1data);
                MUST(tmpalloc(c, &tr));
                MUST(wrapreg(c, &o, tr));
                seto1(c, o);
            }
        }
        break;
    case UNC_QINSTR_OP_GATTR:
    case UNC_QINSTR_OP_GATTRQ:
    case UNC_QINSTR_OP_GATTRF:
    case UNC_QINSTR_OP_SATTR:
    case UNC_QINSTR_OP_GINDX:
    case UNC_QINSTR_OP_GINDXQ:
    case UNC_QINSTR_OP_SINDX:
    case UNC_QINSTR_OP_LNOT:
        {
            Unc_QOperand o;
            if (!unc0_qcode_isopreg(c->next.o1type)) {
                Unc_Dst tr;
                o = makeoperand(c->next.o1type, c->next.o1data);
                MUST(tmpalloc(c, &tr));
                MUST(wrapreg(c, &o, tr));
                seto1(c, o);
            }
        }
        break;
    case UNC_QINSTR_OP_CAT:
        {
            Unc_QOperand o;
            if (!unc0_qcode_isopreg(c->next.o1type)) {
                Unc_Dst tr;
                o = makeoperand(c->next.o1type, c->next.o1data);
                MUST(tmpalloc(c, &tr));
                MUST(wrapreg(c, &o, tr));
                seto1(c, o);
            }
            if (!unc0_qcode_isopreg(c->next.o2type)) {
                Unc_Dst tr;
                o = makeoperand(c->next.o2type, c->next.o2data);
                MUST(tmpalloc(c, &tr));
                MUST(wrapreg(c, &o, tr));
                seto2(c, o);
            }
        }
        break;
    case UNC_QINSTR_OP_IFT:
    case UNC_QINSTR_OP_IFF:
        ASSERT(unc0_qcode_isopreg(c->next.o0type));
        if (!c->fence && c->cd_n && c->next.o0type == UNC_QOPER_TYPE_TMP
                && c->cd[c->cd_n - 1].op == UNC_QINSTR_OP_LNOT
                && c->cd[c->cd_n - 1].o0type == UNC_QOPER_TYPE_TMP
                && c->cd[c->cd_n - 1].o0data == c->next.o0data
                && unc0_qcode_isopreg(c->cd[c->cd_n - 1].o1type)) {
            c->next.o0type = c->cd[c->cd_n - 1].o1type;
            c->next.o0data = c->cd[c->cd_n - 1].o1data.o;
            --c->cd_n;
            if (c->next.op != UNC_QINSTR_OP_IFF)
                c->next.op = UNC_QINSTR_OP_IFF;
            else
                c->next.op = UNC_QINSTR_OP_IFT;
        }
    case UNC_QINSTR_OP_EXPUSH:
    case UNC_QINSTR_OP_JMP:
        ASSERT(c->next.o1type == UNC_QOPER_TYPE_JUMP);
        break;
    case UNC_QINSTR_OP_DCALL:
        ASSERT(c->next.o2type == UNC_QOPER_TYPE_UNSIGN);
    case UNC_QINSTR_OP_FCALL:
        ASSERT(unc0_qcode_isopreg(c->next.o0type)
                || c->next.o0type == UNC_QOPER_TYPE_STACK);
        break;
    case UNC_QINSTR_OP_GPUB:
    case UNC_QINSTR_OP_GBIND:
        ASSERT(unc0_qcode_isopreg(c->next.o0type));
        break;
    case UNC_QINSTR_OP_SPUB:
    case UNC_QINSTR_OP_SBIND:
        ASSERT(unc0_qcode_isopreg(c->next.o0type));
        break;
    case UNC_QINSTR_OP_FMAKE:
        ASSERT(c->next.o1type == UNC_QOPER_TYPE_FUNCTION);
        break;
    case UNC_QINSTR_OP_MLIST:
        ASSERT(unc0_qcode_isopreg(c->next.o0type));
        break;
    case UNC_QINSTR_OP_NDICT:
        ASSERT(unc0_qcode_isopreg(c->next.o0type));
        break;
    default:
        break;
    }
    return pushins(c, &c->next);
}

INLINE Unc_RetVal emit0(Unc_ParserContext *c, byte op) {
    c->next.op = op;
    c->next.o0type = 0, c->next.o1type = 0, c->next.o2type = 0;
    c->next.o0data = 0;
    c->next.o1data = qdatanone();
    c->next.o2data = qdatanone();
    return push(c);
}

INLINE Unc_RetVal emit1(Unc_ParserContext *c, byte op,
                        byte t0, union Unc_QInstr_Data d0) {
    c->next.op = op;
    c->next.o1type = 0, c->next.o2type = 0;
    c->next.o1data = qdatanone();
    c->next.o2data = qdatanone();
    MUST(retarget(c, t0, d0));
    return push(c);
}

INLINE Unc_RetVal emit2(Unc_ParserContext *c, byte op,
                        byte t0, union Unc_QInstr_Data d0,
                        byte t1, union Unc_QInstr_Data d1) {
    c->next.op = op;
    c->next.o1type = t1, c->next.o2type = 0;
    c->next.o1data = d1;
    c->next.o2data = qdatanone();
    MUST(retarget(c, t0, d0));
    return push(c);
}

INLINE Unc_RetVal emit3(Unc_ParserContext *c, byte op,
                        byte t0, union Unc_QInstr_Data d0,
                        byte t1, union Unc_QInstr_Data d1,
                        byte t2, union Unc_QInstr_Data d2) {
    c->next.op = op;
    c->next.o1type = t1, c->next.o2type = t2;
    c->next.o1data = d1;
    c->next.o2data = d2;
    MUST(retarget(c, t0, d0));
    return push(c);
}

/*
INLINE Unc_RetVal emitre0(Unc_ParserContext *c, byte t0, Unc_Dst d0) {
    c->next.o0type = t0;
    c->next.o0data = d0;
    return push(c);
}
*/

INLINE Unc_RetVal emitre(Unc_ParserContext *c, byte t0,
                         union Unc_QInstr_Data d0) {
    MUST(retarget(c, t0, d0));
    return push(c);
}

INLINE Unc_RetVal unwrapwr(Unc_ParserContext *c, byte t0,
                           union Unc_QInstr_Data d0) {
    if (c->cd_n && unc0_qcode_iswrite0op(c->cd[c->cd_n - 1].op)
                && c->cd[c->cd_n - 1].o0type == UNC_QOPER_TYPE_TMP
                && c->cd[c->cd_n - 1].o0type == t0
                && c->cd[c->cd_n - 1].o0data == d0.o) {
        c->next = c->cd[--c->cd_n];
    }
    return 0;
}

INLINE Unc_RetVal unwrapmov(Unc_ParserContext *c, byte t0,
                            union Unc_QInstr_Data d0) {
    if (c->cd_n && unc0_qcode_ismov(c->cd[c->cd_n - 1].op)
                && c->cd[c->cd_n - 1].o0type == UNC_QOPER_TYPE_TMP
                && c->cd[c->cd_n - 1].o0type == t0
                && c->cd[c->cd_n - 1].o0data == d0.o) {
        c->next = c->cd[--c->cd_n];
    }
    return 0;
}

INLINE Unc_QInstr make_gbind_exh(Unc_Dst u, Unc_Dst tmp, Unc_Size lineno) {
    Unc_QInstr in;
    in.op = UNC_QINSTR_OP_GBIND;
    in.o0type = UNC_QOPER_TYPE_TMP;
    in.o1type = UNC_QOPER_TYPE_EXHALE;
    in.o2type = UNC_QOPER_TYPE_NONE;
    in.o0data = tmp;
    in.o1data = qdataz(u);
    in.o2data = qdataz(0);
    in.lineno = lineno;
    return in;
}

INLINE Unc_QInstr make_sbind_exh(Unc_Dst u, Unc_Dst tmp, Unc_Size lineno) {
    Unc_QInstr in;
    in.op = UNC_QINSTR_OP_SBIND;
    in.o0type = UNC_QOPER_TYPE_TMP;
    in.o1type = UNC_QOPER_TYPE_EXHALE;
    in.o2type = UNC_QOPER_TYPE_NONE;
    in.o0data = tmp;
    in.o1data = qdataz(u);
    in.o2data = qdataz(0);
    in.lineno = lineno;
    return in;
}

static Unc_RetVal dobloat(Unc_Allocator *alloc, Unc_ParserParent *p,
                          Unc_Size i, Unc_Size qu, Unc_Size qd) {
    Unc_Size add = qu + qd, rem;
    if (*p->cd_n + add > *p->cd_c) {
        Unc_Size z = *p->cd_c, nz = z + 32;
        Unc_QInstr *np = TMREALLOC(Unc_QInstr, alloc, 0, *p->cd, z, nz);
        if (!np)
            return UNCIL_ERR(MEM);
        *p->cd_c = nz;
        *p->cd = np;
    }
    rem = *p->cd_n - i - 1;
    if (rem) {
        Unc_QInstr *os, *ns;
        os = *p->cd + i + 1;
        ns = os + add;
        unc0_memmove(ns, os, rem * sizeof(Unc_QInstr));
    }
    (*p->cd)[i + qu] = (*p->cd)[i];
    *p->cd_n += add;
    return 0;
}

static Unc_RetVal loctoexh_code(Unc_Allocator *alloc, Unc_ParserParent *p,
                                Unc_Dst l, Unc_Dst u, int shift) {
    Unc_Size i, e = *p->cd_n;
    Unc_QInstr *qi, *qc = *p->cd;
    Unc_Dst tmp = *p->tmphigh, tr, th = tmp;
    for (i = 0; i < e; ++i) {
        int o, oo;
        Unc_Size qu = 0, qd = 0;
        tr = tmp;
        qi = &qc[i];
        o = oo = unc0_qcode_operandcount(qi->op);
        if (o > 0 && qi->o0type == UNC_QOPER_TYPE_LOCAL) {
            if (qi->o0data == l) {
                if (unc0_qcode_isread0op(qi->op))
                    ++qu;
                else
                    ++qd;
            } else if (shift && qi->o0data > l) {
                --qi->o0data;
            }
        } else if (o < 0)
            o = -o;
        if (o > 1 && qi->o1type == UNC_QOPER_TYPE_LOCAL) {
            if (qi->o1data.o == l) {
                ++qu;
            } else if (shift && qi->o1data.o > l) {
                --qi->o1data.o;
            }
        }
        if (o > 2 && qi->o2type == UNC_QOPER_TYPE_LOCAL) {
            if (qi->o2data.o == l) {
                ++qu;
            } else if (shift && qi->o2data.o > l) {
                --qi->o2data.o;
            }
        }

        if (!qu && !qd) continue;

        {
            Unc_RetVal err = dobloat(alloc, p, i, qu, qd);
            if (err) return err;
            qc = *p->cd;
        }
        
        o = oo;
        qi += qu;
        qu += i;
        qd = qu + 1;
        if (o > 0 && qi->o0type == UNC_QOPER_TYPE_LOCAL) {
            if (qi->o0data == l) {
                if (unc0_qcode_isread0op(qi->op))
                    qc[qu++] = make_gbind_exh(u, tr, qi->lineno);
                else
                    qc[qd++] = make_sbind_exh(u, tr, qi->lineno);
                qi->o0type = UNC_QOPER_TYPE_TMP;
                qi->o0data = tr++;
            }
        } else if (o < 0)
            o = -o;
        if (o > 1 && qi->o1type == UNC_QOPER_TYPE_LOCAL) {
            if (qi->o1data.o == l) {
                qc[qu++] = make_gbind_exh(u, tr, qi->lineno);
                qi->o1type = UNC_QOPER_TYPE_TMP;
                qi->o1data.o = tr++;
            }
        }
        if (o > 2 && qi->o2type == UNC_QOPER_TYPE_LOCAL) {
            if (qi->o2data.o == l) {
                qc[qu++] = make_gbind_exh(u, tr, qi->lineno);
                qi->o2type = UNC_QOPER_TYPE_TMP;
                qi->o2data.o = tr++;
            }
        }
        if (th < tr)
            th = tr;
        i = qd - 1;
        e = *p->cd_n;
    }
    *p->tmphigh = th;
    return 0;
}

/* shift other local variables down */
static Unc_RetVal loctoexh_vars_iter(Unc_Size key, Unc_BTreeRecord *value,
                                     void *u) {
    if (value->first == UNC_QOPER_TYPE_LOCAL && value->second > *(Unc_Dst *)u)
        --value->second;
    return 0;
}

static Unc_RetVal loctoexh_vars(Unc_BTree *b, Unc_Dst l, Unc_Dst u) {
    (void)u;
    return unc0_iterbtreerecords(b, &loctoexh_vars_iter, &l);
}

static Unc_RetVal inhalloc(Unc_ParserContext *c, Unc_Size depth,
                           Unc_Dst *out, Unc_QOperand o) {
    Unc_ParserParent *pp;
    Unc_Dst u;
    ASSERT(depth <= c->pframes_n);
    pp = depth ? &c->pframes[c->pframes_n - depth] : NULL;
    u = pp ? *pp->inhnext : c->inhnext;
    if (!c->quiet) {
        Unc_Size z = pp ? *pp->inhl_c : c->inhl_c;
        CHECK_UNSIGNED_OVERFLOW(u);
        if (u == z) {
            Unc_Size nz = z + 8;
            Unc_QOperand *ninhl = TMREALLOC(Unc_QOperand, c->c.alloc, 0,
                        pp ? *pp->inhl : c->inhl, z, nz);
            if (!ninhl) return UNCIL_ERR(MEM);
            if (pp)
                *pp->inhl = ninhl, *pp->inhl_c = nz;
            else
                c->inhl = ninhl, c->inhl_c = nz;
        }
        if (pp)
            (*pp->inhl)[(*pp->inhnext)++] = o;
        else
            c->inhl[c->inhnext++] = o;
    }
    *out = u;
    return 0;
}

/* convert local to exhale */
static Unc_RetVal dobind(Unc_ParserContext *c, Unc_Size key,
                         Unc_BTreeRecord *rec) {
    Unc_ParserParent *p;
    Unc_BTreeRecord *br, tmp;
    Unc_Dst ex = 0;
    Unc_Size generation = 0, depth = rec->second;

    ASSERT(!c->quiet);
    ASSERT(rec->first == UNC_QOPER_TYPE_BINDABLE);
    ASSERT(c->pframes_n >= depth);
    for (;;) {
        p = depth ? &c->pframes[c->pframes_n - depth] : NULL;
        br = p ? unc0_getbtree(p->book, key) : rec;
        if (generation) {
            if (br->first != UNC_QOPER_TYPE_INHALE) {
                ASSERT(br->first == UNC_QOPER_TYPE_BINDABLE);
                MUST(inhalloc(c, depth, &ex,
                            makeoperand((byte)tmp.first, qdataz(tmp.second))));
                br->first = UNC_QOPER_TYPE_INHALE;
                br->second = ex;
            }
            if (!depth)
                return 0;
        } else if (br->first == UNC_QOPER_TYPE_LOCAL) {
            Unc_Dst u = *p->exhnext, l = (Unc_Dst)br->second;
            ASSERT(!generation);
            CHECK_UNSIGNED_OVERFLOW(u);
            ++(*p->exhnext);
            br->first = UNC_QOPER_TYPE_EXHALE;
            br->second = u;
            if (l < p->argc) {
                MUST(loctoexh_code(c->c.alloc, p, l, u, 0));
                p->arg_exh[l] = u + 1;
            } else {
                --(*p->locnext);
                MUST(loctoexh_code(c->c.alloc, p, l, u, 1));
                loctoexh_vars(p->book, l, u);
            }
        }
        tmp = *br;
        --depth, ++generation;
    }
}

static Unc_RetVal addstr(Unc_Allocator *alloc, byte **buf, Unc_Size *buf_z,
                         Unc_Size n, const byte *c) {
    Unc_Size z = *buf_z;
    Unc_Size l = unc0_utf8patdownl(n, c), ll;
    byte lb[UNC_VLQ_SIZE_MAXLEN];
    Unc_Size ln = unc0_vlqencz(l, sizeof(lb), lb);
    byte *p = unc0_mrealloc(alloc, 0, *buf, z, z + ln + l);
    if (!p) return UNCIL_ERR(MEM);
    unc0_memcpy(p + z, lb, ln);
    ll = unc0_utf8patdown(n, p + z + ln, c);
    ASSERT(l == ll);
    (void)ll;
    *buf = p;
    *buf_z += ln + l;
    return 0;
}

void scanstrs(Unc_Size *out, const byte *in, Unc_Size n, Unc_Size c) {
    const byte *p = in;
    (void)c;
    while (n--) {
        ASSERT(p <= in + c);
        *out++ = p - in;
        p += unc0_strlen((const char *)p) + 1;
    }
}

Unc_RetVal copystrs(Unc_ParserContext *c, int sub) {
    /* check for used but non-saved strings */
    int flag = sub ? STFLAG_USED_SUB : STFLAG_USED_MAIN,
        mask = STFLAG_SAVED | flag;
    Unc_Size i, n = c->lex.st_n;
    for (i = 0; i < n; ++i) {
        if ((c->st_status[i] & mask) == flag) {
            const byte *se = &c->lex.st[c->st_offset[i]];
            const byte *xe = se + unc0_strlen((const char *)se);
            c->st_offset[i] = c->out.st_sz;
            c->st_status[i] |= STFLAG_SAVED;
            MUST(addstr(c->c.alloc, &c->st, &c->out.st_sz, xe - se, se));
        }
    }

    n = c->cd_n;
    for (i = 0; i < n; ++i) {
        int o = unc0_qcode_operandcount(c->cd[i].op);
        if (o == 0) continue;
        if (o < 0) {
            o = -o;
        } else if (c->cd[i].o0type == UNC_QOPER_TYPE_STR) {
            c->cd[i].o0data = c->st_offset[c->cd[i].o0data];
        }
        if (o > 1 && c->cd[i].o1type == UNC_QOPER_TYPE_STR) {
            c->cd[i].o1data.o = c->st_offset[c->cd[i].o1data.o];
        }
        if (o > 2 && c->cd[i].o2type == UNC_QOPER_TYPE_STR) {
            c->cd[i].o2data.o = c->st_offset[c->cd[i].o2data.o];
        }
    }

    return 0;
}

INLINE int ismoveid(byte t) {
    switch (t) {
    case UNC_QOPER_TYPE_PUBLIC:
    case UNC_QOPER_TYPE_IDENT:
    case UNC_QOPER_TYPE_STRIDENT:
        return 1;
    default:
        return 0;
    }
}

Unc_RetVal copyids(Unc_ParserContext *c, int sub) {
    /* check for used but non-saved strings */
    int flag = sub ? STFLAG_USED_SUB : STFLAG_USED_MAIN,
        mask = STFLAG_SAVED | flag;
    Unc_Size i, n = c->lex.id_n;
    for (i = 0; i < n; ++i) {
        if ((c->id_status[i] & mask) == flag) {
            const byte *se = &c->lex.id[c->id_offset[i]];
            const byte *xe = se + unc0_strlen((const char *)se);
            c->id_offset[i] = c->out.st_sz;
            c->id_status[i] |= STFLAG_SAVED;
            MUST(addstr(c->c.alloc, &c->st, &c->out.st_sz, xe - se, se));
        }
    }

    n = c->cd_n;
    for (i = 0; i < n; ++i) {
        int o = unc0_qcode_operandcount(c->cd[i].op);
        if (o == 0) continue;
        if (o < 0) {
            o = -o;
        } else if (ismoveid(c->cd[i].o0type)) {
            c->cd[i].o0data = c->id_offset[c->cd[i].o0data];
            if (c->cd[i].o0type == UNC_QOPER_TYPE_STRIDENT)
                c->cd[i].o0type = UNC_QOPER_TYPE_STR;
        }
        if (o > 1 && ismoveid(c->cd[i].o1type)) {
            c->cd[i].o1data.o = c->id_offset[c->cd[i].o1data.o];
            if (c->cd[i].o1type == UNC_QOPER_TYPE_STRIDENT)
                c->cd[i].o1type = UNC_QOPER_TYPE_STR;
        }
        if (o > 2 && ismoveid(c->cd[i].o2type)) {
            c->cd[i].o2data.o = c->id_offset[c->cd[i].o2data.o];
            if (c->cd[i].o2type == UNC_QOPER_TYPE_STRIDENT)
                c->cd[i].o2type = UNC_QOPER_TYPE_STR;
        }
    }

    return 0;
}

static Unc_RetVal startstack(Unc_ParserContext *c, int always) {
    if (always || !c->pushfs) {
        /* set up stack region */
        Unc_QInstr instr;
        instr.op = UNC_QINSTR_OP_PUSHF;
        instr.o0type = 0;
        instr.o1type = 0;
        instr.o2type = 0;
        instr.o0data = 0;
        instr.o1data = qdatanone();
        instr.o2data = qdatanone();
        MUST(pushins(c, &instr));
        ++c->pushfs;
    }
    return 0;
}

static void endstack(Unc_ParserContext *c) {
    ASSERT(c->pushfs);
    --c->pushfs;
}

static Unc_RetVal dokillstack(Unc_ParserContext *c) {
    endstack(c);
    return emit0(c, UNC_QINSTR_OP_POPF);
}

static Unc_RetVal tostack(Unc_ParserContext *c) {
    switch (c->valtype) {
    case VALTYPE_NONE:
        MUST(startstack(c, 0));
        c->valtype = VALTYPE_STACK;
    case VALTYPE_STACK:
        ASSERT(c->pushfs);
        return 0;
    case VALTYPE_FSTACK:
        if (isfstackop(c->next.op))
            ASSERT(c->pushfs);
    case VALTYPE_HOLD:
        MUST(startstack(c, 0));
        MUST(emitre(c, QOPER_STACK_TO()));
        c->valtype = VALTYPE_STACK;
        ASSERT(c->pushfs);
        return 0;
    default:
        NEVER();
    }
}

static Unc_RetVal holdval(Unc_ParserContext *c, int *s) {
    switch (c->valtype) {
    case VALTYPE_NONE:
    case VALTYPE_HOLD:
    case VALTYPE_FSTACK:
        *s = 0;
        break;
    case VALTYPE_STACK:
        MUST(emit2(c, UNC_QINSTR_OP_STKGE, QOPER_NONE(), QOPER_UNSIGN(1)));
        configure2x(c, UNC_QINSTR_OP_MOV, QOPER_STACK(0));
        *s = 1;
        break;
    default:
        NEVER();
    }
    return 0;
}

static Unc_RetVal capture(Unc_ParserContext *c, Unc_QOperand op) {
    switch (c->valtype) {
    case VALTYPE_NONE:
        break;
    case VALTYPE_STACK:
        MUST(emit2(c, UNC_QINSTR_OP_STKGE, QOPER_NONE(), QOPER_UNSIGN(1)));
        MUST(emit2(c, UNC_QINSTR_OP_MOV, op.type, op.data, QOPER_STACK(0)));
        MUST(dokillstack(c));
        break;
    case VALTYPE_HOLD:
        MUST(emitre(c, op.type, op.data));
        break;
    case VALTYPE_FSTACK:
        MUST(emitre(c, op.type, op.data));
        break;
    }
    return 0;
}

/* holdval + holdend should work */
static Unc_RetVal holdend(Unc_ParserContext *c, int s) {
    if (s)
        MUST(dokillstack(c));
    return 0;
}

static Unc_RetVal killstack(Unc_ParserContext *c) {
    if (c->valtype == VALTYPE_STACK) {
        MUST(dokillstack(c));
        c->valtype = VALTYPE_NONE;
    }
    return 0;
}

static Unc_RetVal killvalue(Unc_ParserContext *c) {
    switch (c->valtype) {
    case VALTYPE_NONE:
        break;
    case VALTYPE_STACK:
        return killstack(c);
    case VALTYPE_HOLD:
        if (!unc0_qcode_ismov(c->next.op))
    case VALTYPE_FSTACK:
            MUST(emitre(c, QOPER_TMP(0)));
        break;
    }
    c->valtype = VALTYPE_NONE;
    return 0;
}

static Unc_RetVal eatexpr(Unc_ParserContext *c, int prec);
static Unc_RetVal eatelist(Unc_ParserContext *c, int stack, Unc_Size *pushed);
static Unc_RetVal eatfunc(Unc_ParserContext *c, int ftype);
static Unc_RetVal eatobjdef(Unc_ParserContext *c, Unc_Dst tr);

static Unc_RetVal wrapmov(Unc_ParserContext *c, Unc_QOperand *op) {
    if (!c->fence && c->next.op == UNC_QINSTR_OP_MOV) {
        *op = makeoperand(c->next.o1type, c->next.o1data);
    } else {
        Unc_Dst u;
        MUST(tmpalloc(c, &u));
        *op = makeoperand(QOPER_TMP(u));
        MUST(emitre(c, op->type, op->data));
    }
    return 0;
}

INLINE int atomcontnext(Unc_ParserContext *c) {
    switch (peek(c)) {
    case ULT_SParenL:
    case ULT_SBracketL:
    case ULT_ODot:
    case ULT_ODotQue:
    case ULT_OArrow:
        return 1;
    default:
        return 0;
    }
}

INLINE int endblocknext(Unc_ParserContext *c) {
    switch (peek(c)) {
    case ULT_END:
    case ULT_N:
    case ULT_NL:
    case ULT_Kend:
    case ULT_Kelse:
    case ULT_Kcatch:
        return 1;
    default:
        return 0;
    }
}

INLINE int endexprnext(Unc_ParserContext *c) {
    switch (peek(c)) {
    case ULT_SParenR:
    case ULT_SBraceR:
    case ULT_SBracketR:
    case ULT_SComma:
        return 1;
    default:
        return endblocknext(c);
    }
}

INLINE int compoundok(int c) {
    switch (c) {
    case ULT_OAdd:
    case ULT_OSub:
    case ULT_OMul:
    case ULT_ODiv:
    case ULT_OIdiv:
    case ULT_OMod:
    case ULT_OBshl:
    case ULT_OBshr:
    case ULT_OBand:
    case ULT_OBxor:
    case ULT_OBor:
    case ULT_OBinv:
        return 1;
    }
    return 0;
}

INLINE int getbinaryop(int opc) {
    switch (opc) {
    case ULT_OAdd:
        return UNC_QINSTR_OP_ADD;
    case ULT_OSub:
        return UNC_QINSTR_OP_SUB;
    case ULT_OMul:
        return UNC_QINSTR_OP_MUL;
    case ULT_ODiv:
        return UNC_QINSTR_OP_DIV;
    case ULT_OIdiv:
        return UNC_QINSTR_OP_IDIV;
    case ULT_OMod:
        return UNC_QINSTR_OP_MOD;
    case ULT_OBshl:
        return UNC_QINSTR_OP_SHL;
    case ULT_OBshr:
        return UNC_QINSTR_OP_SHR;
    case ULT_OBand:
        return UNC_QINSTR_OP_AND;
    case ULT_OBxor:
        return UNC_QINSTR_OP_XOR;
    case ULT_OBor:
        return UNC_QINSTR_OP_OR;
    case ULT_OBinv:
        return UNC_QINSTR_OP_CAT;
    default:
        NEVER();
        return 0;
    }
}

int allowlocals(const Unc_ParserContext *c) {
    return !c->quiet && (!c->c.extend || c->pframes_n);
}

Unc_RetVal symboltolocal(Unc_ParserContext *c, Unc_Size z,
                         Unc_BTreeRecord **rec, int cond) {
    if (allowlocals(c) && cond) {
        int created;
        MUST(unc0_putbtree(c->book, z, &created, rec));
        if (created) {
            Unc_BTreeRecord *trec = *rec;
            Unc_Dst u;
            MUST(localloc(c, &u));
            trec->first = UNC_QOPER_TYPE_LOCAL;
            trec->second = u;
        }
    } else {
        *rec = unc0_getbtree(c->book, z);
    }
    return 0;
}

/* write = -1 for delete, 0 for read, 1 for write */
INLINE Unc_RetVal eatatomx(Unc_ParserContext *c, Unc_QOperand *op, int write) {
    int writable = 1;
    int nc = peek(c);
    switch (nc) {
    case ULT_SParenL:
    {
        int s;
        consume(c);
        if (!++c->allownl) return UNCIL_ERR(SYNTAX_TOODEEP);
        if (peek(c) == ULT_SParenR)
            return UNCIL_ERR(SYNTAX);
        MUST(eatexpr(c, 0));
        --c->allownl;
        MUST(holdval(c, &s));
        writable = 0;
        if (peek(c) != ULT_SParenR)
            return UNCIL_ERR(SYNTAX);
        consume(c);
        MUST(wrapmov(c, op));
        MUST(holdend(c, s));
        break;
    }
    case ULT_SBracketL:
    {
        Unc_QOperand o;
        Unc_Dst tr;
        MUST(tmpalloc(c, &tr));
        consume(c);
        if (!++c->allownl) return UNCIL_ERR(SYNTAX_TOODEEP);
        MUST(startstack(c, 1));
        if (peek(c) != ULT_SBracketR) {
            MUST(eatelist(c, 1, NULL));
            MUST(tostack(c));
        }
        if (peek(c) != ULT_SBracketR)
            return UNCIL_ERR(SYNTAX);
        consume(c);
        --c->allownl;
        o = makeoperand(QOPER_TMP(tr));
        MUST(emit1(c, UNC_QINSTR_OP_MLIST, o.type, o.data));
        endstack(c);
        writable = 0;
        *op = o;
        break;
    }
    case ULT_SBraceL:
    {
        Unc_Dst tr;
        MUST(tmpalloc(c, &tr));
        consume(c);
        MUST(emit1(c, UNC_QINSTR_OP_NDICT, QOPER_TMP(tr)));
        if (!++c->allownl) return UNCIL_ERR(SYNTAX_TOODEEP);
        MUST(eatobjdef(c, tr));
        if (peek(c) != ULT_SBraceR)
            return UNCIL_ERR(SYNTAX);
        consume(c);
        --c->allownl;
        writable = 0;
        *op = makeoperand(QOPER_TMP(tr));
        break;
    }
    case ULT_I:
    {
        Unc_Size s;
        Unc_BTreeRecord *rec;
        consume(c);
        s = consumez(c);
        MUST(symboltolocal(c, s, &rec, write > 0 && !atomcontnext(c)));
        if (rec) {
            if (!c->quiet && rec->first == UNC_QOPER_TYPE_BINDABLE)
                MUST(dobind(c, s, rec));
            *op = makeoperand((byte)rec->first, qdataz(rec->second));
        /*} else if (write < 0) {
            *op = makeoperand(QOPER_NONE());*/
        } else {
            MARK_ID_USED(c, s);
            *op = makeoperand(QOPER_PUBLIC(s));
        }
        break;
    }
    case ULT_LInt:
        consume(c);
        *op = makeoperand(QOPER_INT(consumei(c)));
        return write ? UNCIL_ERR(SYNTAX) : 0; /* no dot etc. */
    case ULT_LFloat:
        consume(c);
        *op = makeoperand(QOPER_FLOAT(consumef(c)));
        return write ? UNCIL_ERR(SYNTAX) : 0; /* no dot etc. */
    case ULT_LStr:
        consume(c);
        {
            Unc_Size z;
            z = consumez(c);
            MARK_STR_USED(c, z);
            *op = makeoperand(QOPER_STR(z));
        }
        writable = 0;
        break;
    case ULT_Ktrue:
        consume(c);
        *op = makeoperand(QOPER_TRUE());
        writable = 0;
        break;
    case ULT_Kfalse:
        consume(c);
        *op = makeoperand(QOPER_FALSE());
        writable = 0;
        break;
    case ULT_Knull:
        consume(c);
        *op = makeoperand(QOPER_NULL());
        writable = 0;
        break;
    default:
        return UNCIL_ERR(SYNTAX);
    }

    if (atomcontnext(c)) {
        int pass, prev = 0, arrow = 0;
        Unc_Dst tr = 0, tr2 = 0, trt = 0;
        Unc_QOperand o = *op, pq = o;
        for (;;) {
            pass = atomcontnext(c);
            if (!pass && write)
                break;
            
            switch (o.type) {
            case UNC_QOPER_TYPE_ATTR:
                if (!tr)
                    MUST(tmpalloc(c, &tr));
                if (arrow) {
                    if (!trt)
                        MUST(tmpalloc(c, &trt));
                    c->fence = 1;
                    MUST(emit2(c, UNC_QINSTR_OP_MOV, QOPER_TMP(trt),
                                                     pq.type, pq.data));
                    MUST(emit3(c, arrow ? UNC_QINSTR_OP_GATTRF :
                    prev == ULT_ODotQue ? UNC_QINSTR_OP_GATTRQ
                                        : UNC_QINSTR_OP_GATTR, 
                                                QOPER_TMP(tr),
                                                QOPER_TMP(trt),
                                                QOPER_IDENT(o.data.o)));
                } else {
                    MUST(emit3(c, arrow ? UNC_QINSTR_OP_GATTRF :
                    prev == ULT_ODotQue ? UNC_QINSTR_OP_GATTRQ
                                        : UNC_QINSTR_OP_GATTR, 
                                                QOPER_TMP(tr),
                                                (byte)pq.type, pq.data,
                                                QOPER_IDENT(o.data.o)));
                }
                o = makeoperand(QOPER_TMP(tr));
                break;
            case UNC_QOPER_TYPE_INDEX:
                if (!tr)
                    MUST(tmpalloc(c, &tr));
                ASSERT(!arrow);
                MUST(emit3(c, UNC_QINSTR_OP_GINDX, 
                                            QOPER_TMP(tr),
                                            pq.type, pq.data,
                                            QOPER_TMP(o.data.o)));
                o = makeoperand(QOPER_TMP(tr));
                break;
            }
            
            if (!pass && !write)
                break;

            pq = o;
            if (o.type == UNC_QOPER_TYPE_FUNCSTACK) {
                ASSERT(tr != 0); /* should have been set already */
                MUST(emitre(c, QOPER_TMP(tr)));
                pq = makeoperand(QOPER_TMP(tr));
            } else {
                if (!tr)
                    MUST(tmpalloc(c, &tr));
                MUST(wrapreg(c, &pq, tr));
            }

            prev = consume(c);
            if (arrow && prev != ULT_SParenL) {
                MUST(emit3(c, UNC_QINSTR_OP_FBIND, pq.type, pq.data,
                                                   pq.type, pq.data,
                                                   QOPER_TMP(trt)));
            }
            switch (prev) {
            case ULT_SParenL:
            {
                /* function call */
                Unc_Size argc, pushfo;
                if (!++c->allownl) return UNCIL_ERR(SYNTAX_TOODEEP);
                MUST(startstack(c, 1));
                pushfo = c->cd_n - 1;
                o = pq;
                if (!tr)
                    MUST(tmpalloc(c, &tr));
                MUST(wraptreg(c, &o, tr));
                if ((arrow = !!arrow))
                    MUST(emit2(c, UNC_QINSTR_OP_MOV, QOPER_STACK_TO(),
                                                     QOPER_TMP(trt)));
                if (peek(c) != ULT_SParenR) {
                    MUST(eatelist(c, 1, &argc));
                    MUST(tostack(c));
                } else
                    argc = 0;
                if (consume(c) != ULT_SParenR)
                    return UNCIL_ERR(SYNTAX);
                if (argc < 256 - (unsigned)arrow) {
                    configure3x(c, UNC_QINSTR_OP_DCALL, o.type, o.data,
                                                QOPER_UNSIGN(argc + arrow));
                    if (!c->quiet) c->cd[pushfo].op = UNC_QINSTR_OP_DELETE;
                } else
                    configure2x(c, UNC_QINSTR_OP_FCALL, o.type, o.data);
                endstack(c);
                if (write) {
                    MUST(emitre(c, o.type, o.data));
                } else {
                    o.type = UNC_QOPER_TYPE_FUNCSTACK;
                }
                --c->allownl;
                writable = 0;
                arrow = 0;
                break;
            }
            case ULT_SBracketL:
                if (!++c->allownl) return UNCIL_ERR(SYNTAX_TOODEEP);
                if (!tr2)
                    MUST(tmpalloc(c, &tr2));
                MUST(eatexpr(c, 0));
                MUST(capture(c, makeoperand(QOPER_TMP(tr2))));
                o.type = UNC_QOPER_TYPE_INDEX;
                o.data = qdataz(tr2);
                if (consume(c) != ULT_SBracketR)
                    return UNCIL_ERR(SYNTAX);
                --c->allownl;
                writable = 1;
                arrow = 0;
                break;
            case ULT_ODot:
            case ULT_ODotQue:
            case ULT_OArrow:
            {
                Unc_Size z;
                if (peek(c) != ULT_I)
                    return UNCIL_ERR(SYNTAX);
                consume(c);
                arrow = prev == ULT_OArrow;
                o.type = UNC_QOPER_TYPE_ATTR;
                z = consumez(c);
                MARK_ID_USED(c, z);
                o.data = qdataz(z);
                writable = 1;
                break;
            }
            }
        }
        if (arrow) {
            MUST(emit3(c, UNC_QINSTR_OP_FBIND, o.type, o.data,
                                               pq.type, pq.data,
                                               QOPER_TMP(trt)));
        }
        switch (o.type) {
        case UNC_QOPER_TYPE_ATTR:
        case UNC_QOPER_TYPE_INDEX:
            MUST(auxpush(c, pq));
            break;
        }
        *op = o;
    }
    if (!writable && write)
        return UNCIL_ERR(SYNTAX);
    return 0;
}

INLINE Unc_RetVal eatatomr(Unc_ParserContext *c, Unc_QOperand *op) {
    return eatatomx(c, op, 0);
}

static Unc_RetVal eatatomw(Unc_ParserContext *c, Unc_QOperand *op) {
    Unc_RetVal e;
    Unc_QInstr nx = c->next;
    e = eatatomx(c, op, 1);
    c->next = nx;
    return e;
}

static Unc_RetVal eatatomdel(Unc_ParserContext *c, Unc_QOperand *op) {
    Unc_RetVal e;
    Unc_QInstr nx = c->next;
    e = eatatomx(c, op, -1);
    c->next = nx;
    return e;
}

static Unc_RetVal startfunc(Unc_ParserContext *c, Unc_Size *findex,
                            Unc_Size parent) {
    Unc_QFunc *nfn, *fn;
    if (c->quiet) {
        *findex = c->out.fn_sz;
        return 0;
    }
    nfn = TMREALLOC(Unc_QFunc, c->c.alloc, 0, c->out.fn,
                     c->out.fn_sz, c->out.fn_sz + 1);
    if (!nfn)
        return UNCIL_ERR(MEM);
    c->out.fn = nfn;
    fn = nfn + (*findex = c->out.fn_sz++);
    fn->lineno = c->out.lineno;
    fn->name = UNC_FUNC_NONAME;
    fn->inhales = NULL;
    fn->flags = 0;
    fn->cnt_arg = 0;
    fn->cnt_opt = 0;
    fn->parent = parent;
    return 0;
}

#define OUT_FUNCTION(c, findex) (&((c)->out.fn[findex]))

static void endfunc(Unc_ParserContext *c, Unc_Size findex) {
    Unc_QFunc *fn = OUT_FUNCTION(c, findex);
    fn->cnt_tmp = c->tmphigh;
    fn->cnt_loc = c->locnext;
    fn->cnt_inh = c->inhnext;
    fn->cnt_exh = c->exhnext;
    ASSERT(c->cd);
    fn->cd = TMREALLOC(Unc_QInstr, c->c.alloc, 0, c->cd, c->cd_c, c->cd_n);
    fn->cd_sz = c->cd_n;
}

#define MIN_OP ULT_Kand
#define MAX_OP ULT_OBinv

/* operator precedences, starting from MIN_OP
    1 = lowest precedence, 2 = higher, etc. */
static const int op_precs[] = {
    2,
    1,
    8,
    8,
    9,
    9,
    9,
    9,
    7,
    7,
    5,
    4,
    3,
    6,
    6,
    6,
    6,
    6,
    6,
    8
};
/* comparison operators should all have the same precedence
   or bad things will happen */

#define OPPREC(o) op_precs[o - MIN_OP]

INLINE int inrange(int x, int a, int b) {
    return a <= x && x <= b;
}

static void skipunary(Unc_ParserContext *c) {
    for (;;) {
        switch (peek(c)) {
        case ULT_OAdd:
        case ULT_OSub:
        case ULT_OBinv:
        case ULT_Knot:
            consume(c);
            break;
        default:
            return;
        }
    }
}

INLINE void ufold(Unc_ParserContext *c) {
    int op = c->next.op;
    switch (op) {
    case UNC_QINSTR_OP_UPOS:
    case UNC_QINSTR_OP_UNEG:
        if ((c->next.o1type != UNC_QOPER_TYPE_INT &&
             c->next.o1type != UNC_QOPER_TYPE_FLOAT))
            return;
        break;
    case UNC_QINSTR_OP_UXOR:
        if (c->next.o1type != UNC_QOPER_TYPE_INT)
            return;
        break;
    case UNC_QINSTR_OP_LNOT:
        if ((c->next.o1type != UNC_QOPER_TYPE_INT &&
             c->next.o1type != UNC_QOPER_TYPE_TRUE &&
             c->next.o1type != UNC_QOPER_TYPE_FALSE &&
             c->next.o1type != UNC_QOPER_TYPE_NULL))
            return;
        break;
    default:
        return;
    }
    
    switch (op) {
    case UNC_QINSTR_OP_UPOS:
        c->next.op = UNC_QINSTR_OP_MOV;
        if (c->next.o1type != UNC_QOPER_TYPE_INT) {
            c->next.o1data.uf = +c->next.o1data.uf;
        } else {
            c->next.o1data.ui = +c->next.o1data.ui;
        }
        break;
    case UNC_QINSTR_OP_UNEG:
        if (c->next.o1type != UNC_QOPER_TYPE_INT) {
            c->next.op = UNC_QINSTR_OP_MOV;
            c->next.o1data.uf = -c->next.o1data.uf;
        } else if (!unc0_negovf(c->next.o1data.ui)) {
            c->next.op = UNC_QINSTR_OP_MOV;
            c->next.o1data.ui = -c->next.o1data.ui;
        }
        break;
    case UNC_QINSTR_OP_UXOR:
        c->next.o1data.ui = ~c->next.o1data.ui;
        c->next.op = UNC_QINSTR_OP_MOV;
        break;
    case UNC_QINSTR_OP_LNOT:
        switch (c->next.o1type) {
        case UNC_QOPER_TYPE_INT:
            c->next.o1type = c->next.o1data.ui ? UNC_QOPER_TYPE_TRUE
                                               : UNC_QOPER_TYPE_FALSE;
            c->next.op = UNC_QINSTR_OP_MOV;
            break;
        case UNC_QOPER_TYPE_NULL:
        case UNC_QOPER_TYPE_FALSE:
            c->next.o1type = UNC_QOPER_TYPE_TRUE;
            c->next.op = UNC_QINSTR_OP_MOV;
            break;
        case UNC_QOPER_TYPE_TRUE:
            c->next.o1type = UNC_QOPER_TYPE_FALSE;
            c->next.op = UNC_QINSTR_OP_MOV;
            break;
        }
        break;
    default:
        return;
    }
}

static Unc_RetVal eatunary(Unc_ParserContext *c, const byte *l0, const byte *l,
                           Unc_QOperand *opp) {
    byte opc;
    Unc_QOperand op = makeoperand(QOPER_NONE());
    ASSERT(c->next.op == UNC_QINSTR_OP_MOV);
    while (--l >= l0) {
        switch (*l) {
        case ULT_OAdd:
            opc = UNC_QINSTR_OP_UPOS;
            break;
        case ULT_OSub:
            opc = UNC_QINSTR_OP_UNEG;
            break;
        case ULT_OBinv:
            opc = UNC_QINSTR_OP_UXOR;
            break;
        case ULT_Knot:
            opc = UNC_QINSTR_OP_LNOT;
            break;
        default:
            goto outtaunary;
        }
        if (c->next.op == UNC_QINSTR_OP_MOV) {
            c->next.op = opc;
            ufold(c);
        } else {
            /* dump existing instruction */
            if (op.type != UNC_QOPER_TYPE_TMP) {
                Unc_Dst u;
                MUST(tmpalloc(c, &u));
                op = makeoperand(QOPER_TMP(u));
            }
            MUST(emitre(c, op.type, op.data));
            configure2x(c, opc, op.type, op.data);
        }
    }
outtaunary:
    if (op.type != UNC_QOPER_TYPE_NONE)
        *opp = op;
    return 0;
}

INLINE void bfold(Unc_ParserContext *c) {
    int op = c->next.op;
    switch (op) {
    case UNC_QINSTR_OP_ADD:
    case UNC_QINSTR_OP_SUB:
    case UNC_QINSTR_OP_MUL:
        if ((c->next.o1type != UNC_QOPER_TYPE_INT &&
             c->next.o1type != UNC_QOPER_TYPE_FLOAT)
                || (c->next.o2type != UNC_QOPER_TYPE_INT &&
                    c->next.o2type != UNC_QOPER_TYPE_FLOAT))
            return;
        break;
    case UNC_QINSTR_OP_SHL:
    case UNC_QINSTR_OP_SHR:
    case UNC_QINSTR_OP_AND:
    case UNC_QINSTR_OP_XOR:
    case UNC_QINSTR_OP_OR:
        if (c->next.o1type != UNC_QOPER_TYPE_INT
                || c->next.o2type != UNC_QOPER_TYPE_INT)
            return;
        break;
    default:
        return;
    }
    
    switch (op) {
    case UNC_QINSTR_OP_ADD:
        if (c->next.o1type == UNC_QOPER_TYPE_INT &&
            c->next.o2type == UNC_QOPER_TYPE_INT) {
            Unc_Int a = c->next.o1data.ui, b = c->next.o2data.ui;
            if (!unc0_addovf(a, b))
                configure2x(c, UNC_QINSTR_OP_MOV, QOPER_INT(a + b));
        } else {
            Unc_Float a = c->next.o1type == UNC_QOPER_TYPE_INT ? 
                (Unc_Float)c->next.o1data.ui : c->next.o1data.uf;
            Unc_Float b = c->next.o2type == UNC_QOPER_TYPE_INT ? 
                (Unc_Float)c->next.o2data.ui : c->next.o2data.uf;
            configure2x(c, UNC_QINSTR_OP_MOV, QOPER_FLOAT(a + b));
        }
        break;
    case UNC_QINSTR_OP_SUB:
        if (c->next.o1type == UNC_QOPER_TYPE_INT &&
            c->next.o2type == UNC_QOPER_TYPE_INT) {
            Unc_Int a = c->next.o1data.ui, b = c->next.o2data.ui;
            if (!unc0_subovf(a, b))
                configure2x(c, UNC_QINSTR_OP_MOV, QOPER_INT(a - b));
        } else {
            Unc_Float a = c->next.o1type == UNC_QOPER_TYPE_INT ? 
                (Unc_Float)c->next.o1data.ui : c->next.o1data.uf;
            Unc_Float b = c->next.o2type == UNC_QOPER_TYPE_INT ? 
                (Unc_Float)c->next.o2data.ui : c->next.o2data.uf;
            configure2x(c, UNC_QINSTR_OP_MOV, QOPER_FLOAT(a - b));
        }
        break;
    case UNC_QINSTR_OP_MUL:
        if (c->next.o1type == UNC_QOPER_TYPE_INT &&
            c->next.o2type == UNC_QOPER_TYPE_INT) {
            Unc_Int a = c->next.o1data.ui, b = c->next.o2data.ui;
            if (!unc0_mulovf(a, b))
                configure2x(c, UNC_QINSTR_OP_MOV, QOPER_INT(a * b));
        } else {
            Unc_Float a = c->next.o1type == UNC_QOPER_TYPE_INT ? 
                (Unc_Float)c->next.o1data.ui : c->next.o1data.uf;
            Unc_Float b = c->next.o2type == UNC_QOPER_TYPE_INT ? 
                (Unc_Float)c->next.o2data.ui : c->next.o2data.uf;
            configure2x(c, UNC_QINSTR_OP_MOV, QOPER_FLOAT(a * b));
        }
        break;
    case UNC_QINSTR_OP_SHL:
    {
        Unc_Int a = c->next.o1data.ui, b = c->next.o2data.ui;
        configure2x(c, UNC_QINSTR_OP_MOV, QOPER_INT(unc0_shiftl(a, b)));
        break;
    }
    case UNC_QINSTR_OP_SHR:
    {
        Unc_Int a = c->next.o1data.ui, b = c->next.o2data.ui;
        configure2x(c, UNC_QINSTR_OP_MOV, QOPER_INT(unc0_shiftr(a, b)));
        break;
    }
    case UNC_QINSTR_OP_AND:
    {
        Unc_Int a = c->next.o1data.ui, b = c->next.o2data.ui;
        configure2x(c, UNC_QINSTR_OP_MOV, QOPER_INT(a & b));
        break;
    }
    case UNC_QINSTR_OP_XOR:
    {
        Unc_Int a = c->next.o1data.ui, b = c->next.o2data.ui;
        configure2x(c, UNC_QINSTR_OP_MOV, QOPER_INT(a ^ b));
        break;
    }
    case UNC_QINSTR_OP_OR:
    {
        Unc_Int a = c->next.o1data.ui, b = c->next.o2data.ui;
        configure2x(c, UNC_QINSTR_OP_MOV, QOPER_INT(a | b));
        break;
    }
    default:
        return;
    }
}

static int ctrlnext(Unc_ParserContext *c) {
    int p = peek(c);
    switch (p) {
    /* case ULT_Kdo: */
    case ULT_Kif:
    case ULT_Ktry:
    case ULT_Kwhile:
        return 1;
    default:
        return 0;
    }
}

INLINE int isrelop(int p) {
    switch (p) {
    case ULT_OCEq:
    case ULT_OCNe:
    case ULT_OCLt:
    case ULT_OCLe:
    case ULT_OCGt:
    case ULT_OCGe:
        return 1;
    default:
        return 0;
    }
}

static int relnext(Unc_ParserContext *c) {
    return isrelop(peek(c));
}

/* for handling short-circuiting and/or */
static Unc_RetVal eatcexpr(Unc_ParserContext *c, int prec) {
    int s = 0;
    Unc_Size l;
    Unc_Dst t;
    MUST(lblalloc(c, &l, 1));
    MUST(tmpalloc(c, &t));
    while (inrange(peek(c), MIN_OP, MAX_OP)) {
        int opc = peek(c);
        int oprec = OPPREC(opc);
        if (oprec < prec)
            break;
        MUST(emitre(c, QOPER_TMP(t)));
        MUST(holdend(c, s));
        consume(c);
        /* these two operators must have the lowest precedence, and
           thus operators with higher precedence get handled by eatexpr call */
        ASSERT(opc == ULT_Kand || opc == ULT_Kor);
        switch (opc) {
        case ULT_Kand:
            MUST(emit2(c, UNC_QINSTR_OP_IFF, QOPER_TMP(t), QOPER_JUMP(l)));
            break;
        case ULT_Kor:
            MUST(emit2(c, UNC_QINSTR_OP_IFT, QOPER_TMP(t), QOPER_JUMP(l)));
            break;
        }
        MUST(eatexpr(c, oprec + 1));
        MUST(holdval(c, &s));
    }
    MUST(emitre(c, QOPER_TMP(t)));
    MUST(holdend(c, s));
    SETLABEL(c, l);
    configure2x(c, UNC_QINSTR_OP_MOV, QOPER_TMP(t));
    c->fence = 1;
    c->valtype = VALTYPE_HOLD;
    return 0;
}

#define SWAP(T, a, b) do { T _tmp = a; a = b; b = _tmp; } while (0)

INLINE void swap12(Unc_ParserContext *c) {
    SWAP(byte, c->next.o1type, c->next.o2type);
    SWAP(union Unc_QInstr_Data, c->next.o1data, c->next.o2data);
}

/* for handling comparison operators */
static Unc_RetVal eatrexpr(Unc_ParserContext *c, int prec, Unc_QOperand op) {
    int q = 0;
    Unc_Dst t = 0;
    if (c->next.op != UNC_QINSTR_OP_MOV)
        MUST(wrapmov(c, &op));
    while (inrange(peek(c), MIN_OP, MAX_OP)) {
        int opc = peek(c);
        int oprec = OPPREC(opc);
        if (oprec < prec)
            break;
        consume(c);
        /* wrap LHS */
        switch (q) {
        case 0:
            if (!isintern(op.type))
                MUST(wrapreg(c, &op, 0));
            break;
        case 1:
            /* we have boolean left over... move to tmp */
            MUST(tmpalloc(c, &t));
        case 2:
            MUST(emitre(c, QOPER_TMP(t)));
            break;
        }
        MUST(eatexpr(c, oprec + 1));
        /* RHS */
        if (c->valtype == VALTYPE_STACK) {
            /* some exprs might put stuff on the stack */
            Unc_Dst u;
            MUST(tmpalloc(c, &u));
            MUST(capture(c, makeoperand(QOPER_TMP(u))));
            c->next.o2type = UNC_QOPER_TYPE_TMP;
            c->next.o2data = qdataz(u);
        } else if (c->next.op == UNC_QINSTR_OP_MOV
                        && isintern(c->next.o1type)) {
            c->next.o2type = c->next.o1type;
            c->next.o2data = c->next.o1data;
        } else {
            /* alloc tmp */
            Unc_Dst u;
            MUST(tmpalloc(c, &u));
            MUST(emitre(c, QOPER_TMP(u)));
            c->next.o2type = UNC_QOPER_TYPE_TMP;
            c->next.o2data = qdataz(u);
        }
        c->next.o1type = op.type;
        c->next.o1data = op.data;
        op = makeoperand(c->next.o2type, c->next.o2data);
        switch (opc) {
        case ULT_OCEq:
            c->next.op = UNC_QINSTR_OP_CEQ;
            break;
        case ULT_OCNe:
            c->next.op = UNC_QINSTR_OP_CEQ;
            MUST(emitre(c, QOPER_TMP(0)));
            configure2x(c, UNC_QINSTR_OP_LNOT, QOPER_TMP(0));
            break;
        case ULT_OCLt:
            c->next.op = UNC_QINSTR_OP_CLT;
            break;
        case ULT_OCGt:
            c->next.op = UNC_QINSTR_OP_CLT;
            swap12(c);
            break;
        case ULT_OCGe:
            c->next.op = UNC_QINSTR_OP_CLT;
            MUST(emitre(c, QOPER_TMP(0)));
            configure2x(c, UNC_QINSTR_OP_LNOT, QOPER_TMP(0));
            break;
        case ULT_OCLe:
            c->next.op = UNC_QINSTR_OP_CLT;
            swap12(c);
            MUST(emitre(c, QOPER_TMP(0)));
            configure2x(c, UNC_QINSTR_OP_LNOT, QOPER_TMP(0));
            break;
        }
        if (q < 2) ++q;
        if (q == 2) {
            MUST(emitre(c, QOPER_TMP(0)));
            configure3x(c, UNC_QINSTR_OP_AND, QOPER_TMP(t), QOPER_TMP(0));
        }
    }

    c->valtype = VALTYPE_HOLD;
    return 0;
}

#define BLOCK_KIND_MAIN 0
#define BLOCK_KIND_BLOCK 1
#define BLOCK_KIND_IF 2
#define BLOCK_KIND_TRY 3
#define BLOCK_KIND_DO 4
#define BLOCK_KIND_WHILE 5
#define BLOCK_KIND_FUNCTION 6

static Unc_RetVal eatblock(Unc_ParserContext *c, int kind);

static Unc_RetVal eatdoblk(Unc_ParserContext *c) {
    MUST(eatblock(c, BLOCK_KIND_DO));
    MUST(killvalue(c));
    return 0;
}

static Unc_RetVal eatifblk(Unc_ParserContext *c, int expr) {
    Unc_Size jelse, jend;
    Unc_Dst tr;
    BOOL r = 0, had_else = 0;
    MUST(lblalloc(c, &jelse, 1));
    jend = jelse;
    if (expr)
        MUST(startstack(c, 0));
    MUST(tmpalloc(c, &tr));
eatelseif:
    consume(c);
    if (ctrlnext(c))
        return UNCIL_ERR(SYNTAX);
    MUST(eatexpr(c, 1));
    {
        int s;
        MUST(holdval(c, &s));
        ASSERT(!s);
    }
    MUST(emitre(c, QOPER_TMP(tr)));
    MUST(emit2(c, UNC_QINSTR_OP_IFF, QOPER_TMP(tr), QOPER_JUMP(jelse)));
    c->valtype = VALTYPE_NONE;

    if (consume(c) != ULT_Kthen)
        return UNCIL_ERR(SYNTAX);
    if (expr) {
        MUST(eatexpr(c, 0));
        MUST(capture(c, makeoperand(QOPER_TMP(tr))));
    } else {
        MUST(eatblock(c, BLOCK_KIND_IF));
        MUST(killstack(c));
    }

    switch (peek(c)) {
    default:
        if (!r)
            return UNCIL_ERR(SYNTAX);
        goto exit_if;
    case ULT_Kelse:
        consume(c);
        if (jelse == jend)
            MUST(lblalloc(c, &jend, 1));
        MUST(emit2(c, UNC_QINSTR_OP_JMP, QOPER_NONE(), QOPER_JUMP(jend)));
        SETLABEL(c, jelse);
        if (peek(c) == ULT_Kif) {
            MUST(lblalloc(c, &jelse, 1));
            goto eatelseif;
        }
        had_else = 1;
        jelse = jend;
        if (expr) {
            MUST(eatexpr(c, 0));
            MUST(capture(c, makeoperand(QOPER_TMP(tr))));
        } else {
            MUST(eatblock(c, BLOCK_KIND_BLOCK));
            MUST(killstack(c));
        }
        if (peek(c) != ULT_Kend)
            return UNCIL_ERR(SYNTAX);
    case ULT_Kend:
        if (!had_else && expr)
            return UNCIL_ERR(SYNTAX_INLINEIFNOELSE);
        consume(c);
exit_if:
        SETLABEL(c, jelse);
        if (jelse != jend)
            SETLABEL(c, jend);
        c->xfence = 1;
        break;
    }

    if (expr) {
        c->fence |= 1;
        c->valtype = VALTYPE_HOLD;
        configure2x(c, UNC_QINSTR_OP_MOV, QOPER_TMP(tr));
    } else
        c->valtype = VALTYPE_NONE;
    return 0;
}

static Unc_RetVal eattryblk(Unc_ParserContext *c) {
    Unc_Size jcatch;
    Unc_QOperand dst;
    MUST(lblalloc(c, &jcatch, 2));

    MUST(emit2(c, UNC_QINSTR_OP_EXPUSH, QOPER_TMP(0), QOPER_JUMP(jcatch)));
    consume(c);
    MUST(eatblock(c, BLOCK_KIND_TRY));
    MUST(killstack(c));

    if (peek(c) != ULT_Kcatch)
        return UNCIL_ERR(SYNTAX);
    consume(c);

    MUST(emit0(c, UNC_QINSTR_OP_EXPOP));
    MUST(emit2(c, UNC_QINSTR_OP_JMP, QOPER_NONE(), QOPER_JUMP(jcatch + 1)));

    MUST(eatatomw(c, &dst));
    MUST(bewvar(c, &dst));
    SETLABEL(c, jcatch);
    MUST(emit2(c, UNC_QINSTR_OP_MOV, dst.type, dst.data, QOPER_TMP(0)));

    if (peek(c) != ULT_Kdo)
        return UNCIL_ERR(SYNTAX);
    consume(c);
    MUST(eatblock(c, BLOCK_KIND_BLOCK));
    MUST(killstack(c));
    if (consume(c) != ULT_Kend)
        return UNCIL_ERR(SYNTAX);
    SETLABEL(c, jcatch + 1);
    c->valtype = VALTYPE_NONE;
    return 0;
}

static Unc_RetVal eatforblk_cond(Unc_ParserContext *c, Unc_QOperand dst,
                                 byte relop, Unc_Dst tr1) {
    switch (relop) {
    case ULT_OCEq:
        MUST(emit3(c, UNC_QINSTR_OP_CEQ, QOPER_TMP(0), dst.type, dst.data,
                                                    QOPER_TMP(tr1)));
        return 0;
    case ULT_OCNe:
        MUST(emit3(c, UNC_QINSTR_OP_CEQ, QOPER_TMP(0), dst.type, dst.data,
                                                    QOPER_TMP(tr1)));
        return 1;
    case ULT_OCLt:
        MUST(emit3(c, UNC_QINSTR_OP_CLT, QOPER_TMP(0), dst.type, dst.data,
                                                    QOPER_TMP(tr1)));
        return 0;
    case ULT_OCGt:
        MUST(emit3(c, UNC_QINSTR_OP_CLT, QOPER_TMP(0), QOPER_TMP(tr1),
                                                    dst.type, dst.data));
        return 0;
    case ULT_OCGe:
        MUST(emit3(c, UNC_QINSTR_OP_CLT, QOPER_TMP(0), dst.type, dst.data,
                                                    QOPER_TMP(tr1)));
        return 1;
    case ULT_OCLe:
        MUST(emit3(c, UNC_QINSTR_OP_CLT, QOPER_TMP(0), QOPER_TMP(tr1),
                                                    dst.type, dst.data));
        return 1;
    default:
        NEVER();
    }
}

static Unc_RetVal eatforblk(Unc_ParserContext *c, int expr) {
    Unc_Size jstart;
    int func, ellipsis = 0;
    Unc_Dst tr1, tr2;
    Unc_QOperand dst;
    Unc_Size anchor = c->withbanchor;
    byte relop;
    
    /* in case of future "inline for" */
    ASSERT(!expr);

    MUST(lblalloc(c, &jstart, 4));
    MUST(plbpush(c, jstart));
    c->withbanchor = c->withbs;
    consume(c);
    if (peek(c) == ULT_SEllipsis) {
        consume(c);
        ellipsis = 1;
    }
    MUST(eatatomw(c, &dst));
    MUST(bewvar(c, &dst));

    switch (peek(c)) {
    case ULT_OSet:
        if (ellipsis) return UNCIL_ERR(SYNTAX);
    {
        int s;
        MUST(tmpalloc(c, &tr1));
        MUST(tmpalloc(c, &tr2));
        consume(c);
        MUST(eatexpr(c, 0));
        MUST(holdval(c, &s));
        MUST(emitre(c, dst.type, dst.data));
        MUST(holdend(c, s));
        if (consume(c) != ULT_SComma)
            return UNCIL_ERR(SYNTAX);
        if (!relnext(c))
            return UNCIL_ERR(SYNTAX_NOFOROP);
        relop = consume(c);
        MUST(eatexpr(c, 0));
        MUST(holdval(c, &s));
        MUST(emitre(c, QOPER_TMP(tr1)));
        MUST(holdend(c, s));
        if (peek(c) == ULT_SComma) {
            consume(c);
            MUST(eatexpr(c, 0));
            MUST(holdval(c, &s));
            MUST(emitre(c, QOPER_TMP(tr2)));
            MUST(holdend(c, s));
        } else {
            tr2 = 0;
        }
        func = 0;
        if (peek(c) != ULT_Kdo)
            return UNCIL_ERR(SYNTAX);
        consume(c);
        SETLABEL(c, jstart);
        break;
    }
    case ULT_OBshl:
        if (ellipsis) goto eatforblk_adv;
    {
        int s;
        MUST(tmpalloc(c, &tr1));
        consume(c);
        tr2 = 0;
        MUST(eatexpr(c, 0));
        MUST(holdval(c, &s));
        MUST(emitre(c, QOPER_TMP(tr1)));
        MUST(holdend(c, s));
        while (peek(c) == ULT_SComma) {
            consume(c);
            MUST(eatexpr(c, 0));
            MUST(killvalue(c));
        }
        MUST(emit2(c, UNC_QINSTR_OP_IITER, QOPER_TMP(tr1), QOPER_TMP(tr1)));
        func = 1;
        if (peek(c) != ULT_Kdo)
            return UNCIL_ERR(SYNTAX);
        consume(c);
        SETLABEL(c, jstart);
        MUST(emit3(c, UNC_QINSTR_OP_INEXT, dst.type, dst.data, QOPER_TMP(tr1),
                                                    QOPER_JUMP(jstart + 1)));
        break;
    }
    case ULT_SComma:    /* only works with ULT_OBshl */
    eatforblk_adv:
    {
        Unc_Save save;
        Unc_QOperand dst2;
        lsave(c, &save);
        qpause(c);
        if (peek(c) != ULT_OBshl) {
            int nowellipsis = ellipsis;
            for (;;) {
                if (consume(c) != ULT_SComma)
                    return UNCIL_ERR(SYNTAX);
                if (peek(c) == ULT_SEllipsis) {
                    if (nowellipsis)
                        return UNCIL_ERR(SYNTAX_MANYELLIPSES);
                    nowellipsis = 1;
                    consume(c);
                }
                MUST(eatatomw(c, &dst2));
                MUST(bewvar(c, &dst2));
                if (peek(c) == ULT_OBshl)
                    break;
            }
        }
        consume(c); /* ULT_OBshl */
        qresume(c);
        {
            int s;
            MUST(tmpalloc(c, &tr1));
            tr2 = 0;
            MUST(eatexpr(c, 0));
            MUST(holdval(c, &s));
            MUST(emitre(c, QOPER_TMP(tr1)));
            MUST(holdend(c, s));
            while (peek(c) == ULT_SComma) {
                consume(c);
                MUST(eatexpr(c, 0));
                MUST(killvalue(c));
            }
            MUST(emit2(c, UNC_QINSTR_OP_IITER, QOPER_TMP(tr1),
                                               QOPER_TMP(tr1)));
        }
        
        if (peek(c) != ULT_Kdo)
            return UNCIL_ERR(SYNTAX);
        consume(c);
        SETLABEL(c, jstart);

        MUST(startstack(c, 1));
        MUST(emit3(c, UNC_QINSTR_OP_INEXTS, QOPER_STACK_TO(), QOPER_TMP(tr1),
                                                    QOPER_JUMP(jstart + 1)));
        {
            Unc_Save save2;
            lsave(c, &save2);
            lrestore(c, &save);
            save = save2;
        }
        
        {
            Unc_Size stkge, p = 0, k = 0;
            MUST(emit2(c, UNC_QINSTR_OP_STKGE, QOPER_NONE(), QOPER_UNSIGN(0)));
            stkge = c->cd_n - 1;
            if (ellipsis)
                MUST(emit3(c, UNC_QINSTR_OP_MLISTP, dst.type, dst.data,
                              QOPER_UNSIGN(p), QOPER_UNSIGN(0)));
            else
                MUST(emit2(c, UNC_QINSTR_OP_MOV, dst.type, dst.data,
                              QOPER_STACK(p++)));
            if (!ellipsis || peek(c) == ULT_SComma) {
                for (;;) {
                    if (consume(c) != ULT_SComma)
                        return UNCIL_ERR(SYNTAX);
                    if (peek(c) == ULT_SEllipsis) {
                        consume(c);
                        ellipsis = 1;
                        MUST(eatatomw(c, &dst));
                        MUST(bewvar(c, &dst));
                        MUST(emit3(c, UNC_QINSTR_OP_MLISTP,
                                    (byte)dst.type, dst.data,
                                    QOPER_UNSIGN(p), QOPER_UNSIGN(0)));
                    } else {
                        MUST(eatatomw(c, &dst));
                        MUST(bewvar(c, &dst));
                        MUST(emit2(c, UNC_QINSTR_OP_MOV,
                                        (byte)dst.type, dst.data,
                                        ellipsis ? UNC_QOPER_TYPE_STACKNEG
                                                 : UNC_QOPER_TYPE_STACK,
                                        qdataz(ellipsis ? k++ : p++)));
                    }
                    if (peek(c) == ULT_OBshl)
                        break;
                }
            }
            
            if (ellipsis) {
                Unc_Size j = c->cd_n;
                do {
                    Unc_QInstr *q;
                    q = &c->cd[--j];
                    if (q->op == UNC_QINSTR_OP_MOV &&
                            q->o1type == UNC_QOPER_TYPE_STACKNEG)
                        q->o1data = qdataz(k - q->o1data.o);
                    else if (q->op == UNC_QINSTR_OP_MLISTP)
                        q->o2data = qdataz(k);
                    else if (q->op == UNC_QINSTR_OP_STKGE) {
                        q->o1data = qdataz(p + k);
                        break;
                    }
                } while (j);
            } else
                c->cd[stkge].o1data = qdataz(p + k);
        }
        MUST(emit0(c, UNC_QINSTR_OP_POPF));
        
        endstack(c);
        lrestore(c, &save);
        func = 2;
        break;
    }
    default:
        return UNCIL_ERR(SYNTAX);
    }

    if (!func) {
        MUST(emit2(c, UNC_QINSTR_OP_JMP, QOPER_TMP(0),
                                         QOPER_JUMP(jstart + 3)));
        SETLABEL(c, jstart + 2);
    }

    c->valtype = VALTYPE_NONE;
    MUST(eatblock(c, BLOCK_KIND_WHILE));
    MUST(killstack(c));

    if (peek(c) != ULT_Kend)
        return UNCIL_ERR(SYNTAX);
    consume(c);
    if (!func) {
        int e;
        SETLABEL(c, jstart);
        if (tr2)
            MUST(emit3(c, UNC_QINSTR_OP_ADD, dst.type, dst.data,
                                             dst.type, dst.data,
                                             QOPER_TMP(tr2)));
        else
            MUST(emit3(c, UNC_QINSTR_OP_ADD, dst.type, dst.data,
                                             dst.type, dst.data,
                                             QOPER_INT(1)));
        SETLABEL(c, jstart + 3);
        e = eatforblk_cond(c, dst, relop, tr1);
        MUST(emit2(c, e ? UNC_QINSTR_OP_IFF : UNC_QINSTR_OP_IFT,
            QOPER_TMP(0), QOPER_JUMP(jstart + 2)));
    } else {
        SETLABEL(c, jstart + 2);
        SETLABEL(c, jstart + 3);
        MUST(emit2(c, UNC_QINSTR_OP_JMP, QOPER_NONE(), QOPER_JUMP(jstart)));
    }
    SETLABEL(c, jstart + 1);
    if (func == 2)
        MUST(emit0(c, UNC_QINSTR_OP_POPF));

    c->fence |= expr;
    c->withbanchor = anchor;
    plbpop(c);
    c->valtype = VALTYPE_NONE;
    return 0;
}

static Unc_RetVal eatwhileblk(Unc_ParserContext *c) {
    Unc_Size tmp, jstart;
    Unc_Size anchor = c->withbanchor;
    MUST(lblalloc(c, &jstart, 2));
    MUST(plbpush(c, jstart));
    c->withbanchor = c->withbs;
    SETLABEL(c, jstart);
    consume(c);
    if (ctrlnext(c))
        return UNCIL_ERR(SYNTAX);
    {
        Unc_Dst tr;
        MUST(tmpalloc(c, &tr));
        tmp = tr;
    }

    MUST(eatexpr(c, 1));
    {
        int s;
        MUST(holdval(c, &s));
        ASSERT(!s);
    }

    MUST(emitre(c, QOPER_TMP(tmp)));
    if (peek(c) != ULT_Kdo)
        return UNCIL_ERR(SYNTAX);
    consume(c);
    MUST(emit2(c, UNC_QINSTR_OP_IFF, QOPER_TMP(tmp), QOPER_JUMP(jstart + 1)));
    MUST(eatblock(c, BLOCK_KIND_WHILE));
    MUST(killvalue(c));

    if (peek(c) != ULT_Kend)
        return UNCIL_ERR(SYNTAX);
    consume(c);
    MUST(emit2(c, UNC_QINSTR_OP_JMP, QOPER_NONE(), QOPER_JUMP(jstart)));
    SETLABEL(c, jstart + 1);

    plbpop(c);
    c->withbanchor = anchor;
    c->valtype = VALTYPE_NONE;
    return 0;
}

static Unc_RetVal eatwithblk(Unc_ParserContext *c) {
    Unc_Save save;
    size_t s = 0;

    consume(c);
    MUST(emit0(c, UNC_QINSTR_OP_WPUSH));
    
    lsave(c, &save);
    qpause(c);
    for (;;) {
        if (peek(c) != ULT_I)
            return UNCIL_ERR(SYNTAX);
        consume(c);
        consumez(c);
        ++s;
        if (peek(c) == ULT_SComma) {
            /* more values to scan */
            consume(c);
            continue;
        } else if (peek(c) == ULT_OSet) {
            consume(c);
            break;
        } else if (peek(c) == ULT_Kdo) {
            Unc_Dst tr;
            lrestore(c, &save);
            qresume(c);
            for (;;) {
                tr = c->tmpnext;
                MUST(eatexpr(c, 0));
                MUST(capture(c, makeoperand(QOPER_TMP(0))));
                MUST(emit2(c, UNC_QINSTR_OP_PUSHW,
                    QOPER_WSTACK(), QOPER_TMP(0)));
                c->tmpnext = tr;
                if (peek(c) == ULT_SComma) {
                    /* more values to scan */
                    consume(c);
                    continue;
                } else {
                    break;
                }
            }
            goto eatwithblk_do;
        } else {
            return UNCIL_ERR(SYNTAX);
        }
    }

    qresume(c);

    MUST(startstack(c, 1));
    MUST(eatelist(c, 1, NULL));
    MUST(tostack(c));

    {
        Unc_Save save2;
        lsave(c, &save2);
        lrestore(c, &save);
        save = save2;
    }
    
    MUST(emit2(c, UNC_QINSTR_OP_STKGE, QOPER_NONE(), QOPER_UNSIGN(s)));
    s = 0;
    for (;;) {
        size_t z;
        Unc_BTreeRecord *rec;
        if (peek(c) != ULT_I)
            return UNCIL_ERR(SYNTAX);
        consume(c);
        z = consumez(c);
        MUST(symboltolocal(c, z, &rec, 1));
        if (!rec) {
            MARK_ID_USED(c, z);
            MUST(emit2(c, UNC_QINSTR_OP_MOV, QOPER_TMP(0), QOPER_STACK(s++)));
            MUST(emit2(c, UNC_QINSTR_OP_PUSHW, QOPER_WSTACK(), QOPER_TMP(0)));
            MUST(emit2(c, UNC_QINSTR_OP_MOV,
                        UNC_QOPER_TYPE_PUBLIC, qdataz(z), QOPER_TMP(0)));
        } else if (unc0_qcode_isopreg(rec->first)) {
            MUST(emit2(c, UNC_QINSTR_OP_MOV,
                    (byte)rec->first, qdataz(rec->second), QOPER_STACK(s++)));
            MUST(emit2(c, UNC_QINSTR_OP_PUSHW, QOPER_WSTACK(),
                    (byte)rec->first, qdataz(rec->second)));
        } else {
            MUST(emit2(c, UNC_QINSTR_OP_MOV, QOPER_TMP(0), QOPER_STACK(s++)));
            MUST(emit2(c, UNC_QINSTR_OP_PUSHW, QOPER_WSTACK(), QOPER_TMP(0)));
            MUST(emit2(c, UNC_QINSTR_OP_MOV,
                    (byte)rec->first, qdataz(rec->second), QOPER_TMP(0)));
        }

        if (peek(c) == ULT_SComma) {
            /* more values to scan */
            consume(c);
            continue;
        } else if (peek(c) == ULT_OSet) {
            break;
        } else {
            return UNCIL_ERR(SYNTAX);
        }
    }

    c->valtype = VALTYPE_STACK;
    MUST(killstack(c));

    lrestore(c, &save);
eatwithblk_do:
    if (peek(c) != ULT_Kdo)
        return UNCIL_ERR(SYNTAX);
    consume(c);
    ++c->withbs;
    MUST(eatblock(c, BLOCK_KIND_BLOCK));
    --c->withbs;
    if (peek(c) != ULT_Kend)
        return UNCIL_ERR(SYNTAX);
    consume(c);
    MUST(killvalue(c));
    MUST(emit0(c, UNC_QINSTR_OP_WPOP));

    c->valtype = VALTYPE_NONE;
    return 0;
}

/* ends with leftovers in c->next */
static Unc_RetVal eatexpr(Unc_ParserContext *c, int prec) {
    Unc_QOperand op;

    if (prec == 0) {
        if (peek(c) == ULT_Kif)
            return eatifblk(c, 1);
        else if (peek(c) == ULT_Kfunction)
            return eatfunc(c, FUNCTYPE_EXPR);
        else if (peek(c) == ULT_SEllipsis) {
            Unc_QOperand op;
            consume(c);
            MUST(eatatomr(c, &op));
            if (op.type == UNC_QOPER_TYPE_FUNCSTACK) {
                MUST(capture(c, makeoperand(QOPER_TMP(0))));
                configure2x(c, UNC_QINSTR_OP_SPREAD, QOPER_TMP(0));
            } else {
                MUST(wrapreg(c, &op, 0));
                configure2x(c, UNC_QINSTR_OP_SPREAD, op.type, op.data);
            }
            c->valtype = VALTYPE_FSTACK;
            return 0;
        }
    }

    {
        const byte *start, *end;
        start = c->lex.lc;
        skipunary(c);
        end = c->lex.lc;
        MUST(eatatomr(c, &op));
        if (op.type == UNC_QOPER_TYPE_FUNCSTACK) {
            Unc_Dst u;
            c->valtype = VALTYPE_FSTACK;
            /* return immediately if there are no operators */
            if (!inrange(peek(c), MIN_OP, MAX_OP) && start == end)
                return 0;
            /* unfurl funcstack */
            MUST(tmpalloc(c, &u));
            op = makeoperand(QOPER_TMP(u));
            MUST(capture(c, makeoperand(QOPER_TMP(u))));
        }
        configure2x(c, UNC_QINSTR_OP_MOV, op.type, op.data);
        MUST(eatunary(c, start, end, &op));
    }

    while (inrange(peek(c), MIN_OP, MAX_OP)) {
        int opc = peek(c);
        int oprec = OPPREC(opc);
        if (oprec < prec)
            break;
        if (opc == ULT_Kand || opc == ULT_Kor) {
            /* handling these separately... */
            eatcexpr(c, oprec);
            continue;
        } else if (isrelop(opc)) {
            /* and these */
            eatrexpr(c, oprec, op);
            continue;
        }
        consume(c);
        /* wrap LHS */
        MUST(wrapmov(c, &op));
        /* RHS */
        if (peek(c) == ULT_OSet && compoundok(opc)) {
            c->compoundop = opc;
            return UNCIL_ERR(SYNTAXL_ASSIGNOP);
        }
        MUST(eatexpr(c, oprec + 1));
        if (c->valtype == VALTYPE_STACK) {
            /* some exprs might put stuff on the stack */
            Unc_Dst u;
            MUST(tmpalloc(c, &u));
            MUST(capture(c, makeoperand(QOPER_TMP(u))));
            c->next.o2type = UNC_QOPER_TYPE_TMP;
            c->next.o2data = qdataz(u);
        } else if (c->next.op == UNC_QINSTR_OP_MOV) {
            c->next.o2type = c->next.o1type;
            c->next.o2data = c->next.o1data;
        } else {
            /* alloc tmp */
            Unc_Dst u;
            MUST(tmpalloc(c, &u));
            MUST(emitre(c, QOPER_TMP(u)));
            c->next.o2type = UNC_QOPER_TYPE_TMP;
            c->next.o2data = qdataz(u);
        }
        c->next.o1type = op.type;
        c->next.o1data = op.data;
        c->next.op = getbinaryop(opc);
        switch (opc) {
        case ULT_OAdd:
            c->next.op = UNC_QINSTR_OP_ADD;
            break;
        case ULT_OSub:
            c->next.op = UNC_QINSTR_OP_SUB;
            break;
        case ULT_OMul:
            c->next.op = UNC_QINSTR_OP_MUL;
            break;
        case ULT_ODiv:
            c->next.op = UNC_QINSTR_OP_DIV;
            break;
        case ULT_OIdiv:
            c->next.op = UNC_QINSTR_OP_IDIV;
            break;
        case ULT_OMod:
            c->next.op = UNC_QINSTR_OP_MOD;
            break;
        case ULT_OBshl:
            c->next.op = UNC_QINSTR_OP_SHL;
            break;
        case ULT_OBshr:
            c->next.op = UNC_QINSTR_OP_SHR;
            break;
        case ULT_OBand:
            c->next.op = UNC_QINSTR_OP_AND;
            break;
        case ULT_OBxor:
            c->next.op = UNC_QINSTR_OP_XOR;
            break;
        case ULT_OBor:
            c->next.op = UNC_QINSTR_OP_OR;
            break;
        case ULT_OBinv:
            c->next.op = UNC_QINSTR_OP_CAT;
            break;
        }
        bfold(c);
    }

    c->valtype = VALTYPE_HOLD;
    return 0;
}

static Unc_RetVal eatelist(Unc_ParserContext *c, int stack, Unc_Size *pushed) {
    Unc_Dst ctr = 0, tr;
    int noctr = 0;
    if (stack)
        MUST(startstack(c, 0));
    for (;;) {
        tr = c->tmpnext;
        MUST(eatexpr(c, 0));
        if (c->valtype == VALTYPE_STACK || c->valtype == VALTYPE_FSTACK)
            noctr = 1;
        ++ctr;
        MUST(stack ? tostack(c) : killvalue(c));
        c->tmpnext = tr;
        if (peek(c) == ULT_SComma) {
            /* more values to scan */
            consume(c);
            if (peek(c) == ULT_NL)
                consume(c);
            continue;
        } else {
            break;
        }
    }
    if (pushed)
        *pushed = noctr ? UNC_SIZE_MAX : ctr;
    return 0;
}

static Unc_RetVal eatcompoundeqlist(Unc_ParserContext *c, Unc_Save save,
                                    int comma) {
    Unc_Save save2;
    /* we are assigning. do preliminary read of assignables */
    Unc_QOperand shell, wrapper;
    Unc_Size pushed = 0, pn;
    int opm = getbinaryop(c->compoundop);
    if (consume(c) != ULT_OSet) return UNCIL_ERR(SYNTAX);
    lrestore(c, &save);
    /* get actual number of values to read */
    for (;;) {
        if (peek(c) == ULT_SEllipsis) /* not ok with compound assignment */
            return UNCIL_ERR(SYNTAX_COMPOUNDELLIP);
        else
            MUST(eatatomw(c, &shell)); /* ghosted */
        ++pushed;
        if (peek(c) == ULT_SComma) {
            consume(c);
            if (peek(c) == ULT_NL)
                consume(c);
        } else if (compoundok(peek(c))) {
            consume(c);
            if (consume(c) != ULT_OSet) return UNCIL_ERR(SYNTAX);
            break;
        } else if (peek(c) == ULT_OSet) {
            consume(c);
            break;
        } else {
            return UNCIL_ERR(SYNTAX);
        }
    }

    qresume(c);
    if (pushed == 1) {
        /* single target optimization */
        Unc_Dst tr = 0;
        int s;
        for (;;) {
            MUST(eatexpr(c, 0));
            MUST(holdval(c, &s));
            MUST(wrapmov(c, &wrapper));
            if (pushed) {
                /* now do an actual read of the value to be written to */
                lsave(c, &save2);
                lrestore(c, &save);
                MUST(eatatomw(c, &shell));
                lrestore(c, &save2);
            }
            switch (shell.type) {
            case UNC_QOPER_TYPE_ATTR:
                {
                    Unc_QOperand top;
                    auxpop(c, &top);
                    if (!tr)
                        MUST(tmpalloc(c, &tr));
                    MUST(wrapreg(c, &wrapper, 0));
                    MUST(emit3(c, UNC_QINSTR_OP_GATTR,
                                    QOPER_TMP(tr),
                                    (byte)top.type, top.data,
                                    QOPER_IDENT(shell.data.o)));
                    MUST(emit3(c, opm, QOPER_TMP(tr), QOPER_TMP(tr),
                                    (byte)wrapper.type, wrapper.data));
                    MUST(emit3(c, UNC_QINSTR_OP_SATTR,
                                    QOPER_TMP(tr),
                                    (byte)top.type, top.data,
                                    QOPER_IDENT(shell.data.o)));
                }
                break;
            case UNC_QOPER_TYPE_INDEX:
                {
                    Unc_QOperand top;
                    auxpop(c, &top);
                    if (!tr)
                        MUST(tmpalloc(c, &tr));
                    MUST(wrapreg(c, &wrapper, 0));
                    MUST(emit3(c, UNC_QINSTR_OP_GINDX,
                                    QOPER_TMP(tr),
                                    (byte)top.type, top.data,
                                    QOPER_TMP(shell.data.o)));
                    MUST(emit3(c, opm, QOPER_TMP(tr), QOPER_TMP(tr),
                                    (byte)wrapper.type, wrapper.data));
                    MUST(emit3(c, UNC_QINSTR_OP_SINDX,
                                    QOPER_TMP(tr),
                                    (byte)top.type, top.data,
                                    QOPER_TMP(shell.data.o)));
                }
                break;
            default:
                MUST(unwrapmov(c, wrapper.type, wrapper.data));
                if (pushed) {
                    MUST(emit3(c, opm, (byte)shell.type, shell.data,
                                    (byte)shell.type, shell.data,
                                    (byte)wrapper.type, wrapper.data));
                } else {
                    MUST(emitre(c, (byte)shell.type, shell.data));
                }
            }
            MUST(holdend(c, s));
            if (peek(c) == ULT_SComma) {
                consume(c);
                if (pushed)
                    shell = makeoperand(QOPER_TMP(0)), --pushed;
                if (peek(c) == ULT_NL)
                    consume(c);
            } else if (endblocknext(c)) {
                break;
            } else {
                return UNCIL_ERR(SYNTAX);
            }
        }
    } else {
        Unc_Dst tr = 0, tr2 = 0;
        ASSERT(!c->pushfs);
        MUST(startstack(c, 1));
        MUST(eatelist(c, 1, NULL));
        MUST(tostack(c));
        if (!endblocknext(c))
            return UNCIL_ERR(SYNTAX_TRAILING);

        lsave(c, &save2);
        lrestore(c, &save);

        MUST(emit2(c, UNC_QINSTR_OP_STKGE, QOPER_NONE(),
                                           QOPER_UNSIGN(pushed)));
        for (pn = 0; pn < pushed; ++pn) {
            if (pn > 0)
                if (consume(c) != ULT_SComma) return UNCIL_ERR(SYNTAX);
            MUST(eatatomw(c, &shell));
            switch (shell.type) {
            case UNC_QOPER_TYPE_ATTR:
                {
                    Unc_QOperand top;
                    if (!tr)
                        MUST(tmpalloc(c, &tr));
                    if (!tr2)
                        MUST(tmpalloc(c, &tr2));
                    auxpop(c, &top);
                    MUST(emit3(c, UNC_QINSTR_OP_GATTR,
                                    QOPER_TMP(tr2),
                                    (byte)top.type, top.data,
                                    QOPER_TMP(shell.data.o)));
                    MUST(emit2(c, UNC_QINSTR_OP_MOV, QOPER_TMP(tr),
                                UNC_QOPER_TYPE_STACK, qdataz(pn)));
                    MUST(emit3(c, (byte)opm, QOPER_TMP(tr2), QOPER_TMP(tr2),
                                    QOPER_TMP(tr)));
                    MUST(emit3(c, UNC_QINSTR_OP_SATTR,
                                    QOPER_TMP(tr2),
                                    (byte)top.type, top.data,
                                    QOPER_TMP(shell.data.o)));
                }
                break;
            case UNC_QOPER_TYPE_INDEX:
                {
                    Unc_QOperand top;
                    if (!tr)
                        MUST(tmpalloc(c, &tr));
                    if (!tr2)
                        MUST(tmpalloc(c, &tr2));
                    auxpop(c, &top);
                    MUST(emit3(c, UNC_QINSTR_OP_GINDX,
                                    QOPER_TMP(tr2),
                                    (byte)top.type, top.data,
                                    QOPER_TMP(shell.data.o)));
                    MUST(emit2(c, UNC_QINSTR_OP_MOV, QOPER_TMP(tr),
                                UNC_QOPER_TYPE_STACK, qdataz(pn)));
                    MUST(emit3(c, (byte)opm, QOPER_TMP(tr2), QOPER_TMP(tr2),
                                    QOPER_TMP(tr)));
                    MUST(emit3(c, UNC_QINSTR_OP_SINDX,
                                    QOPER_TMP(tr2),
                                    (byte)top.type, top.data,
                                    QOPER_TMP(shell.data.o)));
                }
                break;
            default:
                MUST(emit3(c, opm, (byte)shell.type, shell.data,
                                   (byte)shell.type, shell.data,
                                UNC_QOPER_TYPE_STACK, qdataz(pn)));
            }
        }
        
        lrestore(c, &save2);

        c->valtype = VALTYPE_STACK;
        MUST(killstack(c));
    }
    c->valtype = VALTYPE_NONE;
    return 0;
}

static Unc_RetVal eateqlist(Unc_ParserContext *c) {
    Unc_Save save;
    /* check for equals */
    char flags = 0;
    unsigned nl = c->allownl;
    c->allownl = 0;
    lsave(c, &save);
    qpause(c);
    for (;;) {
        Unc_RetVal e;
        e = eatexpr(c, 0);
        if (e == UNCIL_ERR_SYNTAXL_ASSIGNOP) {
            /* compound assignment */
            return eatcompoundeqlist(c, save, flags & 1);
        }
        if (e) return e;
        if (peek(c) == ULT_SComma) {
            /* more values to scan */
            flags |= 1;
            consume(c);
            continue;
        } else if (peek(c) == ULT_OSet) {
            /* binary operator before? */
            flags |= 2;
            break;
        } else {
            break;
        }
    }

    if (!(flags & 2)) {
        lrestore(c, &save);
        qresume(c);
        /* read a bunch of values in, nostore them all */
        MUST(eatelist(c, c->c.extend && !c->pframes_n, NULL));
    } else {
        Unc_Save save2;
        /* we are assigning. do preliminary read of assignables */
        Unc_QOperand shell, wrapper;
        Unc_Size pushed = 0, pn, pushedpast = 0;
        int ellipsis = 0;
        lrestore(c, &save);
        /* get actual number of values to read */
        for (;;) {
            if (peek(c) == ULT_SEllipsis) { /* ghosted */
                if (ellipsis)
                    return UNCIL_ERR(SYNTAX_MANYELLIPSES);
                consume(c);
                if (peek(c) != ULT_I && peek(c) != ULT_SComma
                                     && peek(c) != ULT_OSet)
                    return UNCIL_ERR(SYNTAX);
                if (peek(c) == ULT_I) {
                    consume(c);
                    (void)consumez(c);
                }
                ellipsis = 1;
            } else
                MUST(eatatomw(c, &shell)); /* ghosted */
            ++pushed;
            if (peek(c) == ULT_SComma) {
                consume(c);
                if (peek(c) == ULT_NL)
                    consume(c);
            } else if (peek(c) == ULT_OSet) {
                consume(c);
                break;
            } else {
                return UNCIL_ERR(SYNTAX);
            }
        }

        qresume(c);
        if (!ellipsis && pushed == 1) {
            /* single target optimization */
            int s;
            for (;;) {
                MUST(eatexpr(c, 0));
                MUST(holdval(c, &s));
                MUST(wrapmov(c, &wrapper));
                if (pushed) {
                    /* now do an actual read of the value to be written to */
                    lsave(c, &save2);
                    lrestore(c, &save);
                    MUST(eatatomw(c, &shell));
                    lrestore(c, &save2);
                }
                switch (shell.type) {
                case UNC_QOPER_TYPE_ATTR:
                    {
                        Unc_QOperand top;
                        auxpop(c, &top);
                        MUST(wrapreg(c, &wrapper, 0));
                        MUST(emit3(c, UNC_QINSTR_OP_SATTR,
                                      wrapper.type, wrapper.data,
                                      top.type, top.data,
                                      QOPER_IDENT(shell.data.o)));
                    }
                    break;
                case UNC_QOPER_TYPE_INDEX:
                    {
                        Unc_QOperand top;
                        auxpop(c, &top);
                        MUST(wrapreg(c, &wrapper, 0));
                        MUST(emit3(c, UNC_QINSTR_OP_SINDX,
                                      wrapper.type, wrapper.data,
                                      top.type, top.data,
                                      QOPER_TMP(shell.data.o)));
                    }
                    break;
                default:
                    MUST(unwrapwr(c, wrapper.type, wrapper.data));
                    MUST(emitre(c, shell.type, shell.data));
                }
                MUST(holdend(c, s));
                if (peek(c) == ULT_SComma) {
                    consume(c);
                    if (pushed)
                        shell = makeoperand(QOPER_TMP(0)), --pushed;
                    if (peek(c) == ULT_NL)
                        consume(c);
                } else if (endblocknext(c)) {
                    break;
                } else {
                    return UNCIL_ERR(SYNTAX);
                }
            }
        } else {
            Unc_Dst tr = 0;
            ASSERT(!c->pushfs);
            MUST(startstack(c, 1));
            MUST(eatelist(c, 1, NULL));
            MUST(tostack(c));
            if (!endblocknext(c))
                return UNCIL_ERR(SYNTAX_TRAILING);

            lsave(c, &save2);
            lrestore(c, &save);

            MUST(emit2(c, UNC_QINSTR_OP_STKGE,
                    QOPER_NONE(), QOPER_UNSIGN(pushed - !!ellipsis)));
            ellipsis = 0;
            for (pn = 0; pn < pushed; ++pn) {
                if (pn > 0)
                    if (consume(c) != ULT_SComma) return UNCIL_ERR(SYNTAX);
                if (peek(c) == ULT_SEllipsis) {
                    if (ellipsis)
                        return UNCIL_ERR(SYNTAX_MANYELLIPSES);
                    ellipsis = 1;
                    consume(c);
                    if (peek(c) != ULT_I && peek(c) != ULT_SComma
                                         && peek(c) != ULT_OSet)
                        return UNCIL_ERR(SYNTAX);
                    if (peek(c) == ULT_I) {
                        Unc_Size z;
                        consume(c);
                        z = consumez(c);
                        if (!c->quiet) {
                            Unc_BTreeRecord *rec;
                            MUST(symboltolocal(c, z, &rec, 1));
                            if (!rec) {
                                MARK_ID_USED(c, z);
                                MUST(emit3(c, UNC_QINSTR_OP_MLISTP,
                                            UNC_QOPER_TYPE_PUBLIC, qdataz(z),
                                            QOPER_UNSIGN(pn),
                                            QOPER_UNSIGN(0)));
                            } else {
                                if (rec->first == UNC_QOPER_TYPE_BINDABLE)
                                    MUST(dobind(c, z, rec));
                                MUST(emit3(c, UNC_QINSTR_OP_MLISTP,
                                        (byte)rec->first, qdataz(rec->second),
                                        QOPER_UNSIGN(pn), QOPER_UNSIGN(0)));
                            }
                        }
                    }
                    continue;
                }
                MUST(eatatomw(c, &shell));
                switch (shell.type) {
                case UNC_QOPER_TYPE_ATTR:
                    {
                        Unc_QOperand top;
                        if (!tr)
                            MUST(tmpalloc(c, &tr));
                        MUST(emit2(c, UNC_QINSTR_OP_MOV, QOPER_TMP(tr),
                                    ellipsis ? UNC_QOPER_TYPE_STACKNEG
                                             : UNC_QOPER_TYPE_STACK,
                                    qdataz(ellipsis ? pushedpast : pn)));
                        auxpop(c, &top);
                        MUST(emit3(c, UNC_QINSTR_OP_SATTR,
                                      QOPER_TMP(tr),
                                      (byte)top.type, top.data,
                                      QOPER_IDENT(shell.data.o)));
                    }
                    break;
                case UNC_QOPER_TYPE_INDEX:
                    {
                        Unc_QOperand top;
                        if (!tr)
                            MUST(tmpalloc(c, &tr));
                        MUST(emit2(c, UNC_QINSTR_OP_MOV, QOPER_TMP(tr),
                                    ellipsis ? UNC_QOPER_TYPE_STACKNEG
                                             : UNC_QOPER_TYPE_STACK,
                                    qdataz(ellipsis ? pushedpast : pn)));
                        auxpop(c, &top);
                        MUST(emit3(c, UNC_QINSTR_OP_SINDX,
                                      QOPER_TMP(tr),
                                      (byte)top.type, top.data,
                                      QOPER_TMP(shell.data.o)));
                    }
                    break;
                default:
                    MUST(emit2(c, UNC_QINSTR_OP_MOV,
                                (byte)shell.type, shell.data,
                                    ellipsis ? UNC_QOPER_TYPE_STACKNEG
                                             : UNC_QOPER_TYPE_STACK,
                                    qdataz(ellipsis ? pushedpast : pn)));
                }
                pushedpast += ellipsis;
            }
            
            lrestore(c, &save2);

            if (ellipsis) {
                Unc_Size j = c->cd_n;
                do {
                    Unc_QInstr *q;
                    q = &c->cd[--j];
                    if (q->op == UNC_QINSTR_OP_MOV &&
                            q->o1type == UNC_QOPER_TYPE_STACKNEG)
                        q->o1data = qdataz(pushedpast - q->o1data.o);
                    else if (q->op == UNC_QINSTR_OP_MLISTP)
                        q->o2data = qdataz(pushedpast);
                    else if (q->op == UNC_QINSTR_OP_STKGE)
                        break;
                } while (j);
            }

            c->valtype = VALTYPE_STACK;
            MUST(killstack(c));
        }
        c->valtype = VALTYPE_NONE;
    }
    c->allownl = nl;
    return 0;
}

static Unc_RetVal eatpublist(Unc_ParserContext *c) {
    int comma = 0;
    for (;;) {
        int created, ok;
        Unc_Size z;
        Unc_BTreeRecord *rec;
        if (peek(c) != ULT_I)
            return UNCIL_ERR(SYNTAX);
        consume(c);
        z = consumez(c);
        MUST(unc0_putbtree(c->book, z, &created, &rec));
        ok = 1;
        if (!created) {
            switch (rec->first) {
            case UNC_QOPER_TYPE_PUBLIC:
                ok = 0;
                break;
            case UNC_QOPER_TYPE_BINDABLE:
                break;
            default:
                return UNCIL_ERR(SYNTAX_PUBLICONLOCAL);
            }
        }
        if (ok) {
            MARK_ID_USED(c, z);
            rec->first = UNC_QOPER_TYPE_PUBLIC;
            rec->second = z;
        }
        if (peek(c) == ULT_OSet) {
            if (comma)
                return UNCIL_ERR(SYNTAX_PUBLICONLYONE);
            consume(c);
            MUST(eatexpr(c, 0));
            MUST(capture(c, makeoperand(QOPER_PUBLIC(z))));
            if (peek(c) == ULT_SComma)
                return UNCIL_ERR(SYNTAX_PUBLICONLYONE);
            return 0;
        }
        if (peek(c) == ULT_SComma) {
            consume(c);
            comma = 1;
            continue;
        }
        return 0;
    }
}

static Unc_RetVal eatdellist(Unc_ParserContext *c) {
    Unc_QOperand shell;
    for (;;) {
        MUST(eatatomdel(c, &shell));
        switch (shell.type) {
        case UNC_QOPER_TYPE_NONE:
            break;
        case UNC_QOPER_TYPE_LOCAL:
        case UNC_QOPER_TYPE_INHALE:
        case UNC_QOPER_TYPE_EXHALE:
            MUST(emit2(c, UNC_QINSTR_OP_MOV, shell.type, shell.data,
                                             QOPER_NULL()));
            break;
        case UNC_QOPER_TYPE_PUBLIC:
            MUST(emit2(c, UNC_QINSTR_OP_DPUB, QOPER_NONE(),
                                              shell.type, shell.data));
            break;
        case UNC_QOPER_TYPE_ATTR:
            {
                Unc_QOperand top;
                auxpop(c, &top);
                MUST(emit3(c, UNC_QINSTR_OP_DATTR, QOPER_NONE(),
                                top.type, top.data,
                                QOPER_IDENT(shell.data.o)));
            }
            break;
        case UNC_QOPER_TYPE_INDEX:
            {
                Unc_QOperand top;
                auxpop(c, &top);
                MUST(emit3(c, UNC_QINSTR_OP_DINDX, QOPER_NONE(),
                                top.type, top.data,
                                QOPER_TMP(shell.data.o)));
            }
            break;
        default:
            NEVER();
        }
        if (peek(c) == ULT_SComma) {
            consume(c);
            continue;
        }
        return 0;
    }
}

static Unc_RetVal eatobjdef(Unc_ParserContext *c, Unc_Dst tr) {
    Unc_Dst tr2, tr3 = 0;
    Unc_QOperand op;
    MUST(tmpalloc(c, &tr2));
    for (;;) {
        switch (peek(c)) {
        case ULT_SBraceR:
            return 0;
        case ULT_LInt:
            consume(c);
            op = makeoperand(QOPER_INT(consumei(c)));
            break;
        case ULT_LFloat:
            consume(c);
            op = makeoperand(QOPER_FLOAT(consumef(c)));
            break;
        case ULT_LStr:
            consume(c);
            {
                Unc_Size z;
                z = consumez(c);
                MARK_STR_USED(c, z);
                op = makeoperand(QOPER_STR(z));
            }
            break;
        case ULT_I: {
            Unc_Size s;
            consume(c);
            s = consumez(c);
            MARK_ID_USED(c, s);
            op = makeoperand(QOPER_STRIDENT(s));
            break;
        }
        case ULT_Kfunction:
            MUST(eatfunc(c, FUNCTYPE_DICT));
            op = makeoperand(QOPER_STRIDENT(c->next.o2data.o));
            goto eatobjdef_gotvalue;
        case ULT_SParenL:
            consume(c);
            if (!tr3)
                MUST(tmpalloc(c, &tr3));
            MUST(eatexpr(c, 0));
            MUST(capture(c, makeoperand(QOPER_TMP(tr3))));
            if (peek(c) != ULT_SParenR)
                return UNCIL_ERR(SYNTAX);
            consume(c);
            op = makeoperand(QOPER_TMP(tr3));
            return UNCIL_ERR(SYNTAX);
        default:
            return UNCIL_ERR(SYNTAX);
        }
        if (consume(c) != ULT_SColon)
            return UNCIL_ERR(SYNTAX);
        MUST(eatexpr(c, 0));
        MUST(capture(c, makeoperand(QOPER_TMP(0))));
eatobjdef_gotvalue:
        MUST(wrapreg(c, &op, tr2));
        MUST(emit3(c, UNC_QINSTR_OP_SINDX,
                        QOPER_TMP(0), QOPER_TMP(tr), op.type, op.data));
        switch (peek(c)) {
        case ULT_SBraceR:
            return 0;
        case ULT_SComma:
            consume(c);
            continue;
        default:
            return UNCIL_ERR(SYNTAX);
        }
    }
}

static Unc_RetVal eatblock(Unc_ParserContext *c, int kind) {
    Unc_Dst ud = c->tmpnext;
    if (kind == BLOCK_KIND_MAIN) {
        Unc_Size s;
        MUST(lblalloc(c, &s, 1));
        ASSERT(s == 0);
    }

    for (;;) {
        switch (peek(c)) {
        case ULT_NL:
        case ULT_N:
        case ULT_Kend:
        case ULT_Kelse:
        case ULT_Kcatch:
        case ULT_END:
            break;
        case ULT_Kfor:
            MUST(eatforblk(c, 0));
            MUST(killvalue(c));
            c->tmpnext = ud;
            break;
        case ULT_Kif:
            MUST(eatifblk(c, 0));
            MUST(killvalue(c));
            c->tmpnext = ud;
            break;
        case ULT_Kdo:
            MUST(eatdoblk(c));
            MUST(killvalue(c));
            c->tmpnext = ud;
            break;
        case ULT_Ktry:
            MUST(eattryblk(c));
            MUST(killvalue(c));
            c->tmpnext = ud;
            break;
        case ULT_Kwhile:
            MUST(eatwhileblk(c));
            MUST(killvalue(c));
            c->tmpnext = ud;
            break;
        case ULT_Kwith:
            MUST(eatwithblk(c));
            MUST(killvalue(c));
            c->tmpnext = ud;
            break;
        case ULT_Kfunction:
            MUST(eatfunc(c, FUNCTYPE_LOCAL));
            MUST(killvalue(c));
            c->tmpnext = ud;
            break;
        case ULT_Kpublic:
            consume(c);
            if (peek(c) == ULT_Kfunction) {
                MUST(eatfunc(c, FUNCTYPE_PUBLIC));
                MUST(killvalue(c));
                c->tmpnext = ud;
                break;
            } else {
                MUST(killvalue(c));
                MUST(eatpublist(c));
                break;
            }
        case ULT_Kdelete:
            consume(c);
            MUST(killvalue(c));
            MUST(eatdellist(c));
            break;
        case ULT_Kreturn:
            consume(c);
            switch (peek(c)) {
            default:
            {
                Unc_Size pushed, pushfo;
                /* ASSERT(!c->pushfs); */
                if (c->pushfs)
                    return UNCIL_ERR(SYNTAX);
                MUST(startstack(c, 1));
                pushfo = c->cd_n - 1;
                MUST(eatelist(c, 1, &pushed));
                MUST(tostack(c));
                if (!pushed) {
                    MUST(emit0(c, UNC_QINSTR_OP_EXIT0));
                    if (!c->quiet) c->cd[pushfo].op = UNC_QINSTR_OP_DELETE;
                } else if (pushed == 1) {
                    ASSERT(c->cd[c->cd_n - 1].op == UNC_QINSTR_OP_PUSH);
                    c->cd[c->cd_n - 1].op = UNC_QINSTR_OP_EXIT1;
                    c->cd[c->cd_n - 1].o0type = c->cd[c->cd_n - 1].o1type;
                    c->cd[c->cd_n - 1].o0data = c->cd[c->cd_n - 1].o1data.o;
                    if (!c->quiet) c->cd[pushfo].op = UNC_QINSTR_OP_DELETE;
                }
                else
                    MUST(emit0(c, UNC_QINSTR_OP_END));
                endstack(c);
                break;
            }
            case ULT_NL:
            case ULT_N:
            case ULT_END:
            case ULT_Kend:
                MUST(emit0(c, UNC_QINSTR_OP_EXIT0));
            }
            c->valtype = VALTYPE_NONE;
            c->tmpnext = ud;
            break;
        case ULT_Kbreak:
        case ULT_Kcontinue:
            if (plbhas(c)) {
                Unc_Size a;
                MUST(killvalue(c));
                c->tmpnext = ud;
                for (a = c->withbanchor; a < c->withbs; ++a)
                    MUST(emit0(c, UNC_QINSTR_OP_WPOP));
                MUST(emit2(c, UNC_QINSTR_OP_JMP, QOPER_NONE(),
                        QOPER_JUMP(plbpeek(c) + (consume(c) == ULT_Kbreak))));
                break;
            } else {
                switch (peek(c)) {
                case ULT_Kbreak:
                    return UNCIL_ERR(SYNTAX_BADBREAK);
                case ULT_Kcontinue:
                    return UNCIL_ERR(SYNTAX_BADCONTINUE);
                default:
                    return UNCIL_ERR(SYNTAX);
                }
            }
            break;
        default:
            MUST(killstack(c));
            MUST(eateqlist(c));
            if (kind == BLOCK_KIND_MAIN && c->c.extend)
                MUST(tostack(c));
            else
                MUST(killvalue(c));
            c->tmpnext = ud;
        }

        switch (peek(c)) {
        case ULT_Kelse:
            if (kind == BLOCK_KIND_IF)
                goto exitblock;
        case ULT_Kcatch:
            if (kind == BLOCK_KIND_TRY)
                goto exitblock;
            return UNCIL_ERR(SYNTAX);
        case ULT_Kend:
            if (kind != BLOCK_KIND_MAIN)
                goto exitblock;
            return UNCIL_ERR(SYNTAX_STRAYEND);
        case ULT_N:
            MUST(killstack(c));
            if (kind == BLOCK_KIND_MAIN)
                c->valtype = VALTYPE_NONE;
        case ULT_NL:
            consume(c);
            break;
        case ULT_END:
        exitblock:
            c->tmpnext = ud;
            if (kind == BLOCK_KIND_MAIN || kind == BLOCK_KIND_FUNCTION) {
                /* don't bother if we already have a END */
                if (c->cd_n && isexitop(c->cd[c->cd_n - 1].op) && !c->xfence)
                    return 0;
                if (kind == BLOCK_KIND_MAIN && c->c.extend) {
                    if (!c->pushfs) MUST(emit0(c, UNC_QINSTR_OP_PUSHF));
                    SETLABEL(c, 0);
                    MUST(emit0(c, UNC_QINSTR_OP_END));
                } else {
                    MUST(emit0(c, UNC_QINSTR_OP_EXIT0));
                }
            }
            return 0;
        default:
            return UNCIL_ERR(SYNTAX_TRAILING);
        }
    }
}

static Unc_RetVal allocparent(Unc_ParserContext *c, Unc_ParserParent **pp) {
    if (c->pframes_n == c->pframes_c) {
        Unc_Size z = c->pframes_c, nz = z + 4;
        Unc_ParserParent *npf = TMREALLOC(Unc_ParserParent, c->c.alloc, 0,
                                                        c->pframes, z, nz);
        if (!npf) return UNCIL_ERR(MEM);
        c->pframes = npf;
        c->pframes_c = nz;
    }
    *pp = &c->pframes[c->pframes_n++];
    return 0;
}

static Unc_RetVal setupbinds(Unc_Size key, Unc_BTreeRecord *value,
                             void *udata) {
    Unc_BTree *newtree = udata;
    switch (value->first) {
    case UNC_QOPER_TYPE_LOCAL:
    case UNC_QOPER_TYPE_EXHALE:
    case UNC_QOPER_TYPE_INHALE:
    {
        int c;
        Unc_BTreeRecord *r;
        MUST(unc0_putbtree(newtree, key, &c, &r));
        ASSERT(c);
        r->first = UNC_QOPER_TYPE_BINDABLE;
        r->second = 1;
        break;
    }
    case UNC_QOPER_TYPE_BINDABLE:
    {
        int c;
        Unc_BTreeRecord *r;
        MUST(unc0_putbtree(newtree, key, &c, &r));
        ASSERT(c);
        r->first = UNC_QOPER_TYPE_BINDABLE;
        r->second = value->second + 1;
        break;
    }
    }
    return 0;
}

static void patchjumps(Unc_ParserContext *c, Unc_Size base) {
    Unc_Size i = 0, n = c->cd_n, ln = c->lb_n;
    for (i = 0; i < n; ++i) {
        Unc_QInstr *instr = &c->cd[i];
        int o = unc0_qcode_getjumpd(instr->op);
        if (o == 1 && instr->o1type == UNC_QOPER_TYPE_JUMP
                   && instr->o1data.o >= base
                   && instr->o1data.o < ln) {
            instr->o1data.o = c->lb[instr->o1data.o];
        } else if (o == 2 && instr->o2type == UNC_QOPER_TYPE_JUMP
                          && instr->o2data.o >= base
                          && instr->o2data.o < ln) {
            instr->o2data.o = c->lb[instr->o2data.o];
        }
    }
}

static Unc_RetVal patchboundargs(Unc_ParserContext *c) {
    Unc_Size i, ba = 0;
    for (i = 0; i < c->argc; ++i)
        ba += c->arg_exh[i] != 0;
    if (ba > 0) {
        Unc_Size j = 0;
        if (c->cd_n + ba >= c->cd_c) {
            Unc_Size z = c->cd_c, nz = c->cd_n + ba;
            Unc_QInstr *np = TMREALLOC(Unc_QInstr, c->c.alloc, 0,
                                       c->cd, z, nz);
            if (!np)
                return UNCIL_ERR(MEM);
            c->cd_c = nz;
            c->cd = np;
        }
        unc0_memmove(c->cd + ba, c->cd, c->cd_n * sizeof(Unc_QInstr));
        c->cd_n += ba;
        for (i = ba; i < c->cd_n; ++i) {
            Unc_QInstr *instr = &c->cd[i];
            int o = unc0_qcode_getjumpd(instr->op);
            if (o == 1 && instr->o1type == UNC_QOPER_TYPE_JUMP)
                instr->o1data.o += ba;
            else if (o == 2 && instr->o2type == UNC_QOPER_TYPE_JUMP)
                instr->o2data.o += ba;
        }
        for (i = 0; i < c->argc; ++i) {
            if (c->arg_exh[i] != 0) {
                Unc_QInstr instr;
                instr.op = UNC_QINSTR_OP_SBIND;
                instr.o0type = UNC_QOPER_TYPE_LOCAL;
                instr.o1type = UNC_QOPER_TYPE_EXHALE;
                instr.o2type = UNC_QOPER_TYPE_NONE;
                instr.o0data = i;
                instr.o1data = qdataz(c->arg_exh[i] - 1);
                instr.o2data = qdatanone();
                c->cd[j] = instr;
                c->cd[j].lineno = c->out.lineno;
                ++j;
            }
        }
        ASSERT(j == ba);
    }
    return 0;
}

static Unc_RetVal eatfunc(Unc_ParserContext *c, int ftype) {
    Unc_RetVal e;
    Unc_Size fnindex;
    Unc_Save pre;
    Unc_QInstr cnx;
    MUST(startfunc(c, &fnindex, c->curfunc));
    consume(c);
    lsave(c, &pre);
    MUST(startstack(c, 1));
    if (peek(c) == ULT_I) {
        Unc_Size z;
        consume(c);
        z = consumez(c);
        MARK_ID_USED_BOTH(c, z);
        lsave(c, &pre);
        if (!c->quiet) {
            OUT_FUNCTION(c, fnindex)->name = z;
            OUT_FUNCTION(c, fnindex)->flags |= UNC_FUNCTION_FLAG_NAMED;
        }

        if (peek(c) == ULT_SParenL) {
            /* read optional parameter default values */
            consume(c);
            if (peek(c) != ULT_SParenR) {
                int opt = 0;
                for (;;) {
                    if (peek(c) == ULT_SEllipsis) {
                        consume(c);
                        if (peek(c) != ULT_I)
                            return UNCIL_ERR(SYNTAX);
                        consume(c);
                        (void)consumez(c);
                        if (peek(c) == ULT_SComma)
                            return UNCIL_ERR(SYNTAX_UNPACKLAST);
                        else if (peek(c) == ULT_OSet)
                            return UNCIL_ERR(SYNTAX_UNPACKDEFAULT);
                        else if (peek(c) != ULT_SParenR)
                            return UNCIL_ERR(SYNTAX);
                        break;
                    }
                    if (peek(c) != ULT_I)
                        return UNCIL_ERR(SYNTAX);
                    consume(c);
                    (void)consumez(c);
                    if (peek(c) == ULT_OSet) {
                        int s;
                        opt = 1;
                        consume(c);
                        MUST(eatexpr(c, 0));
                        MUST(holdval(c, &s));
                        MUST(emitre(c, QOPER_TMP(0)));
                        MUST(holdend(c, s));
                        MUST(emit2(c, UNC_QINSTR_OP_MOV, QOPER_STACK_TO(),
                                                         QOPER_TMP(0)));
                        break;
                    } else if (opt)
                        return (peek(c) == ULT_SParenR
                             || peek(c) == ULT_SComma)
                                ? UNCIL_ERR(SYNTAX_OPTAFTERREQ)
                                : UNCIL_ERR(SYNTAX);
                    if (peek(c) == ULT_SParenR)
                        break;
                    else if (peek(c) != ULT_SComma)
                        return UNCIL_ERR(SYNTAX);
                    consume(c);
                }
            }
            consume(c);
        }

        switch (ftype) {
        case FUNCTYPE_EXPR:
            configure2x(c, UNC_QINSTR_OP_FMAKE,
                            QOPER_FUNCTION(c->out.fn_sz - 1));
            break;
        case FUNCTYPE_LOCAL:
            if (allowlocals(c)) {
                Unc_BTreeRecord *rec;
                int created;
                MUST(unc0_putbtree(c->book, z, &created, &rec));
                if (created || rec->first != UNC_QOPER_TYPE_LOCAL) {
                    Unc_Dst u;
                    MUST(localloc(c, &u));
                    rec->first = UNC_QOPER_TYPE_LOCAL;
                    rec->second = u;
                }
                MUST(emit2(c, UNC_QINSTR_OP_FMAKE, (byte)rec->first,
                                            qdataz(rec->second),
                                            QOPER_FUNCTION(c->out.fn_sz - 1)));
                break;
            }
        case FUNCTYPE_PUBLIC:
            MUST(emit2(c, UNC_QINSTR_OP_FMAKE, QOPER_PUBLIC(z),
                                        QOPER_FUNCTION(c->out.fn_sz - 1)));
            break;
        case FUNCTYPE_DICT:
            MUST(emit2(c, UNC_QINSTR_OP_FMAKE, QOPER_TMP(0),
                                        QOPER_FUNCTION(c->out.fn_sz - 1)));
            c->next.o2data = qdataz(z);
            break;
        }
    } else {
        if (ftype == FUNCTYPE_DICT)
            return UNCIL_ERR_SYNTAX_TBLNONAMEFUNC;
        configure2x(c, UNC_QINSTR_OP_FMAKE, QOPER_FUNCTION(c->out.fn_sz - 1));
        if (ftype != FUNCTYPE_EXPR) {
            c->valtype = VALTYPE_HOLD;
            MUST(killvalue(c));
        }
    }

    cnx = c->next;
    endstack(c);
    lrestore(c, &pre);
    {
        /* back up a whole bunch of the state for a context switch */
        Unc_BTree *parentbook = c->book;
        Unc_QInstr *parentcd = c->cd;
        Unc_Dst parentlocnext = c->locnext,
                parenttmpnext = c->tmpnext,
                parentinhnext = c->inhnext,
                parentexhnext = c->exhnext,
                parenttmphigh = c->tmphigh;
        Unc_Size parentcd_n = c->cd_n, parentcd_c = c->cd_c;
        Unc_Size parentlb_n = c->lb_n;
        Unc_Size parentaux_n = c->aux_n;
        Unc_Size *parentplb = c->plb;
        Unc_Size parentplb_n = c->plb_n;
        Unc_Size parentplb_c = c->plb_c;
        Unc_QOperand *parentinhl = c->inhl;
        Unc_Size parentinhl_c = c->inhl_c;
        unsigned parentallownl = c->allownl;
        Unc_Size parentpushfs = c->pushfs;
        Unc_Size parentcurfunc = c->curfunc;
        Unc_Size parentargc = c->argc;
        Unc_Size *parentarg_exh = c->arg_exh;
        Unc_BTree newbook;
        unc0_initbtree(&newbook, c->c.alloc);

        {
            Unc_ParserParent *pp;
            MUST(allocparent(c, &pp));
            pp->book = parentbook;
            pp->cd = &parentcd;
            pp->cd_n = &parentcd_n;
            pp->cd_c = &parentcd_c;
            pp->locnext = &parentlocnext;
            pp->tmphigh = &parenttmphigh;
            pp->exhnext = &parentexhnext;
            pp->inhnext = &parentinhnext;
            pp->inhl = &parentinhl;
            pp->inhl_c = &parentinhl_c;
            pp->argc = c->argc;
            pp->arg_exh = c->arg_exh;
        }
        
        c->book = &newbook;
        c->cd = NULL;
        c->cd_n = c->cd_c = 0;
        c->locnext = 0;
        c->tmphigh = c->tmpnext = 1;
        c->inhnext = 0;
        c->exhnext = 0;
        c->fence = 0;
        c->xfence = 0;
        c->pushfs = 0;
        c->curfunc = fnindex;
        c->plb = NULL;
        c->plb_n = c->plb_c = 0;
        c->argc = 0;

        MUST(unc0_iterbtreerecords(parentbook, &setupbinds, &newbook));

        /* read parameters */
        if (peek(c) != ULT_SParenL)
            return UNCIL_ERR(SYNTAX);
        consume(c);

        if (peek(c) == ULT_I || peek(c) == ULT_SEllipsis) {
            Unc_Size s, oarg = 0;
            int n;
            Unc_BTreeRecord *vrec;
            Unc_Dst l;
nextparam:
            if (peek(c) == ULT_SEllipsis) {
                consume(c);
                if (peek(c) != ULT_I)
                    return UNCIL_ERR(SYNTAX);
                consume(c);
                s = consumez(c);
                if (peek(c) != ULT_SParenR)
                    return UNCIL_ERR(SYNTAX);
                MUST(unc0_putbtree(c->book, s, &n, &vrec));
                if (n && vrec->first == UNC_QOPER_TYPE_LOCAL) {
                    /* duplicate parameter name */
                    return UNCIL_ERR(SYNTAX);
                }
                MUST(localloc(c, &l));
                ++c->argc;
                vrec->first = UNC_QOPER_TYPE_LOCAL;
                vrec->second = l;
                if (!c->quiet)
                    OUT_FUNCTION(c, fnindex)->flags
                        |= UNC_FUNCTION_FLAG_ELLIPSIS;
            } else {
                consume(c);
                s = consumez(c);
                MUST(unc0_putbtree(c->book, s, &n, &vrec));
                if (n && vrec->first == UNC_QOPER_TYPE_LOCAL) {
                    /* duplicate parameter name */
                    return UNCIL_ERR(SYNTAX);
                }
                MUST(localloc(c, &l));
                if (peek(c) == ULT_OSet) {
                    ++oarg;
                    consume(c);
                    qpause(c);
                    MUST(eatexpr(c, 0));
                    qresume(c);
                }
                ++c->argc;
                vrec->first = UNC_QOPER_TYPE_LOCAL;
                vrec->second = l;

                if (peek(c) == ULT_SComma) {
                    consume(c);
                    if (peek(c) != ULT_I && peek(c) != ULT_SEllipsis)
                        return UNCIL_ERR(SYNTAX);
                    goto nextparam;
                }
            }
            if (!c->quiet)
                OUT_FUNCTION(c, fnindex)->cnt_opt = oarg;
        }
        if (!c->quiet)
            OUT_FUNCTION(c, fnindex)->cnt_arg = c->argc;
        if (c->argc) {
            if (!(c->arg_exh = TMALLOCZ(Unc_Size, c->c.alloc, 0, c->argc)))
                return UNCIL_ERR(MEM);
        } else
            c->arg_exh = NULL;

        if (peek(c) != ULT_SParenR)
            return UNCIL_ERR(SYNTAX);
        consume(c);

        {
            unsigned oldallownl = c->allownl;
            c->allownl = 0;
            e = peek(c) == ULT_OSet;
            c->allownl = oldallownl;
        }

        /* begin eatblock */
        if (e) {
            /* expr mode */
            consume(c);
            MUST(eatexpr(c, 0));
            switch (c->valtype) {
            case VALTYPE_HOLD:
            case VALTYPE_FSTACK:
                MUST(emitre(c, QOPER_TMP(0)));
                MUST(emit1(c, UNC_QINSTR_OP_EXIT1, QOPER_TMP(0)));
                break;
            default:
                MUST(tostack(c));
                MUST(emit0(c, UNC_QINSTR_OP_END));
            }
            if (!endexprnext(c))
                return UNCIL_ERR(SYNTAX);
        } else {
            /* block */
            MUST(eatblock(c, BLOCK_KIND_FUNCTION));
            if (peek(c) != ULT_Kend)
                return UNCIL_ERR(SYNTAX);
            consume(c);
        }

        patchjumps(c, parentlb_n);
        e = patchboundargs(c);
        TMFREE(Unc_Size, c->c.alloc, c->arg_exh, c->argc);
        TMFREE(Unc_Size, c->c.alloc, c->plb, c->plb_c);
        if (!e) e = copystrs(c, 1);
        if (!e) e = copyids(c, 1);
        if (!c->quiet) {
            if (OUT_FUNCTION(c, fnindex)->flags & UNC_FUNCTION_FLAG_NAMED)
                OUT_FUNCTION(c, fnindex)->name = c->id_offset[
                            OUT_FUNCTION(c, fnindex)->name];
            endfunc(c, fnindex);
            if (c->inhl_c)
                OUT_FUNCTION(c, fnindex)->inhales = c->inhl;
        }
        unc0_dropbtree(&newbook);
        c->book = parentbook;
        c->cd = parentcd;
        c->cd_n = parentcd_n;
        c->cd_c = parentcd_c;
        c->allownl = parentallownl;
        c->locnext = parentlocnext;
        c->tmpnext = parenttmpnext;
        c->tmphigh = parenttmphigh;
        c->inhnext = parentinhnext;
        c->exhnext = parentexhnext;
        c->pushfs = parentpushfs;
        c->lb_n = parentlb_n;
        c->aux_n = parentaux_n;
        c->plb = parentplb;
        c->plb_n = parentplb_n;
        c->plb_c = parentplb_c;
        c->inhl = parentinhl;
        c->inhl_c = parentinhl_c;
        c->curfunc = parentcurfunc;
        c->argc = parentargc;
        c->arg_exh = parentarg_exh;
        --c->pframes_n;
    }

    if (ftype == FUNCTYPE_EXPR || ftype == FUNCTYPE_DICT)
        c->next = cnx;
    c->valtype = ftype == FUNCTYPE_EXPR ? VALTYPE_HOLD : VALTYPE_NONE;
    return e;
}

/* warning: clobbers lex */
Unc_RetVal unc0_parsec1(Unc_Context *cxt, Unc_QCode *out, Unc_LexOut *lex) {
    Unc_ParserContext c;
    Unc_Allocator *alloc = cxt->alloc;
    Unc_QCode s = { 1, 0, NULL, 0, NULL };
    Unc_RetVal e;
    Unc_Size sumidstr_n = lex->st_n + lex->id_n;
    Unc_BTree book;

    c.c = *cxt;
    c.lex = *lex;
    c.out = s;
    unc0_initbtree(&book, alloc);
    c.book = &book;
    c.cd = NULL;
    c.st = NULL;
    c.quiet = 0;
    c.allownl = 0;
    c.cd_n = 0;
    c.cd_c = 0;
    c.fence = 0;
    c.pushfs = 0;
    c.locnext = 0;
    c.tmphigh = c.tmpnext = 1; /* reserve temp0 for "single return value" */
    c.inhnext = c.exhnext = 0;
    c.lb = NULL;
    c.lb_n = 0;
    c.lb_c = 0;
    c.aux = NULL;
    c.aux_n = 0;
    c.aux_c = 0;
    c.plb = NULL;
    c.plb_n = 0;
    c.plb_c = 0;
    c.inhl = NULL;
    c.inhl_c = 0;
    c.pframes = NULL;
    c.pframes_n = 0;
    c.pframes_c = 0;
    c.curfunc = 0;
    c.valtype = VALTYPE_NONE;
    c.argc = 0;
    c.withbs = 0;
    c.withbanchor = 0;
    if (!sumidstr_n) {
        c.st_status = NULL;
        c.st_offset = NULL;
    } else {
        if (!(c.st_offset = TMALLOC(Unc_Size, alloc, 0, sumidstr_n)))
            return UNCIL_ERR(MEM);
        if (!(c.st_status = unc0_mallocz(alloc, 0, sumidstr_n))) {
            TMFREE(Unc_Size, alloc, c.st_offset, sumidstr_n);
            return UNCIL_ERR(MEM);
        }
    }
    c.id_status = c.st_status + lex->st_n;
    c.id_offset = c.st_offset + lex->st_n;
    scanstrs(c.st_offset, lex->st, lex->st_n, lex->st_sz);
    scanstrs(c.id_offset, lex->id, lex->id_n, lex->id_sz);
    {
        Unc_Size s;
        e = startfunc(&c, &s, 0);
        ASSERT(s == 0);
    }
    if (!e)
        e = eatblock(&c, BLOCK_KIND_MAIN);
    if (UNCIL_ERR_KIND(e) == UNCIL_ERR_KIND_SYNTAXL)
        e = UNCIL_ERR_SYNTAX;
    if (e) {
        /* free stuff */
        TMFREE(Unc_QInstr, alloc, c.cd, c.cd_c);
    } else {
        /* replace labels, strings, IDs */
        patchjumps(&c, 0);
        c.c.main_dta = c.out.st_sz;
        e = copystrs(&c, 0);
        if (!e) e = copyids(&c, 0);
        endfunc(&c, 0);
    }
    unc0_mfree(alloc, lex->lc, lex->lc_sz);
    unc0_mfree(alloc, lex->st, lex->st_sz);
    unc0_mfree(alloc, lex->id, lex->id_sz);
    unc0_mfree(alloc, c.st_status, sumidstr_n);
    unc0_dropbtree(&book);
    TMFREE(Unc_ParserParent, alloc, c.pframes, c.pframes_c);
    TMFREE(Unc_Size, alloc, c.st_offset, sumidstr_n);
    TMFREE(Unc_Size, alloc, c.plb, c.plb_c);
    TMFREE(Unc_QOperand, alloc, c.aux, c.aux_c);
    TMFREE(Unc_Size, alloc, c.lb, c.lb_c);
    lex->id = lex->lc = NULL;
    lex->id_sz = lex->lc_sz = 0;
    c.out.st = c.st;
    *cxt = c.c;
    *out = c.out;
    return e;
}
