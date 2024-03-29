/*******************************************************************************
 
Uncil -- standard library impl

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

#include <stdio.h>

#define UNCIL_DEFINES

#include "uarithm.h"
#include "uarr.h"
#include "ublob.h"
#include "udebug.h"
#include "uctype.h"
#include "umodule.h"
#include "uncil.h"
#include "usort.h"
#include "ustr.h"
#include "utxt.h"
#include "uutf.h"
#include "uvali.h"
#include "uview.h"
#include "uvop.h"
#include "uvsio.h"

#define MUST(cond) do { Unc_RetVal e; if ((e = (cond))) return e; } while (0)

#if !UNCIL_SANDBOXED
int unc0_g_print_out_(Unc_Size n, const byte *s, void *udata) {
    (void)udata;
    fwrite(s, 1, n, stdout);
    return 0;
}

Unc_RetVal unc0_g_print(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Size i;
    (void)udata;

    for (i = 0; i < args.count; ++i) {
        if (i) putchar('\t');
        unc0_vcvt2str(w, &args.values[i], &unc0_g_print_out_, NULL);
    }

    putchar('\n');
    return 0;
}

Unc_RetVal unc0_g_input(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    int c;
    struct unc0_strbuf s;
    Unc_Value out = UNC_BLANK;
    (void)udata;

    if (args.values[0].type) {
        Unc_RetVal e;
        Unc_Size sn;
        const char *sp;
        e = unc_getstring(w, &args.values[0], &sn, &sp);
        if (e) return e;
        fwrite(sp, 1, sn, stdout);
        fflush(stdout);
    }

    unc0_strbuf_init(&s, &w->world->alloc, Unc_AllocString);
    for (;;) {
        c = getchar();
        if (c < 0) {
            if (!s.length) {
                Unc_Value v = UNC_BLANK;
                return unc_returnlocal(w, 0, &v);
            } else {
                break;
            }
        } else if (c == '\n') {
            break;
        }
        if (unc0_strbuf_put1(&s, (byte)c)) goto memfail;
    }

    e = unc0_buftostring(w, &out, &s);
    if (!e) return unc_returnlocal(w, 0, &out);
memfail:
    unc0_strbuf_free(&s);
    return UNCIL_ERR_MEM;
}
#endif

Unc_RetVal unc0_g_require(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Size sn;
    const char *sp;
    Unc_Entity *en;
    Unc_Value v, *ov;
    Unc_HTblS *cache = &w->world->modulecache;
    (void)udata;

    ASSERT(args.count == 1);
    e = unc_getstring(w, &args.values[0], &sn, &sp);
    if (e) return e;
    ov = unc0_gethtbls(w, cache, sn, (const byte *)sp);
    if (ov) return unc_push(w, 1, ov);
    e = unc0_puthtbls(w, cache, sn, (const byte *)sp, &ov);
    if (e) return UNCIL_ERR_MEM;
    VSETNULL(w, ov);
    en = unc0_wake(w, Unc_TObject);
    if (!en) {
        e = UNCIL_ERR_MEM;
        goto memfail;
    }
    e = unc0_initobj(w, LEFTOVER(Unc_Object, en), NULL);
    if (e) {
        unc0_unwake(en, w);
        goto memfail;
    }
    e = unc0_dorequire(w, sn, (const byte *)sp, LEFTOVER(Unc_Object, en));
    if (e) {
        unc0_hibernate(en, w);
        goto memfail;
    }

    VINITENT(&v, Unc_TObject, en);
    VCOPY(w, ov, &v);
    return unc_returnlocal(w, 0, &v);

memfail:
    unc0_delhtbls(w, cache, sn, (const byte *)sp);
    return e;
}

Unc_RetVal unc0_g_type(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    const char *sp = unc0_getvaluetypename(unc_gettype(w, &args.values[0]));
    Unc_Value v = UNC_BLANK;
    e = unc_newstringc(w, &v, sp);
    return unc_returnlocal(w, e, &v);
}

Unc_RetVal unc0_g_throw(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Size sn, sn2;
    const char *sp, *sp2;
    int msg;
    Unc_Entity *ed, *es1, *es2;
    Unc_Object *o;
    Unc_Value v = UNC_BLANK;
    
    if (args.values[0].type == Unc_TObject)
        return unc_throw(w, &args.values[0]);

    e = unc_getstring(w, &args.values[0], &sn2, &sp2);
    if (e) return e;
    if (args.values[1].type) {
        e = unc_getstring(w, &args.values[1], &sn, &sp);
        if (e) return e;
        msg = 0;
    } else {
        msg = 1;
    }
    
    ed = unc0_wake(w, Unc_TObject);
    if (!ed) return UNCIL_ERR_MEM;
    o = LEFTOVER(Unc_Object, ed);
    e = unc0_initobj(w, o, NULL);
    if (e) {
        unc0_unwake(ed, w);
        return e;
    }
    if (!e) {
        es1 = unc0_wake(w, Unc_TString);
        if (!es1) e = UNCIL_ERR_MEM;
        else {
            if (msg)
                e = unc0_initstringcl(&w->world->alloc,
                                      LEFTOVER(Unc_String, es1),
                                      "custom");
            else
                e = unc0_initstring(&w->world->alloc,
                                    LEFTOVER(Unc_String, es1),
                                    sn, (const byte *)sp);
            if (e) {
                unc0_unwake(es1, w);
                es1 = NULL;
            }
        }
        if (es1) {
            Unc_Value tmp;
            VINITENT(&tmp, Unc_TString, es1);
            e = unc0_osetattrs(w, o, PASSSTRL("type"), &tmp);
        }
    }
    if (!e) {
        es2 = unc0_wake(w, Unc_TString);
        if (!es2) e = UNCIL_ERR_MEM;
        else {
            e = unc0_initstring(&w->world->alloc,
                                LEFTOVER(Unc_String, es2),
                                sn2, (const byte *)sp2);
            if (e) {
                unc0_unwake(es2, w);
                es2 = NULL;
            }
        }
        if (es2) {
            Unc_Value tmp;
            VINITENT(&tmp, Unc_TString, es2);
            e = unc0_osetattrs(w, o, PASSSTRL("message"), &tmp);
            if (e && es2)
                unc0_hibernate(es2, w);
        }
    } else {
        if (es1) unc0_hibernate(es1, w);
    }
    if (e) {
        unc0_hibernate(ed, w);
        return e;
    }
    VINITENT(&v, Unc_TObject, ed);
    return unc_throw(w, &v);
}

Unc_RetVal unc0_g_bool(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value v;
    int e = unc0_vcvt2bool(w, &args.values[0]);
    if (UNCIL_IS_ERR(e)) return e;
    VINITBOOL(&v, e);
    return unc_returnlocal(w, 0, &v);
}

Unc_RetVal unc0_g_object(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    int freeze;
    Unc_Value v = UNC_BLANK;
    switch (args.values[0].type) {
    case Unc_TNull:
    case Unc_TTable:
    case Unc_TObject:
    case Unc_TOpaque:
        break;
    default:
        return UNCIL_ERR_ARG_INVALIDPROTOTYPE;
    }
    freeze = unc_getbool(w, &args.values[2], 0);
    if (UNCIL_IS_ERR(freeze)) return freeze;
    if (args.values[1].type && args.values[1].type != Unc_TTable)
        return unc0_throwexc(w, "value", "object initializer must be a dict");
    e = unc_newobject(w, &v, &args.values[0]);
    if (e) return e;
    if (args.values[1].type == Unc_TTable) {
        Unc_Size i;
        Unc_Object *o = LEFTOVER(Unc_Object, VGETENT(&v));
        Unc_Dict *dict;
        Unc_HTblV_V *nx;

        dict = LEFTOVER(Unc_Dict, VGETENT(&args.values[1]));
        UNC_LOCKL(dict->lock);
        i = -1;
        nx = NULL;

        for (;;) {
            while (!nx) {
                if (++i >= dict->data.capacity) {
                    UNC_UNLOCKL(dict->lock);
                    goto unc0_g_object_exit;
                }
                nx = dict->data.buckets[i];
            }
            e = unc0_osetindxraw(w, o, &nx->key, &nx->val);
            if (e) {
                unc0_hibernate(VGETENT(&v), w);
                UNC_UNLOCKL(dict->lock);
                return e;
            }
            nx = nx->next;
        }
    }
unc0_g_object_exit:
    if (freeze) unc0_ofreeze(w, LEFTOVER(Unc_Object, VGETENT(&v)));
    return unc_returnlocal(w, 0, &v);
}

Unc_RetVal unc0_g_getprototype(Unc_View *w, Unc_Tuple args, void *udata) {
    switch (args.values[0].type) {
    case Unc_TObject:
        return unc_push(w, 1,
            &LEFTOVER(Unc_Object, VGETENT(&args.values[0]))->prototype);
    case Unc_TOpaque:
        return unc_push(w, 1,
            &LEFTOVER(Unc_Opaque, VGETENT(&args.values[0]))->prototype);
    default:
        return UNCIL_ERR_TYPE_NOTOBJECT;
    }
}

Unc_RetVal unc0_g_getversion(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value v[3] = UNC_BLANKS;
    (void)udata;

    VINITINT(&v[0], UNCIL_VER_MAJOR);
    VINITINT(&v[1], UNCIL_VER_MINOR);
    VINITINT(&v[2], UNCIL_VER_PATCH);
    return unc_push(w, 3, v);
}

Unc_RetVal unc0_g_weakref(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Value v;
    e = unc0_makeweak(w, &args.values[0], &v);
    return unc_returnlocal(w, e, &v);
}

Unc_RetVal unc0_g_int(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Value sv = UNC_BLANK;
    e = unc0_vcvt2int(w, &sv, &args.values[1]);
    return unc_returnlocal(w, e, &sv);
}

Unc_RetVal unc0_g_float(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Value sv = UNC_BLANK;
    e = unc0_vcvt2flt(w, &sv, &args.values[1]);
    return unc_returnlocal(w, e, &sv);
}

int unc0_g_str_out_(Unc_Size n, const byte *s, void *udata) {
    struct unc0_strbuf *bstr = (struct unc0_strbuf *)udata;
    return unc0_strbuf_putn(bstr, n, s);
}

Unc_RetVal unc0_g_str(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    struct unc0_strbuf sbuf;
    Unc_Value sv = UNC_BLANK;

    (void)udata;
    unc0_strbuf_init(&sbuf, &w->world->alloc, Unc_AllocString);

    e = unc0_vcvt2str(w, &args.values[1], &unc0_g_str_out_, &sbuf);
    if (e) goto memfail;

    e = unc0_buftostring(w, &sv, &sbuf);
    if (!e) return unc_returnlocal(w, 0, &sv);

memfail:
    unc0_strbuf_free(&sbuf);
    return UNCIL_ERR_MEM;
}

Unc_RetVal unc0_g_blob(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Size sn;
    byte *sp;
    Unc_Int ui;
    Unc_Value v = UNC_BLANK;

    ASSERT(args.count == 2);
    e = unc_getint(w, &args.values[1], &ui);
    if (e) return e;
    if (ui < 0)
        return unc0_throwexc(w, "value", "blob size cannot be negative");
    sn = (Unc_Size)ui;
    e = unc_newblob(w, &v, sn, &sp);
    if (e) return e;
    unc0_mbzero(sp, sn);
    unc_unlock(w, &v);
    return unc_returnlocal(w, 0, &v);
}

Unc_RetVal unc0_g_array(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Value arr = UNC_BLANK;
    Unc_Value iter = UNC_BLANK;
    Unc_Pile pile;
    Unc_Tuple tuple;
    Unc_Array *a;

    e = unc0_vgetiter(w, &iter, &args.values[1]);
    if (e) return e;
    e = unc_newarrayempty(w, &arr);
    if (e) goto fail;

    a = LEFTOVER(Unc_Array, VGETENT(&arr));
    do {
        e = unc_call(w, &iter, 0, &pile);
        if (e) goto fail;
        unc_returnvalues(w, &pile, &tuple);
        if (tuple.count) {
            e = unc0_arraypush(w, a, &tuple.values[0]);
            if (e) {
                unc_discard(w, &pile);
                goto fail;
            }
        }
        unc_discard(w, &pile);
    } while (tuple.count);

    VCLEAR(w, &iter);
    return unc_returnlocal(w, 0, &arr);
fail:
    VCLEAR(w, &iter);
    VCLEAR(w, &arr);
    return e;
}

Unc_RetVal unc0_g_table(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Value tbl = UNC_BLANK;
    Unc_Value iter = UNC_BLANK;
    Unc_Pile pile;
    Unc_Tuple tuple;
    Unc_Dict *a;

    e = unc0_vgetiter(w, &iter, &args.values[1]);
    if (e) return e;
    e = unc_newtable(w, &tbl);
    if (e) goto fail;

    a = LEFTOVER(Unc_Dict, VGETENT(&tbl));
    do {
        e = unc_call(w, &iter, 0, &pile);
        if (e) goto fail;
        unc_returnvalues(w, &pile, &tuple);
        if (tuple.count == 1) {
            unc_discard(w, &pile);
            e = unc0_throwexc(w, "value", 
                "iterator returned only 1 value, but table expected 2");
            goto fail;
        }
        if (tuple.count) {
            e = unc0_dsetindx(w, a, &tuple.values[0], &tuple.values[1]);
            if (e) {
                unc_discard(w, &pile);
                goto fail;
            }
        }
        unc_discard(w, &pile);
    } while (tuple.count);

    VCLEAR(w, &iter);
    return unc_returnlocal(w, 0, &tbl);
fail:
    VCLEAR(w, &iter);
    VCLEAR(w, &tbl);
    return e;
}

Unc_RetVal unc0_gs_size(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Size sn;
    const char *sp;
    Unc_Int ui;
    Unc_Value v;

    ASSERT(args.count == 1);
    e = unc_getstring(w, &args.values[0], &sn, &sp);
    if (e) return e;
    ui = (Unc_Int)sn;
    
    VINITINT(&v, ui);
    return unc_returnlocal(w, 0, &v);
}

Unc_RetVal unc0_gs_length(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Size sn;
    const char *sp;
    Unc_Int ui;
    Unc_Value v;

    ASSERT(args.count == 1);
    e = unc_getstring(w, &args.values[0], &sn, &sp);
    if (e) return e;
    ui = (Unc_Int)unc0_utf8unshift((const byte *)sp, sn);
    
    VINITINT(&v, ui);
    return unc_returnlocal(w, 0, &v);
}

Unc_RetVal unc0_gs_find(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Size sn, sn2;
    const byte *sp, *sp2;
    Unc_Int ui;
    Unc_Value v;

    e = unc_getstring(w, &args.values[0], &sn, (const char **)&sp);
    if (e) return e;
    e = unc_getstring(w, &args.values[1], &sn2, (const char **)&sp2);
    if (e) return e;
    if (unc_gettype(w, &args.values[2])) {
        e = unc_getint(w, &args.values[2], &ui);
        if (e) return e;
        if (ui < 0)
            sp = unc0_utf8shiftback(sp, &sn, -ui);
        else
            sp = unc0_utf8shift(sp, &sn, ui);
    } else {
        ui = 0;
    }
    if (sp) {
        const byte *sq = unc0_strsearch(sp, sn, sp2, sn2);
        VINITINT(&v, sq
            ? unc0_utf8unshift(sp, sq - sp) + (ui < 0 ? 0 : ui) : -1);
    } else
        VINITINT(&v, -1);
    
    return unc_returnlocal(w, 0, &v);
}

Unc_RetVal unc0_gs_findlast(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Size sn, sn2;
    const byte *sp, *sp2;
    Unc_Int ui;
    Unc_Value v;

    e = unc_getstring(w, &args.values[0], &sn, (const char **)&sp);
    if (e) return e;
    e = unc_getstring(w, &args.values[1], &sn2, (const char **)&sp2);
    if (e) return e;
    if (unc_gettype(w, &args.values[2])) {
        e = unc_getint(w, &args.values[2], &ui);
        if (e) return e;
        if (ui < 0)
            sp = unc0_utf8shiftaway(sp, &sn, -ui);
        else if (ui > 0) {
            Unc_Size qn = sn;
            (void)unc0_utf8shift(sp, &qn, ui);
            sn = sn - qn;
        }
    } else {
        ui = 0;
    }
    if (sp) {
        const byte *sq = unc0_strsearchr(sp, sn, sp2, sn2);
        VINITINT(&v, sq
            ? unc0_utf8unshift(sp, sq - sp) + (ui < 0 ? 0 : ui) : -1);
    } else
        VINITINT(&v, -1);
    
    return unc_returnlocal(w, 0, &v);
}

Unc_RetVal unc0_gs_sub(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Size sn;
    const byte *sp;
    Unc_Int ui1, ui2;
    Unc_Value v = UNC_BLANK;

    e = unc_getstring(w, &args.values[0], &sn, (const char **)&sp);
    if (e) return e;
    e = unc_getint(w, &args.values[1], &ui1);
    if (e) return e;
    if (ui1 < 0)
        sp = unc0_utf8shiftback(sp, &sn, -ui1);
    else
        sp = unc0_utf8shift(sp, &sn, ui1);
    if (sp && unc_gettype(w, &args.values[2])) {
        const byte *sq;
        e = unc_getint(w, &args.values[2], &ui2);
        if (e) return e;
        if (ui2 >= 0) {
            if (ui2 <= ui1)
                e = unc_newstring(w, &v, 0, NULL);
            else {
                sq = unc0_utf8shift(sp, &sn, ui2 - ui1);
                if (!sq) sq = sp + sn;
                e = unc_newstring(w, &v, sq - sp, (const char *)sp);
            }
        } else {
            (void)unc0_utf8shiftaway(sp, &sn, -ui2);
            e = unc_newstring(w, &v, sn, (const char *)sp);
        }
    } else
        e = unc_newstring(w, &v, sn, (const char *)sp);
    return unc_returnlocal(w, e, &v);
}

Unc_RetVal unc0_gs_charcode(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Size sn;
    const byte *sp;
    Unc_Int ui = 0;
    Unc_Value v;

    e = unc_getstring(w, &args.values[0], &sn, (const char **)&sp);
    if (e) return e;
    if (unc_gettype(w, &args.values[1])) {
        e = unc_getint(w, &args.values[1], &ui);
        if (e) return e;
        if (ui < 0)
            sp = unc0_utf8shiftback(sp, &sn, -ui);
        else
            sp = unc0_utf8shift(sp, &sn, ui);
        if (ui && !sn)
            return unc0_throwexc(w, "value", "string index out of bounds");
    }
    if (!sn)
        return unc0_throwexc(w, "value", "string is empty");
    ui = unc0_utf8decd(sp);
    VINITINT(&v, ui);
    return unc_returnlocal(w, 0, &v);
}

Unc_RetVal unc0_gs_char(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Size sn;
    byte buf[UNC_UTF8_MAX_SIZE];
    Unc_Int ui = 0;
    Unc_Value v = UNC_BLANK;

    e = unc_getint(w, &args.values[0], &ui);
    if (e) return e;
    if (ui < 0 || ui >= 0x110000L)
        return unc0_throwexc(w, "value", "code point out of range");
    sn = unc0_utf8enc((Unc_UChar)ui, sizeof(buf), buf);
    e = unc_newstring(w, &v, sn, (const char *)buf);
    return unc_returnlocal(w, e, &v);
}

Unc_RetVal unc0_gs_reverse(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Size sn;
    const byte *sp;
    byte *buf;
    Unc_Value v;
    Unc_Entity *en;

    ASSERT(args.count == 1);
    e = unc_getstring(w, &args.values[0], &sn, (const char **)&sp);
    if (e) return e;
    
    buf = unc0_malloc(&w->world->alloc, Unc_AllocString, sn + 1);
    if (!buf) {
        return UNCIL_ERR_MEM;
    }
    
    en = unc0_wake(w, Unc_TString);
    if (!en) {
        unc0_mfree(&w->world->alloc, buf, sn + 1);
        return UNCIL_ERR_MEM;
    }
    
    unc0_utf8rev(buf, sp, sn);
    buf[sn] = 0;
    e = unc0_initstringmove(&w->world->alloc,
                            LEFTOVER(Unc_String, en), sn, buf);
    if (e) {
        unc0_mfree(&w->world->alloc, buf, sn + 1);
        unc0_unwake(en, w);
        return e;
    }
    VINITENT(&v, Unc_TString, en);
    return unc_returnlocal(w, 0, &v);
}

Unc_RetVal unc0_gs_asciilower(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Size sn, i;
    const byte *sp;
    byte *buf, c;
    Unc_Value v;
    Unc_Entity *en;

    ASSERT(args.count == 1);
    e = unc_getstring(w, &args.values[0], &sn, (const char **)&sp);
    if (e) return e;
    
    buf = unc0_malloc(&w->world->alloc, Unc_AllocString, sn + 1);
    if (!buf) {
        return UNCIL_ERR_MEM;
    }
    
    en = unc0_wake(w, Unc_TString);
    if (!en) {
        unc0_mfree(&w->world->alloc, buf, sn + 1);
        return UNCIL_ERR_MEM;
    }
    
    for (i = 0; i < sn; ++i) {
        c = sp[i];
        if (unc0_isalpha(c))
            c = unc0_tolower(c);
        buf[i] = c;
    }
    buf[sn] = 0;

    e = unc0_initstringmove(&w->world->alloc,
                            LEFTOVER(Unc_String, en), sn, buf);
    if (e) {
        unc0_mfree(&w->world->alloc, buf, sn + 1);
        unc0_unwake(en, w);
        return e;
    }
    VINITENT(&v, Unc_TString, en);
    return unc_returnlocal(w, 0, &v);
}

Unc_RetVal unc0_gs_asciiupper(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Size sn, i;
    const byte *sp;
    byte *buf, c;
    Unc_Value v;
    Unc_Entity *en;

    ASSERT(args.count == 1);
    e = unc_getstring(w, &args.values[0], &sn, (const char **)&sp);
    if (e) return e;
    
    buf = unc0_malloc(&w->world->alloc, Unc_AllocString, sn + 1);
    if (!buf) {
        return UNCIL_ERR_MEM;
    }
    
    en = unc0_wake(w, Unc_TString);
    if (!en) {
        unc0_mfree(&w->world->alloc, buf, sn + 1);
        return UNCIL_ERR_MEM;
    }
    
    for (i = 0; i < sn; ++i) {
        c = sp[i];
        if (unc0_isalpha(c))
            c = unc0_toupper(c);
        buf[i] = c;
    }
    buf[sn] = 0;

    e = unc0_initstringmove(&w->world->alloc,
                            LEFTOVER(Unc_String, en), sn, buf);
    if (e) {
        unc0_mfree(&w->world->alloc, buf, sn + 1);
        unc0_unwake(en, w);
        return e;
    }
    VINITENT(&v, Unc_TString, en);
    return unc_returnlocal(w, 0, &v);
}

Unc_RetVal unc0_gs_repeat(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Size sn;
    const byte *sp;
    byte *buf;
    Unc_Value v;
    Unc_Int ui, ut;
    Unc_Entity *en;

    ASSERT(args.count == 2);
    e = unc_getstring(w, &args.values[0], &sn, (const char **)&sp);
    if (e) return e;
    e = unc_getint(w, &args.values[1], &ui);
    if (e) return e;
    if (ui < 0)
        return unc0_throwexc(w, "value",
                    "cannot repeat a negative number of times");
    
    if (ui) {
        buf = unc0_malloc(&w->world->alloc, Unc_AllocString, sn * ui + 1);
        if (!buf) {
            return UNCIL_ERR_MEM;
        }
    } else
        buf = NULL;
    
    en = unc0_wake(w, Unc_TString);
    if (!en) {
        unc0_mfree(&w->world->alloc, buf, sn * ui + 1);
        return UNCIL_ERR_MEM;
    }
    
    for (ut = 0; ut < ui; ++ut)
        unc0_memcpy(buf + ut * sn, sp, sn);
    buf[sn * ui] = 0;

    e = unc0_initstringmove(&w->world->alloc,
                            LEFTOVER(Unc_String, en), sn * ui, buf);
    if (e) {
        unc0_mfree(&w->world->alloc, buf, sn * ui + 1);
        unc0_unwake(en, w);
        return e;
    }
    VINITENT(&v, Unc_TString, en);
    return unc_returnlocal(w, 0, &v);
}

int unc0_gs_join_out_(Unc_Size n, const byte *s, void *udata) {
    struct unc0_strbuf *buf = udata;
    return unc0_strbuf_putn(buf, n, s);
}

Unc_RetVal unc0_gs_join(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    int addsep = 0;
    Unc_Value v = UNC_BLANK;
    Unc_Value iter = UNC_BLANK;
    Unc_Pile pile;
    Unc_Tuple tuple;
    Unc_Size sepn;
    const byte *sep;
    struct unc0_strbuf buf;

    e = unc0_vgetiter(w, &iter, &args.values[1]);
    if (e) return e;

    e = unc_getstring(w, &args.values[0], &sepn, (const char **)&sep);
    if (e) return e;
    
    unc0_strbuf_init(&buf, &w->world->alloc, Unc_AllocString);

    do {
        e = unc_call(w, &iter, 0, &pile);
        if (e) goto fail;
        unc_returnvalues(w, &pile, &tuple);
        if (tuple.count) {
            if (!addsep)
                addsep = 1;
            else
                e = unc0_strbuf_putn(&buf, sepn, sep);
            if (!e)
                e = unc0_vcvt2str(w, &tuple.values[0],
                                  &unc0_gs_join_out_, &buf);
            if (e) goto fail;
        }
        unc_discard(w, &pile);
    } while (tuple.count);

    VCLEAR(w, &iter);
    e = unc0_buftostring(w, &v, &buf);
    if (!e) return unc_returnlocal(w, 0, &v);

fail:
    VCLEAR(w, &iter);
    unc0_strbuf_free(&buf);
    return e;
}

Unc_RetVal unc0_gs_replace(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    int dir = 0;
    Unc_Value v = UNC_BLANK;
    Unc_Size srcn, findn, dstn, maxrepl;
    const byte *src, *find, *dst;
    struct unc0_strbuf buf;

    e = unc_getstring(w, &args.values[0], &srcn, (const char **)&src);
    if (e) return e;

    e = unc_getstring(w, &args.values[1], &findn, (const char **)&find);
    if (e) return e;

    e = unc_getstring(w, &args.values[2], &dstn, (const char **)&dst);
    if (e) return e;

    if (unc_gettype(w, &args.values[3])) {
        Unc_Int ui;
        e = unc_getint(w, &args.values[3], &ui);
        if (e) return e;
        dir = ui < 0;
        if (ui < 0)
            maxrepl = (Unc_Size)-ui;
        else
            maxrepl = (Unc_Size)ui;
    } else
        maxrepl = UNC_SIZE_MAX;

    if (!maxrepl)
        return unc_returnlocal(w, 0, &args.values[0]);

    unc0_strbuf_init(&buf, &w->world->alloc, Unc_AllocString);

    if (!findn) {
        if (dir) {
            e = unc0_strbuf_putn_rv(&buf, dstn, dst);
            if (e) goto unc0_gs_replace_fail;
            while (srcn && --maxrepl) {
                const byte *se = src + srcn,
                           *srcp = unc0_utf8scanbackw(src, se, 0);
                if (!srcp)
                    srcn = 0;
                else {
                    e = unc0_strbuf_putn_rv(&buf, se - srcp, srcp);
                    if (e) goto unc0_gs_replace_fail;
                    srcn = srcp - src;
                }
                e = unc0_strbuf_putn_rv(&buf, dstn, dst);
                if (e) goto unc0_gs_replace_fail;
            }
            if (srcn) {
                e = unc0_strbuf_putn_rv(&buf, srcn, src);
                if (e) goto unc0_gs_replace_fail;
            }
            unc0_memrev(buf.buffer, buf.length);
        } else {
            e = unc0_strbuf_putn(&buf, dstn, dst);
            if (e) goto unc0_gs_replace_fail;
            while (srcn && --maxrepl) {
                const byte *se = src + srcn,
                           *srcp = unc0_utf8scanforw(src, se, 1);
                if (!srcp) srcp = se;
                e = unc0_strbuf_putn(&buf, srcp - src, src);
                if (e) goto unc0_gs_replace_fail;
                srcn -= srcp - src;
                src = srcp;
                e = unc0_strbuf_putn(&buf, dstn, dst);
                if (e) goto unc0_gs_replace_fail;
            }
            if (srcn) {
                e = unc0_strbuf_putn(&buf, srcn, src);
                if (e) goto unc0_gs_replace_fail;
            }
        }
    } else {
        if (dir) {
            while (srcn && maxrepl) {
                const byte *se = src + srcn,
                           *sq = unc0_strsearchr(src, srcn, find, findn),
                           *sr;
                if (!sq) break;
                sr = sq + findn;
                e = unc0_strbuf_putn_rv(&buf, se - sr, sr);
                if (e) goto unc0_gs_replace_fail;
                e = unc0_strbuf_putn_rv(&buf, dstn, dst);
                if (e) goto unc0_gs_replace_fail;
                srcn = sq - src;
                --maxrepl;
            }
            if (srcn) {
                e = unc0_strbuf_putn_rv(&buf, srcn, src);
                if (e) goto unc0_gs_replace_fail;
            }
            unc0_memrev(buf.buffer, buf.length);
        } else {
            while (srcn && maxrepl) {
                const byte *sq = unc0_strsearch(src, srcn, find, findn), *sr;
                if (!sq) break;
                sr = sq + findn;
                e = unc0_strbuf_putn(&buf, sq - src, src);
                if (e) goto unc0_gs_replace_fail;
                e = unc0_strbuf_putn(&buf, dstn, dst);
                if (e) goto unc0_gs_replace_fail;
                srcn -= sr - src;
                src = sr;
                --maxrepl;
            }
            if (srcn) {
                e = unc0_strbuf_putn(&buf, srcn, src);
                if (e) goto unc0_gs_replace_fail;
            }
        }
    }

    e = unc0_buftostring(w, &v, &buf);
    if (!e) return unc_returnlocal(w, 0, &v);

unc0_gs_replace_fail:
    unc0_strbuf_free(&buf);
    return e;
}

Unc_RetVal unc0_gs_split(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    int dir = 0;
    Unc_Value arr = UNC_BLANK, v = UNC_BLANK;
    Unc_Size srcn, findn, dstn, maxrepl;
    const byte *src, *find;
    Unc_Value *dst;

    e = unc_getstring(w, &args.values[0], &srcn, (const char **)&src);
    if (e) return e;

    e = unc_getstring(w, &args.values[1], &findn, (const char **)&find);
    if (e) return e;

    if (unc_gettype(w, &args.values[2])) {
        Unc_Int ui;
        e = unc_getint(w, &args.values[2], &ui);
        if (e) return e;
        dir = ui < 0;
        if (ui < 0)
            maxrepl = (Unc_Size)-ui;
        else
            maxrepl = (Unc_Size)ui;
    } else
        maxrepl = UNC_SIZE_MAX;

    dstn = 0;
    e = unc_newarray(w, &arr, dstn, &dst);
    if (e) return e;
    
    if (!findn) {
        if (dir) {
            while (srcn && maxrepl--) {
                const byte *se = src + srcn,
                           *srcp = unc0_utf8scanbackw(src, se, 0);
                if (!srcp)
                    srcn = 0;
                else {
                    e = unc_newstring(w, &v, se - srcp, (const char *)srcp);
                    if (e) goto unc0_gs_split_fail;
                    e = unc_resizearray(w, &arr, dstn + 1, &dst);
                    if (e) goto unc0_gs_split_fail;
                    unc_copy(w, &dst[dstn++], &v);
                    srcn = srcp - src;
                }
            }
            if (srcn) {
                e = unc_newstring(w, &v, srcn, (const char *)src);
                if (e) goto unc0_gs_split_fail;
                e = unc_resizearray(w, &arr, dstn + 1, &dst);
                if (e) goto unc0_gs_split_fail;
                unc_copy(w, &dst[dstn++], &v);
            }
        } else {
            while (srcn && maxrepl--) {
                const byte *se = src + srcn,
                           *srcp = unc0_utf8scanforw(src, se, 1);
                if (!srcp) srcp = se;
                e = unc_newstring(w, &v, srcp - src, (const char *)src);
                if (e) goto unc0_gs_split_fail;
                e = unc_resizearray(w, &arr, dstn + 1, &dst);
                if (e) goto unc0_gs_split_fail;
                unc_copy(w, &dst[dstn++], &v);
                srcn -= srcp - src;
                src = srcp;
            }
            if (srcn) {
                e = unc_newstring(w, &v, srcn, (const char *)src);
                if (e) goto unc0_gs_split_fail;
                e = unc_resizearray(w, &arr, dstn + 1, &dst);
                if (e) goto unc0_gs_split_fail;
                unc_copy(w, &dst[dstn++], &v);
            }
        }
    } else {
        if (dir) {
            while (srcn && maxrepl) {
                const byte *se = src + srcn,
                           *sq = unc0_strsearchr(src, srcn, find, findn),
                           *sr;
                if (!sq) break;
                sr = sq + findn;
                e = unc_newstring(w, &v, se - sr, (const char *)sr);
                if (e) goto unc0_gs_split_fail;
                e = unc_resizearray(w, &arr, dstn + 1, &dst);
                if (e) goto unc0_gs_split_fail;
                unc_copy(w, &dst[dstn++], &v);
                srcn = sq - src;
                --maxrepl;
            }
            e = unc_newstring(w, &v, srcn, (const char *)src);
            if (e) goto unc0_gs_split_fail;
            e = unc_resizearray(w, &arr, dstn + 1, &dst);
            if (e) goto unc0_gs_split_fail;
            unc_copy(w, &dst[dstn++], &v);
        } else {
            while (srcn && maxrepl) {
                const byte *sq = unc0_strsearch(src, srcn, find, findn), *sr;
                if (!sq) break;
                sr = sq + findn;
                e = unc_newstring(w, &v, sq - src, (const char *)src);
                if (e) goto unc0_gs_split_fail;
                e = unc_resizearray(w, &arr, dstn + 1, &dst);
                if (e) goto unc0_gs_split_fail;
                unc_copy(w, &dst[dstn++], &v);
                srcn -= sr - src;
                src = sr;
                --maxrepl;
            }
            e = unc_newstring(w, &v, srcn, (const char *)src);
            if (e) goto unc0_gs_split_fail;
            e = unc_resizearray(w, &arr, dstn + 1, &dst);
            if (e) goto unc0_gs_split_fail;
            unc_copy(w, &dst[dstn++], &v);
        }
    }

    if (dir) /* reverse array */
        unc0_reversevalues(dst, dstn);

    VCLEAR(w, &v);
    unc_unlock(w, &arr);
    return unc_returnlocal(w, 0, &arr);
