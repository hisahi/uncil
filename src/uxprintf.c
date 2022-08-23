/*******************************************************************************
 
Uncil -- xprintf impl

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

/* xprintf is a nearly standard printf. differences:
    always on C locale
    no support for %lc or %ls
    no support for %a (yet?)
*/

#include <float.h>
#include <limits.h>

#define UNCIL_DEFINES

#include "uctype.h"
#include "udebug.h"
#include "udef.h"
#include "umem.h"
#include "uosdef.h"
#include "uxprintf.h"

#if UNCIL_C99
#include <stdint.h>
#include <tgmath.h>
#else
#include <math.h>
#endif

#ifndef INTMAX_MAX
#if UNCIL_C99
typedef long long intmax_t;
#else
typedef long intmax_t;
#endif
#endif

#ifndef UINTMAX_MAX
#if UNCIL_C99
typedef unsigned long long uintmax_t;
#else
typedef unsigned long uintmax_t;
#endif
#endif

#define PUTC(c) do { if (out(c, udata)) return -1; ++outp; } while (0)

enum length_mod {
    L_, L_hh, L_h, L_l, L_ll, L_j, L_z, L_t, L_L
};

#define PR_FLAG_UPPER (1 << 0)
#define PR_FLAG_LEFT (1 << 1)
#define PR_FLAG_SIGN (1 << 2)
#define PR_FLAG_SPACE (1 << 3)
#define PR_FLAG_POUND (1 << 4)
#define PR_FLAG_ZERO (1 << 5)
#define PR_FLAG_PREC (1 << 6)
#define PR_FLAG_SCIENTIFIC (1 << 7)
#define PR_FLAG_HEX (1 << 8)
#define PR_FLAG_OCT (1 << 9)
#define PR_FLAG_CUT_ZEROS (1 << 10)

static int fromdecimal(int c) {
#if '9' - '0' == 9
    return c - '0';
#else
    switch (c) {
    case '0':   return 0;
    case '1':   return 1;
    case '2':   return 2;
    case '3':   return 3;
    case '4':   return 4;
    case '5':   return 5;
    case '6':   return 6;
    case '7':   return 7;
    case '8':   return 8;
    case '9':   return 9;
    default:    return -1;
    }
#endif
}

static int todecimal(int c) {
#if '9' - '0' == 9
    return '0' + c;
#else
    return "0123456789"[c];
#endif
}

static int tooctal(int c) {
#if '7' - '0' == 7
    return '0' + c;
#else
    return "01234567"[c];
#endif
}

static int tohex(int c, int u) {
#if '9' - '0' == 9 && 'F' - 'A' == 5 && 'f' - 'a' == 5
    return c >= 10 ? (u ? 'A' : 'a') + c - 10 : '0' + c;
#else
    return (u ? "0123456789ABCDEF" : "0123456789abcdef")[c];
#endif
}

INLINE int toibase(int c, int b, int u) {
    switch (b) {
    case 8:
        return tooctal(c);
    case 10:
        return todecimal(c);
    case 16:
        return tohex(c, u);
    default:
        return 0;
    }
}

static int unc__vxprintf_i(int (*out)(char outp, void *udata),
                 void *udata, int f, size_t w, size_t p, intmax_t x) {
    int outp = 0;
    char bufa[(sizeof(intmax_t) * CHAR_BIT * 5 + 15) / 16 + 1], *buf = bufa;
    char sign = 0;
    size_t sz, iw = 0, i;
    uintmax_t u = (uintmax_t)(x < 0 ? -x : x);

    buf += sizeof(bufa);
    while (u) {
        *--buf = todecimal(u % 10);
        u /= 10;
        ++iw;
    }
    sz = iw;
    if (sz < p) sz = p;

    if (x < 0)
        sign = '-', ++sz;
    else if (f & PR_FLAG_SIGN)
        sign = '+', ++sz;
    else if (f & PR_FLAG_SPACE)
        sign = ' ', ++sz;

    if (f & PR_FLAG_LEFT) {
        for (i = sz; i < w; ++i)
            PUTC(' ');
        if (sign) PUTC(sign);
    } else if (f & PR_FLAG_ZERO) {
        if (sign) PUTC(sign);
        for (i = sz; i < w; ++i)
            PUTC('0');
    } else {
        if (sign) PUTC(sign);
    }
    
    for (i = iw; i < p; ++i)
        PUTC('0');
    for (i = 0; i < iw; ++i)
        PUTC(buf[i]);
    
    if (!(f & (PR_FLAG_LEFT | PR_FLAG_ZERO)))
        for (i = sz; i < w; ++i)
            PUTC(' ');
    
    return outp;
}

