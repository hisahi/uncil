/*******************************************************************************
 
Uncil -- lexer (text -> L-code)

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

#include <limits.h>

#define UNCIL_DEFINES

#include "uarithm.h"
#include "uctype.h"
#include "udebug.h"
#include "udef.h"
#include "uerr.h"
#include "uhash.h"
#include "ulex.h"
#include "umem.h"
#include "uutf.h"

#define SWAP(T, a, b) do { T _tmp = a; a = b; b = _tmp; } while (0)

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

INLINE int kwmatch(const char *s, const char *kw) {
    while (*kw)
        if (*kw++ != *s++)
            return 0;
    return !*s;
}

#define KWFOUND(target, kw) return (target = kw, 1)
/* look up keyword with a trie-like structure */
static int kwtrie(const byte *sb, Unc_LexToken *nx) {
    const char *s = (const char *)sb;
    switch (*s++) {
    case 'a':
        if (kwmatch(s, "nd"))
            KWFOUND(*nx, ULT_Kand);
        break;
    case 'b':
        if (kwmatch(s, "reak"))
            KWFOUND(*nx, ULT_Kbreak);
        break;
    case 'c':
        switch (*s++) {
        case 'a':
            if (kwmatch(s, "tch"))
                KWFOUND(*nx, ULT_Kcatch);
            break;
        case 'o':
            if (kwmatch(s, "ntinue"))
                KWFOUND(*nx, ULT_Kcontinue);
        }
        break;
    case 'd':
        switch (*s++) {
        case 'e':
            if (kwmatch(s, "lete"))
                KWFOUND(*nx, ULT_Kdelete);
            break;
        case 'o':
            if (kwmatch(s, ""))
                KWFOUND(*nx, ULT_Kdo);
        }
        break;
    case 'e':
        switch (*s++) {
        case 'l':
            if (kwmatch(s, "se"))
                KWFOUND(*nx, ULT_Kelse);
            break;
        case 'n':
            if (kwmatch(s, "d"))
                KWFOUND(*nx, ULT_Kend);
        }
        break;
    case 'f':
        switch (*s++) {
        case 'a':
            if (kwmatch(s, "lse"))
                KWFOUND(*nx, ULT_Kfalse);
            break;
        case 'o':
            if (kwmatch(s, "r"))
                KWFOUND(*nx, ULT_Kfor);
            break;
        case 'u':
            if (kwmatch(s, "nction"))
                KWFOUND(*nx, ULT_Kfunction);
        }
        break;
    case 'i':
        if (kwmatch(s, "f"))
            KWFOUND(*nx, ULT_Kif);
        break;
    case 'n':
        switch (*s++) {
        case 'o':
            if (kwmatch(s, "t"))
                KWFOUND(*nx, ULT_Knot);
            break;
        case 'u':
            if (kwmatch(s, "ll"))
                KWFOUND(*nx, ULT_Knull);
        }
        break;
    case 'o':
        if (kwmatch(s, "r"))
            KWFOUND(*nx, ULT_Kor);
        break;
    case 'p':
        if (kwmatch(s, "ublic"))
            KWFOUND(*nx, ULT_Kpublic);
        break;
    case 'r':
        if (kwmatch(s, "eturn"))
            KWFOUND(*nx, ULT_Kreturn);
        break;
    case 't':
        switch (*s++) {
        case 'h':
            if (kwmatch(s, "en"))
                KWFOUND(*nx, ULT_Kthen);
            break;
        case 'r':
            switch (*s++) {
            case 'u':
                if (kwmatch(s, "e"))
                    KWFOUND(*nx, ULT_Ktrue);
                break;
            case 'y':
                if (kwmatch(s, ""))
                    KWFOUND(*nx, ULT_Ktry);
            }
        }
        break;
    case 'w':
        switch (*s++) {
        case 'h':
            if (kwmatch(s, "ile"))
                KWFOUND(*nx, ULT_Kwhile);
            break;
        case 'i':
            if (kwmatch(s, "th"))
                KWFOUND(*nx, ULT_Kwith);
            break;
        }
        break;
    }
    return 0;
}