unc0_gs_split_fail:
    VCLEAR(w, &v);
    unc_unlock(w, &arr);
    unc_decref(w, &arr);
    return e;
}

Unc_RetVal unc0_gb_new(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Size sn;
    byte *sp;
    Unc_Int ui;
    Unc_Value v = UNC_BLANK;

    ASSERT(args.count == 1);
    e = unc_getint(w, &args.values[0], &ui);
    if (e) return e;
    if (ui < 0)
        return unc0_throwexc(w, "value", "blob size cannot be negative");
    sn = (Unc_Size)ui;
    e = unc_newblob(w, &v, sn, &sp);
    if (e) return e;
    unc0_mbzero(sp, sn);
    unc_unlock(w, &v);
    return unc_returnlocal(w, 0, &v);
}

Unc_RetVal unc0_gb_from(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Size sn, i;
    byte *sp;
    Unc_Int ui;
    Unc_Value v = UNC_BLANK;

    if (args.count == 1 && args.values[0].type == Unc_TBlob) {
        e = unc_lockblob(w, &args.values[0], &sn, &sp);
        if (e) return e;
        e = unc_newblobfrom(w, &v, sn, sp);
        unc_unlock(w, &args.values[0]);
        return unc_returnlocal(w, e, &v);
    } else if (args.count == 1 && args.values[0].type == Unc_TArray) {
        Unc_Value *ap;
        e = unc_lockarray(w, &args.values[0], &sn, &ap);
        if (e) return e;
        e = unc_newblob(w, &v, sn, &sp);
        if (e) {
            unc_unlock(w, &args.values[0]);
            return e;
        }
        for (i = 0; i < sn; ++i) {
            e = unc_getint(w, &ap[i], &ui);
            if (e) {
                unc_unlock(w, &v);
                unc_unlock(w, &args.values[0]);
                VDECREF(w, &v);
                return e;
            }
            if (ui < -128 || ui > 255) {
                unc_unlock(w, &v);
                unc_unlock(w, &args.values[0]);
                VDECREF(w, &v);
                return unc0_throwexc(w, "value",
                    "blob values must be valid bytes");
            }
            sp[i] = (byte)ui;
        }
        unc_unlock(w, &v);
        unc_unlock(w, &args.values[0]);
        return unc_returnlocal(w, 0, &v);
    } else if (args.count == 1 && args.values[0].type == Unc_TObject) {
        /* TODO: might be iterable? */
        return UNCIL_ERR_ARG_NOTITERABLE;
    } else {
        sn = args.count;
        e = unc_newblob(w, &v, sn, &sp);
        if (e) return e;
        for (i = 0; i < sn; ++i) {
            e = unc_getint(w, &args.values[i], &ui);
            if (e) {
                unc_unlock(w, &v);
                VDECREF(w, &v);
                return e;
            }
            if (ui < -128 || ui > 255) {
                unc_unlock(w, &v);
                VDECREF(w, &v);
                return unc0_throwexc(w, "value",
                    "blob values must be valid bytes");
            }
            sp[i] = (byte)ui;
        }
        unc_unlock(w, &v);
        return unc_returnlocal(w, 0, &v);
    }
}

