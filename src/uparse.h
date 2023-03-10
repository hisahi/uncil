/*******************************************************************************
 
Uncil -- parser step 1 (L-code -> Q-code) header

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

#ifndef UNCIL_UPARSE_H
#define UNCIL_UPARSE_H

#include <limits.h>

#include "ucxt.h"
#include "udef.h"
#include "ulex.h"

typedef unsigned Unc_Dst;
#define UNC_DST_MAX UINT_MAX

#define UNC_QINSTR_OP_DELETE 0
#define UNC_QINSTR_OP_MOV 1
#define UNC_QINSTR_OP_ADD 2
#define UNC_QINSTR_OP_SUB 3
#define UNC_QINSTR_OP_MUL 4
#define UNC_QINSTR_OP_DIV 5
#define UNC_QINSTR_OP_IDIV 6
#define UNC_QINSTR_OP_MOD 7
#define UNC_QINSTR_OP_SHL 8
#define UNC_QINSTR_OP_SHR 9
#define UNC_QINSTR_OP_CAT 10
#define UNC_QINSTR_OP_AND 11
#define UNC_QINSTR_OP_OR 12
#define UNC_QINSTR_OP_XOR 13
#define UNC_QINSTR_OP_CEQ 14
#define UNC_QINSTR_OP_CLT 15
#define UNC_QINSTR_OP_JMP 16
#define UNC_QINSTR_OP_IFT 17
#define UNC_QINSTR_OP_IFF 18
#define UNC_QINSTR_OP_PUSH 19
#define UNC_QINSTR_OP_UPOS 20
#define UNC_QINSTR_OP_UNEG 21
#define UNC_QINSTR_OP_UXOR 22
#define UNC_QINSTR_OP_LNOT 23
#define UNC_QINSTR_OP_EXPUSH 24
#define UNC_QINSTR_OP_EXPOP 25
#define UNC_QINSTR_OP_GATTR 26
#define UNC_QINSTR_OP_GATTRQ 27
#define UNC_QINSTR_OP_SATTR 28
#define UNC_QINSTR_OP_GINDX 29
#define UNC_QINSTR_OP_GINDXQ 30
#define UNC_QINSTR_OP_SINDX 31
#define UNC_QINSTR_OP_PUSHF 32
#define UNC_QINSTR_OP_POPF 33
#define UNC_QINSTR_OP_FCALL 34
#define UNC_QINSTR_OP_GPUB 35
#define UNC_QINSTR_OP_SPUB 36
#define UNC_QINSTR_OP_IITER 37
#define UNC_QINSTR_OP_INEXT 38
#define UNC_QINSTR_OP_FMAKE 39
#define UNC_QINSTR_OP_MLIST 40
#define UNC_QINSTR_OP_NDICT 41
#define UNC_QINSTR_OP_GBIND 42
#define UNC_QINSTR_OP_SBIND 43
#define UNC_QINSTR_OP_SPREAD 44
#define UNC_QINSTR_OP_MLISTP 45
#define UNC_QINSTR_OP_STKEQ 46
#define UNC_QINSTR_OP_STKGE 47
#define UNC_QINSTR_OP_GATTRF 48
#define UNC_QINSTR_OP_INEXTS 49
#define UNC_QINSTR_OP_DPUB 50
#define UNC_QINSTR_OP_DATTR 51
#define UNC_QINSTR_OP_DINDX 52
#define UNC_QINSTR_OP_FBIND 53
#define UNC_QINSTR_OP_WPUSH 54
#define UNC_QINSTR_OP_WPOP 55
#define UNC_QINSTR_OP_PUSHW 56
#define UNC_QINSTR_OP_DCALL 57
#define UNC_QINSTR_OP_FTAIL 58
#define UNC_QINSTR_OP_DTAIL 59
#define UNC_QINSTR_OP_EXIT0 60
#define UNC_QINSTR_OP_EXIT1 61

#define UNC_QINSTR_OP_NOP 254
#define UNC_QINSTR_OP_END 255

/* undefined */
#define UNC_QOPER_TYPE_NONE 0
/* temporary (o = index), source/sink */
#define UNC_QOPER_TYPE_TMP 1
/* local variable (o = index), source/sink */
#define UNC_QOPER_TYPE_LOCAL 2
/* bound value (o = index), source/sink */
#define UNC_QOPER_TYPE_EXHALE 3
/* bound value (o = index), source/sink */
#define UNC_QOPER_TYPE_INHALE 4
/* public value (addressed by name, o = index into string table), source/sink */
#define UNC_QOPER_TYPE_PUBLIC 5
/* integer literal (ui), source only */
#define UNC_QOPER_TYPE_INT 6
/* float literal (uf), source only */
#define UNC_QOPER_TYPE_FLOAT 7
/* null, source only */
#define UNC_QOPER_TYPE_NULL 8
/* string literal (o = offset into table), source only */
#define UNC_QOPER_TYPE_STR 9
/* jump target (o = offset into q-code), source only */
#define UNC_QOPER_TYPE_JUMP 10
/* stack (either as sink, or source with o = index) */
#define UNC_QOPER_TYPE_STACK 11
/* identifier (o = index into string table), source only */
#define UNC_QOPER_TYPE_IDENT 12
/* boolean false, source only */
#define UNC_QOPER_TYPE_FALSE 13
/* boolean true, source only */
#define UNC_QOPER_TYPE_TRUE 14
/* function (o = index into table), source only (for FMAKE) */
#define UNC_QOPER_TYPE_FUNCTION 15
/* stack from the end (source only) */
#define UNC_QOPER_TYPE_STACKNEG 16
/* arbitrary unsigned parameter for some special instrs */
#define UNC_QOPER_TYPE_UNSIGN 17
/* with-stack, sink only */
#define UNC_QOPER_TYPE_WSTACK 18

