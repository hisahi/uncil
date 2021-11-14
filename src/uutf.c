/*******************************************************************************
 
Uncil -- Unicode conversion impl

Copyright (c) 2021 Sampo Hippeläinen (hisahi)

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

#include "uutf.h"

Unc_Size unc__utf8enc(Unc_UChar u, Unc_Size n, byte *out) {
    Unc_Size i = 0;
    if (u < 0x80UL) {
        if (i++ < n) *out++ = (byte)(u & 0x7F);
    } else if (u < 0x800UL) {
        if (i++ < n) *out++ = (byte)(0xC0 | ((u >>  6) & 0x1F));
        if (i++ < n) *out++ = (byte)(0x80 | ( u        & 0x3F));
    } else if (u < 0x10000UL) {
        if (i++ < n) *out++ = (byte)(0xE0 | ((u >> 12) & 0x0F));
        if (i++ < n) *out++ = (byte)(0x80 | ((u >>  6) & 0x3F));
        if (i++ < n) *out++ = (byte)(0x80 | ( u        & 0x3F));
    } else if (u < 0x110000UL) {
        if (i++ < n) *out++ = (byte)(0xF0 | ((u >> 18) & 0x07));
        if (i++ < n) *out++ = (byte)(0x80 | ((u >> 12) & 0x3F));
        if (i++ < n) *out++ = (byte)(0x80 | ((u >>  6) & 0x3F));
        if (i++ < n) *out++ = (byte)(0x80 | ( u        & 0x3F));
    } else {
        return unc__utf8enc(u, 0xFFFDUL, out);
    }
    return i;
}

Unc_UChar unc__utf8dec(Unc_Size n, const byte **in) {
    Unc_Size i = 0;
    const byte *p = *in;
    if (i < n) {
        Unc_UChar u;
        byte b = *p++;
        int j;
        ++i;
        if (!(b & 0x80)) {
            *in = p;
            return b;
        } else if ((b & 0xE0) == 0xC0) {
            u = b & 0x1F;
            j = 2;
        } else if ((b & 0xF0) == 0xE0) {
            u = b & 0x0F;
            j = 3;
        } else if ((b & 0xF8) == 0xF0) {
            u = b & 0x07;
            j = 4;
        } else {
            *in = p;
            return 0xFFFDUL;
        }
        for (; i < j && i < n; ++i) {
            b = *p++;
            if ((b & 0xC0) != 0x80) {
                --p;
                break;
            }
            u = (u << 6) | (b & 0x3F);
        }
        *in = p;
        if (i < j || u >= 0x110000UL) return 0xFFFDU;
        return u;
    }
    return 0;
}

Unc_UChar unc__utf8decx(Unc_Size *n, const byte **in) {
    const byte *ins = *in;
    Unc_UChar u = unc__utf8dec(*n, in);
    *n -= *in - ins;
    return u;
}

Unc_UChar unc__utf8decd(const byte *in) {
    return unc__utf8dec(UNC_UTF8_MAX_SIZE, &in);
}

#define UEAT() (in < e ? *in++ : -1)
#define UEMIT(c) do { ++q; if (out) *out++ = c; } while (0)
/* shorten overlong encodings and pad remaining spaces with 00, modify buffer.
   invalid bytes are ignored */
Unc_Size unc__utf8patdown(Unc_Size n, byte *out, const byte *in) {
    int c;
    Unc_Size q = 0;
    const byte *e = in + n;
    byte b[UNC_UTF8_MAX_SIZE];
    while (in < e) {
        c = *in++;
        if (c < 0x80) {
            UEMIT(c);
        } else if (c < 0xC0) {
            /* continuation when not expected... skip */
            continue;
        } else if (c > 0xF4) {
            /* currently invalid too */
            continue;
        } else {
            int i, j;
            Unc_UChar u;
            
            b[0] = c;
            if (c < 0xE0) j = 1;
            else if (c < 0xF0) j = 2;
            else j = 3;
            for (i = 1; i <= j; ++i) {
                c = UEAT();
                if (c < 0)
                    goto ufailed;
                if ((c & 0xC0) != 0x80) {
                    --in;
                    goto ufailed;
                }
                b[i] = c;
            }
            {
                const byte *x = b;
                u = unc__utf8dec(i, &x);
            }
            j = (int)unc__utf8enc(u, sizeof(b), b);
            for (i = 0; i < j; ++i)
                UEMIT(b[i]);
        }
ufailed:;
    }
    return q;
}

/* return length if unc__utf8patdown */
Unc_Size unc__utf8patdownl(Unc_Size n, const byte *in) {
    return unc__utf8patdown(n, NULL, in);
}

const byte *unc__utf8scanforw(const byte *s0, const byte *s1, Unc_Size count) {
    for (;;) {
        if (s0 >= s1)
            return NULL;
        if (!count)
            return s0;
        --count;
        if (*s0 < 0xc0) {
            s0 += 1;
        } else if (*s0 < 0xe0) {
            s0 += 2;
        } else if (*s0 < 0xf0) {
            s0 += 3;
        } else {
            s0 += 4;
        }
    }
}

