/*******************************************************************************
 
Uncil -- value string I/O impl

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

#include <string.h>

#define UNCIL_DEFINES

#include "uarr.h"
#include "ublob.h"
#include "ucommon.h"
#include "udebug.h"
#include "uobj.h"
#include "ustr.h"
#include "uutf.h"
#include "uval.h"
#include "uvali.h"
#include "uview.h"
#include "uvm.h"
#include "uvop.h"
#include "uxprintf.h"
#include "uxscanf.h"

#define MUST(cond) do { int _e; if ((_e = (cond))) return _e; } while (0)

INLINE char hexdigit(int c) {
#if '9' - '0' == 9 && 'f' - 'a' == 5
    return c >= 10 ? 'a' + c - 10 : '0' + c;
#else
    switch (c) {
    case  0:    return '0';
    case  1:    return '1';
    case  2:    return '2';
    case  3:    return '3';
    case  4:    return '4';
    case  5:    return '5';
    case  6:    return '6';
    case  7:    return '7';
    case  8:    return '8';
    case  9:    return '9';
    case 10:    return 'a';
    case 11:    return 'b';
    case 12:    return 'c';
    case 13:    return 'd';
    case 14:    return 'e';
    case 15:    return 'f';
    default:    return ' ';
    }
#endif
}

Unc_RetVal unc0_buftostring(struct Unc_View *w, Unc_Value *out,
                            struct unc0_strbuf *buf) {
    Unc_RetVal e;
    Unc_Value tmp;
    if (buf->length && buf->buffer[buf->length - 1]) {
        e = unc0_strbuf_put1(buf, 0); /* null-terminate */
        if (e) return e;
    }
    unc0_strbuf_compact(buf);
    e = unc0_vrefnew(w, &tmp, Unc_TString);
    if (e) return e;
    if (!buf->length)
        e = unc0_initstringempty(&w->world->alloc,
                                 LEFTOVER(Unc_String, VGETENT(&tmp)));
    else
        e = unc0_initstringmove(&w->world->alloc,
                                LEFTOVER(Unc_String, VGETENT(&tmp)),
                                buf->length - 1, buf->buffer);
    if (e)
        unc0_unwake(VGETENT(&tmp), w);
    else {
        VMOVE(w, out, &tmp);
        buf->buffer = NULL;
        buf->length = buf->capacity = 0;
    }
    return e;
}

Unc_RetVal unc0_buftoblob(struct Unc_View *w, Unc_Value *out,
                          struct unc0_strbuf *buf) {
    Unc_RetVal e;
    Unc_Value tmp;
    unc0_strbuf_compact(buf);
    e = unc0_vrefnew(w, &tmp, Unc_TBlob);
    if (e) return e;
    e = unc0_initblobmove(&w->world->alloc,
                          LEFTOVER(Unc_Blob, VGETENT(&tmp)),
                          buf->length, buf->buffer);
    if (e)
        unc0_unwake(VGETENT(&tmp), w);
    else {
        VMOVE(w, out, &tmp);
        buf->buffer = NULL;
        buf->length = buf->capacity = 0;
    }
    return e;
}

struct printf_wrapper {
    void *udata;
    int (*out)(Unc_Size n, const byte *s, void *udata);
    Unc_RetVal e;
};

int printf_wrapper_f(char outp, void *udata) {
    struct printf_wrapper *s = udata;
    return s->e = (s->out)(1, (byte *)&outp, s->udata);
}

static Unc_RetVal cvt2str_i(int (*out)(Unc_Size n, const byte *s, void *udata),
            void *udata, Unc_Int i) {
    struct printf_wrapper pf;
    pf.udata = udata;
    pf.out = out;
    pf.e = 0;
    unc0_xprintf(&printf_wrapper_f, &pf, 0, 0, "%"PRIUnc_Int"d", i);
    return pf.e;
}

