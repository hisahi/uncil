/*******************************************************************************
 
Uncil -- builtin JSON library impl

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

#include <errno.h>
#include <math.h>
#include <stdarg.h>

#define UNCIL_DEFINES

#include "uctype.h"
#include "ulibio.h"
#include "uncil.h"
#include "uutf.h"
#include "uxprintf.h"

struct json_decode_string {
    Unc_Size n;
    const char *c;
};

/* -1 = eof, -2 = error, etc. */
static int json_decode_string_do(void *p) {
    struct json_decode_string *s = p;
    if (!s->n) return -1;
    --s->n;
    return *s->c++;
}
/*
static int json_decode_file_do(void *p) {
    struct ulib_io_file *filep = p;
    int c = unc__io_fgetc(filep);
    return c == EOF ? (unc__io_ferror(filep) ? -2 : -1) : c;
}*/

struct json_decode_file {
    struct ulib_io_file *fp;
    int i, n;
    char buffer[UNC_IO_GETC_BUFFER];
    int err;
    Unc_View *view;
};

static int json_decode_file_do(void *p_) {
    struct json_decode_file *p = p_;
    if (p->err) return p->err;
    while (p->i == p->n) {
        int c = unc__io_fgetc_text(p->view, p->fp, p->buffer);
        if (c < 0) {
            if (c == -2) {
                p->err = c;
                return c;
            }
            p->err = -1;
            return unc__io_ferror(p->fp) ? -2 : -1;
        }
        p->i = 0, p->n = c;
    }
    return p->buffer[p->i++];
}

struct json_decode_context {
    int next;
    int (*getch)(void *);
    void *getch_data;
    Unc_Size recurse;
    Unc_View *view;
};

INLINE int jsonnext(struct json_decode_context *c) {
    return (*c->getch)(c->getch_data);
}

INLINE int jsondec_err(struct json_decode_context *c,
                       const char *type, const char *msg) {
    return unc_throwexc(c->view, type, msg);
}

static int jsondec_skipw(struct json_decode_context *c) {
    int x = c->next;
    while (x >= 0 && unc__isspace(x))
        x = jsonnext(c);
    return x;
}

static int jsondec_rest(struct json_decode_context *c, const char *s) {
    while (*s) {
        int x = jsonnext(c);
        if (x != *s++) return UNCIL_ERR_SYNTAX;
    }
    return 0;
}

static int jsondec_num(struct json_decode_context *c, Unc_Value *v) {
    int x = c->next;
    Unc_UInt u, up;
    Unc_Float f;
    int neg = 0, floated = 0;
    if (x == '-') {
        neg = 1;
        x = jsonnext(c);
    }
    if (!unc__isdigit(x))
        return jsondec_err(c, "syntax",
            "JSON syntax error: - not followed by digit");
    u = 0;
    do {
        if (floated) {
            f = (f * 10) + (x - '0');
        } else {
            up = u;
            u = u * 10 + (x - '0');
            if (up > u) {
                f = ((Unc_Float)u * 10) + (x - '0');
                floated = 1;
            }
        }
        x = jsonnext(c);
    } while (unc__isdigit(x));

    if (x == '.') {
        Unc_Float fp = 0;
        Unc_Size pow10 = 0, nonzeroes = 0;
        if (!floated) {
            floated = 1;
            f = (Unc_Float)u;
        }
        x = jsonnext(c);
        if (!unc__isdigit(x))
            return jsondec_err(c, "syntax",
                "JSON syntax error: . not followed by digit");
        do {
            if (fp && ++nonzeroes >= DBL_DIG + 2) {
                do {
                    x = jsonnext(c);
                } while (unc__isdigit(x));
                break;
            }
            fp = (fp * 10) + (x - '0');
            x = jsonnext(c);
        } while (unc__isdigit(x));
        f += fp / pow(10, pow10);
    }

    if (x == 'e' || x == 'E') {
        int eneg = 0;
        if (!floated) {
            floated = 1;
            f = (Unc_Float)u;
        }
        x = jsonnext(c);
        if (x == '-') {
            eneg = 1;
            x = jsonnext(c);
        } else if (x == '+') {
            x = jsonnext(c);
        }
        if (!unc__isdigit(x))
            return jsondec_err(c, "syntax",
                "JSON syntax error: e/E not followed by digit");
        up = u = 0;
        do {
            up = u;
            u = u * 10 + (x - '0');
            if (up > u) {
                u = UNC_UINT_MAX;
                break;
            }
            x = jsonnext(c);
        } while (unc__isdigit(x));

        if (!eneg) {
            if (u > DBL_MAX_10_EXP * 2)
                u = DBL_MAX_10_EXP * 2;
            x = x * pow(10, (int)u);
        } else {
            if (u > -DBL_MIN_10_EXP * 2)
                u = -DBL_MIN_10_EXP * 2;
            x = x * pow(10, -(int)u);
        }
    }

    if (neg) {
        if (floated) {
            f = -f;
        } else {
            if (u > -(Unc_UInt)UNC_INT_MIN)
                f = -(Unc_Float)u, floated = 1;
            else
                unc_setint(c->view, v, -(Unc_Int)u);
        }
    } else if (!floated)
        unc_setint(c->view, v, (Unc_Int)u);
    if (floated)
        unc_setfloat(c->view, v, f);
    c->next = x;
    return 0;
}