const byte *unc__utf8scanbackw(const byte *s0, const byte *s1, Unc_Size count) {
    ++count;
    for (;;) {
        if (!count)
            return s1;
        if (s0 >= s1)
            return NULL;
        if ((*--s1 & 0xC0) != 0x80)
            --count;
    }
}

const byte *unc__utf8nextchar(const byte *sb, Unc_Size *n) {
    Unc_Size o, q = *n;
    if (!q)
        return NULL;
    if (*sb < 0xc0) {
        o = 1;
    } else if (*sb < 0xe0) {
        o = 2;
    } else if (*sb < 0xf0) {
        o = 3;
    } else {
        o = 4;
    }
    if (o > q)
        o = q;
    *n -= o;
    return sb + o;
}

const byte *unc__utf8lastchar(const byte *sb, Unc_Size n) {
    return unc__utf8scanbackw(sb, sb + n, 0);
}

int unc__utf8validate(Unc_Size n, const char *s) {
    int c, j;
    Unc_UChar u, m;
    while (n--) {
        c = *s++;
        if (!(c & 0x80)) {
            continue;
        } else if ((c & 0xE0) == 0xC0) {
            j = 1;
            m = ~(Unc_UChar)0x7FUL;
            u = c & 0x1F;
        } else if ((c & 0xF0) == 0xE0) {
            j = 2;
            m = ~(Unc_UChar)0x7FFUL;
            u = c & 0x0F;
        } else if ((c & 0xF8) == 0xF0) {
            j = 3;
            m = ~(Unc_UChar)0xFFFFUL;
            u = c & 0x07;
        } else {
            return 1;
        }
        if (n < j) return 1;
        n -= j;
        do {
            c = *s++;
            if ((c & 0xC0) != 0x80) return 1;
            u = (u << 6) | (c & 0x3F);
        } while (--j);
        if (!(u & m)) return 1;
    }
    return 0;
}

int unc__cconv_utf8ts(Unc_CConv_In in, void *in_data,
                      Unc_CConv_Out out, void *out_data,
                      Unc_Size n) {
    Unc_Size b = 0;
    byte buf[UNC_UTF8_MAX_SIZE];
    int i, j, c;
    while (b < n) {
        c = (*in)(in_data);
        if (c < 0) break;
        if (!(c & 0x80)) {
            j = 1;
        } else if ((c & 0xE0) == 0xC0) {
            j = 2;
        } else if ((c & 0xF0) == 0xE0) {
            j = 3;
        } else if ((c & 0xF8) == 0xF0) {
            j = 4;
        } else {
            return 1;
        }
        buf[0] = c;
        for (i = 1; i < j; ++i) {
            c = (*in)(in_data);
            if (c < 0) return b;
            if ((c & 0xC0) != 0x80)
                return 1;
            buf[i] = c;
        }
        c = (*out)(out_data, j, buf);
        if (c > 0)
            return 0;
        else if (!c)
            ++b;
    }
    return 0;
}

const byte *unc__utf8shift(const byte *s, Unc_Size *n, Unc_Size i) {
    Unc_Size q = *n;
    while (i--) {
        s = unc__utf8nextchar(s, &q);
        if (!s) {
            *n = 0;
            return NULL;
        }
    }
    *n = q;
    return s;
}

const byte *unc__utf8shiftback(const byte *s, Unc_Size *n, Unc_Size i) {
    Unc_Size q = *n, r = 0;
    const byte *e = s + q;
    while (i--) {
        do {
            if (!q)
                return NULL;
            --q;
            ++r;
        } while ((*--e & 0xC0) == 0x80);
    }
    *n = r;
    return e;
}

const byte *unc__utf8shiftaway(const byte *s, Unc_Size *n, Unc_Size i) {
    Unc_Size q = *n;
    const byte *e = s + q;
    while (i--) {
        do {
            if (!q) {
                *n = 0;
                return NULL;
            }
            --q;
        } while ((*--e & 0xC0) == 0x80);
    }
    *n = q;
    return s;
}

Unc_Size unc__utf8reshift(const byte *s, Unc_Size z, Unc_Size n) {
    const byte *sp = unc__utf8shift(s, &z, n);
    if (!sp) return UNC_RESHIFTBAD;
    return sp - s;
}

Unc_Size unc__utf8unshift(const byte *s, Unc_Size n) {
    Unc_Size q = 0;
    while (n--)
        if ((*s++ & 0xC0) != 0x80)
            ++q;
    return q;
}

void unc__utf8rev(byte *b, const byte *s, Unc_Size n) {
    byte c;
    Unc_Size i, z = n, k = 0;
    for (i = 0; i < n; ++i) {
        c = s[i];
        if (!(c & 0x80)) {
            z -= 1;
            k = 0;
        } else if ((c & 0xE0) == 0xC0) {
            z -= 2;
            k = 0;
        } else if ((c & 0xF0) == 0xE0) {
            z -= 3;
            k = 0;
        } else if ((c & 0xF8) == 0xF0) {
            z -= 4;
            k = 0;
        } else {
            ++k;
        }
        b[z + k] = c;
    }
}
