/*******************************************************************************
 
Uncil -- lexer (text -> L-code)

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
#include <string.h>

#define UNCIL_DEFINES

#include "uctype.h"
#include "udebug.h"
#include "udef.h"
#include "uerr.h"
#include "uhash.h"
#include "ulex.h"
#include "umem.h"
#include "uutf.h"

INLINE int isbdigit_(int c) {
    return '0' == c || c == '1';
}

INLINE int isodigit_(int c) {
    return '0' <= c && c <= '7';
}

INLINE int isxdigit_(int c) {
    return unc0_isdigit(c) || ('A' <= c && c <= 'F') || ('a' <= c && c <= 'f');
}

INLINE int isdigitb_(int c, int b) {
    switch (b) {
    case 2:
        return isbdigit_(c);
    case 8:
        return isodigit_(c);
    case 16:
        return isxdigit_(c);
    }
    return unc0_isdigit(c);
}

INLINE int getdigitx_(int c) {
    if (c >= 'a')
        return 10 + c - 'a';
    else if (c >= 'A')
        return 10 + c - 'A';
    return c - '0';
}

INLINE int getdigitb_(int c, int b) {
    if (b > 10) {
        if (c >= 'a')
            return 10 + c - 'a';
        else if (c >= 'A')
            return 10 + c - 'A';
    }
    return c - '0';
}

INLINE int isspace_(int c) {
    return !c || unc0_isspace(c);
}

INLINE int isident_(int c) {
    return unc0_isdigit(c) || unc0_isalnum(c) || c == '_';
}

INLINE Unc_Float pow10_(long e) {
    Unc_Float f = 1, m = 10;
    if (e < 0)
        return 1 / pow10_(-e);
    else if (!e)
        return 1;
    for (; e > 0; e >>= 1) {
        if (e & 1) f *= m;
        m *= m;
    }
    return f;
}

#define KWFOUND(target, kw) return (target = kw, 1)
/* look up keyword with a trie-like structure */
static int kwtrie(const byte *sb, Unc_LexToken *nx) {
    const char *s = (const char *)sb;
    switch (*s++) {
    case 'a':
        if (!strcmp(s, "nd"))
            KWFOUND(*nx, ULT_Kand);
        break;
    case 'b':
        if (!strcmp(s, "reak"))
            KWFOUND(*nx, ULT_Kbreak);
        break;
    case 'c':
        switch (*s++) {
        case 'a':
            if (!strcmp(s, "tch"))
                KWFOUND(*nx, ULT_Kcatch);
            break;
        case 'o':
            if (!strcmp(s, "ntinue"))
                KWFOUND(*nx, ULT_Kcontinue);
        }
        break;
    case 'd':
        switch (*s++) {
        case 'e':
            if (!strcmp(s, "lete"))
                KWFOUND(*nx, ULT_Kdelete);
            break;
        case 'o':
            if (!strcmp(s, ""))
                KWFOUND(*nx, ULT_Kdo);
        }
        break;
    case 'e':
        switch (*s++) {
        case 'l':
            if (!strcmp(s, "se"))
                KWFOUND(*nx, ULT_Kelse);
            break;
        case 'n':
            if (!strcmp(s, "d"))
                KWFOUND(*nx, ULT_Kend);
        }
        break;
    case 'f':
        switch (*s++) {
        case 'a':
            if (!strcmp(s, "lse"))
                KWFOUND(*nx, ULT_Kfalse);
            break;
        case 'o':
            if (!strcmp(s, "r"))
                KWFOUND(*nx, ULT_Kfor);
            break;
        case 'u':
            if (!strcmp(s, "nction"))
                KWFOUND(*nx, ULT_Kfunction);
        }
        break;
    case 'i':
        if (!strcmp(s, "f"))
            KWFOUND(*nx, ULT_Kif);
        break;
    case 'n':
        switch (*s++) {
        case 'o':
            if (!strcmp(s, "t"))
                KWFOUND(*nx, ULT_Knot);
            break;
        case 'u':
            if (!strcmp(s, "ll"))
                KWFOUND(*nx, ULT_Knull);
        }
        break;
    case 'o':
        if (!strcmp(s, "r"))
            KWFOUND(*nx, ULT_Kor);
        break;
    case 'p':
        if (!strcmp(s, "ublic"))
            KWFOUND(*nx, ULT_Kpublic);
        break;
    case 'r':
        if (!strcmp(s, "eturn"))
            KWFOUND(*nx, ULT_Kreturn);
        break;
    case 't':
        switch (*s++) {
        case 'h':
            if (!strcmp(s, "en"))
                KWFOUND(*nx, ULT_Kthen);
            break;
        case 'r':
            switch (*s++) {
            case 'u':
                if (!strcmp(s, "e"))
                    KWFOUND(*nx, ULT_Ktrue);
                break;
            case 'y':
                if (!strcmp(s, ""))
                    KWFOUND(*nx, ULT_Ktry);
            }
        }
        break;
    case 'w':
        switch (*s++) {
        case 'h':
            if (!strcmp(s, "ile"))
                KWFOUND(*nx, ULT_Kwhile);
            break;
        case 'i':
            if (!strcmp(s, "th"))
                KWFOUND(*nx, ULT_Kwith);
            break;
        }
        break;
    }
    return 0;
}