static int jsondec_str(struct json_decode_context *c, Unc_Value *v) {
    int e, x = c->next;
    char buf_[64], *buf = buf_;
    byte uc[UNC_UTF8_MAX_SIZE];
    int buf_ext = 0;
    Unc_Size buf_n = 0;
    Unc_Size buf_c = sizeof(buf_);
    Unc_UChar unext = 0;
    Unc_Size us;
    ASSERT(x == '"');
    x = jsonnext(c);
    for (;;) {
        Unc_UChar u;
        if (x < 0) return jsondec_err(c, "syntax", 
            "JSON syntax error: unterminated string");
        if (x == '"') {
            x = jsonnext(c);
            break;
        } else if (x == '\\') {
            x = jsonnext(c);
            switch (x) {
            case '"':
                u = '"';
                break;
            case '\\':
                u = '\\';
                break;
            case '/':
                u = '/';
                break;
            case 'b':
                u = '\b';
                break;
            case 'f':
                u = '\f';
                break;
            case 'n':
                u = '\n';
                break;
            case 'r':
                u = '\r';
                break;
            case 't':
                u = '\t';
                break;
            case 'u':
            {
                int i;
                u = 0;
                for (i = 0; i < 4; ++i) {
                    x = jsonnext(c);
                    if (unc__isdigit(x)) {
                        u = (u << 4) | (x - '0');
                    } else if ('A' <= x && x <= 'F') {
                        u = (u << 4) | (x - 'A' + 10);
                    } else if ('a' <= x && x <= 'f') {
                        u = (u << 4) | (x - 'a' + 10);
                    } else if (x < 0)
                        return jsondec_err(c, "syntax", 
                            "JSON syntax error: truncated escape sequence");
                    else
                        return jsondec_err(c, "syntax", 
                            "JSON syntax error: invalid escape sequence");
                }
                break;
            }
            default:
                if (x < 0)
                    return jsondec_err(c, "syntax", 
                        "JSON syntax error: truncated escape sequence");
                return jsondec_err(c, "syntax", 
                    "JSON syntax error: invalid escape sequence");
            }
        } else
            u = x;
        if (unext) {
            if (!(0xdc00 <= u && u < 0xe000))
                return jsondec_err(c, "syntax", 
                    "JSON syntax error: misformatted UTF-16 surrogate pair");
            u = 0x10000 + (((unext & 0x3FF) << 10) | (u & 0x3FF));
            unext = 0;
        } else if (0xd800 <= u && u < 0xdc00) {
            unext = u;
            x = jsonnext(c);
            continue;
        }
        if (u >= 0x80) {
            us = unc__utf8enc(u, sizeof(uc), uc);
        } else {
            us = 1;
        }
        if (buf_n + us > buf_c) {
            Unc_Size z = buf_c + 64;
            char *p;
            if (buf_ext)
                p = unc_mrealloc(c->view, buf, z);
            else {
                p = unc_malloc(c->view, z);
                if (p) unc__memcpy(p, buf, buf_c);
            }
            if (!p) return UNCIL_ERR_MEM;
            buf = p;
            buf_c = z;
            buf_ext = 1;
        }
        if (u >= 0x80) {
            unc__memcpy(&buf[buf_n], uc, us);
            buf_n += us;
        } else {
            buf[buf_n++] = u;
        }
        x = jsonnext(c);
    }
    if (unext) return jsondec_err(c, "syntax", 
                    "JSON syntax error: truncated UTF-16 surrogate pair");
    if (buf_ext) {
        e = unc_newstringmove(c->view, v, buf_n, buf);
        if (e) {
            unc_mfree(c->view, buf);
            return e;
        }
    } else {
        e = unc_newstring(c->view, v, buf_n, buf);
        if (e) return e;
    }
    c->next = x;
    return 0;
}