static Unc_RetVal cvt2str_u(int (*out)(Unc_Size n, const byte *s, void *udata),
            void *udata, Unc_Size u) {
    struct printf_wrapper pf;
    pf.udata = udata;
    pf.out = out;
    pf.e = 0;
    unc0_xprintf(&printf_wrapper_f, &pf, 0, 0, "%zu", u);
    return pf.e;
}

static Unc_RetVal cvt2str_u2x(
            int (*out)(Unc_Size n, const byte *s, void *udata),
            void *udata, byte u) {
    struct printf_wrapper pf;
    pf.udata = udata;
    pf.out = out;
    pf.e = 0;
    unc0_xprintf(&printf_wrapper_f, &pf, 0, 0, "%02X", u);
    return pf.e;
}

static Unc_RetVal cvt2str_f(int (*out)(Unc_Size n, const byte *s, void *udata),
            void *udata, Unc_Float f) {
    struct printf_wrapper pf;
    pf.udata = udata;
    pf.out = out;
    pf.e = 0;
    unc0_xprintf(&printf_wrapper_f, &pf, 0, 0,
                "%."EVALSTRINGIFY(DBL_DIG) PRIUnc_Float"g", f);
    /*unc0_xprintf(&printf_wrapper_f, &pf, 0, "%"PRIUnc_Float"a", f);*/
    return pf.e;
}

#if !UNCIL_SANDBOXED
#define DIVRU(n, d) (((n) + ((d) - 1)) / d)
static Unc_RetVal cvt2str_p(int (*out)(Unc_Size n, const byte *s, void *udata),
            void *udata, void *p) {
    char buf[DIVRU(sizeof(uintptr_t) * CHAR_BIT, 4)];
    Unc_Size j = sizeof(buf);
    uintptr_t ip = (uintptr_t)p;
    do {
        buf[--j] = hexdigit(ip & 15);
        ip >>= 4;
    } while (j);
    return out(sizeof(buf), (const byte *)buf, udata);
}
#endif

static Unc_RetVal cvt2str_sp(
            int (*out)(Unc_Size n, const byte *s, void *udata), void *udata,
            Unc_Size tn, const byte *t, void *p) {
    MUST(out(PASSSTRL("<<"), udata));
    MUST(out(tn, t, udata));
#if !UNCIL_SANDBOXED
    MUST(out(PASSSTRL(" at 0x"), udata));
    MUST(cvt2str_p(out, udata, p));
#endif
    MUST(out(PASSSTRL(">>"), udata));
    return 0;
}

static Unc_RetVal cvt2str_spn(
            int (*out)(Unc_Size n, const byte *s, void *udata), void *udata,
            Unc_Size tn, const byte *t, void *p,
            Unc_Size sn, const byte *sb) {
    MUST(out(PASSSTRL("<<"), udata));
    MUST(out(tn, t, udata));
    if (sn) {
        MUST(out(PASSSTRL(" ("), udata));
        MUST(out(sn, sb, udata));
        MUST(out(PASSSTRL(")"), udata));
    }
#if !UNCIL_SANDBOXED
    MUST(out(PASSSTRL(" at 0x"), udata));
    MUST(cvt2str_p(out, udata, p));
#endif
    MUST(out(PASSSTRL(">>"), udata));
    return 0;
}

static Unc_RetVal cvt2str_spq(
            int (*out)(Unc_Size n, const byte *s, void *udata), void *udata,
            Unc_Size tn, const byte *t, void *p,
            Unc_Size sn, const byte *sb) {
    MUST(out(PASSSTRL("<<"), udata));
    MUST(out(tn, t, udata));
    if (sn) {
        MUST(out(PASSSTRL(" '"), udata));
        MUST(out(sn, sb, udata));
        MUST(out(PASSSTRL("'"), udata));
    }
#if !UNCIL_SANDBOXED
    MUST(out(PASSSTRL(" at 0x"), udata));
    MUST(cvt2str_p(out, udata, p));
#endif
    MUST(out(PASSSTRL(">>"), udata));
    return 0;
}