int unc0_lexcode_i(Unc_Context *cxt, Unc_LexOut *out,
                   int (*getch)(void *ud), void *ud) {
    int c, err = 0, sign = 0;
    Unc_LexToken nx = 0;
    Unc_Allocator *alloc = cxt->alloc;
    byte *olc = NULL; Unc_Size olc_n = 0, olc_c = 0;
    byte *ost = NULL; Unc_Size ost_n = 0, ost_c = 0;
    byte *oid = NULL; Unc_Size oid_n = 0, oid_c = 0;
    Unc_HSet hstr, hid;
    Unc_Size stcount = 0, idcount = 0;

    unc0_inithset(&hstr, alloc);
    unc0_inithset(&hid, alloc);
    out->lineno = 1;

    c = getch(ud);
    for (;;) {
        if (c < 0) {
            if ((err = unc0_strput(alloc, &olc, &olc_n, &olc_c, 2, ULT_END)))
                goto lexreturn;
            break;
        } else if (c == '\n') {
            nx = ULT_NL;
            ++out->lineno;
            c = getch(ud);
        } else if (isspace_(c)) {
            /* ignore */
            c = getch(ud);
            continue;
        } else if (unc0_isdigit(c)) {
            sign = 0;
readnum:
            /* read in number */
            {
                byte buf[1 + (sizeof(Unc_Int) > sizeof(Unc_Float)
                            ? sizeof(Unc_Int) : sizeof(Unc_Float))];
                int dot = sign & 1, base = 10;
                union {
                    Unc_Int i;
                    Unc_Float f;
                } u;
                long exp = 0, off = 0;
                dot ? (u.f = 0) : (u.i = 0);

                if (!dot) {
                    Unc_Int ni;
                    if (c == '0') {
                        c = getch(ud);
                        if (c == 'x') {
                            base = 16;
                            c = getch(ud);
                            if (!isxdigit_(c))
                                { err = UNCIL_ERR_SYNTAX; goto lexreturn; }
                        } else if (c == 'o') {
                            base = 8;
                            c = getch(ud);
                            if (!isodigit_(c))
                                { err = UNCIL_ERR_SYNTAX; goto lexreturn; }
                        } else if (c == 'b') {
                            base = 2;
                            c = getch(ud);
                            if (!isbdigit_(c))
                                { err = UNCIL_ERR_SYNTAX; goto lexreturn; }
                        } else if (!unc0_isdigit(c)) {
                            goto waszero;
                        }
                    }
                    while (isdigitb_(c, base)) {
                        ni = u.i * base + getdigitb_(c, base);
                        if (ni < u.i) {
                            /* overflow */
                            u.f = u.i;
                            while (isdigitb_(c, base)) {
                                u.f = u.f * base + getdigitb_(c, base);
                                c = getch(ud);
                            }
                            dot = 1;
                            break;
                        }
                        u.i = ni;
                        c = getch(ud);
                    }
waszero:
                    if (c == '.') {
                        dot = 1;
                        u.f = u.i;
                        c = getch(ud);
                        if (base != 10)
                            { err = UNCIL_ERR_SYNTAX; goto lexreturn; }
                    } else if (c == 'E' || c == 'e') {
                        dot = 1;
                        u.f = u.i;
                    }
                }

                if (dot) {
                    while (unc0_isdigit(c)) {
                        u.f = u.f * 10 + c - '0';
                        c = getch(ud);
                        --off;
                        if (off >= 0) /* should never happen */
                            { err = UNCIL_ERR_SYNTAX; goto lexreturn; }
                    }

                    if (c == 'E' || c == 'e') {
                        Unc_Int ni;
                        int eneg = 0;
                        c = getch(ud);
                        if (c == '+')
                            c = getch(ud);
                        else if (c == '-')
                            eneg = 1, c = getch(ud);
                        if (!unc0_isdigit(c))
                            { err = UNCIL_ERR_SYNTAX; goto lexreturn; }
                        
                        while (unc0_isdigit(c)) {
                            ni = exp * 10 + c - '0';
                            if (ni < exp) {
                                exp = eneg ? LONG_MIN : LONG_MAX;
                                goto noexpneg;
                            }
                            exp = ni;
                            c = getch(ud);
                        }
                        if (eneg) exp = -exp;
noexpneg:
                        if (exp + off <= exp)
                            off += exp;
                    }

                    if (off)
                        u.f = u.f * pow10_(off);
                    
                    buf[0] = ULT_LFloat;
                    unc0_memcpy(buf + 1, &u.f, sizeof(Unc_Float));
                } else {
                    buf[0] = ULT_LInt;
                    unc0_memcpy(buf + 1, &u.i, sizeof(Unc_Int));
                }
                if ((err = unc0_strputn(alloc, &olc, &olc_n, &olc_c, 6,
                                    sizeof(buf), buf)))
                    goto lexreturn;
            }
            continue;
        } else if (isident_(c)) {
            /* read in identifier */
            Unc_Size oid_n2 = oid_n, res;

            while (isident_(c)) {
                if ((err = unc0_strput(alloc, &oid, &oid_n2, &oid_c, 5, c)))
                    goto lexreturn;
                c = getch(ud);
            }
            if ((err = unc0_strput(alloc, &oid, &oid_n2, &oid_c, 5, 0)))
                goto lexreturn;
            /* handle elseif as else + if as a special case */
            if (!strcmp((const char *)oid + oid_n, "elseif")) {
                byte buf[] = { ULT_Kelse, ULT_Kif };
                if ((err = unc0_strputn(alloc, &olc, &olc_n, &olc_c, 6,
                                    sizeof(buf), buf)))
                    goto lexreturn;
                continue;
            } else if (!kwtrie(oid + oid_n, &nx)) {
                byte buf[1 + sizeof(Unc_Size)];
                if ((err = unc0_puthset(&hid, oid_n2 - oid_n, oid + oid_n,
                                        &res, idcount)))
                    goto lexreturn;
                buf[0] = ULT_I;
                unc0_memcpy(buf + 1, &res, sizeof(Unc_Size));
                if (res == idcount)
                    oid_n = oid_n2, ++idcount;
                if ((err = unc0_strputn(alloc, &olc, &olc_n, &olc_c, 6,
                                    sizeof(buf), buf)))
                    goto lexreturn;
                continue;
            }
        } else if (c == '"') {
            /* read in string */
            byte buf[1 + sizeof(Unc_Size)];
            Unc_Size ost_n2 = ost_n, res, inc = 4;

            c = getch(ud);
            for (;;) {
                if (c < 0 || c == '\n') {
                    err = UNCIL_ERR_SYNTAX_UNTERMSTR;
                    goto lexreturn;
                } else if (c == '"') {
                    c = getch(ud);
                    break;
                } else if (c == '\\') {
                    Unc_UChar uc;
                    int o;
                    c = getch(ud);
                    if (c < 0) {
                        err = UNCIL_ERR_SYNTAX_UNTERMSTR;
                        goto lexreturn;
                    }
                    switch (c) {
                    case '\n':
                    case '\\':
                    case '"':
                        break;
                    case '0': /* convert to $c0 $80 temporarily */
                        if ((err = unc0_strput(alloc, &ost, &ost_n2, &ost_c,
                                            inc, 0xc0)))
                            goto lexreturn;
                        c = 0x80;
                        break;
                    case 'b':
                        c = '\b';
                        break;
                    case 'f':
                        c = '\f';
                        break;
                    case 'n':
                        c = '\n';
                        break;
                    case 'r':
                        c = '\r';
                        break;
                    case 't':
                        c = '\t';
                        break;
                    case 'x':
                        o = 2;
                        goto unicode;
                    case 'u':
                        o = 4;
                        goto unicode;
                    case 'U':
                        o = 8;
                        goto unicode;
                    unicode:
                        uc = 0;
                        c = getch(ud);
                        while (o--) {
                            if (!isxdigit_(c)) {
                                err = UNCIL_ERR_SYNTAX_BADUESCAPE;
                                goto lexreturn;
                            }
                            uc = (uc << 4) | getdigitx_(c);
                            c = getch(ud);
                        }
                        if (uc >= 0x110000) {
                            err = UNCIL_ERR_SYNTAX_BADUESCAPE;
                            goto lexreturn;
                        }
                        {
                            byte ubuf[UNC_UTF8_MAX_SIZE];
                            Unc_Size n = unc0_utf8enc(uc, sizeof(ubuf), ubuf);
                            if ((err = unc0_strputn(alloc, &ost, &ost_n2,
                                                &ost_c, inc, n, ubuf)))
                                goto lexreturn;
                        }
                        continue;
                    default:
                        err = UNCIL_ERR_SYNTAX_BADESCAPE;
                        goto lexreturn;
                    }
                }
                if ((err = unc0_strput(alloc, &ost, &ost_n2, &ost_c, inc, c)))
                    goto lexreturn;
                if (inc < 7)
                    ++inc;
                c = getch(ud);
            }
            if ((err = unc0_strput(alloc, &ost, &ost_n2, &ost_c, inc, 0)))
                goto lexreturn;
            if (ost_n2 - ost_n < 256) {
                /* check hash */
                if ((err = unc0_puthset(&hstr, ost_n2 - ost_n, ost + ost_n,
                                        &res, stcount)))
                    goto lexreturn;
            } else {
                res = stcount;
            }
            buf[0] = ULT_LStr;
            unc0_memcpy(buf + 1, &res, sizeof(Unc_Size));
            if ((err = unc0_strputn(alloc, &olc, &olc_n, &olc_c, 6,
                                sizeof(buf), buf)))
                goto lexreturn;
            if (res == stcount)
                ost_n = ost_n2, ++stcount;
            continue;
        } else {
            /* punctuation symbol table */
            switch (c) {
            case '!':
                c = getch(ud);
                if (c == '=')
                    nx = ULT_OCNe;
                else {
                    err = UNCIL_ERR_SYNTAX; goto lexreturn;
                }
                break;
            case '#':
                /* comment, consume here */
                c = getch(ud);
                if (c == '<') {
                    /* #< block comment ># */
                    while (c >= 0) {
                        while (c >= 0 && c != '>')
                            c = getch(ud);
                        if (c < 0) break;
                        c = getch(ud);
                        if (c == '#') {
                            c = getch(ud);
                            break;
                        }
                    }
                } else {
                    while (c >= 0 && c != '\n')
                        c = getch(ud);
                }
                continue;
            case '%':
                nx = ULT_OMod;
                break;
            case '&':
                nx = ULT_OBand;
                break;
            case '(':
                nx = ULT_SParenL;
                break;
            case ')':
                nx = ULT_SParenR;
                break;
            case '*':
                nx = ULT_OMul;
                break;
            case '+':
                nx = ULT_OAdd;
                break;
            case ',':
                nx = ULT_SComma;
                break;
            case '-':
                c = getch(ud);
                if (c == '>')
                    nx = ULT_OArrow;
                else {
                    nx = ULT_OSub;
                    goto keep_buf;
                }
                break;
            case '.':
                c = getch(ud);
                if (unc0_isdigit(c)) {
                    sign = 1;
                    goto readnum;
                } else if (c == '?') {
                    nx = ULT_ODotQue;
                } else if (c == '.') {
                    c = getch(ud);
                    if (c == '.') {
                        nx = ULT_SEllipsis;
                    } else { /* .. + not-, must be wrong */
                        err = UNCIL_ERR_SYNTAX; goto lexreturn;
                    }
                } else {
                    nx = ULT_ODot;
                    goto keep_buf;
                }
                break;
            case '/':
                c = getch(ud);
                if (c == '/') {
                    nx = ULT_OIdiv;
                } else {
                    nx = ULT_ODiv;
                    goto keep_buf;
                }
                break;
            case ':':
                nx = ULT_SColon;
                break;
            case ';':
                nx = ULT_N;
                break;
            case '<':
                c = getch(ud);
                if (c == '=') {
                    nx = ULT_OCLe;
                } else if (c == '<') {
                    nx = ULT_OBshl;
                } else {
                    nx = ULT_OCLt;
                    goto keep_buf;
                }
                break;
            case '=':
                c = getch(ud);
                if (c == '=') {
                    nx = ULT_OCEq;
                } else {
                    nx = ULT_OSet;
                    goto keep_buf;
                }
                break;
            case '>':
                c = getch(ud);
                if (c == '=') {
                    nx = ULT_OCGe;
                } else if (c == '>') {
                    nx = ULT_OBshr;
                } else {
                    nx = ULT_OCGt;
                    goto keep_buf;
                }
                break;
            case '?':
                c = getch(ud);
                if (c == '?') {
                    nx = ULT_OQque;
                } else {
                    err = UNCIL_ERR_SYNTAX; goto lexreturn;
                }
                break;
            case '[':
                nx = ULT_SBracketL;
                break;
            case ']':
                nx = ULT_SBracketR;
                break;
            case '^':
                nx = ULT_OBxor;
                break;
            case '{':
                nx = ULT_SBraceL;
                break;
            case '|':
                nx = ULT_OBor;
                break;
            case '}':
                nx = ULT_SBraceR;
                break;
            case '~':
                nx = ULT_OBinv;
                break;
            default:
                err = UNCIL_ERR_SYNTAX; goto lexreturn;
            }
            if (c >= 0)
                c = getch(ud);
keep_buf:   ;
        }
        if ((err = unc0_strput(alloc, &olc, &olc_n, &olc_c, 6, nx)))
            goto lexreturn;
    }
    
lexreturn:
    if (err) {
        unc0_mfree(alloc, olc, olc_c);
        unc0_mfree(alloc, ost, ost_c);
        unc0_mfree(alloc, oid, oid_c);
    } else {
        Unc_Size s = olc_n;
        while (s < olc_c)
            olc[s++] = 0;
        out->lc_sz = olc_c, out->lc = olc;
        out->st = unc0_mrealloc(alloc, 0, ost, ost_c, ost_n);
        out->id = unc0_mrealloc(alloc, 0, oid, oid_c, oid_n);
        out->st_sz = ost_n, out->st_n = stcount;
        out->id_sz = oid_n, out->id_n = idcount;
    }
    unc0_drophset(&hstr);
    unc0_drophset(&hid);
    return err;
}