static long add_exp(long a, long b) {
    if (b > 0 && a + b < a) return LONG_MAX;
    if (b < 0 && a + b > a) return LONG_MIN;
    return a + b;
}

struct strbuf {
    byte *data;
    Unc_Size length;
    Unc_Size capacity;
    Unc_Size inc_log2;
};

static Unc_RetVal sputn(Unc_Allocator *alloc, struct strbuf *buf,
                        unsigned inc_log2, Unc_Size qn, const byte *qq) {
    if (buf->length + qn > buf->capacity) {
        Unc_Size inc = 1 << inc_log2;
        Unc_Size oc = buf->capacity, nc = oc + (inc > qn ? inc : qn);
        byte *oo = buf->data, *on;
        nc = (nc + inc - 1) & ~(inc - 1);
        on = unc0_mrealloc(alloc, 0, oo, oc, nc);
        if (!on) return UNCIL_ERR_MEM;
        buf->capacity = nc, buf->data = on;
    }
    buf->length += unc0_memcpy(buf->data + buf->length, qq, qn);
    return 0;
}

static Unc_RetVal sput1(Unc_Allocator *alloc, struct strbuf *buf,
                        unsigned inc_log2, byte q) {
    return sputn(alloc, buf, inc_log2, 1, &q);
}

Unc_RetVal unc0_lexcode_i(Unc_Context *cxt, Unc_LexOut *out,
                          int (*getch)(void *ud), void *ud) {
    int c, sign = 0;
    Unc_RetVal err = 0;
    Unc_LexToken nx = 0;
    Unc_Allocator *alloc = cxt->alloc;
    struct strbuf olc = { NULL, 0, 0 };
    struct strbuf ost = { NULL, 0, 0 };
    struct strbuf oid = { NULL, 0, 0 };
    Unc_HSet hstr, hid;
    Unc_Size stcount = 0, idcount = 0;

    unc0_inithset(&hstr, alloc);
    unc0_inithset(&hid, alloc);
    out->lineno = 1;

    c = getch(ud);
    for (;;) {
        if (c < 0) {
            if ((err = sput1(alloc, &olc, 2, ULT_END)))
                goto lexreturn;
            break;
        } else if (c == '\n') {
            nx = ULT_NL;
            ++out->lineno;
            c = getch(ud);
        } else if (c == '\r') {
            c = getch(ud);
            if (c == '\n') continue;
            err = UNCIL_ERR_SYNTAX;
            goto lexreturn;
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
                            ? sizeof(Unc_Int) : sizeof(Unc_Float))] = { 0 };
                Unc_Size bufn;
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
                        off = add_exp(off, exp);
                    }
                    u.f = unc0_adjexp10(u.f, off);
                    buf[0] = ULT_LFloat;
                    unc0_memcpy(buf + 1, &u.f, sizeof(Unc_Float));
                    bufn = sizeof(Unc_Float) + 1;
                } else {
                    buf[0] = ULT_LInt;
                    unc0_memcpy(buf + 1, &u.i, sizeof(Unc_Int));
                    bufn = sizeof(Unc_Int) + 1;
                }
                if ((err = sputn(alloc, &olc, 6, bufn, buf)))
                    goto lexreturn;
            }
            continue;
        } else if (isident_(c)) {
            /* read in identifier */
            Unc_Size oid_n2 = oid.length, res;

            while (isident_(c)) {
                if ((err = sput1(alloc, &oid, 5, (byte)c)))
                    goto lexreturn;
                c = getch(ud);
            }
            if ((err = sput1(alloc, &oid, 5, 0)))
                goto lexreturn;
            SWAP(Unc_Size, oid_n2, oid.length);
            /* handle elseif as else + if as a special case */
            if (kwmatch((const char *)oid.data + oid.length, "elseif")) {
                byte buf[] = { ULT_Kelse, ULT_Kif };
                if ((err = sputn(alloc, &olc, 6, sizeof(buf), buf)))
                    goto lexreturn;
                continue;
            } else if (!kwtrie(oid.data + oid.length, &nx)) {
                byte buf[1 + sizeof(Unc_Size)];
                if ((err = unc0_puthset(&hid, oid_n2 - oid.length,
                                        oid.data + oid.length, &res, idcount)))
                    goto lexreturn;
                buf[0] = ULT_I;
                unc0_memcpy(buf + 1, &res, sizeof(Unc_Size));
                if (res == idcount)
                    oid.length = oid_n2, ++idcount;
                if ((err = sputn(alloc, &olc, 6, sizeof(buf), buf)))
                    goto lexreturn;
                continue;
            }
        } else if (c == '"') {
            /* read in string */
            byte buf[1 + sizeof(Unc_Size)];
            Unc_Size ost_n2 = ost.length, res, inc = 4;

            c = getch(ud);
            for (;;) {
                if (c < 0 || c == '\n' || c == '\r') {
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
                        if ((err = sput1(alloc, &ost, inc, 0xc0)))
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
                            if ((err = sputn(alloc, &ost, inc, n, ubuf)))
                                goto lexreturn;
                        }
                        continue;
                    case '\r':
                        c = getch(ud);
                        if (c == '\n') break;
                    default:
                        err = UNCIL_ERR_SYNTAX_BADESCAPE;
                        goto lexreturn;
                    }
                }
                if ((err = sput1(alloc, &ost, inc, (byte)c)))
                    goto lexreturn;
                if (inc < 7)
                    ++inc;
                c = getch(ud);
            }
            if ((err = sput1(alloc, &ost, inc, 0)))
                goto lexreturn;
            SWAP(Unc_Size, ost_n2, ost.length);
            if (ost_n2 - ost.length < 256) {
                /* check hash */
                if ((err = unc0_puthset(&hstr, ost_n2 - ost.length,
                                        ost.data + ost.length, &res, stcount)))
                    goto lexreturn;
            } else {
                res = stcount;
            }
            buf[0] = ULT_LStr;
            unc0_memcpy(buf + 1, &res, sizeof(Unc_Size));
            if ((err = sputn(alloc, &olc, 6, sizeof(buf), buf)))
                goto lexreturn;
            if (res == stcount)
                ost.length = ost_n2, ++stcount;
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
        if ((err = sput1(alloc, &olc, 3, (byte)nx)))
            goto lexreturn;
    }
    