Unc_RetVal unc0_vcvt2strrq(Unc_View *w, Unc_String *s,
            int (*out)(Unc_Size n, const byte *s, void *udata), void *udata) {
    Unc_Size n = s->size, q;
    const byte *sb = n ? unc0_getstringdata(s) : (const byte *)"", *p;
    int c;
    MUST(out(PASSSTRL("\""), udata));
    do {
        c = *(p = sb);
        sb = unc0_utf8nextchar(sb, &n);
        if (!sb) break;
        q = sb - p;
        if (c < 32) {
            switch (c) {
            case '\0':
                MUST(out(PASSSTRL("\\0"), udata));
                break;
            case '\b':
                MUST(out(PASSSTRL("\\b"), udata));
                break;
            case '\f':
                MUST(out(PASSSTRL("\\f"), udata));
                break;
            case '\n':
                MUST(out(PASSSTRL("\\n"), udata));
                break;
            case '\r':
                MUST(out(PASSSTRL("\\r"), udata));
                break;
            case '\t':
                MUST(out(PASSSTRL("\\t"), udata));
                break;
            default:
            {
                char buf[4] = { '\\', 'x', 0, 0 };
                buf[2] = hexdigit((c >> 4) & 15);
                buf[3] = hexdigit(c & 15);
                MUST(out(4, (const byte *)buf, udata));
            }
            }
        } else if (c == 34) {
            MUST(out(PASSSTRL("\\\""), udata));
        } else if (c == 92) {
            MUST(out(PASSSTRL("\\\\"), udata));
        } else if (c == 127) {
            MUST(out(PASSSTRL("\\x7f"), udata));
        } else {
            MUST(out(q, p, udata));
        }
    } while (sb);
    MUST(out(PASSSTRL("\""), udata));
    return 0;
}

Unc_RetVal unc0_vcvt2str(Unc_View *w, Unc_Value *in,
            int (*out)(Unc_Size n, const byte *s, void *udata), void *udata);

Unc_RetVal unc0_vcvt2strq(Unc_View *w, Unc_Value *in,
            int (*out)(Unc_Size n, const byte *s, void *udata), void *udata)
{
    if (VGETTYPE(in) == Unc_TObject || VGETTYPE(in) == Unc_TOpaque) {
        Unc_Value vout;
        Unc_RetVal e = unc0_vovlunary(w, in, &vout,
                                PASSSTRL(OPOVERLOAD(quote)));
        if (e) {
            if (UNCIL_IS_ERR(e)) return e;
            if (VGETTYPE(&vout) == Unc_TString) {
                MUST(unc0_vcvt2strrq(w,
                            LEFTOVER(Unc_String, VGETENT(&vout)),
                            out, udata));
                VDECREF(w, &vout);
                return 0;
            }
            if (w->recurse >= w->recurselimit) {
                VDECREF(w, &vout);
                return UNCIL_ERR_TOODEEP;
            }
            ++w->recurse;
            e = unc0_vcvt2str(w, &vout, out, udata);
            --w->recurse;
            VDECREF(w, &vout);
            return e;
        }
    }
    return unc0_vcvt2str(w, in, out, udata);
}


