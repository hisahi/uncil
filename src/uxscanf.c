/*******************************************************************************
 
Uncil -- xscanf impl

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

#include <float.h>
#include <limits.h>
#include <stddef.h>

#define UNCIL_DEFINES
#define UNCIL_NODEFINE_SIZE_MAX 1

#include "uarithm.h"
#include "ucstd.h"
#include "uctype.h"
#include "uxscanf.h"

/* based on <https://github.com/hisahi/scanf> */

#if UNCIL_C99
#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS 1
#include <stdint.h>
#endif

#if defined(UINTMAX_MAX) && defined(ULLONG_MAX) && UINTMAX_MAX == ULLONG_MAX
/* replace (u)intmax_t with (unsigned) long if long long is disabled */
#undef intmax_t
#define intmax_t long
#undef uintmax_t
#define uintmax_t unsigned long
#undef INTMAX_MIN
#define INTMAX_MIN LONG_MIN
#undef INTMAX_MAX
#define INTMAX_MAX LONG_MAX
#undef UINTMAX_MAX
#define UINTMAX_MAX ULONG_MAX
#endif
#else
/* (u)intmax_t, (U)INTMAX_(MIN_MAX) */
#if UNCIL_C99
#ifndef intmax_t
#define intmax_t long long int
#endif
#ifndef uintmax_t
#define uintmax_t unsigned long long int
#endif
#ifndef INTMAX_MIN
#define INTMAX_MIN LLONG_MIN
#endif
#ifndef INTMAX_MAX
#define INTMAX_MAX LLONG_MAX
#endif
#ifndef UINTMAX_MAX
#define UINTMAX_MAX ULLONG_MAX
#endif
#else
#ifndef intmax_t
#define intmax_t long int
#endif
#ifndef uintmax_t
#define uintmax_t unsigned long int
#endif
#ifndef INTMAX_MIN
#define INTMAX_MIN LONG_MIN
#endif
#ifndef INTMAX_MAX
#define INTMAX_MAX LONG_MAX
#endif
#ifndef UINTMAX_MAX
#define UINTMAX_MAX ULONG_MAX
#endif
#endif
#endif

/* try to map size_t to unsigned long long, unsigned long, or int */
#if defined(SIZE_MAX)
#if defined(ULLONG_MAX) && SIZE_MAX == ULLONG_MAX
#define SIZET_ALIAS ll
#elif defined(ULONG_MAX) && SIZE_MAX == ULONG_MAX
#define SIZET_ALIAS l
#elif defined(UINT_MAX) && SIZE_MAX == UINT_MAX
#define SIZET_ALIAS
/* intentionally empty -> maps to int */
#endif
#endif

/* this is intentionally after the previous check */
#ifndef SIZE_MAX
#define SIZE_MAX (size_t)(-1)
#endif

#undef UNCIL_NODEFINE_SIZE_MAX
#include "udef.h"

#include "uarithm.h"
#ifndef NAN
#define NAN unc0_fnan()
#endif
#ifndef INFINITY
#define INFINITY unc0_finfty()
#endif

/* try to map ptrdiff_t to long long, long, or int */
#if defined(PTRDIFF_MAX)
#if defined(LLONG_MAX) && PTRDIFF_MAX == LLONG_MAX && PTRDIFF_MIN == LLONG_MIN
#define PTRDIFFT_ALIAS ll
#elif defined(LONG_MAX) && PTRDIFF_MAX == LONG_MAX && PTRDIFF_MIN == LONG_MIN
#define PTRDIFFT_ALIAS l
#elif defined(INT_MAX) && PTRDIFF_MAX == INT_MAX && PTRDIFF_MIN == INT_MIN
#define PTRDIFFT_ALIAS
/* intentionally empty -> maps to int */
#endif
#endif

/* try to map intmax_t to unsigned long long or unsigned long */
#if defined(INTMAX_MAX) && defined(UINTMAX_MAX)
#if (defined(LLONG_MAX) && INTMAX_MAX == LLONG_MAX && INTMAX_MIN == LLONG_MIN) \
    && (defined(ULLONG_MAX) && UINTMAX_MAX == ULLONG_MAX)
#define INTMAXT_ALIAS ll
#elif (defined(LONG_MAX) && INTMAX_MAX == LONG_MAX && INTMAX_MIN == LONG_MIN)  \
    && (defined(ULONG_MAX) && UINTMAX_MAX == ULONG_MAX)
#define INTMAXT_ALIAS l
#endif
#endif

/* check if short == int */
#if defined(INT_MIN) && defined(SHRT_MIN) && INT_MIN == SHRT_MIN               \
 && defined(INT_MAX) && defined(SHRT_MAX) && INT_MAX == SHRT_MAX               \
 && defined(UINT_MAX) && defined(USHRT_MAX) && UINT_MAX == USHRT_MAX
#define SHORT_IS_INT 1
#endif