Unc_RetVal unc0_gb_copy(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Size sn;
    byte *sp;
    Unc_Value v = UNC_BLANK;

    e = unc_lockblob(w, &args.values[0], &sn, &sp);
    if (e) return e;
    e = unc_newblobfrom(w, &v, sn, sp);
    unc_unlock(w, &args.values[0]);
    return unc_returnlocal(w, e, &v);
}

Unc_RetVal unc0_gb_size(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Size sn;
    Unc_Value v;

    ASSERT(args.count == 1);
    e = unc_getblobsize(w, &args.values[0], &sn);
    if (e) return e;
    VINITINT(&v, sn);
    return unc_returnlocal(w, 0, &v);
}

Unc_RetVal unc0_gb_find(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Size sn, sn2;
    byte *sp, *sp2;
    Unc_Int ui;
    Unc_Value v;

    e = unc_lockblob(w, &args.values[0], &sn, &sp);
    if (e) return e;
    e = unc_lockblob(w, &args.values[1], &sn2, &sp2);
    if (e) {
        unc_unlock(w, &args.values[0]);
        return e;
    }
    if (unc_gettype(w, &args.values[2])) {
        e = unc_getint(w, &args.values[2], &ui);
        if (e) {
            unc_unlock(w, &args.values[1]);
            unc_unlock(w, &args.values[0]);
            return e;
        }
        if (ui < 0)
            sp = -ui > (Unc_Int)sn ? NULL : sp + (sn + ui);
        else
            sp = ui > (Unc_Int)sn ? NULL : sp + ui;
    } else {
        ui = 0;
    }
    if (sp) {
        const byte *sq = unc0_strsearch(sp, sn, sp2, sn2);
        VINITINT(&v, sq ? (sq - sp) + (ui < 0 ? 0 : ui) : -1);
    } else
        VINITINT(&v, -1);
    
    unc_unlock(w, &args.values[1]);
    unc_unlock(w, &args.values[0]);
    return unc_returnlocal(w, 0, &v);
}

