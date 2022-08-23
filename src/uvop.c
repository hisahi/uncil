/*******************************************************************************
 
Uncil -- value operation impl

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

#include "uarithm.h"
#include "uarr.h"
#include "ucommon.h"
#include "uctype.h"
#include "ublob.h"
#include "udebug.h"
#include "uobj.h"
#include "uopaque.h"
#include "ustr.h"
#include "uvali.h"
#include "uvm.h"
#include "uvop.h"
#include "uvsio.h"

int unc__vveq(Unc_View *w, Unc_Value *a, Unc_Value *b) {
    return unc__vveq_j(w, a, b);
}

int unc__vvclt(Unc_View *w, Unc_Value *a, Unc_Value *b) {
    return unc__vvclt_j(w, a, b);
}

int unc__vvcmp(Unc_View *w, Unc_Value *a, Unc_Value *b) {
    if (VGETTYPE(b) == Unc_TObject || VGETTYPE(b) == Unc_TOpaque)
        goto unc__vvcmp_o;
    switch (VGETTYPE(a)) {
    case Unc_TInt:
        switch (VGETTYPE(b)) {
        case Unc_TInt:
            return unc__cmpint(VGETINT(a), VGETINT(b));
        case Unc_TFloat:
            return unc__cmpflt(VGETINT(a), VGETFLT(b));
        default:
            return unc__err_unsup2(w, VGETTYPE(a), VGETTYPE(b));
        }
    case Unc_TFloat:
        switch (VGETTYPE(b)) {
        case Unc_TInt:
            return unc__cmpflt(VGETINT(a), VGETFLT(b));
        case Unc_TFloat:
            return unc__cmpflt(VGETFLT(a), VGETFLT(b));
        default:
            return unc__err_unsup2(w, VGETTYPE(a), VGETTYPE(b));
        }
    case Unc_TString:
        if (VGETTYPE(a) != VGETTYPE(b))
            return unc__err_unsup2(w, VGETTYPE(a), VGETTYPE(b));
        return unc__cmpstr(LEFTOVER(Unc_String, VGETENT(a)),
                           LEFTOVER(Unc_String, VGETENT(b)));
    case Unc_TBlob:
        if (VGETTYPE(a) != VGETTYPE(b))
            return unc__err_unsup2(w, VGETTYPE(a), VGETTYPE(b));
        return unc__cmpblob(LEFTOVER(Unc_Blob, VGETENT(a)),
                            LEFTOVER(Unc_Blob, VGETENT(b)));
    case Unc_TObject:
    case Unc_TOpaque:
unc__vvcmp_o:
    {
        int e;
        Unc_Value vout;
        e = unc__vovlbinary(w, a, b, &vout, PASSSTRL(OPOVERLOAD(cmp)),
                                            PASSSTRL(OPOVERLOAD(cmp2)));
        if (e) {
            switch (VGETTYPE(&vout)) {
            case Unc_TInt:
                return unc__cmpint(VGETINT(&vout), 0);
            case Unc_TFloat:
                return unc__cmpflt(VGETINT(&vout), 0);
            default:
                break;
            }
            /*
            if (UNCIL_IS_ERR(e)) return e;
            if (w->recurse >= w->recurselimit) {
                unc__decref(w, vout);
                return UNCIL_ERR_TOODEEP;
            }
            ++w->recurse;
            e = unc__vvclt(w, vout, unc__vint(0));
            --w->recurse;
            unc__decref(w, vout);
            return e;
            */
        }
    }
    default:
        return unc__err_unsup2(w, VGETTYPE(a), VGETTYPE(b));
    }
}

int unc__vgetattrf(Unc_View *w, Unc_Value *a, Unc_Size sl, const byte *sb,
                                    int q, Unc_Value *v) {
    switch (VGETTYPE(a)) {
    case Unc_TString:
        return unc__vgetattr(w, &w->world->met_str, sl, sb, q, v);
    case Unc_TBlob:
        return unc__vgetattr(w, &w->world->met_blob, sl, sb, q, v);
    case Unc_TArray:
        return unc__vgetattr(w, &w->world->met_arr, sl, sb, q, v);
    case Unc_TTable:
        return unc__vgetattr(w, &w->world->met_dict, sl, sb, q, v);
    case Unc_TObject:
    case Unc_TOpaque:
        return unc__vgetattr(w, a, sl, sb, q, v);
    default:
        return UNCIL_ERR_ARG_NOTATTRABLE;
    }
}