static int jsondec_val(struct json_decode_context *c, Unc_Value *v);

static int jsondec_arr(struct json_decode_context *c, Unc_Value *v) {
    int e, x = c->next;
    Unc_Value arr = UNC_BLANK, tmp = UNC_BLANK;
    Unc_Size arr_n = 0, arr_c;
    Unc_Value *arr_v;
    if (!c->recurse)
        return UNCIL_ERR_TOODEEP;
    --c->recurse;
    arr_c = 0;
    e = unc_newarray(c->view, &arr, arr_c, &arr_v);
    if (e) return e;
    
    ASSERT(x == '[');
    c->next = jsonnext(c);

    for (;;) {
        x = jsondec_skipw(c);
        if (x < 0) {
            unc_clear(c->view, &tmp);
            unc_clear(c->view, &arr);
            return jsondec_err(c, "syntax", 
                    "JSON syntax error: array not terminated");
        }
        if (x == ']') {
            unc_clear(c->view, &tmp);
            x = jsonnext(c);
            break;
        }
        c->next = x;
        e = jsondec_val(c, &tmp);
        x = c->next;
        if (e) {
            unc_clear(c->view, &tmp);
            unc_clear(c->view, &arr);
            return e;
        }
        if (arr_n == arr_c) {
            Unc_Size nz = arr_c + 8;
            e = unc_resizearray(c->view, &arr, nz, &arr_v);
            if (e) {
                unc_clear(c->view, &tmp);
                unc_clear(c->view, &arr);
                return e;
            }
            arr_c = nz;
        }
        unc_copy(c->view, &arr_v[arr_n++], &tmp);
        c->next = x;
        x = jsondec_skipw(c);
        if (x == ',') {
            c->next = jsonnext(c);
        } else if (x == ']') {
            x = jsonnext(c);
            break;
        } else {
            return jsondec_err(c, "syntax", 
                    "JSON syntax error: expected comma in array");
        }
    }

    unc_resizearray(c->view, &arr, arr_n, &arr_v);
    unc_unlock(c->view, &arr);
    unc_move(c->view, v, &arr);
    c->next = x;
    ++c->recurse;
    return 0;
}