/* check if long == int */
#if defined(INT_MIN) && defined(LONG_MIN) && INT_MIN == LONG_MIN               \
 && defined(INT_MAX) && defined(LONG_MAX) && INT_MAX == LONG_MAX               \
 && defined(UINT_MAX) && defined(ULONG_MAX) && UINT_MAX == ULONG_MAX
#define LONG_IS_INT 1
#endif

/* check if long long == long */
#if defined(LONG_MIN) && defined(LLONG_MIN) && LONG_MIN == LLONG_MIN           \
 && defined(LONG_MAX) && defined(LLONG_MAX) && LONG_MAX == LLONG_MAX           \
 && defined(ULONG_MAX) && defined(ULLONG_MAX) && ULONG_MAX == ULLONG_MAX
#define LLONG_IS_LONG 1
#endif

/* check if double == float */
#if defined(FLT_MANT_DIG) && defined(DBL_MANT_DIG)                             \
                  && FLT_MANT_DIG == DBL_MANT_DIG                              \
 && defined(FLT_MIN_EXP) && defined(DBL_MIN_EXP)                               \
                  && FLT_MIN_EXP == DBL_MIN_EXP                                \
 && defined(FLT_MAX_EXP) && defined(DBL_MAX_EXP)                               \
                  && FLT_MAX_EXP == DBL_MAX_EXP
#define DOUBLE_IS_FLOAT 1
#endif

/* check if long double == double */
#if defined(DBL_MANT_DIG) && defined(LDBL_MANT_DIG)                            \
                  && DBL_MANT_DIG == LDBL_MANT_DIG                             \
 && defined(DBL_MIN_EXP) && defined(LDBL_MIN_EXP)                              \
                  && DBL_MIN_EXP == LDBL_MIN_EXP                               \
 && defined(DBL_MAX_EXP) && defined(LDBL_MAX_EXP)                              \
                  && DBL_MAX_EXP == LDBL_MAX_EXP
#define LDOUBLE_IS_DOUBLE 1
#endif

#ifdef UINTPTR_MAX
#define INT_TO_PTR(x) ((void*)(uintptr_t)(uintmax_t)(x))
#else
#define INT_TO_PTR(x) ((void*)(uintmax_t)(x))
#endif

/* try to figure out values for PTRDIFF_MAX and PTRDIFF_MIN */
#ifndef PTRDIFF_MAX
#define PTRDIFF_MAX ptrdiff_max_
#define PTRDIFF_MAX_COMPUTE 1
#endif
#ifndef PTRDIFF_MIN
#define PTRDIFF_MIN ptrdiff_min_
#define PTRDIFF_MIN_COMPUTE 1
#endif

INLINE int ctodn_(int c) {
    return c - '0';
}

INLINE int ctoon_(int c) {
    return c - '0';
}

static int ctoxn_(int c) {
    if (c >= 'a')
        return c - 'a' + 10;
    else if (c >= 'A')
        return c - 'A' + 10;
    return c - '0';
}

INLINE int ctobn_(int c) {
    return c - '0';
}

INLINE int ctorn_(int c, int b) {
    switch (b) {
    case 8:
        return ctoon_(c);
    case 16:
        return ctoxn_(c);
    case 2:
        return ctobn_(c);
    default: /* 10 */
        return ctodn_(c);
    }
}

/* case-insensitive character comparison. compares character c to
   upper (uppercase) and lower (lowercase). only defined if upper and lower
   are letters and it applies that tolower(upper) == lower */
#define ICASEEQ(c, upper, lower) ((((int)(c)) & ~0x20U) == (int)(upper))

INLINE int isdigo_(int c) {
    return '0' <= c && c <= '7';
}

INLINE int isdigx_(int c) {
    return unc0_isdigit(c) || ('A' <= c && c <= 'F')
                           || ('a' <= c && c <= 'f');
}

INLINE int isdigb_(int c) {
    return c == '0' || c == '1';
}

INLINE int isdigr_(int c, int b) {
    switch (b) {
    case 8:
        return isdigo_(c);
    case 16:
        return isdigx_(c);
    case 2:
        return isdigb_(c);
    default: /* 10 */
        return unc0_isdigit(c);
    }
}

INLINE intmax_t clamps_(intmax_t m0, intmax_t v, intmax_t m1) {
    return v < m0 ? m0 : v > m1 ? m1 : v;
}

INLINE uintmax_t clampu_(uintmax_t m0, uintmax_t v, uintmax_t m1) {
    return v < m0 ? m0 : v > m1 ? m1 : v;
}

        /* still characters to read? (not EOF and width not exceeded) */
#define KEEP_READING() (nowread < maxlen && !GOT_EOF())
        /* read next char and increment counter */
#define NEXT_CHAR(counter) (next = getch(p), ++counter)

/* EOF check */
#define IS_EOF(c) ((c) < 0)

#undef GOT_EOF
#define GOT_EOF() (IS_EOF(next))