int unc__vgetattr(Unc_View *w, Unc_Value *a, Unc_Size sl, const byte *sb,
                                    int q, Unc_Value *v) {
    switch (VGETTYPE(a)) {
    case Unc_TTable: {
        Unc_Value *r;
        int e;
        e = unc__dgetattrs(w, LEFTOVER(Unc_Dict, VGETENT(a)), sl, sb, &r);
        if (e) return e;
        if (r)
            VCOPY(w, v, r);
        else {
            if (q)
                VSETNULL(w, v);
            else
                return UNCIL_ERR_ARG_NOSUCHATTR;
        }
        return 0;
    }
    case Unc_TObject: {
        Unc_Value *r;
        int e;
        e = unc__ogetattrs(w, LEFTOVER(Unc_Object, VGETENT(a)), sl, sb, &r);
        if (e) return e;
        if (r)
            VCOPY(w, v, r);
        else {
            if (q)
                VSETNULL(w, v);
            else
                return UNCIL_ERR_ARG_NOSUCHATTR;
        }
        return 0;
    }
    case Unc_TOpaque:
    {
        Unc_Opaque *oq = LEFTOVER(Unc_Opaque, VGETENT(a));
        if (VGETTYPE(&oq->prototype)) {
            int e;
            Unc_Value *o;
            e = unc__getprotomethod(w, a, sl, sb, &o);
            if (e)
                return e;
            else if (o)
                VCOPY(w, v, o);
            else {
                if (q)
                    VSETNULL(w, v);
                else
                    return UNCIL_ERR_ARG_NOSUCHATTR;
            }
            return 0;
        }
    }
    case Unc_TNull:
        if (q) {
            VSETNULL(w, v);
            return 0;
        }
    default:
        return UNCIL_ERR_ARG_NOTATTRABLE;
    }
}

int unc__vsetattr(Unc_View *w, Unc_Value *a, Unc_Size sl, const byte *sb,
                                    Unc_Value *v) {
    switch (VGETTYPE(a)) {
    case Unc_TTable:
        return unc__dsetattrs(w, LEFTOVER(Unc_Dict, VGETENT(a)), sl, sb, v);
    case Unc_TObject:
        return unc__osetattrs(w, LEFTOVER(Unc_Object, VGETENT(a)), sl, sb, v);
    default:
        return UNCIL_ERR_ARG_NOTATTRSETTABLE;
    }
}

int unc__vdelattr(Unc_View *w, Unc_Value *a, Unc_Size sl, const byte *sb) {
    switch (VGETTYPE(a)) {
    case Unc_TTable:
        return unc__ddelattrs(w, LEFTOVER(Unc_Dict, VGETENT(a)), sl, sb);
    case Unc_TObject:
        return unc__odelattrs(w, LEFTOVER(Unc_Object, VGETENT(a)), sl, sb);
    default:
        return UNCIL_ERR_ARG_NOTATTRDELETABLE;
    }
}

int unc__vgetattrv(Unc_View *w, Unc_Value *a, Unc_Value *i, int q,
                   Unc_Value *v) {
    switch (VGETTYPE(a)) {
    case Unc_TTable: {
        Unc_Value *r;
        int e;
        e = unc__dgetattrv(w, LEFTOVER(Unc_Dict, VGETENT(a)), i, &r);
        if (e) return e;
        if (r)
            VCOPY(w, v, r);
        else {
            if (q)
                VSETNULL(w, v);
            else
                return UNCIL_ERR_ARG_NOSUCHATTR;
        }
        return 0;
    }
    case Unc_TObject: {
        Unc_Value *r;
        int e;
        e = unc__ogetattrv(w, LEFTOVER(Unc_Object, VGETENT(a)), i, &r);
        if (e) return e;
        if (r)
            VCOPY(w, v, r);
        else {
            if (q)
                VSETNULL(w, v);
            else
                return UNCIL_ERR_ARG_NOSUCHATTR;
        }
        return 0;
    }
    case Unc_TOpaque:
    default:
        return UNCIL_ERR_ARG_NOTATTRABLE;
    }
}