Unc_RetVal unc0_gb_findlast(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Size sn, sn2;
    byte *sp, *sp2;
    Unc_Int ui;
    Unc_Value v;

    e = unc_lockblob(w, &args.values[0], &sn, &sp);
    if (e) return e;
    e = unc_lockblob(w, &args.values[1], &sn2, &sp2);
    if (e) {
        unc_unlock(w, &args.values[0]);
        return e;
    }
    if (unc_gettype(w, &args.values[2])) {
        e = unc_getint(w, &args.values[2], &ui);
        if (e) {
            unc_unlock(w, &args.values[1]);
            unc_unlock(w, &args.values[0]);
            return e;
        }
        if (ui < 0)
            sn = -ui > (Unc_Int)sn ? 0 : sn - (Unc_Size)-ui;
        else
            sn = ui > (Unc_Int)sn ? 0 : sn - ui;
    } else {
        ui = 0;
    }
    if (sp) {
        const byte *sq = unc0_strsearchr(sp, sn, sp2, sn2);
        VINITINT(&v, sq ? (sq - sp) + (ui < 0 ? 0 : ui) : -1);
    } else
        VINITINT(&v, -1);
    
    unc_unlock(w, &args.values[1]);
    unc_unlock(w, &args.values[0]);
    return unc_returnlocal(w, 0, &v);
}

Unc_RetVal unc0_gb_reverse(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Size sn;
    byte *sp;

    ASSERT(args.count == 1);
    e = unc_lockblob(w, &args.values[0], &sn, &sp);
    if (e) return e;
    unc0_memrev(sp, sn);
    unc_unlock(w, &args.values[0]);
    return 0;
}

Unc_RetVal unc0_gb_sub(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Size sn;
    byte *sp;
    Unc_Int ui1, ui2;
    Unc_Value v = UNC_BLANK;

    e = unc_lockblob(w, &args.values[0], &sn, &sp);
    if (e) return e;
    e = unc_getint(w, &args.values[1], &ui1);
    if (e) {
        unc_unlock(w, &args.values[0]);
        return e;
    }
    if (ui1 < 0)
        sp = -ui1 > (Unc_Int)sn ? NULL : sp + (sn + ui1),
            sn = -ui1 > (Unc_Int)sn ? 0 : sn + ui1;
    else
        sp = ui1 > (Unc_Int)sn ? NULL : sp + ui1,
            sn = ui1 > (Unc_Int)sn ? 0 : sn - ui1;
    if (sp && unc_gettype(w, &args.values[2])) {
        e = unc_getint(w, &args.values[2], &ui2);
        if (e) {
            unc_unlock(w, &args.values[0]);
            return e;
        }
        if (ui2 > (Unc_Int)sn) ui2 = sn;
        if (ui2 >= 0) {
            if (ui2 <= ui1)
                e = unc_newblobfrom(w, &v, 0, NULL);
            else {
                e = unc_newblobfrom(w, &v, ui2 - ui1, sp);
            }
        } else {
            if ((Unc_Int)sn <= -ui2)
                e = unc_newblobfrom(w, &v, 0, NULL);
            else
                e = unc_newblobfrom(w, &v, sn + ui2, sp);
        }
    } else
        e = unc_newblobfrom(w, &v, sn, sp);
    unc_unlock(w, &args.values[0]);
    return unc_returnlocal(w, e, &v);
}