struct utf8_checker {
    int (*getch)(void *ud);
    void *ud;
    int count;
    Unc_UChar u;
    Unc_UChar ou;
    int invalid;
    int eof;
};

static int utf8_checker_getch_(void *ud) {
    int c;
    struct utf8_checker *buf = ud;
    if (buf->eof)
        return -1;
    c = (*buf->getch)(buf->ud);
    if (c < 0)
        buf->eof = 1;
    if (buf->count) {
        if ((c & 0xC0) != 0x80) {
            buf->eof = buf->invalid = 1;
            return -1;
        }
        buf->u = (buf->u << 6) | (c & 0x3F);
        if (!--buf->count) {
            if (buf->u < buf->ou) {
                buf->eof = buf->invalid = 1;
                return -1;
            }
        }
    } else {
        if (c < 0x80)
            ;
        else if ((c & 0xE0) == 0xC0) {
            buf->count = 1;
            buf->u = c & 0x1F;
            buf->ou = 0x80UL;
        } else if ((c & 0xF0) == 0xE0) {
            buf->count = 2;
            buf->u = c & 0x0F;
            buf->ou = 0x800UL;
        } else if ((c & 0xF8) == 0xF0) {
            buf->count = 3;
            buf->u = c & 0x07;
            buf->ou = 0x10000UL;
        } else {
            buf->eof = buf->invalid = 1;
            return -1;
        } 
    }
    return c;
}

int unc0_lexcode(Unc_Context *cxt, Unc_LexOut *out,
                 int (*getch)(void *ud), void *ud) {
    int e;
    struct utf8_checker buf;
    buf.getch = getch;
    buf.ud = ud;
    buf.count = 0;
    buf.u = 0U;
    buf.invalid = 0;
    buf.eof = 0;
    e = unc0_lexcode_i(cxt, out, &utf8_checker_getch_, &buf);
    if (buf.invalid)
        return UNCIL_ERR_IO_INVALIDENCODING;
    return e;
}