int unc__vsetattrv(Unc_View *w, Unc_Value *a, Unc_Value *i, Unc_Value *v) {
    switch (VGETTYPE(a)) {
    case Unc_TTable:
        return unc__dsetattrv(w, LEFTOVER(Unc_Dict, VGETENT(a)), i, v);
    case Unc_TObject:
        return unc__osetattrv(w, LEFTOVER(Unc_Object, VGETENT(a)), i, v);
    default:
        return UNCIL_ERR_ARG_NOTATTRSETTABLE;
    }
}

int unc__vdelattrv(Unc_View *w, Unc_Value *a, Unc_Value *i) {
    switch (VGETTYPE(a)) {
    case Unc_TTable:
        return unc__ddelattrv(w, LEFTOVER(Unc_Dict, VGETENT(a)), i);
    case Unc_TObject:
        return unc__odelattrv(w, LEFTOVER(Unc_Object, VGETENT(a)), i);
    default:
        return UNCIL_ERR_ARG_NOTATTRDELETABLE;
    }
}

int unc__vgetindx(Unc_View *w, Unc_Value *a, Unc_Value *i,
                  int q, Unc_Value *v) {
    switch (VGETTYPE(a)) {
    case Unc_TString:
        return unc__sgetcodepat(w, LEFTOVER(Unc_String, VGETENT(a)), i, q, v);
    case Unc_TBlob:
        return unc__blgetbyte(w, LEFTOVER(Unc_Blob, VGETENT(a)), i, q, v);
    case Unc_TArray:
        return unc__agetindx(w, LEFTOVER(Unc_Array, VGETENT(a)), i, q, v);
    case Unc_TTable: {
        Unc_Value *r;
        int e;
        e = unc__dgetindx(w, LEFTOVER(Unc_Dict, VGETENT(a)), i, &r);
        if (e) return e;
        if (r)
            VCOPY(w, v, r);
        else {
            if (q)
                VSETNULL(w, v);
            else
                return UNCIL_ERR_ARG_NOSUCHATTR;
        }
        return 0;
    }
    case Unc_TObject: {
        Unc_Value *r;
        int e;
        e = unc__ogetindx(w, LEFTOVER(Unc_Object, VGETENT(a)), i, &r);
        if (e) return e;
        if (r)
            VCOPY(w, v, r);
        else {
            if (q)
                VSETNULL(w, v);
            else
                return UNCIL_ERR_ARG_NOSUCHATTR;
        }
        return 0;
    }
    default:
        return UNCIL_ERR_ARG_NOTINDEXABLE;
    }
}

int unc__vsetindx(Unc_View *w, Unc_Value *a, Unc_Value *i, Unc_Value *v) {
    switch (VGETTYPE(a)) {
    case Unc_TString:
        return UNCIL_ERR_ARG_CANNOTSETINDEX;
    case Unc_TBlob:
    {
        int e = unc__blsetbyte(w, LEFTOVER(Unc_Blob, VGETENT(a)), i, v);
        if (e == UNCIL_ERR_CONVERT_TOINT)
            return unc__throwexc(w, "type", "blob value must be a valid byte");
        return e;
    }
    case Unc_TArray:
        return unc__asetindx(w, LEFTOVER(Unc_Array, VGETENT(a)), i, v);
    case Unc_TTable:
        return unc__dsetindx(w, LEFTOVER(Unc_Dict, VGETENT(a)), i, v);
    case Unc_TObject:
        return unc__osetindx(w, LEFTOVER(Unc_Object, VGETENT(a)), i, v);
    default:
        return UNCIL_ERR_ARG_NOTINDEXABLE;
    }
}