static int jsondec_obj(struct json_decode_context *c, Unc_Value *v) {
    int e, x = c->next;
    Unc_Value obj = UNC_BLANK, key = UNC_BLANK, val = UNC_BLANK;
    if (!c->recurse)
        return UNCIL_ERR_TOODEEP;
    --c->recurse;
    e = unc_newtable(c->view, &obj);
    if (e) return e;
    
    ASSERT(x == '{');
    c->next = jsonnext(c);

    for (;;) {
        x = jsondec_skipw(c);
        if (x < 0) {
            unc_clear(c->view, &val);
            unc_clear(c->view, &key);
            unc_clear(c->view, &obj);
            return jsondec_err(c, "syntax", 
                    "JSON syntax error: list not terminated");
        }
        if (x == '}') {
            unc_clear(c->view, &val);
            unc_clear(c->view, &key);
            x = jsonnext(c);
            break;
        }
        c->next = x;
        e = jsondec_str(c, &key);
        x = c->next;
        if (e) {
            unc_clear(c->view, &val);
            unc_clear(c->view, &key);
            unc_clear(c->view, &obj);
            return e;
        }
        if (x == ':') {
            x = jsonnext(c);
        } else {
            return jsondec_err(c, "syntax", 
                    "JSON syntax error: expected colon in object");
        }
        c->next = x;
        c->next = jsondec_skipw(c);
        e = jsondec_val(c, &val);
        x = c->next;
        if (e) {
            unc_clear(c->view, &val);
            unc_clear(c->view, &key);
            unc_clear(c->view, &obj);
            return e;
        }
        e = unc_setattrv(c->view, &obj, &key, &val);
        if (e) {
            unc_clear(c->view, &val);
            unc_clear(c->view, &key);
            unc_clear(c->view, &obj);
            return e;
        }
        c->next = x;
        x = jsondec_skipw(c);
        if (x == ',') {
            c->next = jsonnext(c);
        } else if (x == '}') {
            x = jsonnext(c);
            break;
        } else {
            return jsondec_err(c, "syntax", 
                    "JSON syntax error: expected comma in object");
        }
    }

    unc_move(c->view, v, &obj);
    c->next = x;
    ++c->recurse;
    return 0;
}

static int jsondec_val(struct json_decode_context *c, Unc_Value *v) {
    int x = jsondec_skipw(c);
    switch (x) {
    case '-':
    case '.':
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        return jsondec_num(c, v);
    case '"':
        return jsondec_str(c, v);
    case '[':
        return jsondec_arr(c, v);
    case '{':
        return jsondec_obj(c, v);
    case 'f':
    {
        int e;
        if ((e = jsondec_rest(c, "false")))
            return e;
        VSETBOOL(c->view, v, 0);
        return 0;
    }
    case 'n':
    {
        int e;
        if ((e = jsondec_rest(c, "null")))
            return e;
        VSETNULL(c->view, v);
        return 0;
    }
    case 't':
    {
        int e;
        if ((e = jsondec_rest(c, "true")))
            return e;
        VSETBOOL(c->view, v, 1);
        return 0;
    }
    }
    return jsondec_err(c, "syntax", "JSON syntax error: invalid initial");
}

static int jsondec(struct json_decode_context *c, Unc_Value *v) {
    c->next = (*c->getch)(c->getch_data);
    return jsondec_val(c, v);
}

Unc_RetVal unc__lib_json_decode(Unc_View *w, Unc_Tuple args, void *ud_) {
    int e;
    Unc_Value v = UNC_BLANK;
    struct json_decode_context cxt;
    struct json_decode_string rs;
    (void)ud_;

    e = unc_getstring(w, &args.values[0], &rs.n, &rs.c);
    if (e) return e;

    cxt.view = w;
    cxt.getch = &json_decode_string_do;
    cxt.getch_data = &rs;
    cxt.recurse = unc_recurselimit(w);
    
    e = jsondec(&cxt, &v);
    if (e) return e;
    return unc_push(w, 1, &v, NULL);
}

Unc_RetVal unc__lib_json_decodefile(Unc_View *w, Unc_Tuple args, void *ud_) {
    int e;
    Unc_Value v = UNC_BLANK;
    struct json_decode_context cxt;
    struct json_decode_file buf;
    struct ulib_io_file *pfile;
    (void)ud_;

    e = unc__io_lockfile(w, &args.values[0], &pfile, 0);
    if (e) return e;

    buf.fp = pfile;
    buf.i = buf.n = 0;
    buf.err = 0;
    buf.view = w;

    cxt.view = w;
    cxt.getch = &json_decode_file_do;
    cxt.getch_data = &buf;
    cxt.recurse = unc_recurselimit(w);
    
    e = jsondec(&cxt, &v);
    if (buf.err == -2)
        e = unc_throwexc(w, "io", "json.decodefile(): could not decode text");
    else if (unc__io_ferror(pfile))
        e = unc__io_makeerr(w, "json.decodefile()", errno);
    unc__io_unlockfile(w, &args.values[0]);
    if (e) return e;
    return unc_push(w, 1, &v, NULL);
}