/* convert stream to integer
    getch: stream read function
    p: pointer to pass to above
    nextc: pointer to next character in buffer
    readin: pointer to number of characters read
    maxlen: value that *readin should be at most
    base: 10 for decimal, 16 for hexadecimal, etc.
    unsign: whether the result should be unsigned
    negative: whether there was a - sign
    zero: if no digits, allow zero (i.e. read zero before iaton_)
    dest: intmax_t* or uintmax_t*, where result is stored

    return value: 1 if conversion OK, 0 if not
                  (if 0, dest guaranteed to not be modified)
*/
INLINE BOOL iaton_(int (*getch)(void *p), void *p, int *nextc,
                size_t *readin, size_t maxlen, int base, BOOL unsign,
                BOOL negative, BOOL zero, void *dest) {
    int next = *nextc;
    size_t nowread = *readin;
    uintmax_t r = 0, pr = 0;
    /* read digits? overflow? */
    BOOL digit = 0, ovf = 0;

    /* skip initial zeros */
    while (KEEP_READING() && next == '0') {
        digit = 1;
        NEXT_CHAR(nowread);
    }

    /* read digits and convert to integer */
    while (KEEP_READING() && isdigr_(next, base)) {
        if (!ovf) {
            digit = 1;
            r = r * base + ctorn_(next, base);
            if (pr > r)
                ovf = 1;
            else
                pr = r; 
        }
        NEXT_CHAR(nowread);
    }

    /* if no digits read? */
    if (digit) {
        /* overflow detection, negation, etc. */
        if (unsign) {
            if (ovf)
                r = (intmax_t)UINTMAX_MAX;
            else if (negative)
                r = 0;
            *(uintmax_t *)dest = r;
        } else {
            intmax_t sr;
            if (ovf || (negative ? (intmax_t)r < 0
                                 : r > (uintmax_t)INTMAX_MAX))
                sr = negative ? INTMAX_MIN : INTMAX_MAX;
            else {
                sr = negative ? -(intmax_t)r : (intmax_t)r;
            }
            *(intmax_t *)dest = sr;
        }
    } else if (zero) {
        if (unsign)
            *(uintmax_t *)dest = 0;
        else
            *(intmax_t *)dest = 0;
        digit = 1;
    }

    *nextc = next;
    *readin = nowread;
    return digit;
}

/* convert stream to floating point
    getch: stream read function
    p: pointer to pass to above
    nextc: pointer to next character in buffer
    readin: pointer to number of characters read
    maxlen: value that *readin should be at most
    hex: whether in hex mode
    negative: whether there was a - sign
    zero: if no digits, allow zero (i.e. read zero before iatof_)
    dest: Unc_FloatMax*, where result is stored

    return value: 1 if conversion OK, 0 if not
                  (if 0, dest guaranteed to not be modified)
*/
INLINE BOOL iatof_(int (*getch)(void *p), void *p, int *nextc,
                       size_t *readin, size_t maxlen, BOOL hex, BOOL negative,
                       BOOL zero, Unc_FloatMax *dest) {
    int next = *nextc;
    size_t nowread = *readin;
    Unc_FloatMax r = 0, pr = 0;
    /* saw dot? saw digit? was there an overflow? */
    BOOL dot = 0, digit = 0, ovf = 0;
    /* exponent, offset (with decimals, etc.) */
    intmax_t exp = 0, off = 0;
    /* how much to subtract from off with every digit */
    int sub = 0;
    /* base */
    int base = hex ? 16 : 10;
    /* exponent character */
    char expuc = hex ? 'P' : 'E', explc = hex ? 'p' : 'e';

    while (KEEP_READING() && next == '0') {
        digit = 1;
        NEXT_CHAR(nowread);
    }

    /* read digits and convert */
    while (KEEP_READING()) {
        if (isdigr_(next, base)) {
            if (!ovf) {
                digit = 1;
                r = r * base + ctorn_(next, base);
                if (r > 0 && pr == r) {
                    ovf = 1;
                } else {
                    pr = r;
                    off += sub;
                }
            }
        } else if (next == '.') {
            if (dot)
                break;
            dot = 1, sub = hex ? 4 : 1;
        } else
            break;
        NEXT_CHAR(nowread);
    }

    if (zero && !digit)
        digit = 1;
        /* r == 0 should already apply */

    if (digit) {
        /* exponent? */
        if (KEEP_READING() && (next == explc || next == expuc)) {
            BOOL eneg = 0;
            NEXT_CHAR(nowread);
            if (KEEP_READING()) {
                switch (next) {
                case '-':
                    eneg = 1;
                    /* fall-through */
                case '+':
                    NEXT_CHAR(nowread);
                }
            }

            if (!iaton_(getch, p, &next, &nowread, maxlen, 10,
                            0, eneg, 0, &exp))
                digit = 0;
        }
    }

    if (digit) {
        if (dot) {
            intmax_t oexp = exp;
            exp -= off;
            if (exp > oexp) exp = INTMAX_MIN; /* underflow protection */
        }

#if LDBL_MAX_EXP >= LONG_MAX || LDBL_MAX_10_EXP >= LONG_MAX \
 || LDBL_MIN_EXP <= LONG_MIN || LDBL_MIN_10_EXP <= LONG_MIN
#error "code assumes LDBL exp fits in long"
#endif
        if (r != 0) {
            if (exp > 0) {
                if (exp > (hex ? LDBL_MAX_EXP : LDBL_MAX_10_EXP))
                    r = unc0_finfty();
                else if (hex)
                    r = unc0_mldexp(r, exp);
                else
                    r = unc0_adjexp10(r, (long)exp);
            } else if (exp < 0) {
                if (exp < (hex ? LDBL_MIN_EXP : LDBL_MIN_10_EXP))
                    r = 0;
                else if (hex)
                    r = unc0_mldexp(r, exp);
                else
                    r = unc0_adjexp10(r, (long)exp);
            }
        }

        if (negative) r = -r;
        *dest = r;
    }

    *nextc = next;
    *readin = nowread;
    return digit;
}