Unc_RetVal unc0_vcvt2strobj(Unc_View *w, Unc_Value *in,
            int (*out)(Unc_Size n, const byte *s, void *udata), void *udata) {
    void *arr_[32];
    void **arr = arr_;
    Unc_Size arr_c = ASIZEOF(arr), arr_n = 0;
    void *toplev;

    Unc_Value vx;
    Unc_ValueType intype;
    Unc_Size bucket_i, bucket_c;
    Unc_HTblV_V *dnext;
    Unc_Allocator *alloc = &w->world->alloc;
    int first;
    union {
        Unc_Array *a;
        Unc_Dict *d;
    } current;
    
    toplev = LEFTOVER(void, VGETENT(in));

strobjinit:
    first = 1;
    dnext = NULL;
    intype = VGETTYPE(in);
    switch (intype) {
    case Unc_TArray:
        current.a = LEFTOVER(Unc_Array, VGETENT(in));
        bucket_i = 0;
        bucket_c = current.a->size;
        MUST(out(PASSSTRL("["), udata));
        break;
    case Unc_TTable:
        current.d = LEFTOVER(Unc_Dict, VGETENT(in));
        bucket_i = 0;
        bucket_c = current.d->data.capacity;
        MUST(out(PASSSTRL("{"), udata));
        break;
    default:
        if (arr != arr_)
            TMFREE(void *, &w->world->alloc, arr, arr_c);
        NEVER();
    }

strobjcont:
    switch (intype) {
    case Unc_TArray:
        for (;;) {
            if (bucket_i >= bucket_c) { /* end of array */
                MUST(out(PASSSTRL("]"), udata));
                break;
            }
            if (first)
                first = 0;
            else
                MUST(out(PASSSTRL(", "), udata));
            vx = current.a->data[bucket_i++];
            switch (VGETTYPE(&vx)) {
            case Unc_TString:
                MUST(unc0_vcvt2strrq(w, LEFTOVER(Unc_String, VGETENT(&vx)),
                                    out, udata));
                break;
            case Unc_TArray:
                {
                    /* check for cycles */
                    void *comp = LEFTOVER(void, VGETENT(&vx)),
                         **x, **xe = &arr[arr_n];
                    for (x = xe - 1; x >= arr; --x) {
                        if (*x == comp) {
                            /* cycle! */
                            MUST(out(PASSSTRL("[...]"), udata));
                            goto strobjarrnosave;
                        }
                    }
                    if (toplev == comp) {
                        MUST(out(PASSSTRL("[...]"), udata));
                        goto strobjarrnosave;
                    }
                }
            case Unc_TTable:
                /* save */
                if (arr_n + 2 > arr_c) {
                    void **np;
                    Unc_Size z = arr_c + 32;
                    if (arr == arr_) {
                        np = TMALLOC(void *, alloc, 0, z);
                        if (np) TMEMCPY(void *, np, arr_, arr_c);
                    } else
                        np = TMREALLOC(void *, alloc, 0, arr, arr_c, z);
                    if (!np) {
                        /* nope, cannot save */
                        if (VGETTYPE(&vx) == Unc_TTable)
                            MUST(out(PASSSTRL("{...}"), udata));
                        else
                            MUST(out(PASSSTRL("[...]"), udata));
                        goto strobjarrnosave;
                    } else {
                        arr = np;
                        arr_c = z;
                    }
                }
                arr[arr_n++] = &current.a->data[bucket_i];
                arr[arr_n++] = current.a;
                ASSERT(UNLEFTOVER(current.a)->type == Unc_TArray);
                in = &vx;
                goto strobjinit;
strobjarrnosave:
                break;
            case Unc_TObject:
            case Unc_TOpaque:
                MUST(unc0_vcvt2strq(w, &vx, out, udata));
                break;
            default:
                MUST(unc0_vcvt2str(w, &vx, out, udata));
            }
        }
        break;
    case Unc_TTable:
        for (;;) {
            while (!dnext && bucket_i < bucket_c)
                dnext = current.d->data.buckets[bucket_i++];
            if (!dnext) { /* end of dict */
                MUST(out(PASSSTRL("}"), udata));
                break;
            }
            if (first)
                first = 0;
            else
                MUST(out(PASSSTRL(", "), udata));
            vx = dnext->key;
            switch (vx.type) {
            case Unc_TString:
                MUST(unc0_vcvt2strrq(w, LEFTOVER(Unc_String, VGETENT(&vx)),
                                    out, udata));
                break;
            case Unc_TArray:
            case Unc_TTable:
                NEVER();
            case Unc_TObject:
            case Unc_TOpaque:
                MUST(unc0_vcvt2strq(w, &vx, out, udata));
                break;
            default:
                MUST(unc0_vcvt2str(w, &vx, out, udata));
            }
            MUST(out(PASSSTRL(": "), udata));
            vx = dnext->val;
            dnext = dnext->next;
            switch (vx.type) {
            case Unc_TString:
                MUST(unc0_vcvt2strrq(w, LEFTOVER(Unc_String, VGETENT(&vx)),
                                    out, udata));
                break;
            case Unc_TTable:
                {
                    /* check for cycles */
                    void *comp = LEFTOVER(void, VGETENT(&vx)),
                         **x, **xe = &arr[arr_n];
                    for (x = xe - 1; x >= arr; --x) {
                        if (*x == comp) {
                            /* cycle! */
                            MUST(out(PASSSTRL("{...}"), udata));
                            goto strobjdictnosave;
                        }
                    }
                    if (toplev == comp) {
                        MUST(out(PASSSTRL("{...}"), udata));
                        goto strobjarrnosave;
                    }
                }
            case Unc_TArray:
                /* save */
                if (arr_n + 3 > arr_c) {
                    void **np;
                    Unc_Size z = arr_c + 32;
                    if (arr == arr_) {
                        np = TMALLOC(void *, alloc, 0, z);
                        if (np) TMEMCPY(void *, np, arr_, arr_c);
                    } else
                        np = TMREALLOC(void *, alloc, 0, arr, arr_c, z);
                    if (!np) {
                        /* nope, cannot save */
                        if (VGETTYPE(&vx) == Unc_TTable)
                            MUST(out(PASSSTRL("{...}"), udata));
                        else
                            MUST(out(PASSSTRL("[...]"), udata));
                        goto strobjdictnosave;
                    } else {
                        arr = np;
                        arr_c = z;
                    }
                }
                arr[arr_n++] = dnext;
                arr[arr_n++] = &current.d->data.buckets[bucket_i];
                arr[arr_n++] = current.d;
                ASSERT(UNLEFTOVER(current.d)->type == Unc_TTable);
                in = &vx;
                goto strobjinit;
strobjdictnosave:
                break;
            case Unc_TObject:
            case Unc_TOpaque:
                MUST(unc0_vcvt2strq(w, &vx, out, udata));
                break;
            default:
                MUST(unc0_vcvt2str(w, &vx, out, udata));
            }
        }
        break;
    default:
        if (arr != arr_)
            TMFREE(void *, &w->world->alloc, arr, arr_c);
        NEVER();
    }

    if (arr_n) {
        void *p = arr[--arr_n];
        Unc_Entity *e = UNLEFTOVER(p);
        intype = e->type;

        switch (e->type) {
        case Unc_TArray:
            current.a = (Unc_Array *)p;
            bucket_i = (Unc_Value *)arr[--arr_n] - current.a->data;
            bucket_c = current.a->size;
            break;
        case Unc_TTable:
            current.d = (Unc_Dict *)p;
            bucket_i = (Unc_HTblV_V **)arr[--arr_n] - current.d->data.buckets;
            bucket_c = current.d->data.capacity;
            dnext = arr[--arr_n];
            break;
        default:
            NEVER();
        }
        first = 0;
        goto strobjcont;
    }

    if (arr != arr_)
        TMFREE(void *, &w->world->alloc, arr, arr_c);
    return 0;
}