struct json_encode_string {
    Unc_Allocator *alloc;
    byte *s;
    Unc_Size n, c;
    int err;
};

/* 0 = OK, <>0 = error */
static int json_encode_string_do(Unc_Size n, const char *c, void *p) {
    struct json_encode_string *s = p;
    return (s->err = unc__strpush(s->alloc, &s->s, &s->n, &s->c,
                                  6, n, (const byte *)c));
}

struct json_encode_file {
    struct ulib_io_file *fp;
    int err;
    Unc_View *view;
};

static int json_encode_file_do(Unc_Size n, const char *c, void *p_) {
    struct json_encode_file *p = p_;
    return (p->err = unc__io_fwrite_text(p->view, p->fp, (const byte *)c, n));
}

struct json_encode_context {
    int (*out)(Unc_Size, const char *, void *);
    void *out_data;
    Unc_Size recurse;
    Unc_View *view;
    int fancy;
    int depth;
    Unc_Int indent;
    Unc_Value mapper;
    void **ents;
    Unc_Size ents_n, ents_c;
};

static int jsonenc_printf_wrap(char o, void *udata) {
    struct json_encode_context *s = udata;
    return (*s->out)(1, &o, s->out_data);
}

static int jsonenc_printf(struct json_encode_context *c,
                          const char *format, ...) {
    int r;
    va_list va;
    va_start(va, format);
    r = unc__vxprintf(&jsonenc_printf_wrap, c, format, va);
    va_end(va);
    return r < 0 ? UNCIL_ERR_INTERNAL : 0;
}

INLINE int jsonenc_err(struct json_encode_context *c,
                       const char *type, const char *msg) {
    return unc_throwexc(c->view, type, msg);
}

static const char spaces[65] = "                                "
                               "                                ";

static const char tabs[65] = "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t"
                             "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t"
                             "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t"
                             "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t";

static int jsonenc_indent(struct json_encode_context *c, int depth) {
    if (depth && c->fancy && c->indent) {
        int e;
        if (c->indent < 0) {
            static const Unc_Size block = sizeof(tabs) - 1;
            Unc_Size count = depth * -c->indent;
            while (count >= block) {
                if ((e = (*c->out)(block, tabs, c->out_data)))
                    return e;
                count -= block;
            }
            if (count && (e = (*c->out)(count, tabs, c->out_data)))
                return e;
        } else {
            static const Unc_Size block = sizeof(spaces) - 1;
            Unc_Size count = depth * c->indent;
            while (count >= block) {
                if ((e = (*c->out)(block, spaces, c->out_data)))
                    return e;
                count -= block;
            }
            if (count && (e = (*c->out)(count, spaces, c->out_data)))
                return e;
        }
    }
    return 0;
}

static int jsonenc_signent(struct json_encode_context *c, void *p) {
    Unc_Size i = c->ents_n;
    if (i) {
        --i;
        do {
            if (c->ents[i] == p)
                return unc_throwexc(c->view, "value", "loop detected");
        } while (i--);
    }
    if (c->ents_n == c->ents_c) {
        Unc_Size entsz = c->ents_c + 16;
        void **ents2 = unc_mrealloc(c->view, c->ents, entsz * sizeof(void *));
        if (!ents2) return UNCIL_ERR_MEM;
        c->ents = ents2;
        c->ents_c = entsz;
    }
    c->ents[c->ents_n++] = p;
    return 0;
}

static const char hex[] = "0123456789abcdef";

INLINE void jsonenc_hex4(char *buf, unsigned int u) {
    int i;
    for (i = 0; i < 4; ++i)
        buf[i] = hex[(u >> (4 * (3 - i))) & 15];
}