/* these are only used internally by parse1
   uoptim may also flag instructions with high o0/o1type temporarily */
/* used for attributes, uses aux operand stack. write only */
#define UNC_QOPER_TYPE_ATTR 128
/* used for indexes, uses aux operand stack. write only */
#define UNC_QOPER_TYPE_INDEX 129
/* function call pushed values into the stack, read only */
#define UNC_QOPER_TYPE_FUNCSTACK 130
/* bindable; may be instantiated. o = number of levels up */
#define UNC_QOPER_TYPE_BINDABLE 131
/* UNC_QOPER_TYPE_STR but uses ident */
#define UNC_QOPER_TYPE_STRIDENT 132

/* rest of structs */

union Unc_QInstr_Data {
    Unc_Size o;
    Unc_Int ui;
    Unc_Float uf;
};

int unc0_qcode_isread0op(byte op);
int unc0_qcode_ismov(byte op);
int unc0_qcode_iswrite0op(byte op);
int unc0_qcode_isjump(byte op);
int unc0_qcode_isexit(byte op);
int unc0_qcode_operandcount(byte op);
int unc0_qcode_isopreg(int t);
int unc0_qcode_isoplit(int t);
int unc0_qcode_getjumpd(byte op);

typedef struct Unc_QOperand {
    union Unc_QInstr_Data data;
    byte type;
} Unc_QOperand;

typedef struct Unc_QInstr {
    byte op;
    byte o0type;
    byte o1type;
    byte o2type;
    Unc_Dst o0data;
    union Unc_QInstr_Data o1data;
    union Unc_QInstr_Data o2data;
    Unc_Size lineno;
} Unc_QInstr;

#define UNC_FUNC_NONAME SIZE_MAX
#define UNC_FUNC_HASNAME(f) ((f)->name != SIZE_MAX)

typedef struct Unc_QFunc {
    Unc_Size lineno;        /* starting line number */
    Unc_Dst cnt_tmp, cnt_loc, cnt_exh, cnt_inh, cnt_arg, cnt_opt;
    int flags;
    Unc_Size cd_sz;
    Unc_QInstr *cd;         /* Q-code */
    Unc_Size name;          /* offset into string table */
    Unc_Size parent;        /* index of parent function or 0 for none */
    Unc_QOperand *inhales;  /* which must itself be either exhales or inhales */
} Unc_QFunc;

typedef struct Unc_QCode {
    Unc_Size lineno;        /* line number */
    Unc_Size fn_sz;
    Unc_QFunc *fn;          /* functions (first is the script itself) */
    Unc_Size st_sz;
    byte *st;               /* string table */
} Unc_QCode;

int unc0_parsec1(Unc_Context *cxt, Unc_QCode *out, Unc_LexOut *lex);

#endif /* UNCIL_UPARSE_H */