static int unc__vxprintf_u(int (*out)(char outp, void *udata),
                 void *udata, int f, size_t w, size_t p, uintmax_t u) {
    int outp = 0;
    char bufa[(sizeof(uintmax_t) * CHAR_BIT * 6 + 15) / 16 + 1], *buf = bufa;
    char sign = 0;
    size_t sz, iw = 0, i;
    int base = 10;

    if (f & PR_FLAG_HEX) base = 16;
    else if (f & PR_FLAG_OCT) base = 8;

    buf += sizeof(bufa);
    while (u) {
        *--buf = toibase(u % base, base, f & PR_FLAG_UPPER);
        u /= base;
        ++iw;
    }
    sz = iw;
    if (sz < p) sz = p;

    if (f & PR_FLAG_POUND) {
        if (base == 8 && p <= iw)
            sz += (iw + 1) - p, p = iw + 1;
        else if (base == 16)
            sz += 2;
    }

    if (f & PR_FLAG_LEFT)
        for (i = sz; i < w; ++i)
            PUTC(' ');
    
    if (sign) PUTC(sign);
    if ((f & PR_FLAG_POUND) && base == 16) {
        PUTC('0');
        PUTC((f & PR_FLAG_UPPER) ? 'X' : 'x');
    }

    if (!(f & PR_FLAG_LEFT) && (f & PR_FLAG_ZERO))
        for (i = sz; i < w; ++i)
            PUTC('0');
    for (i = iw; i < p; ++i)
        PUTC('0');
    for (i = 0; i < iw; ++i)
        PUTC(buf[i]);
    
    if (!(f & (PR_FLAG_LEFT | PR_FLAG_ZERO)))
        for (i = sz; i < w; ++i)
            PUTC(' ');
    
    return outp;
}

static int unc__vxprintf_xf(int (*out)(char outp, void *udata),
                 void *udata, int f, size_t w, size_t p, long double x) {
    /* int outp = 0; no support yet TODO */
    NEVER_();
    return -1;
    /* return outp; */
}

static int unc__vxprintf_sf(int (*out)(char outp, void *udata),
                 void *udata, int f, size_t w, size_t p,
                 size_t sn, const char *s, int negative) {
    int outp = 0;
    size_t sz = sn, i;
    char sign = 0;
    if (negative)
        sign = '-', ++sz;
    else if (f & PR_FLAG_SIGN)
        sign = '+', ++sz;
    else if (f & PR_FLAG_SPACE)
        sign = ' ', ++sz;
    
    if (f & (PR_FLAG_LEFT | PR_FLAG_ZERO))
        for (i = sz; i < w; ++i)
            PUTC(' ');
    
    if (sign) PUTC(sign);
    for (i = 0; i < sn; ++i)
        PUTC(s[i]);
    
    if (!(f & (PR_FLAG_LEFT | PR_FLAG_ZERO)))
        for (i = sz; i < w; ++i)
            PUTC(' ');
    
    return outp;
}