Unc_RetVal unc0_vcvt2str(Unc_View *w, Unc_Value *in,
            int (*out)(Unc_Size n, const byte *s, void *udata), void *udata) {
    switch (VGETTYPE(in)) {
    case Unc_TNull:
        return out(PASSSTRL("null"), udata);
    case Unc_TBool:
        if (VGETBOOL(in))
            return out(PASSSTRL("true"), udata);
        else
            return out(PASSSTRL("false"), udata);
    case Unc_TInt:
        return cvt2str_i(out, udata, VGETINT(in));
    case Unc_TFloat:
        return cvt2str_f(out, udata, VGETFLT(in));
    case Unc_TString:
    {
        Unc_String *s = LEFTOVER(Unc_String, VGETENT(in));
        return out(s->size, unc0_getstringdata(s), udata);
    }
    case Unc_TArray:
    case Unc_TTable:
        return unc0_vcvt2strobj(w, in, out, udata);
    case Unc_TBlob:
    {
        Unc_Size i;
        Unc_Blob *s = LEFTOVER(Unc_Blob, VGETENT(in));
        MUST(out(PASSSTRL("<blob("), udata));
        UNC_LOCKL(s->lock);
        MUST(cvt2str_u(out, udata, s->size));
        if (!s->size) {
            MUST(out(PASSSTRL(")>"), udata));
        } else {
            MUST(out(PASSSTRL(") ="), udata));
            for (i = 0; i < s->size; ++i) {
                MUST(out(PASSSTRL(" "), udata));
                MUST(cvt2str_u2x(out, udata, s->data[i]));
            }
            MUST(out(PASSSTRL(">"), udata));
        }
        UNC_UNLOCKL(s->lock);
        return 0;
    }
    case Unc_TObject:
    case Unc_TOpaque:
    {
        Unc_RetVal e;
        Unc_Size nsn = 0;
        const byte *nsb = NULL;
        Unc_Value vout;
        e = unc0_vovlunary(w, in, &vout, PASSSTRL(OPOVERLOAD(string)));
        if (e) {
            if (UNCIL_IS_ERR(e)) return e;
            if (w->recurse >= w->recurselimit) {
                VDECREF(w, &vout);
                return UNCIL_ERR_TOODEEP;
            }
            ++w->recurse;
            e = unc0_vcvt2str(w, &vout, out, udata);
            --w->recurse;
            VDECREF(w, &vout);
            return 0;
        }
        {
            int ofound;
            Unc_Value o = UNC_BLANK;
            unc0_getprotomethod(w, in, PASSSTRL(OPOVERLOAD(name)),
                                &ofound, &o);
            if (ofound && o.type == Unc_TString) {
                Unc_String *s = LEFTOVER(Unc_String, VGETENT(&o));
                nsn = s->size;
                nsb = unc0_getstringdata(s);
            }
            VCLEAR(w, &o);
        }
        if (VGETTYPE(in) == Unc_TOpaque)
            return cvt2str_spn(out, udata, PASSSTRL("opaque"), VGETENT(in),
                                                                nsn, nsb);
        else
            return cvt2str_spn(out, udata, PASSSTRL("object"), VGETENT(in),
                                                                nsn, nsb);
    }
    case Unc_TFunction:
    {
        Unc_Function *f = LEFTOVER(Unc_Function, VGETENT(in));
        if (f->flags & UNC_FUNCTION_FLAG_CFUNC) {
            if (f->f.c.name)   
                return cvt2str_spq(out, udata, PASSSTRL("function"),
                    VGETENT(in), 
                    unc0_strlen(f->f.c.name), (const byte *)f->f.c.name);
            else
                return cvt2str_sp(out, udata, PASSSTRL("function"),
                    VGETENT(in));
        } else if (f->flags & UNC_FUNCTION_FLAG_NAMED) {
            Unc_Size sn;
            const byte *sb;
            unc0_loadstrpx(f->f.u.program->data + f->f.u.nameoff, &sn, &sb);
            return cvt2str_spq(out, udata, PASSSTRL("function"), VGETENT(in),
                            sn, sb);
        } else {
            return cvt2str_sp(out, udata, PASSSTRL("function"), VGETENT(in));
        }
    }
    case Unc_TBoundFunction:
        return cvt2str_sp(out, udata, PASSSTRL("bound function"), VGETENT(in));
    case Unc_TWeakRef:
        return cvt2str_sp(out, udata, PASSSTRL("weakref"), VGETENT(in));
    case Unc_TOpaquePtr:
        return cvt2str_sp(out, udata, PASSSTRL("opaqueptr"), VGETPTR(in));
    default:
        NEVER();
    }
    return 0;
}