lexreturn:
    if (err) {
        unc0_mfree(alloc, olc.data, olc.capacity);
        unc0_mfree(alloc, ost.data, ost.capacity);
        unc0_mfree(alloc, oid.data, oid.capacity);
    } else {
        Unc_Size s = olc.length;
        while (s < olc.capacity)
            olc.data[s++] = 0;
        out->lc_sz = olc.capacity, out->lc = olc.data;
        out->st = unc0_mrealloc(alloc, 0, ost.data, ost.capacity, ost.length);
        out->id = unc0_mrealloc(alloc, 0, oid.data, oid.capacity, oid.length);
        out->st_sz = ost.length, out->st_n = stcount;
        out->id_sz = oid.length, out->id_n = idcount;
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

Unc_RetVal unc0_lexcode(Unc_Context *cxt, Unc_LexOut *out,
                        int (*getch)(void *ud), void *ud) {
    Unc_RetVal e;
    struct utf8_checker buf;
    buf.getch = getch;
    buf.ud = ud;
    buf.count = 0;
    buf.u = 0U;
    buf.invalid = 0;
    buf.eof = 0;
    e = unc0_lexcode_i(cxt, out, &utf8_checker_getch_, &buf);
    if (buf.invalid) return UNCIL_ERR_IO_INVALIDENCODING;
    return e;
}