#if UNCIL_C99 && UNCIL_64BIT
#define NUMTYPE uint_least64_t
#define NUMWIDTH 64
#define NUMLOG2 6
#if defined(__GNUC__) && defined(__SIZEOF_INT128__)
typedef unsigned __int128 uint128_t;
INLINE void widediv(
        uint_least64_t d, uint_least64_t l, uint_least64_t h,
        uint_least64_t *rem, uint_least64_t *qh, uint_least64_t *ql) {
    uint128_t x = ((uint128_t)h << 64) | l, q = x / d;
    *rem = x % d;
    *qh = (uint_least64_t)(q >> 64);
    *ql = (uint_least64_t)(q);
}
INLINE void widemul(uint_least64_t m, uint_least64_t *l, uint_least64_t *h) {
    uint128_t x = ((uint128_t)*h << 64) | *l;
    x *= m;
    *h = (uint_least64_t)(x >> 64);
    *l = (uint_least64_t)(x);
}
#else
#define LONGDIV 1
#endif
#elif !UNCIL_C99
#define NUMWIDTH 32
#define NUMLOG2 5
#define MASK 0xFFFFFFFFUL
#if UINT_MAX >= MASK
#define NUMTYPE unsigned int
#else
#define NUMTYPE unsigned long
#endif
#define LONGDIV 1
#else
#define NUMTYPE uint_least32_t
#define NUMWIDTH 32
#define NUMLOG2 5
INLINE void widediv(
        uint_least32_t d, uint_least32_t l, uint_least32_t h,
        uint_least32_t *rem, uint_least32_t *qh, uint_least32_t *ql) {
    uint_least64_t x = ((uint_least64_t)h << 32) | l, q = x / d;
    *rem = x % d;
    *qh = (uint_least32_t)(q >> 32);
    *ql = (uint_least32_t)(q);
}
INLINE void widemul(uint_least32_t m, uint_least32_t *l, uint_least32_t *h) {
    uint_least64_t x = ((uint_least64_t)*h << 32) | *l;
    x *= m;
    *h = (uint_least32_t)(x >> 32);
    *l = (uint_least32_t)(x);
}
#endif

#if LONGDIV
#if MASK
#define LSHX(h, l, n) (h = MASK & (l >> (NUMWIDTH - (n))),                     \
                       l = MASK & (l << n))
#define LSH(h, l, n) (h = MASK & ((h << (n)) | (l >> (NUMWIDTH - (n)))),       \
                      l = MASK & (l << n))
#define RSH(h, l, n) (l = MASK & ((l >> (n)) | (h << (NUMWIDTH - (n)))),       \
                      h = MASK & (h >> n))
#else
#define LSHX(h, l, n) (h = (l >> (NUMWIDTH - (n))), l <<= (n))
#define LSH(h, l, n) (h = (h << (n)) | (l >> (NUMWIDTH - (n))), l <<= (n))
#define RSH(h, l, n) (l = (l >> (n)) | (h << (NUMWIDTH - (n))), h >>= (n))
#endif
INLINE int ilog2(NUMTYPE x) {
    int n = 0;
    while (x >>= 1) ++n;
    return n;
}

INLINE void widediv(
        NUMTYPE d, NUMTYPE l, NUMTYPE h,
        NUMTYPE *rem, NUMTYPE *qh, NUMTYPE *ql) {
    if (h) {
        NUMTYPE dh, dl, pqh = 0, pql = 0, tmp;
        int k = ilog2(h) - ilog2(d) + NUMWIDTH;
        if (k >= NUMWIDTH) {
            dh = d << (k & (NUMWIDTH - 1)), dl = 0;
        } else {
            dh = 0, dl = d;
            if (k) LSHX(dh, dl, k);
        }
        do {
            LSH(pqh, pql, 1);
            if (h > dh) {
                h -= dh;
                tmp = l;
                l -= dl;
                if (l >= tmp) --h;
                pql |= 1;
            } else if (h == dh && l >= dl) {
                h = 0;
                l -= dl;
                pql |= 1;
            }
            RSH(dh, dl, 1);
        } while (k--);
        *rem = l;
        *qh = pqh;
        *ql = pql;
    } else {
        *rem = l % d;
        *qh = 0;
        *ql = l / d;
    }
}