static int jsonenc_str_i(struct json_encode_context *c,
                         Unc_Size n, const char *r) {
    int e;
    Unc_UChar u;

    e = (*c->out)(1, "\"", c->out_data);
    if (e) return e;

    while (n) {
        u = unc__utf8decx(&n, (const byte **)&r);
        if (u < 0x20 || u >= 0x80) {
            switch (u) {
            case '\0':
                e = (*c->out)(2, "\\0", c->out_data);
                break;
            case '\b':
                e = (*c->out)(2, "\\b", c->out_data);
                break;
            case '\f':
                e = (*c->out)(2, "\\f", c->out_data);
                break;
            case '\n':
                e = (*c->out)(2, "\\n", c->out_data);
                break;
            case '\r':
                e = (*c->out)(2, "\\r", c->out_data);
                break;
            case '\t':
                e = (*c->out)(2, "\\t", c->out_data);
                break;
            default:
            {
                char esc[] = "\\u0000";
                if (u >= 0x10000) {
                    unsigned uhi, ulo;
                    u -= 0x10000;
                    uhi = 0xd800 | ((u >> 10) & 0x3ff);
                    ulo = 0xdc00 | (u & 0x3ff);
                    jsonenc_hex4(esc + 2, uhi);
                    e = (*c->out)(6, esc, c->out_data);
                    if (e) return e;
                    jsonenc_hex4(esc + 2, ulo);
                    e = (*c->out)(6, esc, c->out_data);
                } else {
                    jsonenc_hex4(esc + 2, u);
                    e = (*c->out)(6, esc, c->out_data);
                }
            }
            }
        } else if (u == '\\' || u == '\"') {
            char buf[2] = "\\";
            buf[1] = u;
            e = (*c->out)(2, buf, c->out_data);
        } else {
            char ch = u;
            e = (*c->out)(1, &ch, c->out_data);
        }
        if (e) return e;
    }
    return (*c->out)(1, "\"", c->out_data);
}

INLINE int jsonenc_str(struct json_encode_context *c, Unc_Value *v) {
    int e;
    Unc_Size n;
    const char *r;
    e = unc_getstring(c->view, v, &n, &r);
    if (e) return e;
    return jsonenc_str_i(c, n, r);
}

static int jsonenc(struct json_encode_context *c, Unc_Value *v);

static int jsonenc_arr(struct json_encode_context *c, Unc_Value *v) {
    Unc_Size sn;
    Unc_Value *ss;
    int e, depth;
    if ((e = jsonenc_signent(c, VGETENT(v))))
        return e;
    if (!c->recurse)
        return UNCIL_ERR_TOODEEP;
    --c->recurse;
    e = unc_lockarray(c->view, v, &sn, &ss);
    if (e) return e;
    if ((e = (*c->out)(1, "[", c->out_data))) goto jsonenc_arr_done;
    if (sn) {
        Unc_Size i;
        depth = ++c->depth;
        for (i = 0; i < sn; ++i) {
            if (c->fancy) {
                if ((e = (*c->out)(1, "\n", c->out_data)))
                    goto jsonenc_arr_done;
                if ((e = jsonenc_indent(c, depth)))
                    goto jsonenc_arr_done;
            }
            if ((e = jsonenc(c, &ss[i]))) goto jsonenc_arr_done;
            if (i < sn - 1) {
                if ((e = (*c->out)(1, ",", c->out_data))) goto jsonenc_arr_done;
            }
        }
        c->depth = --depth;
        if (c->fancy) {
            if ((e = (*c->out)(1, "\n", c->out_data))) goto jsonenc_arr_done;
            if ((e = jsonenc_indent(c, depth))) goto jsonenc_arr_done;
        }
    }
    e = (*c->out)(1, "]", c->out_data);
    --c->ents_n;
    ++c->recurse;
jsonenc_arr_done:
    unc_unlock(c->view, v);
    return e;
}

