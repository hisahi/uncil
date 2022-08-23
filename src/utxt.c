/*******************************************************************************
 
Uncil -- character/text conversion impl

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

#include "ucommon.h"
#include "utxt.h"
#include "uutf.h"
#include "uvali.h"

void unc__initenctable(Unc_Allocator *alloc, Unc_EncodingTable *table) {
    table->entries = 0;
    table->data = NULL;
    unc__inithtbls(alloc, &table->names);
}

int unc__cconv_passthru(Unc_CConv_In in, void *in_data,
                        Unc_CConv_Out out, void *out_data) {
    byte buf[16];
    int e, c;
    size_t n = 0;
    while ((c = (*in)(in_data)) >= 0) {
        buf[n++] = c;
        if (n == sizeof(buf)) {
            e = (*out)(out_data, n, buf);
            if (e) return e;
            n = 0;
        }
    }
    return n ? (*out)(out_data, n, buf) : 0;
}

INLINE int unc__addencoding(Unc_View *w, Unc_EncodingTable *table, size_t ctr,
                            Unc_Size enc_n, const byte *enc) {
    Unc_Value *out;
    int e = unc__puthtbls(w, &table->names, enc_n, enc, &out);
    if (!e) VINITINT(out, ctr);
    return 0;
}

void unc__dropenctable(Unc_View *w, Unc_EncodingTable *table) {
    unc__drophtbls(w, &table->names);
    TMFREE(Unc_EncodingEntry, &w->world->alloc, table->data, table->entries);
}

int unc__resolveencindex(Unc_View *w, Unc_Size name_n, const byte *name) {
    Unc_Value *v = unc__gethtbls(w, &w->world->encs.names, name_n, name);
    if (!v) return -1;
    return (int)v->v.i;
}

Unc_EncodingEntry *unc__getbyencindex(struct Unc_View *w, int index) {
    return &w->world->encs.data[index];
}

int unc__cconv_utf16le_dec(Unc_CConv_In in, void *in_data,
                           Unc_CConv_Out out, void *out_data,
                           Unc_Size n) {
    Unc_UChar u = 0;
    int surrogate = 0;
    while (surrogate || n--) {
        int c;
        Unc_Size i;
        for (i = 0; i < 2; ++i) {
            c = (*in)(in_data);
            if (c < 0) {
                if (!i && !surrogate) return 0;
                else return -1;
            }
            u |= c << (i * 8);
        }
        if (surrogate) {
            Unc_UChar ulow = u & 0xFFFFUL;
            if (ulow < 0xDC00UL || ulow > 0xDFFFUL)
                return -1;
            u = (((u >> 6) & 0xFFC00UL) | (ulow & 0x3FFUL)) + 0x10000UL;
            surrogate = 0;
        } else if (0xD800UL <= u && u < 0xDC00UL) {
            u = (u & 0x3FFUL) << 16;
            surrogate = 1;
            continue;
        }
        {
            byte buf[UNC_UTF8_MAX_SIZE];
            Unc_Size n = unc__utf8enc(u, sizeof(buf), buf);
            if ((c = (*out)(out_data, n, buf)))
                return c < 0 ? c : 0;
            u = 0;
        }
    }
    return 0;
}

int unc__cconv_utf16be_dec(Unc_CConv_In in, void *in_data,
                           Unc_CConv_Out out, void *out_data,
                           Unc_Size n) {
    Unc_UChar u = 0;
    int surrogate = 0;
    while (surrogate || n--) {
        int c;
        Unc_Size i;
        for (i = 0; i < 2; ++i) {
            c = (*in)(in_data);
            if (c < 0) {
                if (!i && !surrogate) return 0;
                else return -1;
            }
            u = (u << 8) | c;
        }
        if (surrogate) {
            Unc_UChar ulow = u & 0xFFFFUL;
            if (ulow < 0xDC00UL || ulow > 0xDFFFUL)
                return -1;
            ulow &= 0x3FFUL;
            u = (((u >> 6) & 0xFFC00UL) | ulow) + 0x10000UL;
            surrogate = 0;
        } else if (0xD800UL <= u && u < 0xDC00UL) {
            u &= 0x3FFUL;
            surrogate = 1;
            continue;
        }
        {
            byte buf[UNC_UTF8_MAX_SIZE];
            Unc_Size n = unc__utf8enc(u, sizeof(buf), buf);
            if ((c = (*out)(out_data, n, buf)))
                return c < 0 ? c : 0;
            u = 0;
        }
    }
    return 0;
}

int unc__cconv_utf32le_dec(Unc_CConv_In in, void *in_data,
                           Unc_CConv_Out out, void *out_data,
                           Unc_Size n) {
    while (n--) {
        int c;
        Unc_UChar u = 0;
        Unc_Size i, n;
        byte buf[UNC_UTF8_MAX_SIZE];
        for (i = 0; i < 4; ++i) {
            c = (*in)(in_data);
            if (c < 0) {
                if (!i) return 0;
                else return -1;
            }
            u |= c << (i * 8);
        }
        if (u >= UNC_UTF8_MAX_CHAR)
            return -1;
        n = unc__utf8enc(u, sizeof(buf), buf);
        if ((c = (*out)(out_data, n, buf)))
            return c < 0 ? c : 0;
    }
    return 0;
}

int unc__cconv_utf32be_dec(Unc_CConv_In in, void *in_data,
                           Unc_CConv_Out out, void *out_data,
                           Unc_Size n) {
    while (n--) {
        int c;
        Unc_UChar u = 0;
        Unc_Size i, n;
        byte buf[UNC_UTF8_MAX_SIZE];
        for (i = 0; i < 4; ++i) {
            c = (*in)(in_data);
            if (c < 0) {
                if (!i) return 0;
                else return -1;
            }
            u = (u << 8) | c;
        }
        if (u >= UNC_UTF8_MAX_CHAR)
            return -1;
        n = unc__utf8enc(u, sizeof(buf), buf);
        if ((c = (*out)(out_data, n, buf)))
            return c < 0 ? c : 0;
    }
    return 0;
}

/* -1 = error, 0 = OK, 1 = EOF */
static int unc__cconv_utf8read(Unc_CConv_In in, void *in_data,
                               Unc_UChar *u) {
    byte buf[UNC_UTF8_MAX_SIZE];
    int i, j, c;
    c = (*in)(in_data);
    if (c < 0) return 1;
    if (!(c & 0x80)) {
        j = 1;
    } else if ((c & 0xE0) == 0xC0) {
        j = 2;
    } else if ((c & 0xF0) == 0xE0) {
        j = 3;
    } else if ((c & 0xF8) == 0xF0) {
        j = 4;
    } else {
        return -1;
    }
    buf[0] = c;
    for (i = 1; i < j; ++i) {
        c = (*in)(in_data);
        if (c < 0) return 1;
        if ((c & 0xC0) != 0x80) return -1;
        buf[i] = c;
    }
    *u = unc__utf8decd(buf);
    return 0;
}