#define HALFPART (NUMWIDTH / 2)
#define LOMASK (((NUMTYPE)1 << HALFPART) - 1)
INLINE void widemul(NUMTYPE m, NUMTYPE *l, NUMTYPE *h) {
    NUMTYPE q = *l;
    NUMTYPE ll = q & LOMASK;
    NUMTYPE ml = m & LOMASK;
    NUMTYPE w = ll * ml;
    NUMTYPE wl = w & LOMASK;
    NUMTYPE wh = w >> HALFPART;
    NUMTYPE ww;
    q >>= HALFPART;
    m >>= HALFPART;
    w = q * ml + wh;
    wh = w & LOMASK;
    ww = w >> HALFPART;
    w = ll * m + wh;
    wh = w >> HALFPART;
    *h = (*h + q) * m + ww + wh;
    *l = (w << HALFPART) + wl;
}
#endif

INLINE void wideadd(NUMTYPE *p, NUMTYPE d) {
    NUMTYPE t, v;
    do {
        t = *p, *p++ = v = t + d;
    } while (v < t);
}

static int cvtb_f2i(char **pbuf, long double p) {
    /* pretty inefficient */
    int iw = 0, exp, iexp, hi, lo, i = 0;
    char *buf = *pbuf;
#if UNCIL_C99 && UNCIL_64BIT
    NUMTYPE div = UINT64_C(1000000000000000000);
    int dig = 18;
#elif UNCIL_C99
    NUMTYPE div = UINT32_C(1000000000);
    int dig = 9;
#else
    NUMTYPE div = 1000000000UL;
    int dig = 9;
#endif
    NUMTYPE num[LDBL_MAX_EXP / NUMWIDTH + 1] = { 0 };
    NUMTYPE quo[LDBL_MAX_EXP / NUMWIDTH + 1] = { 0 };
    NUMTYPE rem, tmp, tmpl, tmph;
#if UNCIL_C99
    long double frac = frexpl(p, &exp);
    if (exp >= LDBL_MANT_DIG)
        iexp = LDBL_MANT_DIG, exp -= LDBL_MANT_DIG;
    else
        iexp = exp, exp = 0;
#else
    double frac = frexp(p, &exp);
    if (exp >= DBL_MANT_DIG)
        iexp = DBL_MANT_DIG, exp -= DBL_MANT_DIG;
    else
        iexp = exp, exp = 0;
#endif
#if UNCIL_C99 && NUMWIDTH == 64
    {
        NUMTYPE rem = (NUMTYPE)ldexpl(frac, iexp);
        hi = exp >> NUMLOG2, lo = exp & ((1 << NUMLOG2) - 1);
        num[hi++] = rem << lo;
        if (lo) num[hi++] = rem >> (NUMWIDTH - lo);
    }
#else
    {
        unsigned long reml = (NUMTYPE)ldexp(frac, iexp) & 0xFFFFFFFFUL;
        unsigned long remh = (NUMTYPE)ldexp(frac, iexp - 32) & 0xFFFFFFFFUL;
        hi = exp >> 5, lo = exp & ((1 << 5) - 1);
        if (lo) {
            num[hi++] = reml << lo;
            num[hi++] = (reml >> (32 - lo)) || (remh << lo);
            num[hi++] = remh >> (32 - lo);
        } else {
            num[hi++] = reml;
            num[hi++] = remh;
        }
    }
#endif

    do {
        /* divide num by div */
        i = hi;
        unc__memset(quo, 0, hi * sizeof(NUMTYPE));
        while (i--) {
            widediv(div, num[i], num[i + 1], &rem, &tmph, &tmpl);
            wideadd(&quo[i], tmpl);
            wideadd(&quo[i + 1], tmph);
            widemul(div, &tmpl, &tmph);
            num[i + 1] -= tmph;
            tmp = num[i];
            if ((num[i] -= tmpl) > tmp) --num[i + 1];
        }
        rem = num[0];
        unc__memcpy(num, quo, hi * sizeof(NUMTYPE));
        while (hi && !num[hi - 1]) --hi;
        for (i = 0; i < dig && (rem || hi); ++i) {
            *--buf = todecimal(rem % 10);
            rem /= 10;
            ++iw;
        }
    } while (hi);

    while (iw && *buf == '0')
        --iw, ++buf;

    *pbuf = buf;
    return iw;
}

