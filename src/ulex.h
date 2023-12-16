/*******************************************************************************
 
Uncil -- lexer (text -> L-code) header

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

#ifndef UNCIL_ULEX_H
#define UNCIL_ULEX_H

#include "ucxt.h"
#include "udef.h"
#include "uhash.h"

typedef enum Unc_LexToken {
    ULT_END = 0,        /* no more */
    ULT_N,              /* semicolon, etc. end of line */
    ULT_NL,             /* actual newline */
    ULT_LInt,           /* literals */
    ULT_LFloat,
    ULT_LStr,
    ULT_I,              /* identifier */
    ULT_SParenL,        /* symbols */
    ULT_SParenR,
    ULT_SBracketL,
    ULT_SBracketR,
    ULT_SBraceL,
    ULT_SBraceR,
    ULT_SEllipsis,
    ULT_SComma,
    ULT_SColon,
    ULT_OSet,           /* operators */
    ULT_ODotQue,        /* .? */
    ULT_OQque,          /* ?? */
    ULT_ODot,
    ULT_OArrow,         /* -> */
    ULT_Kand,           /* with these operators also see parse1 op_precs */
    ULT_Kor,
    ULT_OAdd,
    ULT_OSub,
    ULT_OMul,
    ULT_ODiv,
    ULT_OIdiv,          /* // */
    ULT_OMod,
    ULT_OBshl,
    ULT_OBshr,
    ULT_OBand,
    ULT_OBxor,
    ULT_OBor,
    ULT_OCEq,
    ULT_OCNe,
    ULT_OCLt,
    ULT_OCLe,
    ULT_OCGt,
    ULT_OCGe,
    ULT_OBinv,          /* ~ */
    ULT_Kfunction,      /* keywords */
    ULT_Kif,
    ULT_Kfor,
    ULT_Kelse,
    ULT_Kwhile,
    ULT_Kdo,
    ULT_Kthen,
    ULT_Kend,
    ULT_Ktry,
    ULT_Kcatch,
    ULT_Knot,
    ULT_Kpublic,
    ULT_Kreturn,
    ULT_Kbreak,
    ULT_Kcontinue,
    ULT_Ktrue,
    ULT_Kfalse,
    ULT_Knull,
    ULT_Kdelete,
    ULT_Kwith
} Unc_LexToken;

/* L-Code structure
   byte: Unc_LexToken
   if ULT_LInt, followed by Unc_Int
   if ULT_LFloat, followed by Unc_Float
   if ULT_LStr, ULT_I or ULT_IP, followed by Unc_Size
        (index for ULT_LStr, ULT_I, ULT_P) */

typedef struct Unc_LexOut {
    Unc_Size lineno;      /* in case of failure */
    Unc_Size lc_sz;       /* L-code and size */
    byte *lc;
    Unc_Size st_sz;       /* literal string table and size */
    byte *st;
    Unc_Size id_sz;       /* identifier string table and size */
    byte *id;
    Unc_Size st_n;        /* number of strings */
    Unc_Size id_n;        /* one past highest index of identifiers */
} Unc_LexOut;

Unc_RetVal unc0_lexcode(Unc_Context *cxt, Unc_LexOut *out,
                        int (*getch)(void *ud), void *ud);

#endif /* UNCIL_ULEX_H */