int unc__vdelindx(Unc_View *w, Unc_Value *a, Unc_Value *i) {
    switch (VGETTYPE(a)) {
    case Unc_TString:
    case Unc_TBlob:
        return UNCIL_ERR_ARG_CANNOTDELETEINDEX;
    case Unc_TArray:
    {
        Unc_Value n = UNC_BLANK;
        return unc__asetindx(w, LEFTOVER(Unc_Array, VGETENT(a)), i, &n);
    }
    case Unc_TTable:
        return unc__ddelindx(w, LEFTOVER(Unc_Dict, VGETENT(a)), i);
    case Unc_TObject:
        return unc__odelindx(w, LEFTOVER(Unc_Object, VGETENT(a)), i);
    default:
        return UNCIL_ERR_ARG_NOTINDEXABLE;
    }
}

Unc_RetVal unc__iter_string(Unc_View *w, Unc_Tuple args, void *udata);
Unc_RetVal unc__iter_blob(Unc_View *w, Unc_Tuple args, void *udata);
Unc_RetVal unc__iter_array(Unc_View *w, Unc_Tuple args, void *udata);
Unc_RetVal unc__iter_table(Unc_View *w, Unc_Tuple args, void *udata);

int unc__vgetiter(Unc_View *w, Unc_Value *out, Unc_Value *in) {
    switch (VGETTYPE(in)) {
    case Unc_TString:
    {
        int e;
        Unc_Entity *en = unc__wake(w, Unc_TFunction);
        Unc_Value initvalues[2];
        if (!en) return UNCIL_ERR_MEM;
        
        VIMPOSE(w, &initvalues[0], in);
        VINITINT(&initvalues[1], 0);
        e = unc__initfuncc(w, LEFTOVER(Unc_Function, en), 
                    &unc__iter_string, 0, 0, 0,
                    0, NULL, 2, initvalues, 0, NULL,
                    "(string iterator)", NULL);
        if (e) {
            unc__unwake(en, w);
            return e;
        }
        VSETENT(w, out, Unc_TFunction, en);
        return 0;
    }
    case Unc_TBlob:
    {
        int e;
        Unc_Entity *en = unc__wake(w, Unc_TFunction);
        Unc_Value initvalues[2];
        if (!en) return UNCIL_ERR_MEM;
        
        VIMPOSE(w, &initvalues[0], in);
        VINITINT(&initvalues[1], 0);
        e = unc__initfuncc(w, LEFTOVER(Unc_Function, en), 
                    &unc__iter_blob, 0, 0, 0,
                    0, NULL, 2, initvalues, 0, NULL,
                    "(blob iterator)", NULL);
        if (e) {
            unc__unwake(en, w);
            return e;
        }
        VSETENT(w, out, Unc_TFunction, en);
        return 0;
    }
    case Unc_TArray:
    {
        int e;
        Unc_Entity *en = unc__wake(w, Unc_TFunction);
        Unc_Value initvalues[2];
        if (!en) return UNCIL_ERR_MEM;
        
        VIMPOSE(w, &initvalues[0], in);
        VINITINT(&initvalues[1], 0);
        e = unc__initfuncc(w, LEFTOVER(Unc_Function, en), 
                    &unc__iter_array, 0, 0, 0,
                    0, NULL, 2, initvalues, 0, NULL,
                    "(array iterator)", NULL);
        if (e) {
            unc__unwake(en, w);
            return e;
        }
        VSETENT(w, out, Unc_TFunction, en);
        return 0;
    }
    case Unc_TTable:
    {
        int e;
        Unc_Entity *en = unc__wake(w, Unc_TFunction);
        Unc_Value initvalues[4];
        if (!en) return UNCIL_ERR_MEM;
        
        VIMPOSE(w, &initvalues[0], in);
        VINITINT(&initvalues[1], -1);
        VINITPTR(&initvalues[2], NULL);
        VINITINT(&initvalues[3], LEFTOVER(Unc_Dict, VGETENT(in))->generation);
        e = unc__initfuncc(w, LEFTOVER(Unc_Function, en), 
                    &unc__iter_table, 0, 0, 0,
                    0, NULL, 4, initvalues, 0, NULL,
                    "(table iterator)", NULL);
        if (e) {
            unc__unwake(en, w);
            return e;
        }
        VSETENT(w, out, Unc_TFunction, en);
        return 0;
    }
    case Unc_TFunction:
        VCOPY(w, out, in);
        return 0;
    default:
        return UNCIL_ERR_ARG_NOTITERABLE;
    }
}