static int unc__vxprintf_f(int (*out)(char outp, void *udata),
                 void *udata, int f, size_t w, size_t prec, long double x) {
    int outp = 0;
    char sign = 0;
    if (x != x)
        return unc__vxprintf_sf(out, udata, f, w, prec,
            3, (f & PR_FLAG_UPPER) ? "NAN" : "nan", x < 0);
#if UNCIL_C99
    if (x > LDBL_MAX || x < -LDBL_MAX)
#else
    if (x > DBL_MAX || x < -DBL_MAX)
#endif
        return unc__vxprintf_sf(out, udata, f, w, prec,
            3, (f & PR_FLAG_UPPER) ? "INF" : "inf", x < 0);
    
    if (x < 0)
        sign = '-', x = -x;
    else if (f & PR_FLAG_SIGN)
        sign = '+';
    else if (f & PR_FLAG_SPACE)
        sign = ' ';
    
    if (!(f & PR_FLAG_SCIENTIFIC)) {
        char bufa[LDBL_MAX_10_EXP + 3], *buf = bufa;
        size_t sz, iw = 0, i;
        long double q;
        int dot = 0;
#if UNCIL_C99
        long double p;
        if (!prec)
            x += 0.5L;
        else
#if UNCIL_C99
            x += 0.5L * powl(0.1L, prec);
#else
            x += 0.5L * pow(0.1, prec);
#endif
        q = modfl(x, &p);
#else
        double p;
        if (!prec)
            x += 0.5L;
        else
#if UNCIL_C99
            x += 0.5L * powl(0.1L, prec);
#else
            x += 0.5L * pow(0.1, prec);
#endif
        q = modf(x, &p);
#endif
        buf += sizeof(bufa);
        iw = p ? cvtb_f2i(&buf, p) : 0;
        if (!iw)
            *--buf = '0', ++iw;
        sz = iw + (sign != 0);
        if (f & PR_FLAG_CUT_ZEROS) {
            size_t tprec = 0, zprec = 0;
            long double qq = q;
            while (tprec < prec && qq) {
#if UNCIL_C99
                qq = modfl(qq * 10, &p);
#else
                qq = modf(qq * 10, &p);
#endif
                ++tprec;
                if (p) zprec = tprec;
            }
            prec = zprec;
        }
        if (prec || (f & PR_FLAG_POUND)) {
            sz += 1 + prec;
            dot = 1;
        }

        if (f & PR_FLAG_LEFT) {
            for (i = sz; i < w; ++i)
                PUTC(' ');
            if (sign) PUTC(sign);
        } else if (f & PR_FLAG_ZERO) {
            if (sign) PUTC(sign);
            for (i = sz; i < w; ++i)
                PUTC('0');
        } else {
            if (sign) PUTC(sign);
        }
        
        for (i = 0; i < iw; ++i)
            PUTC(buf[i]);
        if (dot) {
            PUTC('.');
            for (i = 0; i < prec; ++i) {
#if UNCIL_C99
                q = modfl(q * 10, &p);
#else
                q = modf(q * 10, &p);
#endif
                PUTC(todecimal((int)p));
            }
        }
        
        if (!(f & (PR_FLAG_LEFT | PR_FLAG_ZERO)))
            for (i = sz; i < w; ++i)
                PUTC(' ');
    } else {
        char bufa[(sizeof(intmax_t) * CHAR_BIT * 5 + 15) / 16 + 1], *buf = bufa;
        char esign;
        intmax_t off;
        int ip, dot = 0;
        size_t eiw = 0, sz, i;
#if UNCIL_C99
        long double p;
#else
        double p;
#endif
        buf += sizeof(bufa);
        if (!prec)
            x += 0.5L;
        if (!x)
            off = 0;
        else {
#if UNCIL_C99
            off = (intmax_t)floor(log10l(fabsl(x)));
            x *= powl(10.0L, -off);
#else
            off = (intmax_t)floor(log10(fabs((double)x)));
            x *= pow(10.0, -off);
#endif
        }
        
        if (prec)
#if UNCIL_C99
            x += 0.5L * powl(0.1L, prec);
#else
            x += 0.5L * pow(0.1, prec);
#endif
#if UNCIL_C99
        x = modfl(x, &p);
#else
        x = modf(x, &p);
#endif
        ip = (int)p;
        
        if (off < 0) {
            off = -off;
            esign = '-';
        } else
            esign = '+';
        while (off) {
            *--buf = todecimal(off % 10);
            off /= 10;
            ++eiw;
        }
        while (eiw < 2)
            *--buf = '0', ++eiw;
        
        sz = 3 + eiw + (sign != 0);
        if (f & PR_FLAG_CUT_ZEROS) {
            size_t tprec = 0, zprec = 0;
#if UNCIL_C99
            long double qq = x;
#else
            double qq = x;
#endif
            while (tprec < prec && qq) {
#if UNCIL_C99
                qq = modfl(qq * 10, &p);
#else
                qq = modf(qq * 10, &p);
#endif
                ++tprec;
                if (p) zprec = tprec;
            }
            prec = zprec;
        }
        if (prec || (f & PR_FLAG_POUND)) {
            sz += 1 + prec;
            dot = 1;
        }

        if (f & PR_FLAG_LEFT) {
            for (i = sz; i < w; ++i)
                PUTC(' ');
            if (sign) PUTC(sign);
        } else if (f & PR_FLAG_ZERO) {
            if (sign) PUTC(sign);
            for (i = sz; i < w; ++i)
                PUTC('0');
        } else {
            if (sign) PUTC(sign);
        }
        
        PUTC(todecimal(ip));
        if (dot) {
            PUTC('.');
            for (i = 0; i < prec; ++i) {
#if UNCIL_C99
                x = modfl(x * 10, &p);
#else
                x = modf(x * 10, &p);
#endif
                PUTC(todecimal((int)p));
            }
        }
        
        PUTC((f & PR_FLAG_UPPER) ? 'E' : 'e');
        PUTC(esign);
        for (i = 0; i < eiw; ++i)
            PUTC(buf[i]);
        
        if (!(f & (PR_FLAG_LEFT | PR_FLAG_ZERO)))
            for (i = sz; i < w; ++i)
                PUTC(' ');
    }

    return outp;
}