static int jsonenc_obj(struct json_encode_context *c, Unc_Value *v) {
    int e, depth;
    Unc_Value iter = UNC_BLANK;
    if ((e = jsonenc_signent(c, VGETENT(v))))
        return e;
    if (!c->recurse)
        return UNCIL_ERR_TOODEEP;
    --c->recurse;
    e = unc_getiterator(c->view, v, &iter);
    if (e) return e;
    if ((e = (*c->out)(1, "{", c->out_data))) goto jsonenc_obj_done;
    if (unc_converttobool(c->view, v) == 1) {
        int comma = 0;
        depth = ++c->depth;
        for (;;) {
            Unc_Pile pile;
            Unc_Tuple tuple;
            e = unc_call(c->view, &iter, 0, &pile);
            if (e) goto jsonenc_obj_done;
            unc_returnvalues(c->view, &pile, &tuple);
            if (!tuple.count) {
                unc_discard(c->view, &pile);
                break;
            }
            if (tuple.count != 2) {
                e = UNCIL_ERR_INTERNAL;
                unc_discard(c->view, &pile);
                goto jsonenc_obj_done;
            }
            if (comma && (e = (*c->out)(1, ",", c->out_data)))
                goto jsonenc_obj_done;
            comma = 1;
            if (c->fancy) {
                if ((e = (*c->out)(1, "\n", c->out_data)))
                    goto jsonenc_obj_done;
                if ((e = jsonenc_indent(c, depth)))
                    goto jsonenc_obj_done;
            }
            if (unc_gettype(c->view, &tuple.values[0]) == Unc_TString) {
                if ((e = jsonenc_str(c, &tuple.values[0])))
                    goto jsonenc_obj_done;
            } else {
                Unc_Size keyn;
                char *keyc;
                e = unc_valuetostringn(c->view, &tuple.values[0], &keyn, &keyc);
                if (e) goto jsonenc_obj_done;
                e = jsonenc_str_i(c, keyn, keyc);
                unc_mfree(c->view, keyc);
                if (e)
                    goto jsonenc_obj_done;
            }
            if (c->fancy) {
                if ((e = (*c->out)(2, ": ", c->out_data)))
                    goto jsonenc_obj_done;
            } else {
                if ((e = (*c->out)(1, ":", c->out_data)))
                    goto jsonenc_obj_done;
            }
            if ((e = jsonenc(c, &tuple.values[1]))) goto jsonenc_obj_done;
            unc_discard(c->view, &pile);
        }
        c->depth = --depth;
        if (c->fancy) {
            if ((e = (*c->out)(1, "\n", c->out_data))) goto jsonenc_obj_done;
            if ((e = jsonenc_indent(c, depth))) goto jsonenc_obj_done;
        }
    }
    e = (*c->out)(1, "}", c->out_data);
    --c->ents_n;
    ++c->recurse;
jsonenc_obj_done:
    unc_clear(c->view, &iter);
    return e;
}

INLINE int jsonenc_val(struct json_encode_context *c, Unc_Value *v) {
    int e;
    switch (unc_gettype(c->view, v)) {
    case Unc_TNull:
        return (*c->out)(4, "null", c->out_data);
    case Unc_TBool:
        if (unc_getbool(c->view, v, 0))
            return (*c->out)(4, "true", c->out_data);
        else
            return (*c->out)(5, "false", c->out_data);
    case Unc_TInt:
    {
        Unc_Int ui;
        e = unc_getint(c->view, v, &ui);
        if (e) return e;
        return jsonenc_printf(c, "%"PRIUnc_Int"d", ui);
    }
    case Unc_TFloat:
    {
        Unc_Float f;
        e = unc_getfloat(c->view, v, &f);
        if (e) return e;
        return jsonenc_printf(c, "%."EVALSTRINGIFY(DBL_DIG) PRIUnc_Float"g", f);
    }
    case Unc_TString:
        return jsonenc_str(c, v);
    case Unc_TArray:
        return jsonenc_arr(c, v);
    case Unc_TTable:
        return jsonenc_obj(c, v);
    default:
        return jsonenc_err(c, "type",
            "cannot encode value of this type as JSON");
    }
}

static int jsonenc(struct json_encode_context *c, Unc_Value *v) {
    int e;
    if (unc_gettype(c->view, &c->mapper)) {
        Unc_View *w = c->view;
        Unc_Value tempval = UNC_BLANK;
        {
            Unc_Pile pile;
            Unc_Tuple tuple;
            e = unc_push(w, 1, v, NULL);
            if (e) return e;
            e = unc_call(w, &c->mapper, 1, &pile);
            if (e) return e;
            unc_returnvalues(w, &pile, &tuple);
            if (!tuple.count) {
                unc_discard(w, &pile);
                return jsonenc_err(c, "value", "mapper did not return a value");
            }
            unc_copy(w, &tempval, &tuple.values[0]);
            unc_discard(w, &pile);
        }
        e = jsonenc_val(c, &tempval);
        unc_clear(w, &tempval);
        return e;
    }
    return jsonenc_val(c, v);
}