static int savxprintf__wrapper(char outp, void *udata) {
    struct unc0_strbuf *s = udata;
    return unc0_strbuf_put1(s, outp);
}

size_t unc0_savxprintf(Unc_Allocator *alloc, byte **s, const char *fmt,
                       va_list arg) {
    size_t e;
    struct unc0_strbuf buf;
    unc0_strbuf_init(&buf, alloc, Unc_AllocString);
    e = unc0_vxprintf(&savxprintf__wrapper, &buf, 0, 0, fmt, arg);
    if (e != UNC_PRINTF_EOF) /* null-terminate */
        e = unc0_strbuf_put1(&buf, 0) ? UNC_PRINTF_EOF : e;
    if (e == UNC_PRINTF_EOF)
        unc0_strbuf_free(&buf);
    else
        *s = unc0_mrealloc(alloc, Unc_AllocString, buf.buffer,
                           buf.capacity, e + 1);
    return e;
}

size_t unc0_saxprintf(Unc_Allocator *alloc, byte **s, const char *fmt, ...) {
    size_t r;
    va_list va;
    va_start(va, fmt);
    r = unc0_savxprintf(alloc, s, fmt, va);
    va_end(va);
    return r;
}

size_t unc0_sacvxprintf(struct unc0_strbuf *buf,
                        int gflags, size_t fmt_n, const char *fmt,
                        va_list arg) {
    return unc0_vxprintf(&savxprintf__wrapper, buf, gflags, fmt_n, fmt, arg);
}