int unc__vcvt2int(Unc_View *w, Unc_Value *out, Unc_Value *in) {
    switch (VGETTYPE(in)) {
    case Unc_TInt:
        VCOPY(w, out, in);
        return 0;
    case Unc_TFloat:
#if UNC_INT_MAX & 1
        if (VGETFLT(in) >= (Unc_Float)((Unc_UInt)UNC_INT_MAX + 1))
#else
        if (VGETFLT(in) > UNC_INT_MAX)
#endif
            return unc__throwexc(w, "value",
                "float value is too large to fit into int");
        if (VGETFLT(in) < UNC_INT_MIN)
            return unc__throwexc(w, "value",
                "float value is too small to fit into int");
        if (VGETFLT(in) != VGETFLT(in))
            return unc__throwexc(w, "value",
                "cannot convert NaN into integer");
        VSETINT(w, out, (Unc_Int)VGETFLT(in));
        return 0;
    case Unc_TString:
    {
        Unc_Size sn;
        const byte *sb;
        Unc_String *s = LEFTOVER(Unc_String, VGETENT(in));
        Unc_Int ri = 0;
        sn = s->size;
        sb = unc__getstringdata(s);
        {
            int valid = 0;
            Unc_UInt i = 0, pi;
            while (sn && unc__isspace(*sb))
                ++sb, --sn;
            if (sn) {
                int sign = 0;
                if (*sb == '+')
                    ++sb, --sn;
                else if (*sb == '-')
                    sign = 1, ++sb, --sn;
                while (sn && unc__isdigit(*sb)) {
                    valid = 1;
                    pi = i;
                    i = i * 10 + (*sb - '0');
                    if (pi > i)
                        return unc__throwexc(w, "value",
                            "value does not fit into int");
                    ++sb, --sn;
                }
                if (sign) {
                    ri = -(Unc_Int)i;
                    if (ri >= 0)
                        return unc__throwexc(w, "value",
                            "value does not fit into int");
                } else if (i > UNC_INT_MAX)
                    return unc__throwexc(w, "value",
                        "value does not fit into int");
                else
                    ri = (Unc_Int)i;
            }
            while (sn && unc__isspace(*sb))
                ++sb, --sn;
            if (!valid || sn)
                return UNCIL_ERR_CONVERT_TOINT;
        }
        VSETINT(w, out, ri);
        return 0;
    }
    case Unc_TObject:
    case Unc_TOpaque:
    {
        Unc_Value vout;
        int e = unc__vovlunary(w, in, &vout,
                                PASSSTRL(OPOVERLOAD(int)));
        if (e) {
            if (UNCIL_IS_ERR(e)) return e;
            if (VGETTYPE(&vout) != Unc_TInt) {
                /*
                if (w->recurse >= w->recurselimit)
                    return UNCIL_ERR_TOODEEP;
                ++w->recurse;
                e = unc__vcvt2int(w, &vout, in);
                --w->recurse;
                if (e) return e;
                */
            } else {
                VMOVE(w, out, &vout);
                return 0;
            }
        }
        /*
        e = unc__vovlunary(w, in, &vout,
                                PASSSTRL(OPOVERLOAD(intval)));
        if (e) {
            if (UNCIL_IS_ERR(e)) return e;
            if (VGETTYPE(&vout) != Unc_TInt) {
                if (w->recurse >= w->recurselimit)
                    return UNCIL_ERR_TOODEEP;
                ++w->recurse;
                e = unc__vcvt2int(w, &vout, in);
                --w->recurse;
            }
            if (e) return e;
                TInt is a value type, it will never be collected
            unc__decref(w, vout);
            if (VGETTYPE(&vout) != Unc_TInt)
                return UNCIL_ERR_CONVERT_TOINT;
            *out = vout;
            return 0;
        }*/
    }
    default:
        return UNCIL_ERR_CONVERT_TOINT;
    }
}

