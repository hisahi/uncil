/*******************************************************************************
 
Uncil -- builtin convert library impl

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

#define UNCIL_DEFINES

#include "ublob.h"
#include "uctype.h"
#include "udef.h"
#include "ugc.h"
#include "uncil.h"
#include "ustr.h"
#include "utxt.h"
#include "uutf.h"
#include "uvali.h"

#include <limits.h>
#include <stddef.h>
#if UNCIL_C99
#include <stdint.h>
#include <tgmath.h>
#else
#include <math.h>
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

Unc_RetVal unc__lib_convert_fromhex(
                        Unc_View *w, Unc_Tuple args, void *udata) {
    int e, c;
    size_t sn;
    const char *sb;
    Unc_Allocator *alloc = &w->world->alloc;
    byte *buf = NULL;
    size_t buf_n = 0, buf_c = 0;
    e = unc_getstring(w, &args.values[0], &sn, &sb);
    if (e) return e;

    for (;;) {
        int b = 0;
        FROMHEX_NEXT();
fromhex_skip:
        if (!sn) break;
        if (unc__isspace(c)) continue;
        if ((b = tohex(c)) >= 0) {
            int h;
            FROMHEX_NEXT();
            h = tohex(c);
            if (h >= 0) {
                b = (b << 4) | h;
                if ((e = unc__strput(alloc, &buf, &buf_n, &buf_c, 6, b))) {
                    unc__mfree(alloc, buf, buf_c);
                    return e;
                }
            } else
                goto fromhex_skip;
        } else {
            unc__mfree(alloc, buf, buf_c);
            return unc_throwexc(w, "value",
                    "unrecognized character in hex string");
        }
    }

    {
        Unc_Entity *en;
        Unc_Value v;
        en = unc__wake(w, Unc_TBlob);
        if (!en) {
            unc__mfree(alloc, buf, buf_c);
            return UNCIL_ERR_MEM;
        }
        buf = unc__mrealloc(alloc, 0, buf, buf_c, buf_n);
        e = unc__initblobmove(alloc, LEFTOVER(Unc_Blob, en), buf_n, buf);
        if (e) {
            unc__mfree(alloc, buf, buf_c);
            return UNCIL_ERR_MEM;
        }
        VINITENT(&v, Unc_TBlob, en);
        return unc_push(w, 1, &v, NULL);
    }
}

static const char *hex_uc = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

Unc_RetVal unc__lib_convert_tohex(
                        Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    size_t sn, i;
    byte *sb;
    Unc_Allocator *alloc = &w->world->alloc;
    byte *buf = NULL;
    size_t buf_n = 0, buf_c = 0;
    byte hex[3], c;
    e = unc_lockblob(w, &args.values[0], &sn, &sb);
    if (e) return e;

    for (i = 0; i < sn; ++i) {
        c = sb[i];
        hex[0] = hex_uc[(c >> 4) & 15];
        hex[1] = hex_uc[c & 15];
        hex[2] = ((i + 1) & 15) ? ' ' : '\n';
        if ((e = unc__strputn(alloc, &buf, &buf_n, &buf_c,
                              6, sizeof(hex), hex))) {
            unc_unlock(w, &args.values[0]);
            unc__mfree(alloc, buf, buf_c);
            return e;
        }
    }
    unc_unlock(w, &args.values[0]);

    if (sn & 15)
        if ((e = unc__strput(alloc, &buf, &buf_n, &buf_c, 6, '\n'))) {
            unc__mfree(alloc, buf, buf_c);
            return e;
        }

    {
        Unc_Entity *en;
        Unc_Value v;
        en = unc__wake(w, Unc_TString);
        if (!en) {
            unc__mfree(alloc, buf, buf_c);
            return UNCIL_ERR_MEM;
        }
        buf = unc__mrealloc(alloc, 0, buf, buf_c, buf_n);
        e = unc__initstringmove(alloc, LEFTOVER(Unc_String, en), buf_n, buf);
        if (e) {
            unc__mfree(alloc, buf, buf_c);
            return UNCIL_ERR_MEM;
        }
        VINITENT(&v, Unc_TString, en);
        return unc_push(w, 1, &v, NULL);
    }
}

Unc_RetVal unc__lib_convert_fromintbase(
                        Unc_View *w, Unc_Tuple args, void *udata) {
    int e, fp;
    Unc_Value v = UNC_BLANK;
    Unc_Int iint, ibase;
    Unc_Float ifloat;
    Unc_Size sn = 0, sb = 0, ssign = 0;
    byte *s = NULL;
    
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
    if (!fp) {
        Unc_UInt intu;
        if (iint < 0) {
            if (unc__strpush1(&w->world->alloc, &s, &sn, &sb, 6, '-')) {
                unc__mmfree(&w->world->alloc, s);
                return UNCIL_ERR_MEM;
            }
            ++ssign;
            intu = -(Unc_UInt)iint;
        } else
            intu = (Unc_UInt)iint;
        if (!intu) {
            if (unc__strpush1(&w->world->alloc, &s, &sn, &sb, 6, '0')) {
                unc__mmfree(&w->world->alloc, s);
                return UNCIL_ERR_MEM;
            }
        } else {
            while (intu) {
                int tmp = intu % ibase;
                if (unc__strpush1(&w->world->alloc, &s, &sn, &sb,
                                  6, hex_uc[tmp])) {
                    unc__mmfree(&w->world->alloc, s);
                    return UNCIL_ERR_MEM;
                }
                intu /= ibase;
            }
        }
    } else {
        Unc_Float fbase = ibase;
        if (ifloat < 0) {
            if (unc__strpush1(&w->world->alloc, &s, &sn, &sb, 6, '-')) {
                unc__mmfree(&w->world->alloc, s);
                return UNCIL_ERR_MEM;
            }
            ++ssign;
            ifloat = -ifloat;
        }
        if (ifloat < 1) {
            if (unc__strpush1(&w->world->alloc, &s, &sn, &sb, 6, '0')) {
                unc__mmfree(&w->world->alloc, s);
                return UNCIL_ERR_MEM;
            }
        } else {
            Unc_Float pfloat;
            while (ifloat >= 1) {
                int tmp = (int)fmod(ifloat, fbase);
                if (unc__strpush1(&w->world->alloc, &s, &sn, &sb,
                                  6, hex_uc[tmp])) {
                    unc__mmfree(&w->world->alloc, s);
                    return UNCIL_ERR_MEM;
                }
                pfloat = ifloat;
                ifloat /= fbase;
                if (ifloat >= pfloat) {
                    unc__mmfree(&w->world->alloc, s);
                    return unc_throwexc(w, "value", "number not finite");
                }
            }
        }
    }
    { /* reverse */
        Unc_Size i = ssign, j = sn - 1;
        while (i < j) {
            char c = s[i];
            s[i++] = s[j];
            s[j--] = c;
        }
    }
    e = unc_newstringmove(w, &v, sn, (char *)s);
    if (!e) e = unc_pushmove(w, &v, NULL);
    else unc_mfree(w, s);
    return e;
}