Unc_RetVal unc0_gb_fill(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Size sn;
    byte *sp, f;
    Unc_Int ui1, ui2;

    e = unc_getint(w, &args.values[1], &ui1);
    if (e) return e;
    if (ui1 < -128 || ui1 > 255) {
        return unc0_throwexc(w, "value",
            "fill value must be a vald byte");
    }
    f = (byte)ui1;
    e = unc_lockblob(w, &args.values[0], &sn, &sp);
    if (e) return e;
    e = unc_getint(w, &args.values[2], &ui1);
    if (e) {
        unc_unlock(w, &args.values[0]);
        return e;
    }
    if (ui1 < 0)
        sp = -ui1 > (Unc_Int)sn ? NULL : sp + (sn + ui1),
            sn = -ui1 > (Unc_Int)sn ? 0 : sn + ui1;
    else
        sp = ui1 > (Unc_Int)sn ? NULL : sp + ui1,
            sn = ui1 > (Unc_Int)sn ? 0 : sn - ui1;
    if (sp && unc_gettype(w, &args.values[3])) {
        e = unc_getint(w, &args.values[3], &ui2);
        if (e) {
            unc_unlock(w, &args.values[0]);
            return e;
        }
        if (ui2 >= 0) {
            if (ui2 > ui1)
                unc0_memset(sp, f, ui2 - ui1);
        } else {
            if ((Unc_Int)sn > -ui2)
                unc0_memset(sp, f, sn + ui2);
        }
    } else
        unc0_memset(sp, f, sn);
    unc_unlock(w, &args.values[0]);
    return 0;
}

Unc_RetVal unc0_gb_resize(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Size sn;
    byte *sp;
    Unc_Int ui;

    e = unc_lockblob(w, &args.values[0], &sn, &sp);
    if (e) return e;
    e = unc_getint(w, &args.values[1], &ui);
    if (e) {
        unc_unlock(w, &args.values[0]);
        return e;
    }
    if (ui < 0)
        return unc0_throwexc(w, "value", "blob size cannot be negative");
    e = unc_resizeblob(w, &args.values[0], (Unc_Size)ui, &sp);
    unc_unlock(w, &args.values[0]);
    if (e) return e;
    return 0;
}

Unc_RetVal unc0_gb_push(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Size bn, sn, i;
    byte *bp;
    Unc_Int ui;

    e = unc_lockblob(w, &args.values[0], &bn, &bp);
    if (e) return e;
    if (args.count == 2 && args.values[1].type == Unc_TBlob) {
        byte *sp;
        e = unc_lockblob(w, &args.values[1], &sn, &sp);
        if (e) goto bfail0;
        e = unc_resizeblob(w, &args.values[0], bn + sn, &bp);
        if (e) goto bfail1;
        unc0_memcpy(bp + bn, sp, sn);
bfail1:
        unc_unlock(w, &args.values[1]);
bfail0:
        unc_unlock(w, &args.values[0]);
        return e;
    } else if (args.count == 2 && args.values[1].type == Unc_TArray) {
        Unc_Value *ap;
        e = unc_lockarray(w, &args.values[1], &sn, &ap);
        if (e) goto afail0;
        e = unc_resizeblob(w, &args.values[0], bn + sn, &bp);
        if (e) goto afail1;
        for (i = 0; i < sn; ++i) {
            e = unc_getint(w, &ap[i], &ui);
            if (e) {
                unc_resizeblob(w, &args.values[0], bn, &bp);
                goto afail1;
            }
            if (ui < -128 || ui > 255) {
                unc_resizeblob(w, &args.values[0], bn, &bp);
                e = unc0_throwexc(w, "value",
                    "blob values must be valid bytes");
                goto afail1;
            }
            bp[bn + i] = (byte)ui;
        }
afail1:
        unc_unlock(w, &args.values[1]);
afail0:
        unc_unlock(w, &args.values[0]);
        return e;
    } else if (args.count == 2 && args.values[1].type == Unc_TObject) {
        /* might be iterable? */
        unc_unlock(w, &args.values[0]);
        return UNCIL_ERR_ARG_NOTITERABLE;
    } else {
        sn = args.count - 1;
        e = unc_resizeblob(w, &args.values[0], bn + sn, &bp);
        if (e) return e;
        for (i = 0; i < sn; ++i) {
            e = unc_getint(w, &args.values[1 + i], &ui);
            if (e) {
                unc_resizeblob(w, &args.values[0], bn, &bp);
                goto pfail;
            }
            if (ui < -128 || ui > 255) {
                unc_resizeblob(w, &args.values[0], bn, &bp);
                e = unc0_throwexc(w, "value",
                    "blob values must be valid bytes");
                goto pfail;
            }
            bp[bn + i] = (byte)ui;
        }
pfail:
        unc_unlock(w, &args.values[0]);
        return e;
    }
}

Unc_RetVal unc0_gb_insert(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Size bn, sn, i, j;
    byte *bp;
    Unc_Int ui, indx;

    e = unc_lockblob(w, &args.values[0], &bn, &bp);
    if (e) return e;
    e = unc_getint(w, &args.values[1], &indx);
    if (e) {
        unc_unlock(w, &args.values[0]);
        return e;
    }
    if (indx < 0)
        indx += bn;
    if (indx < 0 || indx > (Unc_Int)bn) {
        unc_unlock(w, &args.values[0]);
        return UNCIL_ERR_ARG_INDEXOUTOFBOUNDS;
    }
    j = (Unc_Size)indx;
    if (args.count == 3 && args.values[2].type == Unc_TBlob) {
        Unc_RetVal e;
        unc_lockblob(w, &args.values[2], &bn, &bp);
        e = unc0_blobinsf(&w->world->alloc,
                             LEFTOVER(Unc_Blob, VGETENT(&args.values[0])),
                             indx,
                             LEFTOVER(Unc_Blob, VGETENT(&args.values[2])));
        unc_unlock(w, &args.values[2]);
        unc_unlock(w, &args.values[0]);
        return e;
    } else if (args.count == 3 && args.values[2].type == Unc_TArray) {
        Unc_Value *ap;
        e = unc_lockarray(w, &args.values[2], &sn, &ap);
        if (e) goto afail0;
        e = unc_resizeblob(w, &args.values[0], bn + sn, &bp);
        if (e) goto afail2;
        if (j < bn)
            unc0_memmove(bp + j + sn, bp + j, bn - j);
        for (i = 0; i < sn; ++i) {
            e = unc_getint(w, &ap[i], &ui);
            if (e) {
                if (j < bn)
                    unc0_memmove(bp + j, bp + j + sn, bn - j);
                unc_resizeblob(w, &args.values[0], bn, &bp);
                goto afail2;
            }
            if (ui < -128 || ui > 255) {
                if (j < bn)
                    unc0_memmove(bp + j, bp + j + sn, bn - j);
                unc_resizeblob(w, &args.values[0], bn, &bp);
                e = unc0_throwexc(w, "value",
                    "blob values must be valid bytes");
                goto afail2;
            }
            bp[j + i] = (byte)ui;
        }
afail2:
        unc_unlock(w, &args.values[2]);
afail0:
        unc_unlock(w, &args.values[0]);
        return e;
    } else if (args.count == 3 && args.values[2].type == Unc_TObject) {
        /* might be iterable? */
        unc_unlock(w, &args.values[0]);
        return UNCIL_ERR_ARG_NOTITERABLE;
    } else {
        sn = args.count - 2;
        e = unc_resizeblob(w, &args.values[0], bn + sn, &bp);
        if (e) {
            unc_unlock(w, &args.values[0]);
            return e;
        }
        if (j < bn)
            unc0_memmove(bp + j + sn, bp + j, bn - j);
        for (i = 0; i < sn; ++i) {
            e = unc_getint(w, &args.values[2 + i], &ui);
            if (e) {
                if (j < bn)
                    unc0_memmove(bp + j, bp + j + sn, bn - j);
                unc_resizeblob(w, &args.values[0], bn, &bp);
                unc_unlock(w, &args.values[0]);
                return e;
            }
            if (ui < -128 || ui > 255) {
                if (j < bn)
                    unc0_memmove(bp + j, bp + j + sn, bn - j);
                unc_resizeblob(w, &args.values[0], bn, &bp);
                unc_unlock(w, &args.values[0]);
                return unc0_throwexc(w, "value",
                    "blob values must be valid bytes");
            }
            bp[j + i] = (byte)ui;
        }
        unc_unlock(w, &args.values[0]);
        return 0;
    }
}

Unc_RetVal unc0_gb_remove(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Size bn;
    byte *bp;
    Unc_Int indx, cnt;

    e = unc_lockblob(w, &args.values[0], &bn, &bp);
    if (e) return e;
    e = unc_getint(w, &args.values[1], &indx);
    if (e) {
        unc_unlock(w, &args.values[0]);
        return e;
    }
    if (args.values[2].type) {
        e = unc_getint(w, &args.values[2], &cnt);
        if (e) {
            unc_unlock(w, &args.values[0]);
            return e;
        }
    } else
        cnt = 1;
    if (indx < 0)
        indx += bn;
    if (indx < 0 || indx > (Unc_Int)bn) {
        unc_unlock(w, &args.values[0]);
        return UNCIL_ERR_ARG_INDEXOUTOFBOUNDS;
    }
    if (cnt < 0) {
        unc_unlock(w, &args.values[0]);
        return unc0_throwexc(w, "value", "count must be non-negative");
    }
    if (indx + cnt > (Unc_Int)bn)
        cnt = bn - indx;
    if (!cnt)
        return 0;
    e = unc0_blobdel(&w->world->alloc,
                     LEFTOVER(Unc_Blob, VGETENT(&args.values[0])), indx, cnt);
    unc_unlock(w, &args.values[0]);
    return e;
}