int unc__vcvt2flt(Unc_View *w, Unc_Value *out, Unc_Value *in) {
    switch (VGETTYPE(in)) {
    case Unc_TInt:
        VSETFLT(w, out, (Unc_Float)VGETINT(in));
        return 0;
    case Unc_TFloat:
        VCOPY(w, out, in);
        return 0;
    case Unc_TString:
    {
        Unc_Size sn;
        const byte *sb;
        Unc_Float f;
        int n;
        Unc_String *s = LEFTOVER(Unc_String, VGETENT(in));
        sn = s->size;
        sb = unc__getstringdata(s);
        if (unc__sxscanf(sn, sb, "%" PRIUnc_Float"g %n", &f, &n) < 1)
            return UNCIL_ERR_CONVERT_TOFLOAT;
        if (n < sn)
            return UNCIL_ERR_CONVERT_TOFLOAT;
        VSETFLT(w, out, f);
        return 0;
    }
    case Unc_TObject:
    case Unc_TOpaque:
    {
        Unc_Value vout;
        int e = unc__vovlunary(w, in, &vout, PASSSTRL(OPOVERLOAD(float)));
        if (e) {
            if (UNCIL_IS_ERR(e)) return e;
            if (VGETTYPE(&vout) != Unc_TFloat) {
                /*
                if (w->recurse >= w->recurselimit)
                    return UNCIL_ERR_TOODEEP;
                ++w->recurse;
                e = unc__vcvt2int(w, &vout, in);
                --w->recurse;*/
            } else {
                VMOVE(w, out, &vout);
                return 0;
            }
        }
    }
    default:
        return UNCIL_ERR_CONVERT_TOFLOAT;
    }
}

int unc__vovlunary(Unc_View *w, Unc_Value *in,
                        Unc_Value *out, Unc_Size bn, const byte *bb) {
    int e;
    Unc_Value *fn;
    e = unc__getprotomethod(w, in, bn, bb, &fn);
    if (e) return e;
    if (fn) {
        Unc_Size d = unc__stackdepth(&w->sval);
        Unc_Pile ret;
        if ((e = unc__stackpush(w, &w->sval, 1, in))) {
            return e;
        }
        if ((e = unc_newpile(w, &ret))) {
            unc__restoredepth(w, &w->sval, d);
            return e;
        }
        w->region.base[ret.r] -= 1;
        e = unc__fcallv(w, fn, 1, 1, 1, 1, 0);
        if (!e)
            e = unc__run(w);
        else if (!UNCIL_IS_ERR(e))
            e = 0;
        if (e) {
            return e;
        } else {
            Unc_Tuple tuple;
            unc_returnvalues(w, &ret, &tuple);
            if (tuple.count == 1) {
                VIMPOSE(w, out, &tuple.values[0]);
                unc_discard(w, &ret);
                return 1;
            } else if (tuple.count > 1) {
                unc_discard(w, &ret);
                return UNCIL_ERR_LOGIC_OVLTOOMANY;
            }
        }
        unc_discard(w, &ret);
    }
    return 0;
}