Unc_RetVal unc__lib_convert_tointbase(
                        Unc_View *w, Unc_Tuple args, void *udata) {
    int e, negative = 0, floated = 0;
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
    return unc_push(w, 1, &v, NULL);
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
    unc__memcpy(out, &f, sizeof(float));
}

INLINE void encode_float_b64(byte *out, Unc_Float uf) {
    double f = (Unc_Float)uf;
    unc__memcpy(out, &f, sizeof(double));
}

INLINE void encode_reverse(byte *buf, Unc_Size n) {
    Unc_Size i = 0, j = n - 1;
    while (i < j) {
        byte b = buf[i];
        buf[i++] = buf[j];
        buf[j--] = b;
    }
}

Unc_RetVal unc__lib_convert_encode(Unc_View *w, Unc_Tuple args, void *udata) {
#define ENCODE_THROW(etype, emsg) do { e = unc_throwexc(w, etype, emsg);       \
                                goto unc__lib_convert_encode_fail; } while (0)
    int e;
    Unc_Value v = UNC_BLANK;
    Unc_Allocator *alloc = &w->world->alloc;
    const char *s;
    int c, hassize = 0;
    int native = 1, endian = 0, endianfloat = 0;
    /* endian 0 for big-endian, 1 for little-endian */
    Unc_Size i = 1, z;
    byte *b = NULL;
    Unc_Size bn = 0, bc = 0;

    e = unc_getstringc(w, &args.values[0], &s);
    if (e) return e;

    switch ((c = *s)) {
    case '@':
        native = 1;
        ++s;
        break;
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
    case '=':
        native = 0;
        {
            byte b[sizeof(unsigned short)] = { 1, 0 };
            unsigned short u;
            unc__memcpy(&u, b, sizeof(unsigned short));
            endian = u >= (1 << CHAR_BIT);
        }
        ++s;
        break;
    }

    if (!native) {
        /* determine endianfloat */
        float f = 0.5f;
        byte buf[4];
#ifdef CHECK_COMPAT
        (void)float_must_be_ieee_compatible;
        (void)double_must_be_ieee_compatible;
#endif
        unc__memcpy(buf, &f, sizeof(float));
        endianfloat = !buf[0] ^ endian;
    }

    while ((c = *s++)) {
        hassize = 0;
        if (unc__isdigit(c)) {
            Unc_Size pz = 0;
            z = c - '0';
            while (unc__isdigit(*s)) {
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
            if (bn + z > bc) {
                byte *nb = unc__mmrealloc(alloc, Unc_AllocBlob, b, bn + z);
                if (!nb) {
                    e = UNCIL_ERR_MEM;
                    goto unc__lib_convert_encode_fail;
                }
                b = nb;
                bc = bn + z;
            }
            unc__memset(b + bn, 0, z);
            bn += z;
            continue;
        case 'b':
            if (!hassize) z = 1;
            if (bn + z > bc) {
                byte *nb = unc__mmrealloc(alloc, Unc_AllocBlob, b, bn + z);
                if (!nb) {
                    e = UNCIL_ERR_MEM;
                    goto unc__lib_convert_encode_fail;
                }
                b = nb;
                bc = bn + z;
            }
            if (i >= args.count)
                ENCODE_THROW("value", "not enough values to encode "
                                    "(more specifiers than values)");
            {
                Unc_Size bbn;
                byte *bb;
                e = unc_lockblob(w, &args.values[i++], &bbn, &bb);
                if (e) {
                    if (e == UNCIL_ERR_TYPE_NOTBLOB)
                        ENCODE_THROW("value", "b requires a blob");
                    return e;
                }
                if (bbn > z)
                    bbn = z;
                unc__memcpy(b + bn, 0, bbn);
                if (bbn < z)
                    unc__memset(b + bn + bbn, 0, z - bbn);
                bn += z;
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
                e = unc__strpushb(alloc, &b, &bn, &bc, 6, sizeof(tbuf), tbuf);
                if (e) goto unc__lib_convert_encode_fail;
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
                e = unc__strpushb(alloc, &b, &bn, &bc, 6, sizeof(tbuf), tbuf);
                if (e) goto unc__lib_convert_encode_fail;
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
                e = unc__strpushb(alloc, &b, &bn, &bc, 6, sizeof(tbuf), tbuf);
                if (e) goto unc__lib_convert_encode_fail;
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
                e = unc__strpushb(alloc, &b, &bn, &bc, 6, sizeof(tbuf), tbuf);
                if (e) goto unc__lib_convert_encode_fail;
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
                e = unc__strpushb(alloc, &b, &bn, &bc, 6, sizeof(tbuf), tbuf);
                if (e) goto unc__lib_convert_encode_fail;
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
                e = unc__strpushb(alloc, &b, &bn, &bc, 6, sizeof(tbuf), tbuf);
                if (e) goto unc__lib_convert_encode_fail;
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
                e = unc__strpushb(alloc, &b, &bn, &bc, 6, sizeof(tbuf), tbuf);
                if (e) goto unc__lib_convert_encode_fail;
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
                e = unc__strpushb(alloc, &b, &bn, &bc, 6, sizeof(tbuf), tbuf);
                if (e) goto unc__lib_convert_encode_fail;
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
                e = unc__strpushb(alloc, &b, &bn, &bc, 6, sizeof(tbuf), tbuf);
                if (e) goto unc__lib_convert_encode_fail;
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
                e = unc__strpushb(alloc, &b, &bn, &bc, 6, sizeof(tbuf), tbuf);
                if (e) goto unc__lib_convert_encode_fail;
                break;
            }
            case '?':
            {
                byte tbuf[1];
                e = unc_converttobool(w, &args.values[i++]);
                if (UNCIL_IS_ERR(e))
                    goto unc__lib_convert_encode_fail;
                encode_uint_be(tbuf, e, sizeof(tbuf));
                e = unc__strpushb(alloc, &b, &bn, &bc, 6, sizeof(tbuf), tbuf);
                if (e) goto unc__lib_convert_encode_fail;
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
                    encode_reverse(tbuf, sizeof(tbuf));
                e = unc__strpushb(alloc, &b, &bn, &bc, 6, sizeof(tbuf), tbuf);
                if (e) goto unc__lib_convert_encode_fail;
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
                    encode_reverse(tbuf, sizeof(tbuf));
                e = unc__strpushb(alloc, &b, &bn, &bc, 6, sizeof(tbuf), tbuf);
                if (e) goto unc__lib_convert_encode_fail;
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
                ENCODE_THROW("value", "unrecognized format specifier");
            }
            continue;
        }

        switch (c) {
#define ENCODE_VAL(T, src)  do {                                               \
                                byte tbuf[sizeof(T)];                          \
                                T tval;                                        \
                                tval = (T)src;                                 \
                                unc__memcpy(tbuf, &tval, sizeof(tbuf));        \
                                e = unc__strpushb(alloc, &b, &bn, &bc,         \
                                                  6, sizeof(tbuf), tbuf);      \
                                if (e) goto unc__lib_convert_encode_fail;      \
                            } while (0)
#define ENCODE_PAD(type)    do {                                               \
                                size_t pad = ((ALIGN_##type) -                 \
                                    (bn % (ALIGN_##type))) % (ALIGN_##type);   \
                                if (pad) {                                     \
                                    byte buf[ALIGN_##type] = { 0 };            \
                                    e = unc__strpushb(alloc, &b, &bn, &bc,     \
                                                  6, pad, buf);                \
                                    if (e) goto unc__lib_convert_encode_fail;  \
                                }                                              \
                            } while (0)
        case 'c':
        {
            Unc_Int ui;
            e = unc_getint(w, &args.values[i++], &ui);
            if (e)
                ENCODE_THROW("type", "c requires an integer");
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
            if (e)
                ENCODE_THROW("type", "C requires an integer");
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
            if (e)
                ENCODE_THROW("type", "h requires an integer");
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
            if (e)
                ENCODE_THROW("type", "H requires an integer");
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
            if (e)
                ENCODE_THROW("type", "i requires an integer");
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
            if (e)
                ENCODE_THROW("type", "I requires an integer");
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
            if (e)
                ENCODE_THROW("type", "l requires an integer");
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
            if (e)
                ENCODE_THROW("type", "L requires an integer");
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
            if (e)
                ENCODE_THROW("type", "q requires an integer");
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
            if (e)
                ENCODE_THROW("type", "Q requires an integer");
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
            if (e)
                ENCODE_THROW("type", "Z requires an integer");
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
                goto unc__lib_convert_encode_fail;
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
            if (e)
                ENCODE_THROW("type", "p requires a optr");
            ENCODE_PAD(p);
            ENCODE_VAL(void *, p);
            break;
        }
        case 'f':
        {
            Unc_Float uf;
            e = unc_getfloat(w, &args.values[i++], &uf);
            if (e)
                ENCODE_THROW("type", "f requires a number");
            ENCODE_PAD(f);
            ENCODE_VAL(float, uf);
            break;
        }
        case 'd':
        {
            Unc_Float uf;
            e = unc_getfloat(w, &args.values[i++], &uf);
            if (e)
                ENCODE_THROW("type", "d requires a number");
            ENCODE_PAD(d);
            ENCODE_VAL(double, uf);
            break;
        }
        case 'D':
        {
            Unc_Float uf;
            e = unc_getfloat(w, &args.values[i++], &uf);
            if (e)
                ENCODE_THROW("type", "d requires a number");
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
            ENCODE_THROW("value", "unrecognized format specifier");
        }
        continue;
unc__lib_convert_encode_fail:
        unc__mmfree(alloc, &b);
        return e;
    }

    if (b) b = unc__mmrealloc(alloc, Unc_AllocBlob, b, bn);
    e = unc_newblobmove(w, &v, b);
    if (e)
        unc__mmfree(alloc, &b);
    else
        e = unc_pushmove(w, &v, NULL);
    return e;
}


INLINE int decode_uint_be(byte *b, Unc_Size n, Unc_UInt *out) {
    Unc_UInt q = 0, p = 0;
    while (n--) {
        q = (q << CHAR_BIT) | *b++;
        if (q < p) return UNCIL_ERR_ARG_INTOVERFLOW;
        p = q;
    }
    *out = q;
    return 0;
}

INLINE int decode_uint_le(byte *b, Unc_Size n, Unc_UInt *out) {
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

INLINE int setint_range_signed(Unc_View *w, Unc_Value *v, intmax_t i) {
    if (i < UNC_INT_MIN || i > UNC_INT_MAX)
        return UNCIL_ERR_ARG_INTOVERFLOW;
    unc_setint(w, v, (Unc_Int)i);
    return 0;
}

INLINE int setint_range_unsigned(Unc_View *w, Unc_Value *v, uintmax_t i) {
    if (i > UNC_INT_MAX)
        return UNCIL_ERR_ARG_INTOVERFLOW;
    unc_setint(w, v, (Unc_Int)i);
    return 0;
}

Unc_RetVal unc__lib_convert_decode(Unc_View *w, Unc_Tuple args, void *udata) {
#define DECODE_THROW(etype, emsg) do { e = unc_throwexc(w, etype, emsg);       \
                                goto unc__lib_convert_decode_fail; } while (0)
#define DECODE_EOF() DECODE_THROW("value", "unexpected end of blob")
    int e;
    Unc_Value v = UNC_BLANK;
    const char *s;
    byte *b;
    int c, hassize = 0;
    int native = 1, endian = 0, endianfloat = 0;
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
        b += offset;
        bn -= offset;
    } else if (offset < 0) {
        b += bn + offset;
        bn = -offset;
    }

    bno = bn;

    switch ((c = *s)) {
    case '@':
        native = 1;
        ++s;
        break;
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
    case '=':
        native = 0;
        {
            byte b[sizeof(unsigned short)] = { 1, 0 };
            unsigned short u;
            unc__memcpy(&u, b, sizeof(unsigned short));
            endian = u >= (1 << CHAR_BIT);
        }
        ++s;
        break;
    }

    if (!native) {
        /* determine endianfloat */
        float f = 0.5f;
        byte buf[4];
        unc__memcpy(buf, &f, sizeof(float));
        endianfloat = !buf[0] ^ endian;
    }

    while ((c = *s++)) {
        hassize = 0;
        if (unc__isdigit(c)) {
            Unc_Size pz = 0;
            z = c - '0';
            while (unc__isdigit(*s)) {
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
            b += z;
            bn -= z;
            continue;
        case 'b':
            if (!hassize) z = 1;
            if (bn < z) DECODE_EOF();
            e = unc_newblobfrom(w, &v, z, b);
            if (e) goto unc__lib_convert_decode_fail;
            b += z;
            bn -= z;
            e = unc_pushmove(w, &v, &depth);
            if (e) goto unc__lib_convert_decode_fail;
            continue;
        }

        if (hassize)
            DECODE_THROW("value", "size modifiers are only supported "
                                "for b and *");

        if (!native) {
            /* different logic */
            switch (c) {
            case 'c':
            {
                Unc_UInt ui;
                if (bn < 1) DECODE_EOF();
                e = decode_uint_be(b, 1, &ui);
                if (e) goto unc__lib_convert_decode_fail;
                b += 1;
                bn -= 1;
                if (ui > 0x7F)
                    unc_setint(w, &v, (Unc_Int)ui - 0x7F - 1);
                else
                    unc_setint(w, &v, (Unc_Int)ui);
                e = unc_pushmove(w, &v, &depth);
                if (e) goto unc__lib_convert_decode_fail;
                break;
            }
            case 'C':
            {
                Unc_UInt ui;
                if (bn < 1) DECODE_EOF();
                e = decode_uint_be(b, 1, &ui);
                if (e) goto unc__lib_convert_decode_fail;
                b += 1;
                bn -= 1;
                unc_setint(w, &v, (Unc_Int)ui);
                e = unc_pushmove(w, &v, &depth);
                if (e) goto unc__lib_convert_decode_fail;
                break;
            }
            case 'h':
            {
                Unc_UInt ui;
                if (bn < 2) DECODE_EOF();
                e = endian ? decode_uint_le(b, 2, &ui)
                           : decode_uint_be(b, 2, &ui);
                if (e) goto unc__lib_convert_decode_fail;
                b += 2;
                bn -= 2;
                if (ui > 0x7FFF)
                    unc_setint(w, &v, (Unc_Int)ui - 0x7FFF - 1);
                else
                    unc_setint(w, &v, (Unc_Int)ui);
                e = unc_pushmove(w, &v, &depth);
                if (e) goto unc__lib_convert_decode_fail;
                break;
            }
            case 'H':
            {
                Unc_UInt ui;
                if (bn < 2) DECODE_EOF();
                e = endian ? decode_uint_le(b, 2, &ui)
                           : decode_uint_be(b, 2, &ui);
                if (e) goto unc__lib_convert_decode_fail;
                b += 2;
                bn -= 2;
                unc_setint(w, &v, (Unc_Int)ui);
                e = unc_pushmove(w, &v, &depth);
                if (e) goto unc__lib_convert_decode_fail;
                break;
            }
            case 'i':
            {
                Unc_UInt ui;
                if (bn < 4) DECODE_EOF();
                e = endian ? decode_uint_le(b, 4, &ui)
                           : decode_uint_be(b, 4, &ui);
                if (e) goto unc__lib_convert_decode_fail;
                b += 4;
                bn -= 4;
                if (ui > 0x7FFFFFFFL)
                    unc_setint(w, &v, (Unc_Int)ui - 0x7FFFFFFFL - 1);
                else
                    unc_setint(w, &v, (Unc_Int)ui);
                e = unc_pushmove(w, &v, &depth);
                if (e) goto unc__lib_convert_decode_fail;
                break;
            }
            case 'I':
            {
                Unc_UInt ui;
                if (bn < 4) DECODE_EOF();
                e = endian ? decode_uint_le(b, 4, &ui)
                           : decode_uint_be(b, 4, &ui);
                if (e) goto unc__lib_convert_decode_fail;
                b += 4;
                bn -= 4;
                unc_setint(w, &v, (Unc_Int)ui);
                e = unc_pushmove(w, &v, &depth);
                if (e) goto unc__lib_convert_decode_fail;
                break;
            }
            case 'l':
            {
                Unc_UInt ui;
                if (bn < 8) DECODE_EOF();
                e = endian ? decode_uint_le(b, 8, &ui)
                           : decode_uint_be(b, 8, &ui);
                if (e) goto unc__lib_convert_decode_fail;
                b += 8;
                bn -= 8;
                if (ui > 0x7FFFFFFFFFFFFFFFL)
                    unc_setint(w, &v, (Unc_Int)ui - 0x7FFFFFFFFFFFFFFFL - 1);
                else
                    unc_setint(w, &v, (Unc_Int)ui);
                e = unc_pushmove(w, &v, &depth);
                if (e) goto unc__lib_convert_decode_fail;
                break;
            }
            case 'L':
            {
                Unc_UInt ui;
                if (bn < 8) DECODE_EOF();
                e = endian ? decode_uint_le(b, 8, &ui)
                           : decode_uint_be(b, 8, &ui);
                if (e) goto unc__lib_convert_decode_fail;
                b += 8;
                bn -= 8;
                unc_setint(w, &v, (Unc_Int)ui);
                e = unc_pushmove(w, &v, &depth);
                if (e) goto unc__lib_convert_decode_fail;
                break;
            }
            case 'q':
            {
                Unc_UInt ui;
                if (bn < 8) DECODE_EOF();
                e = endian ? decode_uint_le(b, 8, &ui)
                           : decode_uint_be(b, 8, &ui);
                if (e) goto unc__lib_convert_decode_fail;
                b += 8;
                bn -= 8;
                if (ui > 0x7FFFFFFFFFFFFFFFL)
                    unc_setint(w, &v, (Unc_Int)ui - 0x7FFFFFFFFFFFFFFFL - 1);
                else
                    unc_setint(w, &v, (Unc_Int)ui);
                e = unc_pushmove(w, &v, &depth);
                if (e) goto unc__lib_convert_decode_fail;
                break;
            }
            case 'Q':
            {
                Unc_UInt ui;
                if (bn < 8) DECODE_EOF();
                e = endian ? decode_uint_le(b, 8, &ui)
                           : decode_uint_be(b, 8, &ui);
                if (e) goto unc__lib_convert_decode_fail;
                b += 8;
                bn -= 8;
                unc_setint(w, &v, (Unc_Int)ui);
                e = unc_pushmove(w, &v, &depth);
                if (e) goto unc__lib_convert_decode_fail;
                break;
            }
            case '?':
            {
                Unc_UInt ui;
                if (bn < 1) DECODE_EOF();
                e = decode_uint_be(b, 1, &ui);
                if (e) goto unc__lib_convert_decode_fail;
                b += 1;
                bn -= 1;
                unc_setbool(w, &v, ui != 0);
                e = unc_pushmove(w, &v, &depth);
                if (e) goto unc__lib_convert_decode_fail;
                break;
            }
            case 'f':
            {
                Unc_Float uf;
                if (bn < 4) DECODE_EOF();
                uf = decode_float_b32(b, endianfloat);
                b += 4;
                bn -= 4;
                unc_setfloat(w, &v, uf);
                e = unc_pushmove(w, &v, &depth);
                if (e) goto unc__lib_convert_decode_fail;
                break;
            }
            case 'd':
            {
                Unc_Float uf;
                if (bn < 8) DECODE_EOF();
                uf = decode_float_b64(b, endianfloat);
                b += 8;
                bn -= 8;
                unc_setfloat(w, &v, uf);
                e = unc_pushmove(w, &v, &depth);
                if (e) goto unc__lib_convert_decode_fail;
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
                DECODE_THROW("value", "unrecognized format specifier");
            }
            continue;
        }

        switch (c) {
#define DECODE_VAL(T, CONV) do {                                               \
                                T val;                                         \
                                unc__memcpy(&val, b, sizeof(val));             \
                                e = CONV;                                      \
                                if (e) goto unc__lib_convert_decode_fail;      \
                                e = unc_pushmove(w, &v, &depth);               \
                                if (e) goto unc__lib_convert_decode_fail;      \
                            } while (0)
#define DECODE_PAD(type)    do {                                               \
                                size_t pad = ((ALIGN_##type) -                 \
                                    ((bno - bn) % (ALIGN_##type)))             \
                                        % (ALIGN_##type);                      \
                                if (pad) {                                     \
                                    if (bn < pad) DECODE_EOF();                \
                                    b += pad;                                  \
                                    bn -= pad;                                 \
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
            DECODE_THROW("value", "unrecognized format specifier");
        }
        continue;
    }

unc__lib_convert_decode_fail:
    unc_unlock(w, &args.values[0]);
    if (!e) {
        unc_setint(w, &v, bno - bn);
        e = unc_shove(w, depth, 1, &v, NULL);
    }
    return e;
}

static int encodeb64digit(int b, const char* extra) {
    if (b < 26)
        return 'A' + b;
    else if (b < 52)
        return 'a' + b - 26;
    else if (b < 62)
        return '0' + b - 62;
    else
        return extra[b - 62];
}

static int decodeb64digit(int b, const char* extra) {
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

Unc_RetVal unc__lib_convert_encodeb64(Unc_View *w, Unc_Tuple args,
                                      void *udata) {
    int e;
    Unc_Value v = UNC_BLANK;
    byte *b;
    Unc_Size bn, sn;
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

    sn = 1 + ((bn + 2) / 3) * 4;
    s = unc_malloc(w, sn);
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
    e = unc_newstringmove(w, &v, sn - 1, s);
    if (!e) e = unc_pushmove(w, &v, NULL);
    else unc_mfree(w, s);
    return e;
}

Unc_RetVal unc__lib_convert_decodeb64(Unc_View *w, Unc_Tuple args,
                                      void *udata) {
    int e, allowws, tmp, bits = 0;
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
            if (allowws && unc__isspace(*b)) {
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
            bits -= 8;
            *sp++ = tmp >> bits;
        }
    }

    while (bn && *b == '=')
        ++b, --bn;

    unc_unlock(w, &args.values[0]);
    s = unc_mrealloc(w, s, sp - s);
    e = unc_newblobmove(w, &v, s);
    if (!e) e = unc_pushmove(w, &v, NULL);
    return e;
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
    Unc_Allocator *alloc;
    byte *s;
    Unc_Size n;
    Unc_Size c;
    int fail;
};

static int uncconv_out_wrapperb(void *data, Unc_Size n, const byte *b) {
    struct uncconv_out_buffer *buf = data;
    int e = unc__strpushb(buf->alloc, &buf->s, &buf->n, &buf->c, 6, n, b);
    if (e) {
        buf->fail = e;
        return -1;
    }
    return 0;
}

static int uncconv_out_wrappers(void *data, Unc_Size n, const byte *b) {
    struct uncconv_out_buffer *buf = data;
    int e = unc__strpush(buf->alloc, &buf->s, &buf->n, &buf->c, 6, n, b);
    if (e) {
        buf->fail = e;
        return -1;
    }
    return 0;
}

Unc_RetVal unc__lib_convert_encodetext(Unc_View *w, Unc_Tuple args,
                                       void *udata) {
    int e;
    Unc_Value v = UNC_BLANK;
    Unc_Size sn;
    const char *s;
    Unc_CConv_Enc enc = &unc__cconv_passthru;
    struct uncconv_in_buffer bin;
    struct uncconv_out_buffer bout;

    if (unc_gettype(w, &args.values[1])) {
        int ec;
        e = unc_getstring(w, &args.values[1], &sn, &s);
        if (e) return e;
        ec = unc__resolveencindex(w, sn, (const byte *)s);
        if (ec < 0)
            return unc_throwexc(w, "value",
                "unrecognized or unsupported encoding");
        enc = unc__getbyencindex(w, ec)->enc;
    }
    e = unc_getstring(w, &args.values[0], &sn, &s);
    if (e) return e;
    bin.n = sn;
    bin.s = (const byte *)s;
    bout.alloc = &w->world->alloc;
    bout.s = NULL;
    bout.n = 0;
    bout.c = 0;
    bout.fail = 0;
    if ((*enc)(&uncconv_in_wrapper, &bin,
               &uncconv_out_wrapperb, &bout)) {
        return bout.fail ? UNCIL_ERR_MEM : unc_throwexc(w, "value",
            "cannot encode this string with this encoding");
    }
    bout.s = unc__mmrealloc(&w->world->alloc, Unc_AllocBlob,
                            bout.s, bout.n);
    e = unc_newblobmove(w, &v, bout.s);
    if (e) return e;
    return unc_pushmove(w, &v, NULL);
}

Unc_RetVal unc__lib_convert_decodetext(Unc_View *w, Unc_Tuple args,
                                       void *udata) {
    int e;
    Unc_Value v = UNC_BLANK;
    Unc_Size sn;
    byte *s;
    Unc_CConv_Dec dec = &unc__cconv_utf8ts;
    struct uncconv_in_buffer bin;
    struct uncconv_out_buffer bout;

    if (unc_gettype(w, &args.values[1])) {
        int ec;
        const char *sc;
        e = unc_getstring(w, &args.values[1], &sn, &sc);
        if (e) return e;
        ec = unc__resolveencindex(w, sn, (const byte *)sc);
        if (ec < 0)
            return unc_throwexc(w, "value",
                "unrecognized or unsupported encoding");
        dec = unc__getbyencindex(w, ec)->dec;
    }
    e = unc_lockblob(w, &args.values[0], &sn, &s);
    if (e) return e;
    bin.n = sn;
    bin.s = s;
    bout.alloc = &w->world->alloc;
    bout.s = NULL;
    bout.n = 0;
    bout.c = 0;
    bout.fail = 0;
    if ((*dec)(&uncconv_in_wrapper, &bin,
               &uncconv_out_wrappers, &bout, UNC_SIZE_MAX)) {
        unc_unlock(w, &args.values[0]);
        return bout.fail ? UNCIL_ERR_MEM : unc_throwexc(w, "value",
            "blob contains invalid data for this encoding");
    }
    unc_unlock(w, &args.values[0]);
    e = unc_newstringmove(w, &v, bout.n, (char *)bout.s);
    if (e) {
        unc_mfree(w, bout.s);
        return e;
    }
    return unc_pushmove(w, &v, NULL);
}

Unc_RetVal uncilmain_convert(struct Unc_View *w) {
    Unc_RetVal e;
    e = unc_exportcfunction(w, "encode", &unc__lib_convert_encode,
                            UNC_CFUNC_CONCURRENT,
                            1, 1, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "decode", &unc__lib_convert_decode,
                            UNC_CFUNC_CONCURRENT,
                            2, 0, 1, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "encodeb64", &unc__lib_convert_encodeb64,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 1, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "decodeb64", &unc__lib_convert_decodeb64,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 1, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "encodetext", &unc__lib_convert_encodetext,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 2, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "decodetext", &unc__lib_convert_decodetext,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 1, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "fromhex", &unc__lib_convert_fromhex,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "fromintbase", &unc__lib_convert_fromintbase,
                            UNC_CFUNC_CONCURRENT,
                            2, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "tohex", &unc__lib_convert_tohex,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "tointbase", &unc__lib_convert_tointbase,
                            UNC_CFUNC_CONCURRENT,
                            2, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    return 0;
}