Unc_RetVal unc0_gb_repeat(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Size sn;
    byte *sp;
    byte *buf;
    Unc_Value v;
    Unc_Int ui, ut;
    Unc_Entity *en;

    ASSERT(args.count == 2);
    e = unc_lockblob(w, &args.values[0], &sn, &sp);
    if (e) return e;
    e = unc_getint(w, &args.values[1], &ui);
    if (e) {
        unc_unlock(w, &args.values[0]);
        return e;
    }
    if (ui < 0) {
        unc_unlock(w, &args.values[0]);
        return unc0_throwexc(w, "value",
                    "cannot repeat a negative number of times");
    }
    
    if (ui) {
        buf = unc0_malloc(&w->world->alloc, Unc_AllocBlob, sn * ui);
        if (!buf) {
            unc_unlock(w, &args.values[0]);
            return UNCIL_ERR_MEM;
        }
    } else
        buf = NULL;
    
    en = unc0_wake(w, Unc_TBlob);
    if (!en) {
        unc_unlock(w, &args.values[0]);
        unc0_mfree(&w->world->alloc, buf, sn * ui);
        return UNCIL_ERR_MEM;
    }
    
    for (ut = 0; ut < ui; ++ut)
        unc0_memcpy(buf + ut * sn, sp, sn);

    e = unc0_initblobmove(&w->world->alloc,
                          LEFTOVER(Unc_Blob, en), sn * ui, buf);
    unc_unlock(w, &args.values[0]);
    if (e) {
        unc0_mfree(&w->world->alloc, buf, sn * ui);
        unc0_unwake(en, w);
        return e;
    }
    VINITENT(&v, Unc_TBlob, en);
    return unc_returnlocal(w, 0, &v);
}

Unc_RetVal unc0_ga_new(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Size sn;
    Unc_Int ui;
    Unc_Value v;

    e = unc_getint(w, &args.values[0], &ui);
    if (e) return e;
    if (ui < 0)
        return unc0_throwexc(w, "value", "array size cannot be negative");
    sn = (Unc_Size)ui;
    e = unc0_vrefnew(w, &v, Unc_TArray);
    if (e) return e;
    e = unc0_initarrayn(w, LEFTOVER(Unc_Array, VGETENT(&v)), sn);
    if (e) {
        unc0_unwake(VGETENT(&v), w);
        return e;
    } else if (unc_gettype(w, &args.values[1])) {
        Unc_Size i, an;
        Unc_Value *av;
        e = unc_lockarray(w, &v, &an, &av);
        if (e) {
            unc0_unwake(VGETENT(&v), w);
            return e;
        }
        for (i = 0; i < an; ++i)
            VIMPOSE(w, &av[i], &args.values[1]);
        unc_unlock(w, &v);
    }
    return unc_returnlocal(w, 0, &v);
}

Unc_RetVal unc0_ga_copy(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Size sn;
    Unc_Value *sp;
    Unc_Value v = UNC_BLANK;

    e = unc_lockarray(w, &args.values[0], &sn, &sp);
    if (e) return e;
    e = unc_newarrayfrom(w, &v, sn, sp);
    unc_unlock(w, &args.values[0]);
    return unc_returnlocal(w, e, &v);
}

Unc_RetVal unc0_ga_length(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Size sn;
    Unc_Value v;

    e = unc_getarraysize(w, &args.values[0], &sn);
    if (e) return e;
    VINITINT(&v, (Unc_Int)sn);
    return unc_returnlocal(w, 0, &v);
}

Unc_RetVal unc0_ga_clear(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Size sn;
    Unc_Value *sp;

    e = unc_lockarray(w, &args.values[0], &sn, &sp);
    if (e) return e;
    e = unc0_arraydel(w,
                LEFTOVER(Unc_Array, VGETENT(&args.values[0])), 0, sn);
    unc_unlock(w, &args.values[0]);
    return e;
}

Unc_RetVal unc0_ga_find(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Size i, sn;
    Unc_Value *sp;
    Unc_Int ui;
    Unc_Value v, comp;

    e = unc_lockarray(w, &args.values[0], &sn, &sp);
    if (e) return e;
    comp = args.values[1];
    if (unc_gettype(w, &args.values[2])) {
        e = unc_getint(w, &args.values[2], &ui);
        if (e) {
            unc_unlock(w, &args.values[0]);
            return e;
        }
        if (ui < 0)
            ui += sn;
    } else {
        ui = 0;
    }
    
    VINITINT(&v, -1);
    for (i = (Unc_Size)ui; i < sn; ++i) {
        e = unc0_vveq(w, &comp, &sp[i]);
        if (UNCIL_IS_ERR(e)) {
            unc_unlock(w, &args.values[0]);
            return e;
        }
        if (e) {
            VINITINT(&v, i);
            break;
        }
    }

    unc_unlock(w, &args.values[0]);
    return unc_returnlocal(w, 0, &v);
}

Unc_RetVal unc0_ga_findlast(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Size i, sn;
    Unc_Value *sp;
    Unc_Int ui;
    Unc_Value v, comp;

    e = unc_lockarray(w, &args.values[0], &sn, &sp);
    if (e) return e;
    comp = args.values[1];
    if (unc_gettype(w, &args.values[2])) {
        e = unc_getint(w, &args.values[2], &ui);
        if (e) {
            unc_unlock(w, &args.values[0]);
            return e;
        }
        if (ui < 0)
            ui += sn;
        if (ui >= (Unc_Int)sn)
            ui = sn;
    } else {
        ui = sn;
    }
    
    VINITINT(&v, -1);
    i = ui + 1;
    do {
        --i;
        e = unc0_vveq(w, &comp, &sp[i]);
        if (UNCIL_IS_ERR(e)) {
            unc_unlock(w, &args.values[0]);
            return e;
        }
        if (e) {
            VINITINT(&v, i);
            break;
        }
    } while (i);

    unc_unlock(w, &args.values[0]);
    return unc_returnlocal(w, 0, &v);
}

Unc_RetVal unc0_ga_push(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Size sn;
    Unc_Value *sp;

    e = unc_lockarray(w, &args.values[0], &sn, &sp);
    if (e) return e;
    e = unc0_arraycatr(w, LEFTOVER(Unc_Array, VGETENT(&args.values[0])),
                          args.count - 1, &args.values[1]);
    unc_unlock(w, &args.values[0]);
    return e;
}

Unc_RetVal unc0_ga_extend(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Size sn, sn2;
    Unc_Value *sp, *sp2;

    e = unc_lockarray(w, &args.values[0], &sn, &sp);
    if (e) return e;
    e = unc_lockarray(w, &args.values[1], &sn2, &sp2);
    if (e) goto fail0;
    e = unc0_arraycat(w, LEFTOVER(Unc_Array, VGETENT(&args.values[0])),
                         LEFTOVER(Unc_Array, VGETENT(&args.values[1])));
    unc_unlock(w, &args.values[1]);
fail0:
    unc_unlock(w, &args.values[0]);
    return e;
}

Unc_RetVal unc0_ga_pop(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Size sn;
    Unc_Value *sp;

    e = unc_lockarray(w, &args.values[0], &sn, &sp);
    if (e) return e;
    if (!sn) {
        unc_unlock(w, &args.values[0]);
        return unc0_throwexc(w, "value", "array is empty");
    }

    e = unc_pushmove(w, &sp[sn - 1]);
    if (!e) LEFTOVER(Unc_Array, VGETENT(&args.values[0]))->size--;
    unc_unlock(w, &args.values[0]);
    return e;
}

Unc_RetVal unc0_ga_insert(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Size sn, j;
    Unc_Value *sp;
    Unc_Int indx;

    e = unc_lockarray(w, &args.values[0], &sn, &sp);
    if (e) return e;
    e = unc_getint(w, &args.values[1], &indx);
    if (e) {
        unc_unlock(w, &args.values[0]);
        return e;
    }
    if (indx < 0)
        indx += sn;
    if (indx < 0 || indx > (Unc_Int)sn) {
        unc_unlock(w, &args.values[0]);
        return UNCIL_ERR_ARG_INDEXOUTOFBOUNDS;
    }
    j = (Unc_Size)indx;
    e = unc0_arrayinsr(w, LEFTOVER(Unc_Array, VGETENT(&args.values[0])),
                          j, args.count - 2, &args.values[2]);
    unc_unlock(w, &args.values[0]);
    return e;
}

Unc_RetVal unc0_ga_remove(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Size sn;
    Unc_Value *sp;
    Unc_Int indx, cnt;

    e = unc_lockarray(w, &args.values[0], &sn, &sp);
    if (e) return e;
    e = unc_getint(w, &args.values[1], &indx);
    if (e) {
        unc_unlock(w, &args.values[0]);
        return e;
    }
    if (args.values[2].type) {
        e = unc_getint(w, &args.values[2], &cnt);
        if (e) {
            unc_unlock(w, &args.values[0]);
            return e;
        }
    } else
        cnt = 1;
    if (indx < 0)
        indx += sn;
    if (indx < 0 || indx > (Unc_Int)sn) {
        unc_unlock(w, &args.values[0]);
        return UNCIL_ERR_ARG_INDEXOUTOFBOUNDS;
    }
    if (cnt < 0) {
        unc_unlock(w, &args.values[0]);
        return unc0_throwexc(w, "value", "count must be non-negative");
    }
    if (indx + cnt > (Unc_Int)sn)
        cnt = sn - indx;
    if (!cnt) {
        unc_unlock(w, &args.values[0]);
        return 0;
    }
    e = unc0_arraydel(w, LEFTOVER(Unc_Array, VGETENT(&args.values[0])),
                         indx, cnt);
    unc_unlock(w, &args.values[0]);
    return e;
}

Unc_RetVal unc0_ga_sub(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Size sn;
    Unc_Value *sp;
    Unc_Int ui1, ui2;
    Unc_Value v = UNC_BLANK;

    e = unc_lockarray(w, &args.values[0], &sn, &sp);
    if (e) return e;
    e = unc_getint(w, &args.values[1], &ui1);
    if (e) {
        unc_unlock(w, &args.values[0]);
        return e;
    }
    if (ui1 < 0)
        sp = -ui1 > (Unc_Int)sn ? NULL : sp + (sn + ui1),
            sn = -ui1 > (Unc_Int)sn ? 0 : sn + ui1;
    else
        sp = ui1 > (Unc_Int)sn ? NULL : sp + ui1,
            sn = ui1 > (Unc_Int)sn ? 0 : sn - ui1;
    if (sp && unc_gettype(w, &args.values[2])) {
        e = unc_getint(w, &args.values[2], &ui2);
        if (e) {
            unc_unlock(w, &args.values[0]);
            return e;
        }
        if (ui2 > (Unc_Int)sn) ui2 = sn;
        if (ui2 >= 0) {
            if (ui2 <= ui1)
                e = unc_newarrayfrom(w, &v, 0, NULL);
            else {
                e = unc_newarrayfrom(w, &v, ui2 - ui1, sp);
            }
        } else {
            if ((Unc_Int)sn <= -ui2)
                e = unc_newarrayfrom(w, &v, 0, NULL);
            else
                e = unc_newarrayfrom(w, &v, sn + ui2, sp);
        }
    } else
        e = unc_newarrayfrom(w, &v, sn, sp);
    unc_unlock(w, &args.values[0]);
    return unc_returnlocal(w, e, &v);
}