INLINE void enc_u16le(byte *buf, Unc_UChar u) {
    buf[0] = u & 0xFF;
    buf[1] = (u >> 8) & 0xFF;
}

INLINE void enc_u16be(byte *buf, Unc_UChar u) {
    buf[0] = (u >> 8) & 0xFF;
    buf[1] = u & 0xFF;
}

INLINE void enc_u32le(byte *buf, Unc_UChar u) {
    buf[0] = u & 0xFF;
    buf[1] = (u >> 8) & 0xFF;
    buf[2] = (u >> 16) & 0xFF;
    buf[3] = (u >> 24) & 0xFF;
}

INLINE void enc_u32be(byte *buf, Unc_UChar u) {
    buf[0] = (u >> 24) & 0xFF;
    buf[1] = (u >> 16) & 0xFF;
    buf[2] = (u >> 8) & 0xFF;
    buf[3] = u & 0xFF;
}

int unc__cconv_utf16le_enc(Unc_CConv_In in, void *in_data,
                           Unc_CConv_Out out, void *out_data) {
    int e;
    Unc_UChar u;
    byte buf[4];
    for (;;) {
        e = unc__cconv_utf8read(in, in_data, &u);
        if (e) return e < 0 ? e : 0;
        if (0xD800UL <= u && u <= 0xDFFFUL) return -1;
        if (u >= 0x10000UL) {
            u -= 0x10000UL;
            enc_u16le(buf, 0xD800UL + ((u >> 10) & 0x3FFUL));
            enc_u16le(buf + 2, 0xDC00UL + (u & 0x3FFUL));
            e = (*out)(out_data, 4, buf);
            if (e) return e < 0 ? e : 0;
        } else {
            enc_u16le(buf, u);
            e = (*out)(out_data, 2, buf);
            if (e) return e < 0 ? e : 0;
        }
    }
}

int unc__cconv_utf16be_enc(Unc_CConv_In in, void *in_data,
                           Unc_CConv_Out out, void *out_data) {
    int e;
    Unc_UChar u;
    byte buf[4];
    for (;;) {
        e = unc__cconv_utf8read(in, in_data, &u);
        if (e) return e < 0 ? e : 0;
        if (0xD800UL <= u && u <= 0xDFFFUL) return -1;
        if (u >= 0x10000UL) {
            u -= 0x10000UL;
            enc_u16be(buf, 0xD800UL + ((u >> 10) & 0x3FFUL));
            enc_u16be(buf + 2, 0xDC00UL + (u & 0x3FFUL));
            e = (*out)(out_data, 4, buf);
            if (e) return e < 0 ? e : 0;
        } else {
            enc_u16be(buf, u);
            e = (*out)(out_data, 2, buf);
            if (e) return e < 0 ? e : 0;
        }
    }
}