int unc__vovlbinary(Unc_View *w, Unc_Value *a, Unc_Value *b,
                        Unc_Value *out, Unc_Size bn, const byte *bb,
                                        Unc_Size b2n, const byte *b2b) {
    int e;
    Unc_Value *fn;
    e = unc__getprotomethod(w, a, bn, bb, &fn);
    if (e) return e;
    if (fn) {
        Unc_Size d = unc__stackdepth(&w->sval);
        Unc_Pile ret;
        if ((e = unc__stackpush(w, &w->sval, 1, a))) {
            return e;
        }
        if ((e = unc__stackpush(w, &w->sval, 1, b))) {
            unc__restoredepth(w, &w->sval, d);
            return e;
        }
        if ((e = unc_newpile(w, &ret))) {
            unc__restoredepth(w, &w->sval, d);
            return e;
        }
        w->region.base[ret.r] -= 2;
        e = unc__fcallv(w, fn, 2, 1, 1, 1, 0);
        if (!e)
            e = unc__run(w);
        else if (!UNCIL_IS_ERR(e))
            e = 0;
        if (e) {
            return e;
        } else {
            Unc_Tuple tuple;
            unc_returnvalues(w, &ret, &tuple);
            if (tuple.count == 1) {
                VIMPOSE(w, out, &tuple.values[0]);
                unc_discard(w, &ret);
                return 1;
            } else if (tuple.count > 1) {
                unc_discard(w, &ret);
                return UNCIL_ERR_LOGIC_OVLTOOMANY;
            }
        }
        unc_discard(w, &ret);
    }
    e = unc__getprotomethod(w, b, b2n, b2b, &fn);
    if (e) return e;
    if (fn) {
        Unc_Size d = unc__stackdepth(&w->sval);
        Unc_Pile ret;
        if ((e = unc__stackpush(w, &w->sval, 1, a))) {
            return e;
        }
        if ((e = unc__stackpush(w, &w->sval, 1, b))) {
            unc__restoredepth(w, &w->sval, d);
            return e;
        }
        if ((e = unc_newpile(w, &ret))) {
            unc__restoredepth(w, &w->sval, d);
            return e;
        }
        w->region.base[ret.r] -= 2;
        e = unc__fcallv(w, fn, 2, 1, 1, 1, 0);
        if (!e)
            e = unc__run(w);
        else if (!UNCIL_IS_ERR(e))
            e = 0;
        if (e) {
            return e;
        } else {
            Unc_Tuple tuple;
            unc_returnvalues(w, &ret, &tuple);
            if (tuple.count == 1) {
                VIMPOSE(w, out, &tuple.values[0]);
                unc_discard(w, &ret);
                return 1;
            } else if (tuple.count > 1) {
                unc_discard(w, &ret);
                return UNCIL_ERR_LOGIC_OVLTOOMANY;
            }
        }
        unc_discard(w, &ret);
    }
    return 0;
}

int unc__vdowith(Unc_View *w, Unc_Value *v) {
    int e;
    Unc_Value *fn;
    e = unc__getprotomethod(w, v, PASSSTRL(OPOVERLOAD(open)), &fn);
    if (!e && fn) {
        Unc_Size d = unc__stackdepth(&w->sval);
        Unc_Pile ret;
        Unc_Value ex;
        if ((e = unc__stackpush(w, &w->sval, 1, v)))
            return e;
        if ((e = unc_newpile(w, &ret))) {
            unc__restoredepth(w, &w->sval, d);
            return e;
        }
        w->region.base[ret.r] -= 1;
        VIMPOSE(w, &ex, &w->exc);
        e = unc__fcallv(w, fn, 1, 1, 1, 1, 0);
        if (!e) {
            unc__run(w);
            unc_discard(w, &ret);
        }
        VDECREF(w, &w->exc);
        w->exc = ex;
        if (UNCIL_IS_ERR(e))
            return e;
    }
    return 0;
}

void unc__vdowout(Unc_View *w, Unc_Value *v) {
    int e;
    Unc_Value *fn;
    e = unc__getprotomethod(w, v, PASSSTRL(OPOVERLOAD(close)), &fn);
    if (!e && fn) {
        Unc_Size d = unc__stackdepth(&w->sval);
        Unc_Pile ret;
        Unc_Value ex;
        if ((e = unc__stackpush(w, &w->sval, 1, v)))
            return;
        if ((e = unc_newpile(w, &ret))) {
            unc__restoredepth(w, &w->sval, d);
            return;
        }
        w->region.base[ret.r] -= 1;
        VIMPOSE(w, &ex, &w->exc);
        e = unc__fcallv(w, fn, 1, 1, 1, 1, 0);
        if (!e) {
            unc__run(w);
            unc_discard(w, &ret);
        }
        VDECREF(w, &w->exc);
        w->exc = ex;
    }
}
