/*******************************************************************************
 
Uncil -- builtin convert library impl

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
#include "ublob.h"
#include "uctype.h"
#include "udef.h"
#include "ugc.h"
#include "uncil.h"
#include "ustr.h"
#include "utxt.h"
#include "uutf.h"
#include "uvali.h"
#include "uvsio.h"
#include "uxprintf.h"
#include "uxscanf.h"

#include <limits.h>
#include <stddef.h>
#if UNCIL_C99
#include <stdint.h>
#endif

#define FROMHEX_NEXT() (c = sn ? (--sn, *sb++) : -1)

static int tohex(int c) {
    if ('0' <= c && c <= '9')
        return c - '0';
    if ('A' <= c && c <= 'F')
        return c - 'A' + 10;
    if ('a' <= c && c <= 'f')
        return c - 'a' + 10;
    return -1;
}

Unc_RetVal uncl_convert_fromhex(
                        Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    int c;
    size_t sn;
    const char *sb;
    struct unc0_strbuf buf;
    Unc_Value v = UNC_BLANK;

    e = unc_getstring(w, &args.values[0], &sn, &sb);
    if (e) return e;
    unc0_strbuf_init(&buf, &w->world->alloc, Unc_AllocBlob);

    for (;;) {
        int b = 0;
        FROMHEX_NEXT();
fromhex_skip:
        if (!sn) break;
        if (unc0_isspace(c)) continue;
        if ((b = tohex(c)) >= 0) {
            int h;
            FROMHEX_NEXT();
            h = tohex(c);
            if (h >= 0) {
                b = (b << 4) | h;
                if ((e = unc0_strbuf_put1(&buf, b))) {
                    unc0_strbuf_free(&buf);
                    return e;
                }
            } else
                goto fromhex_skip;
        } else {
            unc0_strbuf_free(&buf);
            return unc_throwexc(w, "value",
                    "unrecognized character in hex string");
        }
    }

    e = unc0_buftoblob(w, &v, &buf);
    e = unc_returnlocal(w, e, &v);
    unc0_strbuf_free(&buf);
    return e;
}

static const char *hex_uc = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

Unc_RetVal uncl_convert_tohex(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    size_t sn, i;
    byte *sb;
    struct unc0_strbuf buf;
    Unc_Value v = UNC_BLANK;
    byte hex[3], c;
    e = unc_lockblob(w, &args.values[0], &sn, &sb);
    if (e) return e;

    unc0_strbuf_init(&buf, &w->world->alloc, Unc_AllocString);

    for (i = 0; i < sn; ++i) {
        c = sb[i];
        hex[0] = hex_uc[(c >> 4) & 15];
        hex[1] = hex_uc[c & 15];
        hex[2] = ((i + 1) & 15) ? ' ' : '\n';
        if ((e = unc0_strbuf_putn(&buf, sizeof(hex), hex))) {
            unc_unlock(w, &args.values[0]);
            unc0_strbuf_free(&buf);
            return e;
        }
    }
    unc_unlock(w, &args.values[0]);

    if (sn & 15)
        if ((e = unc0_strbuf_put1(&buf, '\n'))) {
            unc0_strbuf_free(&buf);
            return e;
        }

    e = unc0_buftostring(w, &v, &buf);
    e = unc_returnlocal(w, e, &v);
    unc0_strbuf_free(&buf);
    return e;
}

Unc_RetVal uncl_convert_fromintbase(
                        Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    int fp;
    Unc_Value v = UNC_BLANK;
    Unc_Int iint, ibase;
    Unc_Float ifloat;
    Unc_Size ssign = 0;
    struct unc0_strbuf buf;
    
    e = unc_getint(w, &args.values[0], &iint);
    if (e) {
        fp = 1;
        e = unc_getfloat(w, &args.values[0], &ifloat);
        if (e) {
            return unc_throwexc(w, "type",
                    "first argument to fromintbase must be a number");
        }
    } else
        fp = 0;
    e = unc_getint(w, &args.values[1], &ibase);
    if (e)
        return unc_throwexc(w, "type",
                "second argument to fromintbase must be an integer");
    if (ibase < 2 || ibase > 36)
        return unc_throwexc(w, "value", "base must be between 2 and 36");
        
    unc0_strbuf_init(&buf, &w->world->alloc, Unc_AllocString);

    if (!fp) {
        Unc_UInt intu;
        if (iint < 0) {
            if (unc0_strbuf_put1(&buf, '-')) {
                unc0_strbuf_free(&buf);
                return UNCIL_ERR_MEM;
            }
            ++ssign;
            intu = -(Unc_UInt)iint;
        } else
            intu = (Unc_UInt)iint;
        if (!intu) {
            if (unc0_strbuf_put1(&buf, '0')) {
                unc0_strbuf_free(&buf);
                return UNCIL_ERR_MEM;
            }
        } else {
            while (intu) {
                int tmp = intu % ibase;
                if (unc0_strbuf_put1(&buf, hex_uc[tmp])) {
                    unc0_strbuf_free(&buf);
                    return UNCIL_ERR_MEM;
                }
                intu /= ibase;
            }
        }
    } else {
        Unc_Float fbase = ibase;
        if (ifloat < 0) {
            if (unc0_strbuf_put1(&buf, '-')) {
                unc0_strbuf_free(&buf);
                return UNCIL_ERR_MEM;
            }
            ++ssign;
            ifloat = -ifloat;
        }
        if (ifloat < 1) {
            if (unc0_strbuf_put1(&buf, '0')) {
                unc0_strbuf_free(&buf);
                return UNCIL_ERR_MEM;
            }
        } else {
            Unc_Float pfloat;
            while (ifloat >= 1) {
                int tmp = (int)unc0_fmod(ifloat, fbase);
                if (unc0_strbuf_put1(&buf, hex_uc[tmp])) {
                    unc0_strbuf_free(&buf);
                    return UNCIL_ERR_MEM;
                }
                pfloat = ifloat;
                ifloat /= fbase;
                if (ifloat >= pfloat) {
                    unc0_strbuf_free(&buf);
                    return unc_throwexc(w, "value", "number not finite");
                }
            }
        }
    }

    unc0_memrev(&buf.buffer[ssign], buf.length - ssign);
    e = unc0_buftostring(w, &v, &buf);
    e = unc_returnlocal(w, e, &v);
    unc0_strbuf_free(&buf);
    return e;
}

Unc_RetVal uncl_convert_tointbase(
                        Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    int negative = 0, floated = 0;
    Unc_Value v;
    Unc_Int ui, pui, base;
    Unc_Float uf;
    const char *s;
    Unc_Size sn, i;
    
    e = unc_getstring(w, &args.values[0], &sn, &s);
    if (e)
        return unc_throwexc(w, "type",
                "first argument to tointbase must be a string");
    e = unc_getint(w, &args.values[1], &base);
    if (e)
        return unc_throwexc(w, "type",
                "second argument to tointbase must be an integer");
    if (base < 2 || base > 36)
        return unc_throwexc(w, "value", "base must be between 2 and 36");
    if (!sn)
        return unc_throwexc(w, "value", "string does not contain a "
                                        "valid number in the given base");
    if (*s == '+') {
        --sn;
        ++s;
    } else if (*s == '-') {
        --sn;
        ++s;
        negative = 1;
    }
    if (!sn)
        return unc_throwexc(w, "value", "string does not contain a "
                                        "valid number in the given base");
    ui = 0;
    for (i = 0; i < sn; ++i) {
        int r;
        char c = s[i];
        if ('0' <= c && c <= '9')
            r = c - '0';
        else if ('A' <= c && c <= 'Z')
            r = c - 'A' + 10;
        else if ('a' <= c && c <= 'z')
            r = c - 'a' + 10;
        else
            r = base;
        if (r >= base)
            return unc_throwexc(w, "value", "string does not contain a "
                                        "valid number in the given base");
        if (floated) {
            uf = uf * base + r;
        } else {
            pui = ui;
            ui = ui * base + r;
            if (pui > ui) {
                /* overflow */
                uf = (Unc_Float)ui * base + r;
                floated = 1;
            }
        }
    }
    if (negative) {
        if (floated)
            uf = -uf;
        else {
            ui = -ui;
            if (ui > 0) {
                uf = -(Unc_Float)ui;
                floated = 1;
            }
        }
    }
    if (floated)
        VINITFLT(&v, uf);
    else
        VINITINT(&v, ui);
    return unc_returnlocal(w, 0, &v);
}