static int unc__vxprintf_s(int (*out)(char outp, void *udata),
                 void *udata, int f, size_t w, size_t p, const char *s) {
    int outp = 0;
    size_t l = 0, i;
    while (l < p && s[l])
        ++l;
    
    if (f & PR_FLAG_LEFT)
        for (i = l; i < w; ++i)
            PUTC(' ');
    
    for (i = 0; i < l; ++i)
        PUTC(s[i]);
    
    if (!(f & PR_FLAG_LEFT))
        for (i = l; i < w; ++i)
            PUTC(' ');
    
    return outp;
}

static int unc__vxprintf_ss(int (*out)(char outp, void *udata),
                 void *udata, int f, size_t w, size_t p, const char *s) {
    int outp = 0;
    size_t i;
    
    if (f & PR_FLAG_LEFT)
        for (i = p; i < w; ++i)
            PUTC(' ');
    
    for (i = 0; i < p; ++i)
        PUTC(s[i]);
    
    if (!(f & PR_FLAG_LEFT))
        for (i = p; i < w; ++i)
            PUTC(' ');
    
    return outp;
}

int unc__vxprintf(int (*out)(char outp, void *udata),
                 void *udata, const char *format, va_list arg) {
    int outp = 0, c;
    while ((c = *format++)) {
        if (c != '%')
            PUTC(c);
        else {
            int flags = 0;
            size_t width = 0;
            size_t precision = 0;
            enum length_mod len = L_;
            int e = 0;

            if ((c = *format) == '%') {
                PUTC(c);
                ++format;
                continue;
            }

nextflag:
            switch ((c = *format)) {
            case '-':
                flags |= PR_FLAG_LEFT;
                ++format;
                goto nextflag;
            case '+':
                flags |= PR_FLAG_SIGN;
                ++format;
                goto nextflag;
            case ' ':
                flags |= PR_FLAG_SPACE;
                ++format;
                goto nextflag;
            case '#':
                flags |= PR_FLAG_POUND;
                ++format;
                goto nextflag;
            case '0':
                flags |= PR_FLAG_ZERO;
                ++format;
                goto nextflag;
            }

            if (unc__isdigit(*format)) {
                size_t nw;
                while (unc__isdigit((c = *format))) {
                    nw = width;
                    width = width * 10 + fromdecimal(c);
                    if (width < nw) {
                        width = SIZE_MAX;
                        while (unc__isdigit(*++format))
                            ;
                        break;
                    }
                    ++format;
                }
            } else if (*format == '*') {
                width = (size_t)va_arg(arg, int);
                ++format;
            }
            
            if (*format == '.') {
                flags |= PR_FLAG_PREC;
                ++format;
                if (unc__isdigit(*format)) {
                    size_t nw;
                    while (unc__isdigit((c = *format))) {
                        nw = precision;
                        precision = precision * 10 + fromdecimal(c);
                        if (precision < nw) {
                            precision = SIZE_MAX;
                            while (unc__isdigit(*++format))
                                ;
                            break;
                        }
                        ++format;
                    }
                } else if (*format == '*') {
                    precision = (size_t)va_arg(arg, int);
                    ++format;
                }
            }

            switch ((c = *format)) {
            case 'h':
                if (*++format == 'h')
                    len = L_hh, ++format;
                else
                    len = L_h;
                break;
            case 'l':
                if (*++format == 'l')
                    len = L_ll, ++format;
                else
                    len = L_l;
                break;
            case 'j':
                len = L_j;
                ++format;
                break;
            case 't':
                len = L_t;
                ++format;
                break;
            case 'z':
                len = L_z;
                ++format;
                break;
            case 'L':
                len = L_L;
                ++format;
                break;
            }

            switch ((c = *format++)) {
            case 'd':
            case 'i':
            {
                intmax_t num;
                switch (len) {
                case L_hh:
                    num = (intmax_t)(signed char)va_arg(arg, signed int);
                    break;
                case L_h:
                    num = (intmax_t)(signed short)va_arg(arg, signed int);
                    break;
                case L_l:
                    num = (intmax_t)va_arg(arg, signed long);
                    break;
#if UNCIL_C99
                case L_ll:
                    num = (intmax_t)va_arg(arg, signed long long);
                    break;
#endif
                case L_j:
                    num = (intmax_t)va_arg(arg, intmax_t);
                    break;
                case L_t:
                    num = (intmax_t)va_arg(arg, ptrdiff_t);
                    break;
                case L_z:
                    num = (intmax_t)va_arg(arg, size_t);
                    break;
                default:
                    num = (intmax_t)va_arg(arg, int);
                    break;
                }
                if (!(flags & PR_FLAG_PREC)) precision = 1;
                else flags &= ~PR_FLAG_ZERO;
                e = unc__vxprintf_i(out, udata, flags, width, precision, num);
                break;
            }
            case 'o':
                flags |= PR_FLAG_OCT;
                goto printfuint;
            case 'X':
                flags |= PR_FLAG_UPPER;
            case 'x':
                flags |= PR_FLAG_HEX;
                goto printfuint;
            case 'u':
                e = 0;
            printfuint:
            {
                intmax_t num;
                switch (len) {
                case L_hh:
                    num = (uintmax_t)(unsigned char)va_arg(arg, unsigned int);
                    break;
                case L_h:
                    num = (uintmax_t)(unsigned short)va_arg(arg, unsigned int);
                    break;
                case L_l:
                    num = (uintmax_t)va_arg(arg, unsigned long);
                    break;
#if UNCIL_C99
                case L_ll:
                    num = (uintmax_t)va_arg(arg, unsigned long long);
                    break;
#endif
                case L_j:
                    num = (uintmax_t)va_arg(arg, uintmax_t);
                    break;
                case L_t:
                    num = (uintmax_t)va_arg(arg, ptrdiff_t);
                    break;
                case L_z:
                    num = (uintmax_t)va_arg(arg, size_t);
                    break;
                default:
                    num = (uintmax_t)va_arg(arg, unsigned int);
                    break;
                }
                if (!(flags & PR_FLAG_PREC)) precision = 1;
                else flags &= ~PR_FLAG_ZERO;
                e = unc__vxprintf_u(out, udata, flags, width, precision, num);
                break;
            }
            case 'F':
            case 'E':
            case 'G':
            case 'A':
                flags |= PR_FLAG_UPPER;
                c = unc__tolower(c);
            case 'f':
            case 'e':
            case 'g':
            case 'a':
            {
                long double num;
                switch (len) {
                case L_l:
                    num = (long double)va_arg(arg, double);
                    break;
                case L_L:
                    num = (long double)va_arg(arg, long double);
                    break;
                default:
                    num = (long double)(float)va_arg(arg, double);
                    break;
                }

                if (!(flags & PR_FLAG_PREC)) precision = 6;
                if (c == 'g') {
                    intmax_t exp = 
#if UNCIL_C99
                        num ? (intmax_t)floorl(log10l(fabsl(num))) : 0;
#else
                        num ? (intmax_t)floor(log10(fabs((double)num))) : 0;
#endif
                    if (!precision) precision = 1;
                    if ((exp < 0 || precision > exp) && exp >= -4) {
                        precision -= exp + 1;
                    } else {
                        flags |= PR_FLAG_SCIENTIFIC;
                        precision -= 1;
                    }
                    if (!(flags & PR_FLAG_POUND))
                        flags |= PR_FLAG_CUT_ZEROS;
                }
                else if (c == 'e')
                    flags |= PR_FLAG_SCIENTIFIC;
                else if (c == 'a') {
                    e = unc__vxprintf_xf(out, udata, flags,
                                         width, precision, num);
                    break;
                }
                e = unc__vxprintf_f(out, udata, flags, width, precision, num);
                break;
            }
            case 'c':
                PUTC(va_arg(arg, int));
                break;
            case 's':
                if (!(flags & PR_FLAG_PREC)) precision = SIZE_MAX;
                e = unc__vxprintf_s(out, udata, flags, width, precision,
                                    va_arg(arg, const char *));
                break;
            case 'S':
                if (!(flags & PR_FLAG_PREC)) precision = 1;
                e = unc__vxprintf_ss(out, udata, flags, width, precision,
                                     va_arg(arg, const char *));
                break;
            case 'p':
                PUTC('0');
                PUTC('x');
                e = unc__vxprintf_u(out, udata, (flags | PR_FLAG_HEX)
                                & ~(PR_FLAG_OCT | PR_FLAG_ZERO), 0,
                                (sizeof(uintptr_t) * CHAR_BIT + 3) / 4,
                                (uintptr_t)va_arg(arg, void *));
                break;
            case 'n':
                *(va_arg(arg, int *)) = outp;
                break;
            }

            if (e < 0)
                return e;
            outp += e;
        }
    }
    return outp;
}

int unc__xprintf(int (*out)(char outp, void *udata),
                 void *udata, const char *format, ...) {
    int r;
    va_list va;
    va_start(va, format);
    r = unc__vxprintf(out, udata, format, va);
    va_end(va);
    return r;
}

struct xsnprintf_buffer {
    char *c;
    size_t n;
};

static int xsnprintf_wrapper(char c, void *udata) {
    struct xsnprintf_buffer *buffer = udata;
    if (buffer->n)
        *buffer->c++ = c, --buffer->n;
    return 0;
}

int unc__vxsnprintf(char *out, size_t n, const char *format, va_list arg) {
    int r;
    struct xsnprintf_buffer buf;
    buf.c = out;
    buf.n = n - 1;
    r = unc__vxprintf(&xsnprintf_wrapper, &buf, format, arg);
    *buf.c++ = 0;
    return r;
}

int unc__xsnprintf(char *out, size_t n, const char *format, ...) {
    int r;
    va_list va;
    va_start(va, format);
    r = unc__vxsnprintf(out, n, format, va);
    va_end(va);
    return r;
}