Unc_RetVal unc__lib_json_encode(Unc_View *w, Unc_Tuple args, void *ud_) {
    int e;
    Unc_Value v = UNC_BLANK;
    struct json_encode_context cxt =
        { NULL, NULL, 0, NULL, 0, 0, 0, UNC_BLANK };
    struct json_encode_string rs = { NULL, NULL, 0, 0 };
    (void)ud_;

    cxt.view = w;
    cxt.out = &json_encode_string_do;
    cxt.out_data = &rs;
    cxt.recurse = unc_recurselimit(w);
    if (unc_gettype(w, &args.values[1])) {
        cxt.fancy = 1;
        if ((e = unc_getint(w, &args.values[1], &cxt.indent)))
            return e;
    } else
        cxt.fancy = 0;
    cxt.depth = 0;
    cxt.ents = NULL;
    cxt.ents_n = cxt.ents_c = 0;

    rs.alloc = &w->world->alloc;
    if (unc_gettype(w, &args.values[2]) && !unc_iscallable(w, &args.values[2]))
        return unc_throwexc(w, "type", "mapper must be callable or null");
    unc_copy(w, &cxt.mapper, &args.values[2]);
    
    e = jsonenc(&cxt, &args.values[0]);
    if (rs.err) e = rs.err;
    if (!e) {
        e = unc_newstringmove(w, &v, rs.n, (char *)rs.s);
        if (!e) e = unc_push(w, 1, &v, NULL);
        else unc_mfree(w, rs.s);
    }
    unc_mfree(w, cxt.ents);
    unc_clear(w, &cxt.mapper);
    return e;
}

Unc_RetVal unc__lib_json_encodefile(Unc_View *w, Unc_Tuple args, void *ud_) {
    int e;
    struct json_encode_context cxt =
        { NULL, NULL, 0, NULL, 0, 0, 0, UNC_BLANK };
    struct ulib_io_file *pfile;
    struct json_encode_file buf;
    (void)ud_;

    cxt.view = w;
    cxt.out = &json_encode_file_do;
    cxt.out_data = &buf;
    cxt.recurse = unc_recurselimit(w);
    if (unc_gettype(w, &args.values[1])) {
        cxt.fancy = 1;
        if ((e = unc_getint(w, &args.values[1], &cxt.indent)))
            return e;
    } else
        cxt.fancy = 0;
    cxt.depth = 0;
    cxt.ents = NULL;
    cxt.ents_n = cxt.ents_c = 0;
    buf.err = 0;
    buf.view = w;
    if (unc_gettype(w, &args.values[2]) && !unc_iscallable(w, &args.values[2]))
        return unc_throwexc(w, "type", "mapper must be callable or null");
    unc_copy(w, &cxt.mapper, &args.values[2]);

    e = unc__io_lockfile(w, &args.values[0], &pfile, 0);
    if (e) return e;
    buf.fp = pfile;
    
    e = jsonenc(&cxt, &args.values[0]);
    if (buf.err < 0)
        e = unc_throwexc(w, "io", "json.encodefile(): could not encode text");
    else if (unc__io_ferror(pfile))
        e = unc__io_makeerr(w, "json.encodefile()", errno);
    unc__io_unlockfile(w, &args.values[0]);
    unc_mfree(w, cxt.ents);
    unc_clear(w, &cxt.mapper);
    return e;
}

Unc_RetVal uncilmain_json(Unc_View *w) {
    Unc_RetVal e;

    e = unc__io_init(w);
    if (e) return e;

    e = unc_exportcfunction(w, "decode", &unc__lib_json_decode,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;

    e = unc_exportcfunction(w, "encode", &unc__lib_json_encode,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 2, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;

    e = unc_exportcfunction(w, "decodefile", &unc__lib_json_decodefile,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;

    e = unc_exportcfunction(w, "encodefile", &unc__lib_json_encodefile,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 2, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;

    return 0;
}