Unc_RetVal unc0_ga_sort(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Size sn;
    Unc_Value *sp;
    int hascomp = args.values[1].type;

    e = unc_lockarray(w, &args.values[0], &sn, &sp);
    if (e) return e;
    e = unc0_arrsort(w, hascomp ? &args.values[1] : NULL, sn, sp);
    unc_unlock(w, &args.values[0]);
    return e;
}

Unc_RetVal unc0_ga_repeat(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Size sn, s;
    Unc_Value v, *sp, *dp;
    Unc_Int ui, ut;
    Unc_Entity *en;

    ASSERT(args.count == 2);
    e = unc_lockarray(w, &args.values[0], &sn, &sp);
    if (e) return e;
    e = unc_getint(w, &args.values[1], &ui);
    if (e) {
        unc_unlock(w, &args.values[0]);
        return e;
    }
    if (ui < 0) {
        unc_unlock(w, &args.values[0]);
        return unc0_throwexc(w, "value",
                    "cannot repeat a negative number of times");
    }

    en = unc0_wake(w, Unc_TArray);
    if (!en) {
        unc_unlock(w, &args.values[0]);
        return UNCIL_ERR_MEM;
    }
    
    e = unc0_initarrayn(w, LEFTOVER(Unc_Array, en), sn * ui);
    if (e) {
        unc_unlock(w, &args.values[0]);
        unc0_unwake(en, w);
        return e;
    }

    dp = LEFTOVER(Unc_Array, en)->data;
    for (ut = 0; ut < ui; ++ut) {
        for (s = 0; s < sn; ++s) {
            VCOPY(w, dp++, &sp[s]);
        }
    }
    unc_unlock(w, &args.values[0]);

    VINITENT(&v, Unc_TArray, en);
    return unc_returnlocal(w, 0, &v);
}

Unc_RetVal unc0_ga_reverse(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Size sn;
    Unc_Value *sp;

    ASSERT(args.count == 1);
    e = unc_lockarray(w, &args.values[0], &sn, &sp);
    if (e) return e;
    
    unc0_reversevalues(sp, sn);
    unc_unlock(w, &args.values[0]);
    return 0;
}

Unc_RetVal unc0_gd_length(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value v;

    if (args.values[0].type != Unc_TTable)
        return UNCIL_ERR_TYPE_NOTDICT;
    VINITINT(&v, unc0_dgetsize(w,
            LEFTOVER(Unc_Dict, VGETENT(&args.values[0]))));
    return unc_returnlocal(w, 0, &v);
}

Unc_RetVal unc0_gd_copy(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Size i;
    Unc_Value v;
    Unc_Dict *d, *dict;
    Unc_HTblV_V *nx;
    
    if (args.values[0].type != Unc_TTable)
        return UNCIL_ERR_TYPE_NOTDICT;
    e = unc_newtable(w, &v);
    if (e) return e;

    d = LEFTOVER(Unc_Dict, VGETENT(&v));
    dict = LEFTOVER(Unc_Dict, VGETENT(&args.values[0]));
    UNC_LOCKL(dict->lock);
    i = -1;
    nx = NULL;

    for (;;) {
        while (!nx) {
            if (++i >= dict->data.capacity) {
                UNC_UNLOCKL(dict->lock);
                return unc_returnlocal(w, 0, &v);
            }
            nx = dict->data.buckets[i];
        }
        e = unc0_dsetindx(w, d, &nx->key, &nx->val);
        if (e) {
            UNC_UNLOCKL(dict->lock);
            unc0_hibernate(VGETENT(&v), w);
            return e;
        }
        nx = nx->next;
    }
}

Unc_RetVal unc0_gd_prune(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    int pruned = 0;
    Unc_Size gen, i;
    Unc_Dict *dict;
    Unc_HTblV_V *nx, **prev = NULL;

    if (args.values[0].type != Unc_TTable)
        return UNCIL_ERR_TYPE_NOTDICT;
    dict = LEFTOVER(Unc_Dict, VGETENT(&args.values[0]));
    i = -1;
    gen = dict->generation;
    nx = NULL;

    for (;;) {
        int prune;
        while (!nx) {
            if (++i >= dict->data.capacity) {
                if (pruned)
                    unc0_compacthtblv(w, &dict->data);
                return 0;
            }
            prev = &dict->data.buckets[i];
            nx = *prev;
        }
        {
            Unc_Pile pile;
            Unc_Tuple tuple;
            e = unc_push(w, 1, &nx->key);
            if (e) return e;
            e = unc_push(w, 1, &nx->val);
            if (e) return e;
            e = unc_call(w, &args.values[1], 2, &pile);
            if (e) return e;
            unc_returnvalues(w, &pile, &tuple);
            prune = tuple.count && unc0_vcvt2bool(w, &tuple.values[0]);
            unc_discard(w, &pile);
            if (gen != dict->generation)
                return unc0_throwexc(w, "value",
                            "table modified by other code while iterating");
        }
        if (prune) {
            Unc_HTblV_V *nxx = nx->next;
            VDECREF(w, &nx->key);
            VDECREF(w, &nx->val);
            unc0_mfree(&w->world->alloc, nx, sizeof(Unc_HTblV_V));
            *prev = nx = nxx;
            if (++dict->generation == UNC_INT_MAX)
                dict->generation = 0;
            gen = dict->generation;
            pruned = 1;
        } else {
            prev = &nx->next;
            nx = nx->next;
        }
    }
}

Unc_RetVal unc0_iter_string(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value *arr, *indx;
    Unc_Int i;
    Unc_RetVal e;
    Unc_Size s;
    const char *ss;

    (void)udata;
    ASSERT(unc_boundcount(w) == 2);
    arr = unc_boundvalue(w, 0);
    indx = unc_boundvalue(w, 1);
    e = unc_getstring(w, arr, &s, &ss);
    if (!e)
        e = unc_getint(w, indx, &i);
    if (!e && i < (Unc_Int)s) {
        Unc_Value v = unc_blank;
        e = unc_getindex(w, arr, indx, &v);
        if (!e) {
            e = unc_returnlocal(w, e, &v);
            if (!e) e = unc_push(w, 1, indx);
            unc_setint(w, indx, i + 1);
        }
    }

    return e;
}

Unc_RetVal unc0_iter_blob(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value *arr, *indx;
    Unc_Int i;
    Unc_RetVal e;
    Unc_Size s;

    (void)udata;
    ASSERT(unc_boundcount(w) == 2);
    arr = unc_boundvalue(w, 0);
    indx = unc_boundvalue(w, 1);
    e = unc_getblobsize(w, arr, &s);
    if (!e)
        e = unc_getint(w, indx, &i);
    if (!e && i < (Unc_Int)s) {
        Unc_Value v = unc_blank;
        e = unc_getindex(w, arr, indx, &v);
        if (!e) {
            e = unc_returnlocal(w, e, &v);
            if (!e) e = unc_push(w, 1, indx);
            unc_setint(w, indx, i + 1);
        }
    }

    return e;
}

Unc_RetVal unc0_iter_array(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value *arr, *indx;
    Unc_Int i;
    Unc_RetVal e;
    Unc_Size s;

    (void)udata;
    ASSERT(unc_boundcount(w) == 2);
    arr = unc_boundvalue(w, 0);
    indx = unc_boundvalue(w, 1);
    e = unc_getarraysize(w, arr, &s);
    if (!e)
        e = unc_getint(w, indx, &i);
    if (!e && i < (Unc_Int)s) {
        Unc_Value v = unc_blank;
        e = unc_getindex(w, arr, indx, &v);
        if (!e) {
            e = unc_returnlocal(w, e, &v);
            if (!e) e = unc_push(w, 1, indx);
            unc_setint(w, indx, i + 1);
        }
    }

    return e;
}

Unc_RetVal unc0_iter_table(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value *tbl;
    Unc_Int bucket, gen;
    Unc_Dict *dict;
    Unc_RetVal e;
    void *vp;

    (void)udata;
    ASSERT(unc_boundcount(w) == 4);
    tbl = unc_boundvalue(w, 0);
    if (tbl->type != Unc_TTable) return 0;
    e = unc_getint(w, unc_boundvalue(w, 1), &bucket);
    if (e) return e;
    e = unc_getopaqueptr(w, unc_boundvalue(w, 2), &vp);
    if (e) return e;
    e = unc_getint(w, unc_boundvalue(w, 3), &gen);
    if (e) return e;
    dict = LEFTOVER(Unc_Dict, VGETENT(tbl));
    if (gen != (Unc_Int)dict->generation) {
        unc_setnull(w, unc_boundvalue(w, 0));
        return unc0_throwexc(w, "value", "table modified while iterating");
    }

    if (!vp) {
        do {
            ++bucket;
            if (bucket >= (Unc_Int)dict->data.capacity) {
                unc_setnull(w, unc_boundvalue(w, 0));
                return 0;
            }
            vp = dict->data.buckets[bucket];
        } while (!vp);
        unc_setint(w, unc_boundvalue(w, 1), bucket);
    }

    {
        Unc_HTblV_V *dp = (Unc_HTblV_V *)vp;
        e = unc_push(w, 1, &dp->key);
        if (e) return e;
        e = unc_push(w, 1, &dp->val);
        if (e) return e;
        vp = dp->next;
    }
    unc_setopaqueptr(w, unc_boundvalue(w, 2), vp);
    return 0;
}

static Unc_RetVal unc0_newcallableobject(Unc_View *v, Unc_Value *o,
            const char *sn, Unc_Size argcount, Unc_Size optcount,
            int ellipsis, int cflags, Unc_CFunc fp) {
    Unc_Value f = unc_blank;
    Unc_Value p = unc_blank;
    Unc_RetVal e = unc_newcfunction(v, &f, fp, argcount, optcount, ellipsis,
                             cflags, NULL, 0, NULL, 0, NULL, sn, NULL);
    if (e) return e;
    e = unc_newobject(v, &p, NULL);
    if (e) return e;
    e = unc0_osetattrs(v, LEFTOVER(Unc_Object, VGETENT(&p)),
                       PASSSTRL(OPOVERLOAD(call)), &f);
    if (e) return e;
    e = unc_newobject(v, o, &p);
    VCLEAR(v, &f);
    VCLEAR(v, &p);
    return e;
}

INLINE Unc_Float get_trueeps(void) {
    Unc_Float uf = UNC_FLOAT_MIN, nf;
    while ((nf = uf / 2) > 0)
        uf = nf;
    return uf;
}