int unc__cconv_utf32le_enc(Unc_CConv_In in, void *in_data,
                           Unc_CConv_Out out, void *out_data) {
    int e;
    Unc_UChar u;
    byte buf[4];
    for (;;) {
        e = unc__cconv_utf8read(in, in_data, &u);
        if (e) return e < 0 ? e : 0;
        if (0xD800UL <= u && u <= 0xDFFFUL) return -1;
        enc_u32le(buf, u);
        e = (*out)(out_data, 4, buf);
        if (e) return e < 0 ? e : 0;
    }
}

int unc__cconv_utf32be_enc(Unc_CConv_In in, void *in_data,
                           Unc_CConv_Out out, void *out_data) {
    int e;
    Unc_UChar u;
    byte buf[4];
    for (;;) {
        e = unc__cconv_utf8read(in, in_data, &u);
        if (e) return e < 0 ? e : 0;
        if (0xD800UL <= u && u <= 0xDFFFUL) return -1;
        enc_u32be(buf, u);
        e = (*out)(out_data, 4, buf);
        if (e) return e < 0 ? e : 0;
    }
}

int unc__cconv_ascii_dec(Unc_CConv_In in, void *in_data,
                         Unc_CConv_Out out, void *out_data,
                         Unc_Size n) {
    byte buf;
    while (n--) {
        int c = (*in)(in_data);
        if (c < 0) return 0;
        if (c >= 0x80) return -1;
        buf = c;
        if ((c = (*out)(out_data, 1, &buf)))
            return c < 0 ? c : 0;
    }
    return 0;
}

int unc__cconv_latin1_dec(Unc_CConv_In in, void *in_data,
                          Unc_CConv_Out out, void *out_data,
                          Unc_Size n) {
    byte buf;
    while (n--) {
        int c = (*in)(in_data);
        if (c < 0) return 0;
        buf = c;
        if ((c = (*out)(out_data, 1, &buf)))
            return c < 0 ? c : 0;
    }
    return 0;
}

int unc__cconv_ascii_enc(Unc_CConv_In in, void *in_data,
                         Unc_CConv_Out out, void *out_data) {
    int e;
    Unc_UChar u;
    byte buf;
    for (;;) {
        e = unc__cconv_utf8read(in, in_data, &u);
        if (e) return e < 0 ? e : 0;
        if (u >= 0x80UL) return -1;
        buf = u;
        e = (*out)(out_data, 1, &buf);
        if (e) return e < 0 ? e : 0;
    }
}

int unc__cconv_latin1_enc(Unc_CConv_In in, void *in_data,
                         Unc_CConv_Out out, void *out_data) {
    int e;
    Unc_UChar u;
    byte buf;
    for (;;) {
        e = unc__cconv_utf8read(in, in_data, &u);
        if (e) return e < 0 ? e : 0;
        if (u >= 0x100UL) return -1;
        buf = u;
        e = (*out)(out_data, 1, &buf);
        if (e) return e < 0 ? e : 0;
    }
}

int unc__adddefaultencs(Unc_View *w, Unc_EncodingTable *t) {
    int e = 0;
    size_t ctr = 0;
    if (t->entries < 16) {
        Unc_EncodingEntry *d = TMREALLOC(Unc_EncodingEntry, &w->world->alloc, 0,
                                         t->data, t->entries, 16);
        if (!d) return UNCIL_ERR_MEM;
        t->data = d;
        t->entries = 16;
    }

    t->data[ctr].enc = &unc__cconv_passthru;
    t->data[ctr].dec = &unc__cconv_utf8ts;
    if ((e = unc__addencoding(w, t, ctr++, PASSSTRL("utf8")))) return e;

    t->data[ctr].enc = &unc__cconv_utf16le_enc;
    t->data[ctr].dec = &unc__cconv_utf16le_dec;
    if ((e = unc__addencoding(w, t, ctr++, PASSSTRL("utf16le")))) return e;

    t->data[ctr].enc = &unc__cconv_utf16be_enc;
    t->data[ctr].dec = &unc__cconv_utf16be_dec;
    if ((e = unc__addencoding(w, t, ctr++, PASSSTRL("utf16be")))) return e;

    t->data[ctr].enc = &unc__cconv_utf32le_enc;
    t->data[ctr].dec = &unc__cconv_utf32le_dec;
    if ((e = unc__addencoding(w, t, ctr++, PASSSTRL("utf32le")))) return e;

    t->data[ctr].enc = &unc__cconv_utf32be_enc;
    t->data[ctr].dec = &unc__cconv_utf32be_dec;
    if ((e = unc__addencoding(w, t, ctr++, PASSSTRL("utf32be")))) return e;

    t->data[ctr].enc = &unc__cconv_ascii_enc;
    t->data[ctr].dec = &unc__cconv_ascii_dec;
    if ((e = unc__addencoding(w, t, ctr++, PASSSTRL("ascii")))) return e;

    t->data[ctr].enc = &unc__cconv_latin1_enc;
    t->data[ctr].dec = &unc__cconv_latin1_dec;
    if ((e = unc__addencoding(w, t, ctr++, PASSSTRL("latin1")))) return e;

    return e;
}