size_t unc0_sacxprintf(struct unc0_strbuf *buf,
                       int gflags, size_t fmt_n, const char *fmt, ...) {
    size_t r;
    va_list va;
    va_start(va, fmt);
    r = unc0_sacvxprintf(buf, gflags, fmt_n, fmt, va);
    va_end(va);
    return r;
}

Unc_RetVal unc0_usvxprintf(struct Unc_View *w, Unc_Value *out,
                           const char *fmt, va_list arg) {
    Unc_RetVal e;
    struct unc0_strbuf buf;
    unc0_strbuf_init(&buf, &w->world->alloc, Unc_AllocString);
    e = unc0_vxprintf(&savxprintf__wrapper, &buf, 0, 0, fmt, arg);
    if (e < 0) {
        unc0_strbuf_free(&buf);
        return UNCIL_ERR_MEM;
    }
    unc0_strbuf_compact(&buf);
    e = unc0_buftostring(w, out, &buf);
    unc0_strbuf_free(&buf);
    return e;
}

Unc_RetVal unc0_usxprintf(struct Unc_View *w, Unc_Value *out,
                          const char *fmt, ...) {
    Unc_RetVal r;
    va_list va;
    va_start(va, fmt);
    r = unc0_usvxprintf(w, out, fmt, va);
    va_end(va);
    return r;
}

Unc_RetVal unc0_std_makeerr(Unc_View *w, const char *mt,
                            const char *prefix, Unc_RetVal err) {
    Unc_RetVal e;
    Unc_Value msg = UNC_BLANK;
#if __STDC_LIB_EXT1__
    if (err) {
        size_t s = strerrorlen_s(err);
        int preflen;
        if (s > INT_MAX) s = INT_MAX;
        e = unc0_usxprintf(w, &msg, "%s: %n%*c", prefix, &preflen,
                           (int)s, ' ');
        if (e) return UNCIL_ERR_MEM;
        strerror_s(
            (const char *)unc0_getstringdata(LEFTOVER(Unc_String,
                                             VGETENT(&msg))) + preflen,
                                             s, err);
    } else
#endif
    {
        const char *c = err ? strerror(err) : "unknown error";
        e = unc0_usxprintf(w, &msg, "%s: %s", prefix, c);
        if (e) return UNCIL_ERR_MEM;
    }
    return e ? e : unc_throwext(w, mt, &msg);
}

struct sxscanf_buffer {
    Unc_Size n;
    const byte *s;
};

int sxscanf__wrapper(void *data) {
    struct sxscanf_buffer *buf = data;
    if (!buf->n) return -1;
    --buf->n;
    return *buf->s++;
}

size_t unc0_sxscanf(Unc_Size sn, const byte *bn, const char *format, ...) {
    size_t r;
    struct sxscanf_buffer buf;
    va_list va;
    va_start(va, format);
    buf.n = sn;
    buf.s = bn;
    r = unc0_vxscanf(&sxscanf__wrapper, NULL, &buf, format, va);
    va_end(va);
    return r;
}