enum iscans_type { A_CHAR = 0, A_STRING, A_SCANSET };

struct scanset_ {
    const BOOL *mask;
    BOOL invert;
};

#define STRUCT_SCANSET struct scanset_

INLINE BOOL insset_(const struct scanset_ *set, unsigned char c) {
    return set->mask[c] != set->invert;
}

/* read char(s)/string from stream without char conversion
    getch: stream read function
    p: pointer to pass to above
    nextc: pointer to next character in buffer
    readin: pointer to number of characters read
    maxlen: value that *readin should be at most
    ctype: one of the values of iscans_type
    set: a struct scanset_, only used with A_SCANSET
    nostore: whether nostore was specified
    outp: output char pointer (not dereferenced if nostore=1)

    return value: 1 if conversion OK, 0 if not
*/
INLINE BOOL iscans_(int (*getch)(void *p), void *p, int *nextc,
                    size_t *readin, size_t maxlen, enum iscans_type ctype,
                    const STRUCT_SCANSET *set,
                    BOOL nostore, char *outp) {
    int next = *nextc;
    size_t nowread = *readin;

    while (KEEP_READING()) {
        if (ctype == A_STRING && unc0_isspace(next))
            break;
        if (ctype == A_SCANSET && !insset_(set, (unsigned char)next))
            break;
        if (!nostore) *outp++ = (char)(unsigned char)next;
        NEXT_CHAR(nowread);
    }

    *nextc = next;
    *readin = nowread;
    switch (ctype) {
    case A_CHAR:
        if (nowread < maxlen)
            return 0;
        break;
    case A_STRING:
    case A_SCANSET:
        if (!nowread)
            return 0;
        if (!nostore)
            *outp = '\0';
    }
    return 1;
}

/* =============================== *
 *       main scanf function       *
 * =============================== */

        /* signal input failure and exit loop */
#define INPUT_FAILURE() do { goto read_failure; } while (0)
        /* signal match OK */
#define MATCH_SUCCESS() (noconv = 0)
        /* signal match failure and exit loop */
#define MATCH_FAILURE() do { if (!GOT_EOF()) MATCH_SUCCESS(); \
                             INPUT_FAILURE(); } while (0)
            /* store value to dst with cast */
#define STORE_DST(value, T) (*(T *)(dst) = (T)(value))
            /* store value to dst with cast and possible signed clamp */
#define STORE_DSTI(v, T, minv, maxv) STORE_DST(clamps_(minv, v, maxv), T)
            /* store value to dst with cast and possible unsigned clamp */
#define STORE_DSTU(v, T, minv, maxv) STORE_DST(clampu_(minv, v, maxv), T)

/* enum for possible data types */
enum dlength { LN_ = 0, LN_hh, LN_h, LN_l, LN_ll, LN_L, LN_j, LN_z, LN_t };

#define vLNa_(x) LN_##x
#define vLN_(x) vLNa_(x)

#if PTRDIFF_MAX_COMPUTE
CONSTEXPR int signed_padding_div_ = (int)(                                     \
        (sizeof(ptrdiff_t) > sizeof(uintmax_t) ? 1 :                           \
            INT_MAX < UINTMAX_MAX ?                                            \
                ((uintmax_t)1 << (CHAR_BIT * sizeof(int)))                     \
                    / ((uintmax_t)INT_MAX + 1) :                               \
            SHRT_MAX < UINTMAX_MAX ?                                           \
                ((uintmax_t)1 << (CHAR_BIT * sizeof(short)))                   \
                    / ((uintmax_t)SHRT_MAX + 1) : 2));
CONSTEXPR ptrdiff_t ptrdiff_max_ = (ptrdiff_t)                                 \
        (sizeof(ptrdiff_t) > sizeof(intmax_t) ? 0 :                            \
            sizeof(ptrdiff_t) == sizeof(intmax_t)                              \
                ? INTMAX_MAX                                                   \
                : ((((uintmax_t)1 << (CHAR_BIT * sizeof(ptrdiff_t) - 1))       \
                    * 2 + 1) / signed_padding_div_ + UINTMAX_MAX));