#if UNCIL_C11
#define ALIGN_h _Alignof(short)
#define ALIGN_i _Alignof(int)
#define ALIGN_l _Alignof(long)
#define ALIGN_q _Alignof(long long)
#define ALIGN_z _Alignof(size_t)
#define ALIGN_f _Alignof(float)
#define ALIGN_d _Alignof(double)
#define ALIGN_D _Alignof(long double)
#define ALIGN_p _Alignof(void *)
#else
#define ALIGN_TEST(N, T) struct align_test_##N { T _a; char _c; T _b; };
#define ALIGN_TEST_GET(N) (offsetof(struct align_test_##N, _b)                 \
                         - offsetof(struct align_test_##N, _c))
ALIGN_TEST(h, short)
ALIGN_TEST(i, int)
ALIGN_TEST(l, long)
#if UNCIL_C99
ALIGN_TEST(q, long long)
#endif
ALIGN_TEST(z, size_t)
ALIGN_TEST(f, float)
ALIGN_TEST(d, double)
ALIGN_TEST(D, long double)
ALIGN_TEST(p, void *)
#if UNCIL_C99
ALIGN_TEST(q, long long)
#endif
#define ALIGN_h ALIGN_TEST_GET(h)
#define ALIGN_i ALIGN_TEST_GET(i)
#define ALIGN_l ALIGN_TEST_GET(l)
#if UNCIL_C99
#define ALIGN_q ALIGN_TEST_GET(q)
#endif
#define ALIGN_z ALIGN_TEST_GET(z)
#define ALIGN_f ALIGN_TEST_GET(f)
#define ALIGN_d ALIGN_TEST_GET(d)
#define ALIGN_D ALIGN_TEST_GET(D)
#define ALIGN_p ALIGN_TEST_GET(p)
#endif

#if CHAR_BIT == 8
#if UNCIL_C11
_Static_assert(sizeof(float) == 4, "float not IEEE compatible");
_Static_assert(sizeof(double) == 8, "double not IEEE compatible");
#else
#define CHECK_COMPAT 1
static char float_must_be_ieee_compatible[sizeof(float) == 4 ? 1 : -1];
static char double_must_be_ieee_compatible[sizeof(double) == 8 ? 1 : -1];
#endif
#else
#error "CHAR_BIT == 8 required"
#endif

#if 0
#ifndef INTMAX_MAX
#if UNCIL_C99
typedef signed long long intmax_t;
#else
typedef signed long intmax_t;
#endif
#endif

#ifndef UINTMAX_MAX
#if UNCIL_C99
typedef unsigned long long uintmax_t;
#else
typedef unsigned long uintmax_t;
#endif
#endif
#endif

static void encode_uint_be_i(byte *out, Unc_UInt v, Unc_Size n) {
    do {
        out[--n] = (byte)v;
        v >>= CHAR_BIT;
    } while (n);
}

INLINE void encode_sint_be(byte *out, Unc_Int vi, Unc_Size n) {
    encode_uint_be_i(out, (Unc_UInt)vi, n);
}

INLINE void encode_uint_be(byte *out, Unc_Int vi, Unc_Size n) {
    encode_uint_be_i(out, (Unc_UInt)vi, n);
}

static void encode_uint_le_i(byte *out, Unc_UInt v, Unc_Size n) {
    Unc_Size i;
    for (i = 0; i < n; ++i) {
        out[i] = (byte)v;
        v >>= CHAR_BIT;
    }
}

INLINE void encode_sint_le(byte *out, Unc_Int vi, Unc_Size n) {
    encode_uint_le_i(out, (Unc_UInt)vi, n);
}

INLINE void encode_uint_le(byte *out, Unc_Int vi, Unc_Size n) {
    encode_uint_le_i(out, (Unc_UInt)vi, n);
}

INLINE void encode_float_b32(byte *out, Unc_Float uf) {
    float f = (Unc_Float)uf;
    unc0_memcpy(out, &f, sizeof(float));
}

INLINE void encode_float_b64(byte *out, Unc_Float uf) {
    double f = (Unc_Float)uf;
    unc0_memcpy(out, &f, sizeof(double));
}

Unc_RetVal uncl_convert_encode(Unc_View *w, Unc_Tuple args, void *udata) {
#define ENCODE_THROW(etype, emsg) do { e = unc_throwexc(w, etype, emsg);       \
                                goto uncl_convert_encode_fail; } while (0)
    Unc_RetVal e;
    Unc_Value v = UNC_BLANK;
    Unc_Allocator *alloc = &w->world->alloc;
    const char *s;
    int c, hassize = 0;
    int endian = 0, endianfloat = 0;
#if UNCIL_SANDBOXED
    CONSTEXPR int native = 0;
#else
    int native = 1;
#endif
    /* endian 0 for big-endian, 1 for little-endian */
    Unc_Size i = 1, z;
    struct unc0_strbuf b;

    e = unc_getstringc(w, &args.values[0], &s);
    if (e) return e;

    switch ((c = *s)) {
    case '@':
#if !UNCIL_SANDBOXED
        native = 1;
        ++s;
        break;
#else
    case '=':
#endif
    case '<':
        native = 0;
        endian = 1;
        ++s;
        break;
    case '>':
        native = 0;
        endian = 0;
        ++s;
        break;
#if !UNCIL_SANDBOXED
    case '=':
        native = 0;
        {
            byte b[sizeof(unsigned short)] = { 1, 0 };
            unsigned short u;
            unc0_memcpy(&u, b, sizeof(unsigned short));
            endian = u >= (1 << CHAR_BIT);
        }
        ++s;
        break;
#endif
    }

    if (!native) {
        /* determine endianfloat */
        float f = 0.5f;
        byte buf[4];
#ifdef CHECK_COMPAT
        (void)float_must_be_ieee_compatible;
        (void)double_must_be_ieee_compatible;
#endif
        unc0_memcpy(buf, &f, sizeof(float));
        endianfloat = !buf[0] ^ endian;
    }

    unc0_strbuf_init(&b, alloc, Unc_AllocBlob);

    while ((c = *s++)) {
        hassize = 0;
        if (unc0_isdigit(c)) {
            Unc_Size pz = 0;
            z = c - '0';
            while (unc0_isdigit(*s)) {
                z = z * 10 + (*s++ - '0');
                if (z < pz) {
                    /* overflow */
                    return unc_throwexc(w, "value", "b or * size too large");
                }
                pz = z;
            }
            hassize = 1;
            c = *s++;
            if (!c) break;
        }

        switch (c) {
        case '*':
            if (!hassize) z = 1;
            e = unc0_strbuf_putfill(&b, z, 0);
            if (e) goto uncl_convert_encode_fail;
            continue;
        case 'b':
            if (!hassize) z = 1;
            if (i >= args.count)
                ENCODE_THROW("value", "not enough values to encode "
                                    "(more specifiers than values)");
            {
                byte *out = unc0_strbuf_reserve_next(&b, z);
                Unc_Size bbn;
                byte *bb;
                
                if (!out) {
                    e = UNCIL_ERR_MEM;
                    goto uncl_convert_encode_fail;
                }
                e = unc_lockblob(w, &args.values[i++], &bbn, &bb);
                if (e) {
                    if (e == UNCIL_ERR_TYPE_NOTBLOB)
                        ENCODE_THROW("value", "b requires a blob");
                    return e;
                }
                if (bbn > z) bbn = z;
                unc0_memcpy(out, bb, bbn);
                if (bbn < z) unc0_mbzero(out + bbn, z - bbn);
            }
            continue;
        }

        if (i >= args.count)
            ENCODE_THROW("value", "not enough values to encode "
                                "(more specifiers than values)");
        if (hassize)
            ENCODE_THROW("value", "size modifiers are only supported "
                                "for b and *");

        if (!native) {
            /* different logic */
            switch (c) {
            case 'c':
            {
                Unc_Int ui;
                byte tbuf[1];
                e = unc_getint(w, &args.values[i++], &ui);
                if (e)
                    ENCODE_THROW("type", "c requires an integer");
                if (ui < -0x80 || ui > 0x7F)
                    ENCODE_THROW("type", "value out of range for native c "
                                                "(signed char)");
                encode_uint_be(tbuf, ui, sizeof(tbuf));
                e = unc0_strbuf_putn(&b, sizeof(tbuf), tbuf);
                break;
            }
            case 'C':
            {
                Unc_Int ui;
                byte tbuf[1];
                e = unc_getint(w, &args.values[i++], &ui);
                if (e)
                    ENCODE_THROW("type", "C requires an integer");
                if (ui < 0 || ui > 0xFF)
                    ENCODE_THROW("type", "value out of range for native C "
                                                "(unsigned char)");
                encode_uint_be(tbuf, ui, sizeof(tbuf));
                e = unc0_strbuf_putn(&b, sizeof(tbuf), tbuf);
                break;
            }
            case 'h':
            {
                Unc_Int ui;
                byte tbuf[2];
                e = unc_getint(w, &args.values[i++], &ui);
                if (e)
                    ENCODE_THROW("type", "h requires an integer");
                if (ui < -0x8000 || ui > 0x7FFF)
                    ENCODE_THROW("type", "value out of range for native h "
                                                "(signed short)");
                if (endian)
                    encode_sint_le(tbuf, ui, sizeof(tbuf));
                else
                    encode_sint_be(tbuf, ui, sizeof(tbuf));
                e = unc0_strbuf_putn(&b, sizeof(tbuf), tbuf);
                break;
            }
            case 'H':
            {
                Unc_Int ui;
                byte tbuf[2];
                e = unc_getint(w, &args.values[i++], &ui);
                if (e)
                    ENCODE_THROW("type", "H requires an integer");
                if (ui < 0 || ui > 0xFFFFU)
                    ENCODE_THROW("type", "value out of range for native H "
                                                "(unsigned short)");
                if (endian)
                    encode_uint_le(tbuf, ui, sizeof(tbuf));
                else
                    encode_uint_be(tbuf, ui, sizeof(tbuf));
                e = unc0_strbuf_putn(&b, sizeof(tbuf), tbuf);
                break;
            }
            case 'i':
            {
                Unc_Int ui;
                byte tbuf[4];
                e = unc_getint(w, &args.values[i++], &ui);
                if (e)
                    ENCODE_THROW("type", "i requires an integer");
                if (ui < -0x80000000L || ui > 0x7FFFFFFFL)
                    ENCODE_THROW("type", "value out of range for native i "
                                                "(signed int)");
                if (endian)
                    encode_sint_le(tbuf, ui, sizeof(tbuf));
                else
                    encode_sint_be(tbuf, ui, sizeof(tbuf));
                e = unc0_strbuf_putn(&b, sizeof(tbuf), tbuf);
                break;
            }
            case 'I':
            {
                Unc_Int ui;
                byte tbuf[4];
                e = unc_getint(w, &args.values[i++], &ui);
                if (e)
                    ENCODE_THROW("type", "I requires an integer");
                if (ui < 0 || ui > 0xFFFFFFFFUL)
                    ENCODE_THROW("type", "value out of range for native I "
                                                "(unsigned int)");
                if (endian)
                    encode_uint_le(tbuf, ui, sizeof(tbuf));
                else
                    encode_uint_be(tbuf, ui, sizeof(tbuf));
                e = unc0_strbuf_putn(&b, sizeof(tbuf), tbuf);
                break;
            }
            case 'l':
            {
                Unc_Int ui;
                byte tbuf[8];
                e = unc_getint(w, &args.values[i++], &ui);
                if (e)
                    ENCODE_THROW("type", "l requires an integer");
                if (ui < -0x8000000000000000L || ui > 0x7FFFFFFFFFFFFFFFL)
                    ENCODE_THROW("type", "value out of range for native l "
                                                "(signed long)");
                if (endian)
                    encode_sint_le(tbuf, ui, sizeof(tbuf));
                else
                    encode_sint_be(tbuf, ui, sizeof(tbuf));
                e = unc0_strbuf_putn(&b, sizeof(tbuf), tbuf);
                break;
            }
            case 'L':
            {
                Unc_Int ui;
                byte tbuf[8];
                e = unc_getint(w, &args.values[i++], &ui);
                if (e)
                    ENCODE_THROW("type", "L requires an integer");
                if (ui < 0 || ui > 0xFFFFFFFFFFFFFFFFUL)
                    ENCODE_THROW("type", "value out of range for native L "
                                                "(unsigned long)");
                if (endian)
                    encode_uint_le(tbuf, ui, sizeof(tbuf));
                else
                    encode_uint_be(tbuf, ui, sizeof(tbuf));
                e = unc0_strbuf_putn(&b, sizeof(tbuf), tbuf);
                break;
            }
            case 'q':
            {
                Unc_Int ui;
                byte tbuf[8];
                e = unc_getint(w, &args.values[i++], &ui);
                if (e)
                    ENCODE_THROW("type", "q requires an integer");
                if (ui < -0x8000000000000000L || ui > 0x7FFFFFFFFFFFFFFFL)
                    ENCODE_THROW("type", "value out of range for native q "
                                                "(signed long long)");
                if (endian)
                    encode_sint_le(tbuf, ui, sizeof(tbuf));
                else
                    encode_sint_be(tbuf, ui, sizeof(tbuf));
                e = unc0_strbuf_putn(&b, sizeof(tbuf), tbuf);
                break;
            }
            case 'Q':
            {
                Unc_Int ui;
                byte tbuf[8];
                e = unc_getint(w, &args.values[i++], &ui);
                if (e)
                    ENCODE_THROW("type", "Q requires an integer");
                if (ui < 0 || ui > 0xFFFFFFFFFFFFFFFFUL)
                    ENCODE_THROW("type", "value out of range for native Q "
                                                "(unsigned long long)");
                if (endian)
                    encode_uint_le(tbuf, ui, sizeof(tbuf));
                else
                    encode_uint_be(tbuf, ui, sizeof(tbuf));
                e = unc0_strbuf_putn(&b, sizeof(tbuf), tbuf);
                break;
            }
            case '?':
            {
                byte tbuf[1];
                e = unc_converttobool(w, &args.values[i++]);
                if (UNCIL_IS_ERR(e))
                    goto uncl_convert_encode_fail;
                encode_uint_be(tbuf, e, sizeof(tbuf));
                e = unc0_strbuf_putn(&b, sizeof(tbuf), tbuf);
                break;
            }
            case 'f':
            {
                Unc_Float uf;
                byte tbuf[4];
                e = unc_getfloat(w, &args.values[i++], &uf);
                if (e)
                    ENCODE_THROW("type", "f requires a number");
                encode_float_b32(tbuf, uf);
                if (endianfloat)
                    unc0_memrev(tbuf, sizeof(tbuf));
                e = unc0_strbuf_putn(&b, sizeof(tbuf), tbuf);
                break;
            }
            case 'd':
            {
                Unc_Float uf;
                byte tbuf[8];
                e = unc_getfloat(w, &args.values[i++], &uf);
                if (e)
                    ENCODE_THROW("type", "d requires a number");
                encode_float_b64(tbuf, uf);
                if (endianfloat)
                    unc0_memrev(tbuf, sizeof(tbuf));
                e = unc0_strbuf_putn(&b, sizeof(tbuf), tbuf);
                break;
            }
            case 'Z':
            case 'D':
            case 'p':
                ENCODE_THROW("value", "Z, D, p only available in native mode");
            case '@':
            case '<':
            case '>':
            case '=':
                ENCODE_THROW("value", "mode specifier only allowed as the "
                                      "first character in format");
            default:
                ENCODE_THROW("value", "unrecognized encode format specifier");
            }
            if (e) goto uncl_convert_encode_fail;
            continue;
        }

        switch (c) {
#define ENCODE_VAL(T, src)  do {                                               \
                                T tval = (T)src;                               \
                                e = unc0_strbuf_putn(&b, sizeof(T),            \
                                                (const byte *)&tval);          \
                            } while (0)
#define ENCODE_PAD(type)    do {                                               \
                                size_t pad = ((ALIGN_##type) -                 \
                                    (b.length % (ALIGN_##type)))               \
                                                   % (ALIGN_##type);           \
                                if (pad) {                                     \
                                    e = unc0_strbuf_putfill(&b, pad, 0);       \
                                    if (e) goto uncl_convert_encode_fail;  \
                                }                                              \
                            } while (0)
        case 'c':
        {
            Unc_Int ui;
            e = unc_getint(w, &args.values[i++], &ui);
            if (e) ENCODE_THROW("type", "c requires an integer");
            if (ui < SCHAR_MIN || ui > SCHAR_MAX)
                ENCODE_THROW("type", "value out of range for native c "
                                               "(signed char)");
            ENCODE_VAL(signed char, ui);
            break;
        }
        case 'C':
        {
            Unc_Int ui;
            e = unc_getint(w, &args.values[i++], &ui);
            if (e) ENCODE_THROW("type", "C requires an integer");
            if (ui < 0 || ui > UCHAR_MAX)
                ENCODE_THROW("type", "value out of range for native C "
                                               "(unsigned char)");
            ENCODE_VAL(unsigned char, ui);
            break;
        }
        case 'h':
        {
            Unc_Int ui;
            e = unc_getint(w, &args.values[i++], &ui);
            if (e) ENCODE_THROW("type", "h requires an integer");
            if (ui < SHRT_MIN || ui > SHRT_MAX)
                ENCODE_THROW("type", "value out of range for native h "
                                               "(signed short)");
            ENCODE_PAD(h);
            ENCODE_VAL(signed short, ui);
            break;
        }
        case 'H':
        {
            Unc_Int ui;
            e = unc_getint(w, &args.values[i++], &ui);
            if (e) ENCODE_THROW("type", "H requires an integer");
            if (ui < 0 || ui > USHRT_MAX)
                ENCODE_THROW("type", "value out of range for native H "
                                               "(unsigned short)");
            ENCODE_PAD(h);
            ENCODE_VAL(unsigned short, ui);
            break;
        }
        case 'i':
        {
            Unc_Int ui;
            e = unc_getint(w, &args.values[i++], &ui);
            if (e) ENCODE_THROW("type", "i requires an integer");
            if (ui < INT_MIN || ui > INT_MAX)
                ENCODE_THROW("type", "value out of range for native i "
                                               "(signed int)");
            ENCODE_PAD(i);
            ENCODE_VAL(signed int, ui);
            break;
        }
        case 'I':
        {
            Unc_Int ui;
            e = unc_getint(w, &args.values[i++], &ui);
            if (e) ENCODE_THROW("type", "I requires an integer");
            if (ui < 0 || ui > UINT_MAX)
                ENCODE_THROW("type", "value out of range for native I "
                                               "(unsigned int)");
            ENCODE_PAD(i);
            ENCODE_VAL(unsigned int, ui);
            break;
        }
        case 'l':
        {
            Unc_Int ui;
            e = unc_getint(w, &args.values[i++], &ui);
            if (e) ENCODE_THROW("type", "l requires an integer");
            if (ui < LONG_MIN || ui > LONG_MAX)
                ENCODE_THROW("type", "value out of range for native l "
                                               "(signed long)");
            ENCODE_PAD(l);
            ENCODE_VAL(signed long, ui);
            break;
        }
        case 'L':
        {
            Unc_Int ui;
            e = unc_getint(w, &args.values[i++], &ui);
            if (e) ENCODE_THROW("type", "L requires an integer");
            if (ui < 0 || ui > ULONG_MAX)
                ENCODE_THROW("type", "value out of range for native L "
                                               "(unsigned long)");
            ENCODE_PAD(l);
            ENCODE_VAL(unsigned long, ui);
            break;
        }
#if !UNCIL_C99
        case 'q':
        case 'Q':
            ENCODE_THROW("type", "no native long long types available");
#else
        case 'q':
        {
            Unc_Int ui;
            e = unc_getint(w, &args.values[i++], &ui);
            if (e) ENCODE_THROW("type", "q requires an integer");
            if (ui < LLONG_MIN || ui > LLONG_MAX)
                ENCODE_THROW("type", "value out of range for native q "
                                               "(signed long long)");
            ENCODE_PAD(q);
            ENCODE_VAL(signed long long, ui);
            break;
        }
        case 'Q':
        {
            Unc_Int ui;
            e = unc_getint(w, &args.values[i++], &ui);
            if (e) ENCODE_THROW("type", "Q requires an integer");
            if (ui < 0 || ui > ULLONG_MAX)
                ENCODE_THROW("type", "value out of range for native Q "
                                               "(unsigned long long)");
            ENCODE_PAD(q);
            ENCODE_VAL(unsigned long long, ui);
            break;
        }
#endif
        case 'Z':
        {
            Unc_Int ui;
            e = unc_getint(w, &args.values[i++], &ui);
            if (e) ENCODE_THROW("type", "Z requires an integer");
            if (ui < 0 || ui > SIZE_MAX)
                ENCODE_THROW("type", "value out of range for native Z "
                                               "(size_t)");
            ENCODE_PAD(z);
            ENCODE_VAL(size_t, ui);
            break;
        }
        case '?':
        {
            e = unc_converttobool(w, &args.values[i++]);
            if (UNCIL_IS_ERR(e))
                goto uncl_convert_encode_fail;
#if UNCIL_C99
            ENCODE_VAL(_Bool, e);
#else
            ENCODE_VAL(unsigned char, e);
#endif
            break;
        }
        case 'p':
        {
            void *p;
            e = unc_getopaqueptr(w, &args.values[i++], &p);
            if (e) ENCODE_THROW("type", "p requires a optr");
            ENCODE_PAD(p);
            ENCODE_VAL(void *, p);
            break;
        }
        case 'f':
        {
            Unc_Float uf;
            e = unc_getfloat(w, &args.values[i++], &uf);
            if (e) ENCODE_THROW("type", "f requires a number");
            ENCODE_PAD(f);
            ENCODE_VAL(float, uf);
            break;
        }
        case 'd':
        {
            Unc_Float uf;
            e = unc_getfloat(w, &args.values[i++], &uf);
            if (e) ENCODE_THROW("type", "d requires a number");
            ENCODE_PAD(d);
            ENCODE_VAL(double, uf);
            break;
        }
        case 'D':
        {
            Unc_Float uf;
            e = unc_getfloat(w, &args.values[i++], &uf);
            if (e) ENCODE_THROW("type", "d requires a number");
            ENCODE_PAD(D);
            ENCODE_VAL(long double, uf);
            break;
        }
        case '@':
        case '<':
        case '>':
        case '=':
            ENCODE_THROW("value", "mode specifier only allowed as the first "
                                  "character in format");
        default:
            ENCODE_THROW("value", "unrecognized encode format specifier");
        }
        if (e) goto uncl_convert_encode_fail;
        continue;
uncl_convert_encode_fail:
        unc0_mmfree(alloc, &b);
        return e;
    }

    e = unc0_buftoblob(w, &v, &b);
    e = unc_returnlocal(w, e, &v);
    unc0_strbuf_free(&b);
    return e;
}

INLINE Unc_RetVal decode_uint_be(byte *b, Unc_Size n, Unc_UInt *out) {
    Unc_UInt q = 0, p = 0;
    while (n--) {
        q = (q << CHAR_BIT) | *b++;
        if (q < p) return UNCIL_ERR_ARG_INTOVERFLOW;
        p = q;
    }
    *out = q;
    return 0;
}

INLINE Unc_RetVal decode_uint_le(byte *b, Unc_Size n, Unc_UInt *out) {
    Unc_UInt q = 0, p = 0;
    while (n--) {
        q = (q << CHAR_BIT) | b[n];
        if (q < p) return UNCIL_ERR_ARG_INTOVERFLOW;
        p = q;
    }
    *out = q;
    return 0;
}

INLINE Unc_Float decode_float_b32(byte *b, int reverse) {
    Unc_Size i, n = 4;
    float f;
    byte *o = (byte *)&f;
    for (i = 0; i < n; ++i)
        o[i] = b[reverse ? n - i - 1 : i];
    return f;
}

INLINE Unc_Float decode_float_b64(byte *b, int reverse) {
    Unc_Size i, n = 8;
    double f;
    byte *o = (byte *)&f;
    for (i = 0; i < n; ++i)
        o[i] = b[reverse ? n - i - 1 : i];
    return f;
}

INLINE Unc_RetVal setint_range_signed(Unc_View *w, Unc_Value *v,
                                      intmax_t i) {
    if (i < UNC_INT_MIN || i > UNC_INT_MAX)
        return UNCIL_ERR_ARG_INTOVERFLOW;
    unc_setint(w, v, (Unc_Int)i);
    return 0;
}

INLINE Unc_RetVal setint_range_unsigned(Unc_View *w, Unc_Value *v,
                                        uintmax_t i) {
    if (i > UNC_INT_MAX)
        return UNCIL_ERR_ARG_INTOVERFLOW;
    unc_setint(w, v, (Unc_Int)i);
    return 0;
}

Unc_RetVal uncl_convert_decode(Unc_View *w, Unc_Tuple args, void *udata) {
#define DECODE_THROW(etype, emsg) do { e = unc_throwexc(w, etype, emsg);       \
                                goto uncl_convert_decode_fail; } while (0)
#define DECODE_EOF() DECODE_THROW("value", "unexpected end of blob")
#define DECODE_SHIFT(n) do { b += (n), bn -= (n); } while (0)
    Unc_RetVal e;
    Unc_Value v = UNC_BLANK;
    const char *s;
    byte *b;
    int c, hassize = 0;
    int endian = 0, endianfloat = 0;
#if UNCIL_SANDBOXED
    CONSTEXPR int native = 0;
#else
    int native = 1;
#endif
    /* endian 0 for big-endian, 1 for little-endian */
    Unc_Size z, bn, bno, depth = 0;
    Unc_Int offset = 0;
    
    e = unc_getstringc(w, &args.values[1], &s);
    if (e) return e;

    if (unc_gettype(w, &args.values[2])) {
        e = unc_getint(w, &args.values[2], &offset);
        if (e) return e;
    }

    e = unc_lockblob(w, &args.values[0], &bn, &b);
    if (e) return e;

    if (offset > 0) {
        DECODE_SHIFT(offset);
    } else if (offset < 0) {
        b += bn + offset;
        bn = -offset;
    }

    bno = bn;

    switch ((c = *s)) {
    case '@':
#if !UNCIL_SANDBOXED
        native = 1;
        ++s;
        break;
#else
    case '=':
#endif
    case '<':
        native = 0;
        endian = 1;
        ++s;
        break;
    case '>':
        native = 0;
        endian = 0;
        ++s;
        break;
#if !UNCIL_SANDBOXED
    case '=':
        native = 0;
        {
            byte b[sizeof(unsigned short)] = { 1, 0 };
            unsigned short u;
            unc0_memcpy(&u, b, sizeof(unsigned short));
            endian = u >= (1 << CHAR_BIT);
        }
        ++s;
        break;
#endif
    }

    if (!native) {
        /* determine endianfloat */
        float f = 0.5f;
        byte buf[4];
        unc0_memcpy(buf, &f, sizeof(float));
        endianfloat = !buf[0] ^ endian;
    }

    while ((c = *s++)) {
        hassize = 0;
        if (unc0_isdigit(c)) {
            Unc_Size pz = 0;
            z = c - '0';
            while (unc0_isdigit(*s)) {
                z = z * 10 + (*s++ - '0');
                if (z < pz) {
                    /* overflow */
                    return unc_throwexc(w, "value", "b or * size too large");
                }
                pz = z;
            }
            hassize = 1;
            c = *s++;
            if (!c) break;
        }

        switch (c) {
        case '*':
            if (!hassize) z = 1;
            if (bn < z) DECODE_EOF();
            DECODE_SHIFT(z);
            continue;
        case 'b':
            if (!hassize) z = 1;
            if (bn < z) DECODE_EOF();
            e = unc_newblobfrom(w, &v, z, b);
            if (e) goto uncl_convert_decode_fail;
            DECODE_SHIFT(z);
            e = unc_pushmove(w, &v);
            if (e) goto uncl_convert_decode_fail;
            ++depth; 
            continue;
        }

        if (hassize)
            DECODE_THROW("value",
                    "size modifiers are only supported for b and *");

        if (!native) {
            /* different logic */
            switch (c) {
            case 'c':
            {
                Unc_UInt ui;
                if (bn < 1) DECODE_EOF();
                e = decode_uint_be(b, 1, &ui);
                if (e) goto uncl_convert_decode_fail;
                DECODE_SHIFT(1);
                if (ui > 0x7F)
                    unc_setint(w, &v, (Unc_Int)ui - 0x7F - 1);
                else
                    unc_setint(w, &v, (Unc_Int)ui);
                e = unc_pushmove(w, &v);
                break;
            }
            case 'C':
            {
                Unc_UInt ui;
                if (bn < 1) DECODE_EOF();
                e = decode_uint_be(b, 1, &ui);
                if (e) goto uncl_convert_decode_fail;
                DECODE_SHIFT(1);
                unc_setint(w, &v, (Unc_Int)ui);
                e = unc_pushmove(w, &v);
                break;
            }
            case 'h':
            {
                Unc_UInt ui;
                if (bn < 2) DECODE_EOF();
                e = endian ? decode_uint_le(b, 2, &ui)
                           : decode_uint_be(b, 2, &ui);
                if (e) goto uncl_convert_decode_fail;
                DECODE_SHIFT(2);
                if (ui > 0x7FFF)
                    unc_setint(w, &v, (Unc_Int)ui - 0x7FFF - 1);
                else
                    unc_setint(w, &v, (Unc_Int)ui);
                e = unc_pushmove(w, &v);
                break;
            }
            case 'H':
            {
                Unc_UInt ui;
                if (bn < 2) DECODE_EOF();
                e = endian ? decode_uint_le(b, 2, &ui)
                           : decode_uint_be(b, 2, &ui);
                if (e) goto uncl_convert_decode_fail;
                DECODE_SHIFT(2);
                unc_setint(w, &v, (Unc_Int)ui);
                e = unc_pushmove(w, &v);
                break;
            }
            case 'i':
            {
                Unc_UInt ui;
                if (bn < 4) DECODE_EOF();
                e = endian ? decode_uint_le(b, 4, &ui)
                           : decode_uint_be(b, 4, &ui);
                if (e) goto uncl_convert_decode_fail;
                DECODE_SHIFT(4);
                if (ui > 0x7FFFFFFFL)
                    unc_setint(w, &v, (Unc_Int)ui - 0x7FFFFFFFL - 1);
                else
                    unc_setint(w, &v, (Unc_Int)ui);
                e = unc_pushmove(w, &v);
                break;
            }
            case 'I':
            {
                Unc_UInt ui;
                if (bn < 4) DECODE_EOF();
                e = endian ? decode_uint_le(b, 4, &ui)
                           : decode_uint_be(b, 4, &ui);
                if (e) goto uncl_convert_decode_fail;
                DECODE_SHIFT(4);
                unc_setint(w, &v, (Unc_Int)ui);
                e = unc_pushmove(w, &v);
                break;
            }
            case 'l':
            {
                Unc_UInt ui;
                if (bn < 8) DECODE_EOF();
                e = endian ? decode_uint_le(b, 8, &ui)
                           : decode_uint_be(b, 8, &ui);
                if (e) goto uncl_convert_decode_fail;
                DECODE_SHIFT(8);
                if (ui > 0x7FFFFFFFFFFFFFFFL)
                    unc_setint(w, &v, (Unc_Int)ui - 0x7FFFFFFFFFFFFFFFL - 1);
                else
                    unc_setint(w, &v, (Unc_Int)ui);
                e = unc_pushmove(w, &v);
                break;
            }
            case 'L':
            {
                Unc_UInt ui;
                if (bn < 8) DECODE_EOF();
                e = endian ? decode_uint_le(b, 8, &ui)
                           : decode_uint_be(b, 8, &ui);
                if (e) goto uncl_convert_decode_fail;
                DECODE_SHIFT(8);
                unc_setint(w, &v, (Unc_Int)ui);
                e = unc_pushmove(w, &v);
                break;
            }
            case 'q':
            {
                Unc_UInt ui;
                if (bn < 8) DECODE_EOF();
                e = endian ? decode_uint_le(b, 8, &ui)
                           : decode_uint_be(b, 8, &ui);
                if (e) goto uncl_convert_decode_fail;
                DECODE_SHIFT(8);
                if (ui > 0x7FFFFFFFFFFFFFFFL)
                    unc_setint(w, &v, (Unc_Int)ui - 0x7FFFFFFFFFFFFFFFL - 1);
                else
                    unc_setint(w, &v, (Unc_Int)ui);
                e = unc_pushmove(w, &v);
                break;
            }
            case 'Q':
            {
                Unc_UInt ui;
                if (bn < 8) DECODE_EOF();
                e = endian ? decode_uint_le(b, 8, &ui)
                           : decode_uint_be(b, 8, &ui);
                if (e) goto uncl_convert_decode_fail;
                DECODE_SHIFT(8);
                unc_setint(w, &v, (Unc_Int)ui);
                e = unc_pushmove(w, &v);
                break;
            }
            case '?':
            {
                Unc_UInt ui;
                if (bn < 1) DECODE_EOF();
                e = decode_uint_be(b, 1, &ui);
                if (e) goto uncl_convert_decode_fail;
                DECODE_SHIFT(1);
                unc_setbool(w, &v, ui != 0);
                e = unc_pushmove(w, &v);
                break;
            }
            case 'f':
            {
                Unc_Float uf;
                if (bn < 4) DECODE_EOF();
                uf = decode_float_b32(b, endianfloat);
                DECODE_SHIFT(4);
                unc_setfloat(w, &v, uf);
                e = unc_pushmove(w, &v);
                break;
            }
            case 'd':
            {
                Unc_Float uf;
                if (bn < 8) DECODE_EOF();
                uf = decode_float_b64(b, endianfloat);
                DECODE_SHIFT(8);
                unc_setfloat(w, &v, uf);
                e = unc_pushmove(w, &v);
                break;
            }
            case 'Z':
            case 'D':
            case 'p':
                DECODE_THROW("value", "Z, D, p only available in native mode");
            case '@':
            case '<':
            case '>':
            case '=':
                DECODE_THROW("value", "mode specifier only allowed as the "
                                    "first character in format");
            default:
                DECODE_THROW("value", "unrecognized encode format specifier");
            }
            if (e) goto uncl_convert_decode_fail;
            ++depth;
            continue;
        }

        switch (c) {
#define DECODE_VAL(T, CONV) do {                                               \
                                T val;                                         \
                                unc0_memcpy(&val, b, sizeof(val));             \
                                e = CONV;                                      \
                                if (e) goto uncl_convert_decode_fail;          \
                                e = unc_pushmove(w, &v);                       \
                            } while (0)
#define DECODE_PAD(type)    do {                                               \
                                size_t pad = ((ALIGN_##type) -                 \
                                    ((bno - bn) % (ALIGN_##type)))             \
                                        % (ALIGN_##type);                      \
                                if (pad) {                                     \
                                    if (bn < pad) DECODE_EOF();                \
                                    DECODE_SHIFT(pad);                         \
                                }                                              \
                            } while (0)
        case 'c':
            DECODE_VAL(signed char, setint_range_signed(w, &v, val));
            break;
        case 'C':
            DECODE_VAL(unsigned char, setint_range_unsigned(w, &v, val));
            break;
        case 'h':
            DECODE_PAD(h);
            DECODE_VAL(signed short, setint_range_signed(w, &v, val));
            break;
        case 'H':
            DECODE_PAD(h);
            DECODE_VAL(unsigned short, setint_range_unsigned(w, &v, val));
            break;
        case 'i':
            DECODE_PAD(i);
            DECODE_VAL(signed int, setint_range_signed(w, &v, val));
            break;
        case 'I':
            DECODE_PAD(i);
            DECODE_VAL(unsigned int, setint_range_unsigned(w, &v, val));
            break;
        case 'l':
            DECODE_PAD(l);
            DECODE_VAL(signed long, setint_range_signed(w, &v, val));
            break;
        case 'L':
            DECODE_PAD(l);
            DECODE_VAL(unsigned long, setint_range_unsigned(w, &v, val));
            break;
#if !UNCIL_C99
        case 'q':
        case 'Q':
            DECODE_THROW("type", "no native long long types available");
#else
        case 'q':
            DECODE_PAD(q);
            DECODE_VAL(signed long, setint_range_signed(w, &v, val));
            break;
        case 'Q':
            DECODE_PAD(q);
            DECODE_VAL(unsigned long, setint_range_unsigned(w, &v, val));
            break;
#endif
        case 'Z':
            DECODE_PAD(z);
            DECODE_VAL(size_t, setint_range_unsigned(w, &v, val));
            break;
        case '?':
            DECODE_VAL(unsigned char, (unc_setbool(w, &v, val != 0), 0));
            break;
        case 'p':
            DECODE_PAD(p);
            DECODE_VAL(void *, (unc_setopaqueptr(w, &v, val), 0));
            break;
        case 'f':
            DECODE_PAD(f);
            DECODE_VAL(float, (unc_setfloat(w, &v, val), 0));
            break;
        case 'd':
            DECODE_PAD(d);
            DECODE_VAL(double, (unc_setfloat(w, &v, val), 0));
            break;
        case 'D':
            DECODE_PAD(D);
            DECODE_VAL(long double, (unc_setfloat(w, &v, val), 0));
            break;
        case '@':
        case '<':
        case '>':
        case '=':
            DECODE_THROW("value", "mode specifier only allowed as the first "
                                  "character in format");
        default:
            DECODE_THROW("value", "unrecognized encode format specifier");
        }
        if (e) goto uncl_convert_decode_fail;
        ++depth;
        continue;
    }

uncl_convert_decode_fail:
    unc_unlock(w, &args.values[0]);
    if (!e) {
        unc_setint(w, &v, bno - bn);
        e = unc_shove(w, depth, 1, &v);
    }
    return e;
}

static int encodeb64digit(int b, const char *extra) {
    if (b < 26)
        return 'A' + b;
    else if (b < 52)
        return 'a' + b - 26;
    else if (b < 62)
        return '0' + b - 52;
    else
        return extra[b - 62];
}

static int decodeb64digit(int b, const char *extra) {
    if ('0' <= b && b <= '9')
        return 52 + (b - '0');
    else if ('A' <= b && b <= 'Z')
        return b - 'A';
    else if ('a' <= b && b <= 'z')
        return 26 + (b - 'a');
    else if (b == extra[0])
        return 62;
    else if (b == extra[1])
        return 63;
    else
        return -1;
}

Unc_RetVal uncl_convert_encodeb64(Unc_View *w, Unc_Tuple args,
                                      void *udata) {
    Unc_RetVal e;
    Unc_Value v = UNC_BLANK;
    byte *b;
    Unc_Size bn;
    char extra[2], *s, *sp;

    e = unc_getbool(w, &args.values[1], 0);
    if (UNCIL_IS_ERR(e)) return e;
    if (e) {
        extra[0] = '-';
        extra[1] = '_';
    } else {
        extra[0] = '+';
        extra[1] = '/';
    }
    e = unc_lockblob(w, &args.values[0], &bn, &b);
    if (e) return e;

    s = unc_malloc(w, 1 + ((bn + 2) / 3) * 4);
    if (!s) {
        unc_unlock(w, &args.values[0]);
        return UNCIL_ERR_MEM;
    }
    sp = s;
    while (bn >= 3) {
        *sp++ = encodeb64digit(b[0] >> 2, extra);
        *sp++ = encodeb64digit(((b[0] & 3) << 4) | (b[1] >> 4), extra);
        *sp++ = encodeb64digit(((b[1] & 15) << 2) | (b[2] >> 6), extra);
        *sp++ = encodeb64digit(b[2] & 63, extra);
        b += 3;
        bn -= 3;
    }

    if (bn == 1) {
        *sp++ = encodeb64digit(b[0] >> 2, extra);
        *sp++ = encodeb64digit((b[0] & 3) << 4, extra);
        *sp++ = '=';
        *sp++ = '=';
    } else if (bn == 2) {
        *sp++ = encodeb64digit(b[0] >> 2, extra);
        *sp++ = encodeb64digit(((b[0] & 3) << 4) | (b[1] >> 4), extra);
        *sp++ = encodeb64digit((b[1] & 15) << 2, extra);
        *sp++ = '=';
    }

    unc_unlock(w, &args.values[0]);
    e = unc_newstringmove(w, &v, sp - s, s);
    if (e) unc_mfree(w, s);
    return unc_returnlocal(w, e, &v);
}

Unc_RetVal uncl_convert_decodeb64(Unc_View *w, Unc_Tuple args,
                                      void *udata) {
    Unc_RetVal e;
    int allowws, tmp, bits = 0;
    Unc_Value v = UNC_BLANK;
    const byte *b;
    Unc_Size bn, sn;
    char extra[2];
    byte *s, *sp;

    e = unc_getbool(w, &args.values[1], 0);
    if (UNCIL_IS_ERR(e)) return e;
    if (e) {
        extra[0] = '-';
        extra[1] = '_';
    } else {
        extra[0] = '+';
        extra[1] = '/';
    }
    e = unc_getbool(w, &args.values[2], 0);
    if (UNCIL_IS_ERR(e)) return e;
    allowws = e;
    e = unc_getstring(w, &args.values[0], &bn, (const char **)&b);
    if (e) {
        e = unc_lockblob(w, &args.values[0], &bn, (byte **)&b);
        if (e) return e;
    }

    sn = ((bn + 3) / 4) * 3;
    s = unc_malloc(w, sn);
    if (!s) {
        unc_unlock(w, &args.values[0]);
        return UNCIL_ERR_MEM;
    }
    sp = s;
    while (bn) {
        int q = decodeb64digit(*b, extra);
        if (q < 0) {
            if (allowws && unc0_isspace(*b)) {
                --bn;
                continue;
            } else if (*b == '=')
                break;
            unc_mfree(w, s);
            unc_unlock(w, &args.values[0]);
            return unc_throwexc(w, "value",
                "invalid base64 character in string");
        }
        ++b;
        --bn;
        tmp = (tmp << 6) | q;
        bits += 6;
        if (bits >= 8) {
            unsigned lowbit = bits - 8;
            *sp++ = tmp >> lowbit;
            tmp &= (1 << lowbit) - 1;
            bits = lowbit;
        }
    }

    if (!bits && bn && *b == '=')
        goto invalid_padding; /* always invalid */
    while (bn && *b == '=' && bits <= 24)
        ++b, --bn, bits += 6;
    if (bits > 24 || (bits & 7)) {
invalid_padding:
        unc_mfree(w, s);
        unc_unlock(w, &args.values[0]);
        return unc_throwexc(w, "value", "invalid padding");
    }

    unc_unlock(w, &args.values[0]);
    s = unc_mrealloc(w, s, sp - s);
    e = unc_newblobmove(w, &v, s);
    return unc_returnlocal(w, e, &v);
}

struct uncconv_in_buffer {
    Unc_Size n;
    const byte *s;
};

static int uncconv_in_wrapper(void *data) {
    struct uncconv_in_buffer *buf = data;
    if (!buf->n) return -1;
    --buf->n;
    return *buf->s++;
}

struct uncconv_out_buffer {
    struct unc0_strbuf buffer;
    Unc_RetVal fail;
};

static int uncconv_out_wrapper(void *data, Unc_Size n, const byte *b) {
    struct uncconv_out_buffer *buf = data;
    Unc_RetVal e = unc0_strbuf_putn(&buf->buffer, n, b);
    if (e) {
        buf->fail = e;
        return -1;
    }
    return 0;
}

Unc_RetVal uncl_convert_encodetext(Unc_View *w, Unc_Tuple args,
                                       void *udata) {
    Unc_RetVal e;
    Unc_Value v = UNC_BLANK;
    Unc_Size sn;
    const char *s;
    Unc_CConv_Enc enc = &unc0_cconv_passthru;
    struct uncconv_in_buffer bin;
    struct uncconv_out_buffer bout;

    if (unc_gettype(w, &args.values[1])) {
        int ec;
        e = unc_getstring(w, &args.values[1], &sn, &s);
        if (e) return e;
        ec = unc0_resolveencindex(w, sn, (const byte *)s);
        if (ec < 0)
            return unc_throwexc(w, "value",
                "unrecognized or unsupported encoding");
        enc = unc0_getbyencindex(w, ec)->enc;
    }
    e = unc_getstring(w, &args.values[0], &sn, &s);
    if (e) return e;
    bin.n = sn;
    bin.s = (const byte *)s;
    unc0_strbuf_init(&bout.buffer, &w->world->alloc, Unc_AllocBlob);
    bout.fail = 0;
    if ((*enc)(&uncconv_in_wrapper, &bin,
               &uncconv_out_wrapper, &bout)) {
        unc0_strbuf_free(&bout.buffer);
        return bout.fail ? UNCIL_ERR_MEM : unc_throwexc(w, "value",
            "cannot encode this string with this encoding");
    }
    e = unc0_buftoblob(w, &v, &bout.buffer);
    unc0_strbuf_free(&bout.buffer);
    return unc_returnlocal(w, e, &v);
}

Unc_RetVal uncl_convert_decodetext(Unc_View *w, Unc_Tuple args,
                                       void *udata) {
    Unc_RetVal e;
    Unc_Value v = UNC_BLANK;
    Unc_Size sn;
    byte *s;
    Unc_CConv_Dec dec = &unc0_cconv_utf8ts;
    struct uncconv_in_buffer bin;
    struct uncconv_out_buffer bout;

    if (unc_gettype(w, &args.values[1])) {
        int ec;
        const char *sc;
        e = unc_getstring(w, &args.values[1], &sn, &sc);
        if (e) return e;
        ec = unc0_resolveencindex(w, sn, (const byte *)sc);
        if (ec < 0)
            return unc_throwexc(w, "value",
                "unrecognized or unsupported encoding");
        dec = unc0_getbyencindex(w, ec)->dec;
    }
    e = unc_lockblob(w, &args.values[0], &sn, &s);
    if (e) return e;
    bin.n = sn;
    bin.s = s;
    unc0_strbuf_init(&bout.buffer, &w->world->alloc, Unc_AllocString);
    bout.fail = 0;
    if ((*dec)(&uncconv_in_wrapper, &bin,
               &uncconv_out_wrapper, &bout, UNC_SIZE_MAX)) {
        unc0_strbuf_free(&bout.buffer);
        unc_unlock(w, &args.values[0]);
        return bout.fail ? UNCIL_ERR_MEM : unc_throwexc(w, "value",
            "blob contains invalid data for this encoding");
    }
    unc_unlock(w, &args.values[0]);
    e = unc0_buftostring(w, &v, &bout.buffer);
    unc0_strbuf_free(&bout.buffer);
    return unc_returnlocal(w, e, &v);
}

struct sacxprintf_buf {
    struct unc0_strbuf *buffer;
    Unc_Size fmtn;
    const char *fmt;
};

static size_t unc0_fmt1_printf(struct sacxprintf_buf buf, ...) {
    size_t r;
    va_list va;
    va_start(va, buf);
    r = unc0_sacvxprintf(buf.buffer, UNC0_PRINTF_UTF8 | UNC0_PRINTF_SKIPPOS,
                         buf.fmtn, buf.fmt, va);
    va_end(va);
    return r;
}

static Unc_Size unc0_fmt1_pos(const char *start, const char *end) {
    Unc_Size r = 0;
    unc0_snscanf(start, end - start, "%"PRIUnc_Size"u", &r);
    return r;
}

static int unc0_fmt1(Unc_View *w, struct unc0_strbuf *buf,
                     const char *fmt, const char *fmt_end,
                     size_t spec_n, const unsigned *specs,
                     Unc_Tuple args, Unc_Size *p_argpos) {
    unsigned spec;
    struct sacxprintf_buf sbuf;
    Unc_Value *tmp;
    size_t r;

    if (spec_n != 1)
        return UNCIL_ERR_INTERNAL;
    
    ASSERT(*fmt == '%');
    if (unc0_isdigit(fmt[1])) {
        const char *pos_start = fmt + 1, *pos_end = fmt + 1;
        while (unc0_isdigit(*pos_end))
            ++pos_end;
        if (*pos_end == '$') {
            Unc_Size argpos_read = unc0_fmt1_pos(pos_start, pos_end);
            if (!argpos_read || argpos_read >= args.count)
                return unc_throwexc(w, "value",
                    "convert positional argument position out of range");
            *p_argpos = argpos_read + 1;
            tmp = &args.values[argpos_read];
            goto startspec;
        }
    }

    if (*p_argpos >= args.count) {
        return unc_throwexc(w, "value",
            "not enough values to format (e.g. more specifiers than values)");
    }
    tmp = &args.values[*p_argpos++];
startspec:
    spec = specs[0];
    
    sbuf.buffer = buf;
    sbuf.fmtn = fmt_end - fmt;
    sbuf.fmt = fmt;

    switch (spec) {
    case UNC_PRINTFSPEC_C:     /* int */
    case UNC_PRINTFSPEC_I:     /* signed int */
    case UNC_PRINTFSPEC_LI:    /* signed long */
    case UNC_PRINTFSPEC_LLI:   /* signed long long */
    case UNC_PRINTFSPEC_JI:    /* intmax_t */
    case UNC_PRINTFSPEC_U:     /* unsigned int */
    case UNC_PRINTFSPEC_LU:    /* unsigned long */
    case UNC_PRINTFSPEC_LLU:   /* unsigned long long */
    case UNC_PRINTFSPEC_JU:    /* uintmax_t */
    case UNC_PRINTFSPEC_TI:    /* ptrdiff_t */
    case UNC_PRINTFSPEC_ZI:    /* size_t */
    case UNC_PRINTFSPEC_TU:    /* ptrdiff_t */
    case UNC_PRINTFSPEC_ZU:    /* size_t */
    {
        Unc_Int i;
        Unc_RetVal e = unc_getint(w, tmp, &i);
        if (e) return unc_throwexc(w, "type", "expected integer for %i/%u/%c");
    
        switch (spec) {
        case UNC_PRINTFSPEC_C:     /* int */
        case UNC_PRINTFSPEC_I:     /* signed int */
            r = unc0_fmt1_printf(sbuf, (signed int)i); break;
        case UNC_PRINTFSPEC_LLI:   /* signed long long */
#if UNCIL_C99
            r = unc0_fmt1_printf(sbuf, (signed long long)i); break;
#endif
        case UNC_PRINTFSPEC_LI:    /* signed long */
            r = unc0_fmt1_printf(sbuf, (signed long)i); break;
        case UNC_PRINTFSPEC_JI:    /* intmax_t */
            r = unc0_fmt1_printf(sbuf, (intmax_t)i); break;
        case UNC_PRINTFSPEC_U:     /* unsigned int */
            r = unc0_fmt1_printf(sbuf, (unsigned int)i); break;
        case UNC_PRINTFSPEC_LLU:   /* unsigned long long */
#if UNCIL_C99
            r = unc0_fmt1_printf(sbuf, (unsigned long long)i); break;
#endif
        case UNC_PRINTFSPEC_LU:    /* unsigned long */
            r = unc0_fmt1_printf(sbuf, (unsigned long)i); break;
        case UNC_PRINTFSPEC_JU:    /* uintmax_t */
            r = unc0_fmt1_printf(sbuf, (uintmax_t)i); break;
        case UNC_PRINTFSPEC_TI:    /* ptrdiff_t */
        case UNC_PRINTFSPEC_TU:    /* ptrdiff_t */
            r = unc0_fmt1_printf(sbuf, (ptrdiff_t)i); break;
        case UNC_PRINTFSPEC_ZI:    /* size_t */
        case UNC_PRINTFSPEC_ZU:    /* size_t */
            r = unc0_fmt1_printf(sbuf, (size_t)i); break;
        }
        break;
    }

    case UNC_PRINTFSPEC_LF:    /* double */
    case UNC_PRINTFSPEC_LLF:   /* long double */
    {
        Unc_Float f;
        Unc_RetVal e = unc_getfloat(w, tmp, &f);
        if (e) return unc_throwexc(w, "type", "expected float for %f");
        switch (spec) {
        case UNC_PRINTFSPEC_LF:    /* double */
            r = unc0_fmt1_printf(sbuf, (double)f); break;
        case UNC_PRINTFSPEC_LLF:   /* long double */
            r = unc0_fmt1_printf(sbuf, (long double)f); break;
        }
        break;
    }

    case UNC_PRINTFSPEC_S:     /* const char* */
    {
        char *string;
        int alloc = 0;
        Unc_RetVal e = unc_getstringc(w, tmp, (const char **)&string);
        if (e == UNCIL_ERR_TYPE_NOTSTR) {
            Unc_Size string_n;
            alloc = 1;
            e = unc_valuetostringn(w, tmp, &string_n, &string);
        }
        if (e) return e;
        r = unc0_fmt1_printf(sbuf, (const char *)string);
        if (alloc) unc_mfree(w, string);
        break;
    }
    
    case UNC_PRINTFSPEC_P:
        return unc_throwexc(w, "value", "%p not allowed");
    case UNC_PRINTFSPEC_N:
        return unc_throwexc(w, "value", "%n not allowed");
    default:
        return UNCIL_ERR_INTERNAL;
    }
    
    return r == UNC_PRINTF_EOF ? UNCIL_ERR_MEM : 0;
}

Unc_RetVal uncl_convert_format(Unc_View *w, Unc_Tuple args, void *udata) {
#define ENCODE_THROW(etype, emsg) do { e = unc_throwexc(w, etype, emsg);       \
                                goto uncl_convert_encode_fail; } while (0)
    Unc_RetVal e;
    int c;
    Unc_Value v = UNC_BLANK;
    const char *fmt;
    Unc_Size argpos = 1;
    struct unc0_strbuf buffer;
    const char *copy_from;

    e = unc_getstringc(w, &args.values[0], &fmt);
    if (e) return e;

    copy_from = fmt;
    unc0_strbuf_init(&buffer, &w->world->alloc, Unc_AllocString);

    while ((c = *fmt)) {
        if (c != '%') {
            ++fmt;
            continue;
        }

        if (copy_from < fmt) {
            if (unc0_strbuf_putn(&buffer,
                                 (Unc_Size)(fmt - copy_from),
                                 (const Unc_Byte *)copy_from)) {
                e = UNCIL_ERR_MEM;
                goto uncl_convert_encode_fail;
            }
        }

        c = *++fmt;
        if (!c) {
            break;
        } else if (c == '%') {
            if (unc0_strbuf_put1(&buffer, '%')) {
                e = UNCIL_ERR_MEM;
                goto uncl_convert_encode_fail;
            }
            ++fmt;
        } else {
            unsigned specs[UNC_PRINTFSPEC_MAX];
            const char *fmt_end = fmt;
            size_t spec_n = unc0_printf_specparse(specs, &fmt_end,
                                                  UNC0_PRINTF_SKIPPOS);

            if (!spec_n) {
                e = unc_throwexc(w, "value",
                        "unrecognized or invalid printf format specifier");
                goto uncl_convert_encode_fail;
            }

            if (specs[spec_n] == UNC_PRINTFSPEC_STAR || spec_n > 1) {
                e = unc_throwexc(w, "value",
                        "* not allowed in format width or precision");
                goto uncl_convert_encode_fail;
            }

            if ((e = unc0_fmt1(w, &buffer, fmt - 1, fmt_end,
                               spec_n, specs, args, &argpos))) {
                goto uncl_convert_encode_fail;
            }
            fmt = fmt_end;
        }
        
        copy_from = fmt;
        continue;
uncl_convert_encode_fail:
        unc0_strbuf_free(&buffer);
        return e;
    }

    if (copy_from < fmt) {
        if (unc0_strbuf_putn(&buffer,
                             (Unc_Size)(fmt - copy_from),
                             (const Unc_Byte *)copy_from)) {
            unc0_strbuf_free(&buffer);
            return UNCIL_ERR_MEM;
        }
    }

    e = unc0_buftostring(w, &v, &buffer);
    unc0_strbuf_free(&buffer);
    return unc_returnlocal(w, e, &v);
}

#define FN(x) &uncl_convert_##x, #x
static const Unc_ModuleCFunc lib[] = {
    { FN(encode),      1, 0, 1, UNC_CFUNC_CONCURRENT },
    { FN(decode),      2, 1, 0, UNC_CFUNC_CONCURRENT },
    { FN(encodeb64),   1, 1, 0, UNC_CFUNC_CONCURRENT },
    { FN(decodeb64),   1, 2, 0, UNC_CFUNC_CONCURRENT },
    { FN(encodetext),  1, 2, 0, UNC_CFUNC_CONCURRENT },
    { FN(decodetext),  1, 1, 0, UNC_CFUNC_CONCURRENT },
    { FN(format),      1, 0, 1, UNC_CFUNC_CONCURRENT },
    { FN(tohex),       1, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(tointbase),   2, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(fromhex),     1, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(fromintbase), 2, 0, 0, UNC_CFUNC_CONCURRENT },
};

Unc_RetVal uncilmain_convert(struct Unc_View *w) {
    return unc_exportcfunctions(w, PASSARRAY(lib), 0, NULL, NULL);
}