static const Unc_ModuleCFunc lib_g[] = {
#if !UNCIL_SANDBOXED
    { &unc0_g_print,        "print",        0, 0, 1, UNC_CFUNC_DEFAULT },
    { &unc0_g_input,        "input",        0, 1, 0, UNC_CFUNC_DEFAULT },
#endif
    { &unc0_g_require,      "require",      1, 0, 0, UNC_CFUNC_DEFAULT },
    { &unc0_g_type,         "type",         1, 0, 0, UNC_CFUNC_CONCURRENT },
    { &unc0_g_throw,        "throw",        1, 1, 0, UNC_CFUNC_CONCURRENT },
    { &unc0_g_bool,         "bool",         1, 0, 0, UNC_CFUNC_CONCURRENT },
    { &unc0_g_object,       "object",       0, 3, 0, UNC_CFUNC_CONCURRENT },
    { &unc0_g_weakref,      "weakref",      1, 0, 0, UNC_CFUNC_DEFAULT },
    { &unc0_g_getprototype, "getprototype", 1, 0, 0, UNC_CFUNC_CONCURRENT },
    { &unc0_g_getversion,   "getversion",   0, 0, 0, UNC_CFUNC_CONCURRENT },
};

static const Unc_ModuleCFunc lib_g_str[] = {
    { &unc0_gs_length,      "length",       1, 0, 0, UNC_CFUNC_CONCURRENT },
    { &unc0_gs_size,        "size",         1, 0, 0, UNC_CFUNC_CONCURRENT },
    { &unc0_gs_find,        "find",         2, 1, 0, UNC_CFUNC_CONCURRENT },
    { &unc0_gs_findlast,    "findlast",     2, 1, 0, UNC_CFUNC_CONCURRENT },
    { &unc0_gs_sub,         "sub",          2, 1, 0, UNC_CFUNC_CONCURRENT },
    { &unc0_gs_charcode,    "charcode",     1, 1, 0, UNC_CFUNC_CONCURRENT },
    { &unc0_gs_char,        "char",         1, 0, 0, UNC_CFUNC_CONCURRENT },
    { &unc0_gs_reverse,     "reverse",      1, 0, 0, UNC_CFUNC_CONCURRENT },
    { &unc0_gs_repeat,      "repeat",       2, 0, 0, UNC_CFUNC_CONCURRENT },
    { &unc0_gs_asciilower,  "asciilower",   1, 0, 0, UNC_CFUNC_CONCURRENT },
    { &unc0_gs_asciiupper,  "asciiupper",   1, 0, 0, UNC_CFUNC_CONCURRENT },
    { &unc0_gs_join,        "join",         2, 0, 0, UNC_CFUNC_CONCURRENT },
    { &unc0_gs_replace,     "replace",      3, 1, 0, UNC_CFUNC_CONCURRENT },
    { &unc0_gs_split,       "split",        2, 1, 0, UNC_CFUNC_CONCURRENT },
};

static const Unc_ModuleCFunc lib_g_blob[] = {
    { &unc0_gb_new,         "new",          1, 0, 0, UNC_CFUNC_CONCURRENT },
    { &unc0_gb_from,        "from",         0, 0, 1, UNC_CFUNC_CONCURRENT },
    { &unc0_gb_copy,        "copy",         5, 0, 0, UNC_CFUNC_CONCURRENT },
    { &unc0_gb_size,        "length",       1, 0, 0, UNC_CFUNC_CONCURRENT },
    { &unc0_gb_size,        "size",         1, 0, 0, UNC_CFUNC_CONCURRENT },
    { &unc0_gb_find,        "find",         2, 1, 0, UNC_CFUNC_CONCURRENT },
    { &unc0_gb_findlast,    "findlast",     2, 1, 0, UNC_CFUNC_CONCURRENT },
    { &unc0_gb_sub,         "sub",          2, 1, 0, UNC_CFUNC_CONCURRENT },
    { &unc0_gb_resize,      "resize",       2, 0, 0, UNC_CFUNC_CONCURRENT },
    { &unc0_gb_reverse,     "reverse",      1, 0, 0, UNC_CFUNC_CONCURRENT },
    { &unc0_gb_fill,        "fill",         2, 2, 0, UNC_CFUNC_CONCURRENT },
    { &unc0_gb_repeat,      "repeat",       2, 0, 0, UNC_CFUNC_CONCURRENT },
    { &unc0_gb_push,        "push",         1, 0, 1, UNC_CFUNC_CONCURRENT },
    { &unc0_gb_insert,      "insert",       3, 0, 1, UNC_CFUNC_CONCURRENT },
    { &unc0_gb_remove,      "remove",       2, 1, 0, UNC_CFUNC_CONCURRENT },
};

static const Unc_ModuleCFunc lib_g_arr[] = {
    { &unc0_ga_new,         "new",          1, 1, 0, UNC_CFUNC_CONCURRENT },
    { &unc0_ga_copy,        "copy",         1, 0, 0, UNC_CFUNC_CONCURRENT },
    { &unc0_ga_length,      "length",       1, 0, 0, UNC_CFUNC_CONCURRENT },
    { &unc0_ga_clear,       "clear",        1, 0, 0, UNC_CFUNC_CONCURRENT },
    { &unc0_ga_push,        "push",         1, 0, 1, UNC_CFUNC_CONCURRENT },
    { &unc0_ga_extend,      "extend",       2, 0, 0, UNC_CFUNC_CONCURRENT },
    { &unc0_ga_pop,         "pop",          1, 0, 0, UNC_CFUNC_CONCURRENT },
    { &unc0_ga_find,        "find",         2, 1, 0, UNC_CFUNC_CONCURRENT },
    { &unc0_ga_findlast,    "findlast",     2, 1, 0, UNC_CFUNC_CONCURRENT },
    { &unc0_ga_sub,         "sub",          2, 1, 0, UNC_CFUNC_CONCURRENT },
    { &unc0_ga_insert,      "insert",       3, 0, 1, UNC_CFUNC_CONCURRENT },
    { &unc0_ga_remove,      "remove",       2, 1, 0, UNC_CFUNC_CONCURRENT },
    { &unc0_ga_reverse,     "reverse",      1, 0, 0, UNC_CFUNC_CONCURRENT },
    { &unc0_ga_repeat,      "repeat",       2, 0, 0, UNC_CFUNC_CONCURRENT },
    { &unc0_ga_sort,        "sort",         1, 1, 0, UNC_CFUNC_CONCURRENT },
};

static const Unc_ModuleCFunc lib_g_table[] = {
    { &unc0_gd_length,      "length",       1, 0, 0, UNC_CFUNC_CONCURRENT },
    { &unc0_gd_prune,       "prune",        2, 0, 0, UNC_CFUNC_CONCURRENT },
    { &unc0_gd_copy,        "copy",         1, 0, 0, UNC_CFUNC_CONCURRENT },
};

Unc_RetVal unc0_stdlibinit(Unc_World *ww, Unc_View *w) {
    /* unc_blank? good boy! */
    Unc_Value met_str = UNC_BLANK;
    Unc_Value met_blob = UNC_BLANK;
    Unc_Value met_arr = UNC_BLANK;
    Unc_Value met_table = UNC_BLANK;

    MUST(unc0_newcallableobject(w, &met_str, "string", 2, 0, 0,
                                UNC_CFUNC_CONCURRENT, &unc0_g_str));
    MUST(unc0_newcallableobject(w, &met_blob, "blob", 2, 0, 0,
                                UNC_CFUNC_CONCURRENT, &unc0_g_blob));
    MUST(unc0_newcallableobject(w, &met_arr, "array", 2, 0, 0,
                                UNC_CFUNC_CONCURRENT, &unc0_g_array));
    MUST(unc0_newcallableobject(w, &met_table, "table", 2, 0, 0,
                                UNC_CFUNC_CONCURRENT, &unc0_g_table));

    w->met_str = met_str;
    w->met_blob = met_blob;
    w->met_arr = met_arr;
    w->met_table = met_table;

    MUST(unc_attrcfunctions(w, &met_str, PASSARRAY(lib_g_str),
                            0, NULL, NULL));
    MUST(unc_attrcfunctions(w, &met_blob, PASSARRAY(lib_g_blob),
                            0, NULL, NULL));
    MUST(unc_attrcfunctions(w, &met_arr, PASSARRAY(lib_g_arr),
                            0, NULL, NULL));
    MUST(unc_attrcfunctions(w, &met_table, PASSARRAY(lib_g_table),
                            0, NULL, NULL));

    MUST(unc_setpublic(w, PASSSTRLC("string"), &met_str));
    MUST(unc_setpublic(w, PASSSTRLC("blob"), &met_blob));
    MUST(unc_setpublic(w, PASSSTRLC("array"), &met_arr));
    MUST(unc_setpublic(w, PASSSTRLC("table"), &met_table));

    {
        Unc_Value q = UNC_BLANK;
        Unc_Value tv;
        Unc_Object *obj;
        MUST(unc0_newcallableobject(w, &q, "int", 2, 0, 0,
                       UNC_CFUNC_CONCURRENT, &unc0_g_int));
        obj = LEFTOVER(Unc_Object, VGETENT(&q));

        VINITINT(&tv, UNC_INT_MIN);
        MUST(unc0_osetattrs(w, obj, PASSSTRL("min"), &tv));

        VINITINT(&tv, UNC_INT_MAX);
        MUST(unc0_osetattrs(w, obj, PASSSTRL("max"), &tv));

        MUST(unc_setpublic(w, PASSSTRLC("int"), &q));
        VCLEAR(w, &q);
    }

    {
        Unc_Value q = UNC_BLANK;
        Unc_Value tv;
        Unc_Object *obj;
        MUST(unc0_newcallableobject(w, &q, "float", 2, 0, 0,
                       UNC_CFUNC_CONCURRENT, &unc0_g_float));
        obj = LEFTOVER(Unc_Object, VGETENT(&q));

        VINITFLT(&tv, -UNC_FLOAT_MAX);
        MUST(unc0_osetattrs(w, obj, PASSSTRL("min"), &tv));

        VINITFLT(&tv, UNC_FLOAT_MAX);
        MUST(unc0_osetattrs(w, obj, PASSSTRL("max"), &tv));

        VINITFLT(&tv, get_trueeps());
        MUST(unc0_osetattrs(w, obj, PASSSTRL("eps"), &tv));

        VINITFLT(&tv, unc0_finfty());
        MUST(unc0_osetattrs(w, obj, PASSSTRL("inf"), &tv));

        VINITFLT(&tv, unc0_fnan());
        MUST(unc0_osetattrs(w, obj, PASSSTRL("nan"), &tv));

        MUST(unc_setpublic(w, PASSSTRLC("float"), &q));
        VCLEAR(w, &q);
    }

    MUST(unc_exportcfunctions(w, PASSARRAY(lib_g), 0, NULL, NULL));

    MUST(unc0_adddefaultencs(w, &ww->encs));

    VSETNULL(w, &met_str);
    VSETNULL(w, &met_blob);
    VSETNULL(w, &met_arr);
    VSETNULL(w, &met_table);
    w->entityload = 0;

    return 0;
}