#endif
#if PTRDIFF_MIN_COMPUTE
CONSTEXPR ptrdiff_t ptrdiff_min_ = (ptrdiff_t)                                 \
        (sizeof(ptrdiff_t) > sizeof(intmax_t) ? 0 :                            \
            sizeof(ptrdiff_t) == sizeof(intmax_t)                              \
                ? INTMAX_MIN                                                   \
                : (~(intmax_t)-1 > ~(intmax_t)-2)                              \
                    ? -PTRDIFF_MAX : -PTRDIFF_MAX + ~(intmax_t)0);
#endif

static size_t iscanf_(int (*getch)(void *p), void (*ungetch)(int c, void *p),
                      void *p, const char *ff, va_list va) {
    /* fields = number of fields successfully read; this is the return value */
    size_t fields = 0;
    /* next = the "next" character to be processed */
    int next;
    /* total characters read, returned by %n */
    size_t read_chars = 0;
    /* there were attempts to convert? there were no conversions? */
    BOOL tryconv = 0, noconv = 1;
    const unsigned char *f = (const unsigned char *)ff;
    unsigned char c;

    /* empty format string always returns 0 */
    if (!*f) return 0;

    /* read and cache first character */
    next = getch(p);
    /* ++read_chars; intentionally left out, otherwise %n is off by 1 */
    while ((c = *f++)) {
        if (unc0_isspace(c)) {
            /* skip 0-N whitespace */
            while (!GOT_EOF() && unc0_isspace(next))
                NEXT_CHAR(read_chars);
        } else if (c != '%') {
            if (GOT_EOF()) break;
            /* must match literal character */
            if (next != c) {
                INPUT_FAILURE();
                break;
            }
            NEXT_CHAR(read_chars);
        } else { /* % */
            /* nostore is %*, prevents a value from being stored */
            BOOL nostore;
            /* nowread = characters read for this format specifier
               maxlen = maximum number of chars to be read "field width" */
            size_t nowread = 0, maxlen = 0;
            /* length specifier (l, ll, h, hh...) */
            enum dlength dlen = LN_;
            /* where the value will be stored */
            void *dst;

            /* nostore */
            if (*f == '*') {
                nostore = 1;
                dst = NULL;
                ++f;
            } else {
                nostore = 0;
                dst = va_arg(va, void *);
                /* A pointer to any incomplete or object type may be converted
                   to a pointer to void and back again; the result shall
                   compare equal to the original pointer. */
            }

            /* width specifier => maxlen */
            if (unc0_isdigit(*f)) {
                size_t pr = 0;
                /* skip initial zeros */
                while (*f == '0')
                    ++f;
                while (unc0_isdigit(*f)) {
                    maxlen = maxlen * 10 + ctodn_(*f);
                    if (maxlen < pr) {
                        maxlen = SIZE_MAX;
                        while (unc0_isdigit(*f))
                            ++f;
                        break;
                    } else
                        pr = maxlen;
                    ++f;
                }
            }

            /* length specifier */
            switch (*f++) {
            case 'h':
                if (*f == 'h')
                    dlen = LN_hh, ++f;
                else
                    dlen = LN_h;
                break;
            case 'l':
#if UNCIL_C99
                if (*f == 'l')
                    dlen = LN_ll, ++f;
                else
#endif
                    dlen = LN_l;
                break;
            case 'j':
#ifdef INTMAXT_ALIAS
                dlen = vLN_(INTMAXT_ALIAS);
#else
                dlen = LN_j;
#endif
                break;
            case 't':
#ifdef PTRDIFFT_ALIAS
                dlen = vLN_(PTRDIFFT_ALIAS);
#else
                if (sizeof(ptrdiff_t) > sizeof(intmax_t))
                    MATCH_FAILURE();
                dlen = LN_t;
#endif
                break;
            case 'z':
#ifdef SIZET_ALIAS
                dlen = vLN_(SIZET_ALIAS);
#else
                if (sizeof(size_t) > sizeof(uintmax_t))
                    MATCH_FAILURE();
                dlen = LN_z;
#endif
                break;
            case 'L':
                dlen = LN_L;
                break;
            default:
                --f;
            }

            c = *f;
            switch (c) {
            default:
                /* skip whitespace. include in %n, but not elsewhere */
                while (!GOT_EOF() && unc0_isspace(next))
                    NEXT_CHAR(read_chars);
                /* fall-through */
            /* do not skip whitespace for... */
            case '[':
            case 'c':
                tryconv = 1;
                if (GOT_EOF()) INPUT_FAILURE();
                /* fall-through */
            /* do not even check EOF for... */
            case 'n':
                break;
            }

            /* format */
            switch (c) {
            case '%':
                /* literal % */
                if (next != '%') MATCH_FAILURE();
                NEXT_CHAR(nowread);
                break;
            { /* =========== READ INT =========== */
                /* variables for reading ints */
                /* decimal, hexadecimal, octal, binary */
                int base;
                /* is negative? unsigned? %p? */
                BOOL negative, unsign, isptr;
                /* result: i for signed integers, u for unsigned integers,
                           p for pointers */
                union {
                    intmax_t i;
                    uintmax_t u;
                    void *p;
                } r;

            case 'n': /* number of characters read */
                    if (nostore)
                        break;
                    r.i = (intmax_t)read_chars;
                    unsign = 0, isptr = 0;
                    goto storenum;
            case 'p': /* pointer */
                    isptr = 1;
                    if (next == '(') { /* handle (nil) */
                        int k;
                        const char *rest = "nil)";
                        if (!maxlen) maxlen = SIZE_MAX;
                        NEXT_CHAR(nowread);
                        for (k = 0; k < 4; ++k) {
                            if (!KEEP_READING() || next != rest[k])
                                MATCH_FAILURE();
                            NEXT_CHAR(nowread);
                        }
                        MATCH_SUCCESS();
                        if (!nostore) {
                            ++fields;
                            r.p = NULL;
                            goto storeptr;
                        }
                        break;
                    }
                    base = 10, unsign = 1, negative = 0;
                    goto readptr;
            case 'o': /* unsigned octal integer */
                    base = 8, unsign = 1;
                    goto readnum;
            case 'x': /* unsigned hexadecimal integer */
            case 'X':
                    base = 16, unsign = 1;
                    goto readnum;
            case 'b': /* non-standard: unsigned binary integer */
                    base = 2, unsign = 1;
                    goto readnum;
            case 'u': /* unsigned decimal integer */
            case 'd': /* signed decimal integer */
            case 'i': /* signed decimal/hex/binary integer */
                    base = 10, unsign = c == 'u';
                    /* fall-through */
            readnum:
                    isptr = 0, negative = 0;

                    /* sign, read even for %u */
                    /* maxlen > 0, so this is always fine */
                    switch (next) {
                    case '-':
                        negative = 1;
                        /* fall-through */
                    case '+':
                        NEXT_CHAR(nowread);
                    }
                    /* fall-through */
            readptr:
                {
                    BOOL zero = 0;
                    if (!maxlen) maxlen = SIZE_MAX;

                    /* detect base from string for %i and %p, skip 0x for %x */
                    if (c == 'i' || c == 'p' || ICASEEQ(c, 'X', 'x')
                        || ICASEEQ(c, 'B', 'b')
                    ) {
                        if (KEEP_READING() && next == '0') {
                            zero = 1;
                            NEXT_CHAR(nowread);
                            if (KEEP_READING() && ICASEEQ(next, 'X', 'x')) {
                                if (base == 10)
                                    base = 16;
                                else if (base != 16) {
                                    /* read 0b for %x, etc. */
                                    unsign ? (r.u = 0) : (r.i = 0);
                                    goto readnumok;
                                }
                                NEXT_CHAR(nowread);
                                /* zero = 1. "0x" should be read as 0, because
                                   0 is a valid strtol input, but we cannot
                                   unread x at this point, so it stays read */
                            } else if (KEEP_READING() &&
                                                  ICASEEQ(next, 'B', 'b')) {
                                if (base == 10)
                                    base = 2;
                                else if (base != 2) {
                                    /* read 0x for %b, etc. */
                                    unsign ? (r.u = 0) : (r.i = 0);
                                    goto readnumok;
                                }
                                NEXT_CHAR(nowread);
                            } else if (c == 'i')
                                base = 8;
                        }
                    }

                    /* convert */
                    if (!iaton_(getch, p, &next, &nowread, maxlen, base,
                                unsign, negative, zero, unsign ? (void *)&r.u
                                                               : (void *)&r.i))
                        MATCH_FAILURE();

            readnumok:
                    MATCH_SUCCESS();

                    if (nostore)
                        break;
                    ++fields;
                }
            storenum:
                /* store number, either as ptr, unsigned or signed */
                if (isptr) {
                    r.p = unsign ? INT_TO_PTR(r.u) : INT_TO_PTR(r.i);
            storeptr:
                    STORE_DST(r.p, void *);
                } else {
                    switch (dlen) {
                    case LN_hh:
                        if (unsign) STORE_DSTU(r.u, unsigned char,
                                               0, UCHAR_MAX);
                        else        STORE_DSTI(r.i, signed char,
                                               SCHAR_MIN, SCHAR_MAX);
                        break;
#if !SHORT_IS_INT
                    case LN_h: /* if SHORT_IS_INT, match fails => default: */
                        if (unsign) STORE_DSTU(r.u, unsigned short,
                                               0, USHRT_MAX);
                        else        STORE_DSTI(r.i, short,
                                               SHRT_MIN, SHRT_MAX);
                        break;
#endif
#ifndef INTMAXT_ALIAS
                    case LN_j:
                        if (unsign) STORE_DSTU(r.u, uintmax_t,
                                               0, UINTMAX_MAX);
                        else        STORE_DSTI(r.i, intmax_t,
                                               INTMAX_MIN, INTMAX_MAX);
                        break;
#endif /* INTMAXT_ALIAS */
#ifndef SIZET_ALIAS
                    case LN_z:
                        if (!unsign) {
                            if (r.i < 0)
                                r.u = 0;
                            else
                                r.u = (uintmax_t)r.i;
                        }
                        STORE_DSTU(r.u, size_t, 0, SIZE_MAX);
                        break;
#endif /* SIZET_ALIAS */
#ifndef PTRDIFFT_ALIAS
                    case LN_t:
                        if (unsign) {
                            if (r.u > (uintmax_t)INTMAX_MAX)
                                r.i = INTMAX_MAX;
                            else
                                r.i = (intmax_t)r.u;
                        }
#if PTRDIFF_MAX_COMPUTE
                        /* min/max are wrong, don't even try to clamp */
                        if (PTRDIFF_MIN >= PTRDIFF_MAX)
                            STORE_DST(r.i, ptrdiff_t);
                        else
#endif
                        STORE_DSTI(r.i, ptrdiff_t, PTRDIFF_MIN, PTRDIFF_MAX);
                        break;
#endif /* PTRDIFFT_ALIAS */
#if UNCIL_C99
                    case LN_ll:
#if !LLONG_IS_LONG
                        if (unsign) STORE_DSTU(r.u, unsigned long long,
                                                    0, ULLONG_MAX);
                        else        STORE_DSTI(r.i, long long,
                                                    LLONG_MIN, LLONG_MAX);
                        break;
#endif
#endif
                    case LN_l:
#if !LONG_IS_INT
                        if (unsign) STORE_DSTU(r.u, unsigned long,
                                                    0, ULONG_MAX);
                        else        STORE_DSTI(r.i, long,
                                                    LONG_MIN, LONG_MAX);
                        break;
#endif
                    default:
                        if (unsign) STORE_DSTU(r.u, unsigned,
                                                    0, UINT_MAX);
                        else        STORE_DSTI(r.i, int,
                                                    INT_MIN, INT_MAX);
                    }
                }
                break;
            } /* =========== READ INT =========== */

            case 'e': case 'E': /* scientific format float */
            case 'f': case 'F': /* decimal format float */
            case 'g': case 'G': /* decimal/scientific format float */
            case 'a': case 'A': /* hex format float */
                /* all treated equal by scanf, but not by printf */
            { /* =========== READ FLOAT =========== */
                Unc_FloatMax r;
                /* negative? allow zero? hex mode? */
                BOOL negative = 0, zero = 0, hex = 0;
                if (!maxlen) maxlen = SIZE_MAX;

                switch (next) {
                case '-':
                    negative = 1;
                    /* fall-through */
                case '+':
                    NEXT_CHAR(nowread);
                }

                if (KEEP_READING() && ICASEEQ(next, 'N', 'n')) {
                    NEXT_CHAR(nowread);
                    if (!KEEP_READING() || !ICASEEQ(next, 'A', 'a'))
                        MATCH_FAILURE();
                    NEXT_CHAR(nowread);
                    if (!KEEP_READING() || !ICASEEQ(next, 'N', 'n'))
                        MATCH_FAILURE();
                    NEXT_CHAR(nowread);
                    if (KEEP_READING() && next == '(') {
                        while (KEEP_READING()) {
                            NEXT_CHAR(nowread);
                            if (next == ')') {
                                NEXT_CHAR(nowread);
                                break;
                            } else if (next != '_' && !unc0_isalnum(next))
                                MATCH_FAILURE();
                        }
                    }
                    r = negative ? -NAN : NAN;
                    goto storefp;
                } else if (KEEP_READING() && ICASEEQ(next, 'I', 'i')) {
                    NEXT_CHAR(nowread);
                    if (!KEEP_READING() || !ICASEEQ(next, 'N', 'n'))
                        MATCH_FAILURE();
                    NEXT_CHAR(nowread);
                    if (!KEEP_READING() || !ICASEEQ(next, 'F', 'f'))
                        MATCH_FAILURE();
                    NEXT_CHAR(nowread);
                    /* try reading the rest */
                    if (KEEP_READING()) {
                        int k;
                        const char *rest2 = "INITY";
                        for (k = 0; k < 5; ++k) {
                            if (!KEEP_READING() ||
                                    (next != rest2[k] &&
                                     next != unc0_tolower(rest2[k])))
                                break;
                            NEXT_CHAR(nowread);
                        }
                    }
                    r = negative ? -INFINITY : INFINITY;
                    goto storefp;
                }

                /* 0x for hex floats */
                if (KEEP_READING() && next == '0') {
                    zero = 1;
                    NEXT_CHAR(nowread);
                    if (KEEP_READING() && ICASEEQ(next, 'X', 'x')) {
                        hex = 1;
                        NEXT_CHAR(nowread);
                    }
                }

                /* convert */
                if (!iatof_(getch, p, &next, &nowread, maxlen, hex,
                                negative, zero, &r))
                    MATCH_FAILURE();

            storefp:
                MATCH_SUCCESS();
                if (nostore)
                    break;
                ++fields;
                switch (dlen) {
                case LN_L:
#if !LDOUBLE_IS_DOUBLE
                    STORE_DST(r, long double);
                    break;
#endif
                case LN_l:
#if !DOUBLE_IS_FLOAT
                    STORE_DST(r, double);
                    break;
#endif
                default:
                    STORE_DST(r, float);
                }
                break;
            } /* =========== READ FLOAT =========== */

            case 'c':
            { /* =========== READ char =========== */
                char *outp;
                if (dlen == LN_l) /* read wide but not supported */
                    MATCH_FAILURE();
                outp = (char *)dst;
                if (!maxlen) maxlen = 1;
                if (!iscans_(getch, p, &next, &nowread, maxlen,
                                A_CHAR, NULL, nostore, outp))
                    MATCH_FAILURE();
                if (!nostore) ++fields;
                MATCH_SUCCESS();
                break;
            } /* =========== READ char =========== */

            case 's':
            { /* =========== READ STR =========== */
                char *outp;
                if (dlen == LN_l) /* read wide but not supported */
                    MATCH_FAILURE();
                outp = (char *)dst;
                if (!maxlen) {
                    if (!nostore) MATCH_FAILURE();
                    maxlen = SIZE_MAX;
                }
                if (!iscans_(getch, p, &next, &nowread, maxlen,
                                A_STRING, NULL, nostore, outp))
                    MATCH_FAILURE();
                if (!nostore) ++fields;
                MATCH_SUCCESS();
                break;
            } /* =========== READ STR =========== */

            case '[':
            { /* =========== READ SCANSET =========== */
                char *outp;
                BOOL invert = 0;
                BOOL hyphen = 0;
                BOOL mention[UCHAR_MAX + 1] = { 0 };
                unsigned char prev = 0, c;
                struct scanset_ scanset;
                if (dlen == LN_l) /* read wide but not supported */
                    MATCH_FAILURE();
                outp = (char *)dst;
                if (!maxlen) {
                    if (!nostore) MATCH_FAILURE();
                    maxlen = SIZE_MAX;
                }
                ++f;
                if (*f == '^')
                    invert = 1, ++f;
                if (*f == ']')
                    ++f;
                /* populate mention, 0 if character not listed, 1 if it is */
                while ((c = *f) && c != ']') {
                    if (hyphen) {
                        int k;
                        for (k = prev; k <= c; ++k)
                            mention[k] = 1;
                        hyphen = 0;
                        prev = c;
                    } else if (c == '-' && prev)
                        hyphen = 1;
                    else
                        mention[c] = 1, prev = c;
                    ++f;
                }
                if (hyphen)
                    mention['-'] = 1;
                scanset.mask = mention;
                scanset.invert = invert;
                {
                    if (!iscans_(getch, p, &next, &nowread, maxlen,
                            A_SCANSET, &scanset, nostore, outp))
                        MATCH_FAILURE();
                }
                if (!nostore) ++fields;
                MATCH_SUCCESS();
                break;
            } /* =========== READ SCANSET =========== */
            default:
                /* unrecognized specification */
                MATCH_FAILURE();
            }

            ++f; /* next fmt char */
            read_chars += nowread;
        }
    }
read_failure:
    /* if we have a leftover character, put it back into the stream */
    if (!GOT_EOF() && ungetch)
        ungetch(next, p);
    return tryconv && noconv ? UNC_SCANF_EOF : fields;
}

size_t unc0_vxscanf(int (*getch)(void *data),
                    void (*ungetch)(int c, void *data),
                    void *data, const char *format, va_list arg) {
    return iscanf_(getch, ungetch, data, format, arg);
}

size_t unc0_xscanf(int (*getch)(void *data),
                   void (*ungetch)(int c, void *data),
                   void *data, const char *format, ...) {
    size_t r;
    va_list va;
    va_start(va, format);
    r = unc0_vxscanf(getch, ungetch, data, format, va);
    va_end(va);
    return r;
}

struct unc0_snscanf_buf {
    const char *s;
    size_t n;
};

static int unc0_snscanf_getch(void *data) {
    struct unc0_snscanf_buf *buf = data;
    if (!buf->n) return -1;
    --buf->n;
    return (unsigned char)*buf->s++;
}

size_t unc0_snscanf(const char *s, size_t n, const char *format, ...) {
    size_t r;
    struct unc0_snscanf_buf buf;
    va_list va;
    va_start(va, format);
    buf.s = s;
    buf.n = n;
    r = unc0_vxscanf(&unc0_snscanf_getch, NULL, &buf, format, va);
    va_end(va);
    return r;
}
