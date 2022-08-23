/*******************************************************************************
 
Uncil -- error => exception impl

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

#include <string.h>

#include "uerr.h"
#include "uncil.h"
#include "uobj.h"
#include "ustr.h"
#include "uval.h"
#include "uvali.h"
#include "uvlq.h"
#include "uvsio.h"

#define MUST(cond) do { int e; if ((e = (cond))) return e; } while (0)

/* make sure type is a string literal */
int unc0_makeexception(struct Unc_View *w, const char *type,
                       const char *msg, Unc_Value *out) {
    int e;
    Unc_Entity *ed = unc0_wake(w, Unc_TObject), *es1 = NULL, *es2 = NULL;
    Unc_Object *o;
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
            e = unc0_initstringcl(&w->world->alloc, LEFTOVER(Unc_String, es1),
                                                    type);
            if (e) {
                unc0_unwake(es1, w);
                es1 = NULL;
            }
        }
        if (es1) {
            Unc_Value tv;
            VINITENT(&tv, Unc_TString, es1);
            e = unc0_osetattrs(w, o, PASSSTRL("type"), &tv);
        }
    }
    if (!e) {
        if (msg) {
            es2 = unc0_wake(w, Unc_TString);
            if (!es2) e = UNCIL_ERR_MEM;
            else {
                e = unc0_initstringcl(&w->world->alloc,
                                      LEFTOVER(Unc_String, es2), msg);
                if (e) {
                    unc0_unwake(es2, w);
                    es2 = NULL;
                }
            }
            if (es2) {
                Unc_Value tv;
                VINITENT(&tv, Unc_TString, es2);
                e = unc0_osetattrs(w, o, PASSSTRL("message"), &tv);
                if (e && es2)
                    unc0_hibernate(es2, w);
            }
        } else {
            Unc_Value tv = UNC_BLANK;
            e = unc0_osetattrs(w, o, PASSSTRL("message"), &tv);
        }
    } else {
        if (es1) unc0_hibernate(es1, w);
    }
    if (e) {
        unc0_hibernate(ed, w);
        return e;
    }
    VINITENT(out, Unc_TObject, ed);
    return 0;
}

/* make sure type is a string literal */
int unc0_makeexceptiona(struct Unc_View *w, const char *type, Unc_Size msg_n,
                        byte *msg, Unc_Value *out) {
    int e;
    Unc_Entity *ed = unc0_wake(w, Unc_TObject), *es1 = NULL, *es2 = NULL;
    Unc_Object *o;
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
            e = unc0_initstringcl(&w->world->alloc, LEFTOVER(Unc_String, es1),
                                                    type);
            if (e) {
                unc0_unwake(es1, w);
                es1 = NULL;
            }
        }
        if (es1) {
            Unc_Value tv;
            VINITENT(&tv, Unc_TString, es1);
            e = unc0_osetattrs(w, o, PASSSTRL("type"), &tv);
        }
    }
    if (!e) {
        es2 = unc0_wake(w, Unc_TString);
        if (!es2) e = UNCIL_ERR_MEM;
        else {
            e = unc0_initstringmove(&w->world->alloc, LEFTOVER(Unc_String, es2),
                                                      msg_n, msg);
            if (e) {
                unc0_unwake(es2, w);
                es2 = NULL;
            }
        }
        if (es2) {
            Unc_Value tv;
            VINITENT(&tv, Unc_TString, es2);
            e = unc0_osetattrs(w, o, PASSSTRL("message"), &tv);
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
    VINITENT(out, Unc_TObject, ed);
    return 0;
}

/* make sure type is a string literal */
int unc0_makeexceptiont(struct Unc_View *w, const char *type,
                        Unc_Value *msg, Unc_Value *out) {
    int e;
    Unc_Entity *ed = unc0_wake(w, Unc_TObject), *es1 = NULL;
    Unc_Object *o;
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
            e = unc0_initstringcl(&w->world->alloc, LEFTOVER(Unc_String, es1),
                                                    type);
            if (e) {
                unc0_unwake(es1, w);
                es1 = NULL;
            }
        }
        if (es1) {
            Unc_Value tv;
            VINITENT(&tv, Unc_TString, es1);
            e = unc0_osetattrs(w, o, PASSSTRL("type"), &tv);
        }
    }
    if (!e) {
        e = unc0_osetattrs(w, o, PASSSTRL("message"), msg);
    } else {
        if (es1) unc0_hibernate(es1, w);
    }
    if (e) {
        unc0_hibernate(ed, w);
        return e;
    }
    VINITENT(out, Unc_TObject, ed);
    return 0;
}

int unc0_makeexceptionv(struct Unc_View *w, Unc_Value *type,
                        Unc_Value *msg, Unc_Value *out) {
    int e;
    Unc_Entity *ed = unc0_wake(w, Unc_TObject);
    Unc_Object *o;
    if (!ed) return UNCIL_ERR_MEM;
    o = LEFTOVER(Unc_Object, ed);
    e = unc0_initobj(w, o, NULL);
    if (e) {
        unc0_unwake(ed, w);
        return e;
    }
    if (!e) e = unc0_osetattrs(w, o, PASSSTRL("type"), type);
    if (!e) e = unc0_osetattrs(w, o, PASSSTRL("message"), msg);
    if (e) {
        unc0_hibernate(ed, w);
        return e;
    }
    VINITENT(out, Unc_TObject, ed);
    return 0;
}

void unc0_makeexceptionoroom(Unc_View *w, Unc_Value *out, const char *type,
                             const char *msg) {
    if (unc0_makeexception(w, type, msg, out))
        VCOPY(w, out, &w->world->exc_oom);
}

void unc0_makeexceptionaoroom(Unc_View *w, Unc_Value *out, const char *type,
                              Unc_Size msg_n, byte *msg) {
    if (unc0_makeexceptiona(w, type, msg_n, msg, out)) {
        unc0_mfree(&w->world->alloc, msg, msg_n);
        VCOPY(w, out, &w->world->exc_oom);
    }
}

void unc0_makeexceptiontoroom(Unc_View *w, Unc_Value *out, const char *type,
                              Unc_Value *msg) {
    if (unc0_makeexceptiont(w, type, msg, out))
        VCOPY(w, out, &w->world->exc_oom);
}

void unc0_makeexceptionvoroom(Unc_View *w, Unc_Value *out, Unc_Value *type,
                              Unc_Value *msg) {
    if (unc0_makeexceptionv(w, type, msg, out))
        VCOPY(w, out, &w->world->exc_oom);
}

static void unc0_errtoexcept_unknown(Unc_View *w, int e, Unc_Value *out) {
    unc0_makeexceptionoroom(w, out, "unknown", "unknown error");
}

static void unc0_errtoexcept_fatal(Unc_View *w, int e, Unc_Value *out) {
    switch (e) {
    case UNCIL_ERR_MEM:
        unc0_makeexceptionoroom(w, out, "memory", "out of memory");
        return;
    case UNCIL_ERR_INTERNAL:
        unc0_makeexceptionoroom(w, out, "internal", "internal error");
        return;
    default:
        unc0_errtoexcept_unknown(w, e, out);
        return;
    }
}

static void unc0_errtoexcept_syntax_msg(Unc_View *w, Unc_Value *out,
                                        const char *msg) {
    if (w->comperrlineno) {
        int ee;
        byte *s;
        ee = unc0_saxprintf(&w->world->alloc, &s,
            "%s on line %lu", msg, w->comperrlineno);
        if (ee < 0)
            VCOPY(w, out, &w->world->exc_oom);
        else
            unc0_makeexceptionaoroom(w, out, "syntax", ee, s);
        return;
    }
    unc0_makeexceptionoroom(w, out, "syntax", msg);
}

static void unc0_errtoexcept_syntax(Unc_View *w, int e, Unc_Value *out) {
    switch (e) {
    case UNCIL_ERR_SYNTAX_UNTERMSTR:
        unc0_errtoexcept_syntax_msg(w, out,
            "unterminated string literal");
        return;
    case UNCIL_ERR_SYNTAX_BADESCAPE:
        unc0_errtoexcept_syntax_msg(w, out,
            "invalid string escape");
        return;
    case UNCIL_ERR_SYNTAX_BADUESCAPE:
        unc0_errtoexcept_syntax_msg(w, out,
            "invalid Unicode character escape");
        return;
    case UNCIL_ERR_SYNTAX_TRAILING:
        unc0_errtoexcept_syntax_msg(w, out,
            "unrecognized trailing characters");
        return;
    case UNCIL_ERR_SYNTAX_STRAYEND:
        unc0_errtoexcept_syntax_msg(w, out,
            "unexpected 'end'");
        return;
    case UNCIL_ERR_SYNTAX_TOODEEP:
        unc0_errtoexcept_syntax_msg(w, out,
            "code exceeds interpreter limits");
        return;
    case UNCIL_ERR_SYNTAX_BADBREAK:
        unc0_errtoexcept_syntax_msg(w, out,
            "unexpected 'break' (not in a loop)");
        return;
    case UNCIL_ERR_SYNTAX_BADCONTINUE:
        unc0_errtoexcept_syntax_msg(w, out,
            "unexpected 'continue' (not in a loop)");
        return;
    case UNCIL_ERR_SYNTAX_INLINEIFMUSTELSE:
        unc0_errtoexcept_syntax_msg(w, out,
            "if expressions must have an else");
        return;
    case UNCIL_ERR_SYNTAX_NOFOROP:
        unc0_errtoexcept_syntax_msg(w, out,
            "relational operator missing before the end value");
        return;
    case UNCIL_ERR_SYNTAX_CANNOTPUBLICLOCAL:
        unc0_errtoexcept_syntax_msg(w, out,
            "cannot use public on identifier already assigned to");
        return;
    case UNCIL_ERR_SYNTAX_OPTAFTERREQ:
        unc0_errtoexcept_syntax_msg(w, out,
            "required parameters may not follow optional ones");
        return;
    case UNCIL_ERR_SYNTAX_UNPACKLAST:
        unc0_errtoexcept_syntax_msg(w, out,
            "an ellipsis parameter must be the final parameter");
        return;
    case UNCIL_ERR_SYNTAX_NODEFAULTUNPACK:
        unc0_errtoexcept_syntax_msg(w, out,
            "an ellipsis parameter cannot have a default value");
        return;
    case UNCIL_ERR_SYNTAX_ONLYONEELLIPSIS:
        unc0_errtoexcept_syntax_msg(w, out,
            "only one ellipsis to assign to is allowed");
        return;
    case UNCIL_ERR_SYNTAX_FUNCTABLEUNNAMED:
        unc0_errtoexcept_syntax_msg(w, out,
            "functions directly under table definitions must have a name");
        return;
    case UNCIL_ERR_SYNTAX_ELLIPSISCOMPOUND:
        unc0_errtoexcept_syntax_msg(w, out,
            "cannot use ellipsis with compound assignment");
        return;
    case UNCIL_ERR_SYNTAX_PUBLICONLYONE:
        unc0_errtoexcept_syntax_msg(w, out,
            "public statements may either declare one, declare many or "
            "declare and assign one");
        return;
    default:
        unc0_errtoexcept_syntax_msg(w, out,
            "invalid syntax");
        return;
    }
}

static void unc0_errtoexcept_unsup1(Unc_View *w, int e, Unc_Value *out) {
    if (w->has_lasterr) {
        int ee, t0 = w->lasterr.i1;
        byte *s;
        ee = unc0_saxprintf(&w->world->alloc, &s,
            "unary operator not supported on type %s",
            unc0_getvaluetypename(t0));
        w->has_lasterr = 0;
        if (ee < 0)
            VCOPY(w, out, &w->world->exc_oom);
        else
            unc0_makeexceptionaoroom(w, out, "type", ee, s);
        return;
    }
    unc0_makeexceptionoroom(w, out, "type",
            "unary operation not supported");
}

static void unc0_errtoexcept_unsup2(Unc_View *w, int e, Unc_Value *out) {
    if (w->has_lasterr) {
        int ee, t0 = w->lasterr.i1, t1 = w->lasterr.i2;
        byte *s;
        ee = unc0_saxprintf(&w->world->alloc, &s,
            "binary operator not supported on types %s and %s",
            unc0_getvaluetypename(t0), unc0_getvaluetypename(t1));
        w->has_lasterr = 0;
        if (ee < 0)
            VCOPY(w, out, &w->world->exc_oom);
        else
            unc0_makeexceptionaoroom(w, out, "type", ee, s);
        return;
    }
    unc0_makeexceptionoroom(w, out, "type",
            "binary operation not supported");
}

static void unc0_errtoexcept_withname(Unc_View *w, Unc_Value *out,
                                            const char *type,
                                            const char *fmsg,
                                            const char *msg) {
    if (w->has_lasterr) {
        int ee;
        byte *s;
        ee = unc0_saxprintf(&w->world->alloc, &s, fmsg,
            (int)w->lasterr.s, (const char *)w->lasterr.p.c);
        w->has_lasterr = 0;
        if (ee < 0)
            VCOPY(w, out, &w->world->exc_oom);
        else
            unc0_makeexceptionaoroom(w, out, type, ee, s);
        return;
    }
    unc0_makeexceptionoroom(w, out, type, msg);
}

static void unc0_errtoexcept_arg(Unc_View *w, int e, Unc_Value *out) {
    switch (e) {
    case UNCIL_ERR_PROGRAM_INCOMPATIBLE:
        unc0_makeexceptionoroom(w, out, "internal",
            "program version not supported");
        return;
    case UNCIL_ERR_UNHASHABLE:
        unc0_makeexceptionoroom(w, out, "value",
            "value not hashable");
        return;
    case UNCIL_ERR_ARG_OUTOFBOUNDS:
        unc0_makeexceptionoroom(w, out, "value",
            "index out of bounds");
        return;
    case UNCIL_ERR_ARG_REFCOPYOUTOFBOUNDS:
        unc0_makeexceptionoroom(w, out, "interface",
            "reference copy index out of bounds");
        return;
    case UNCIL_ERR_ARG_INDEXOUTOFBOUNDS:
        unc0_makeexceptionoroom(w, out, "value",
            "index out of bounds");
        return;
    case UNCIL_ERR_ARG_INDEXNOTINTEGER:
        unc0_makeexceptionoroom(w, out, "value",
            "array indices must be integers");
        return;
    case UNCIL_ERR_ARG_CANNOTWEAK:
        unc0_makeexceptionoroom(w, out, "value",
            "weak references may only be created to values of reference types");
        return;
    case UNCIL_ERR_TOODEEP:
        unc0_makeexceptionoroom(w, out, "recursion",
            "maximum recursion level exceeded");
        return;
    case UNCIL_ERR_ARG_NOTENOUGHARGS:
        unc0_makeexceptionoroom(w, out, "call",
            "not enough arguments given to function");
        return;
    case UNCIL_ERR_ARG_TOOMANYARGS:
        unc0_makeexceptionoroom(w, out, "call",
            "too many arguments given to function");
        return;
    case UNCIL_ERR_ARG_NOTINCFUNC:
        unc0_makeexceptionoroom(w, out, "interface",
            "cannot use outside of external C function call");
        return;
    case UNCIL_ERR_ARG_NOSUCHATTR:
        unc0_makeexceptionoroom(w, out, "key", "no such attribute");
        return;
    case UNCIL_ERR_ARG_NOSUCHINDEX:
        unc0_makeexceptionoroom(w, out, "key", "no such index");
        return;
    case UNCIL_ERR_ARG_CANNOTSETINDEX:
        unc0_makeexceptionoroom(w, out, "type",
            "value does not support assigning by index");
        return;
    case UNCIL_ERR_ARG_NOSUCHNAME:
        unc0_errtoexcept_withname(w, out, "name",
            "no such name '%.*s' defined",
            "no such name defined");
        return;
    case UNCIL_ERR_ARG_DIVBYZERO:
        unc0_makeexceptionoroom(w, out, "math",
            "division by zero");
        return;
    case UNCIL_ERR_ARG_NOTITERABLE:
        unc0_makeexceptionoroom(w, out, "type",
            "value does not support iteration");
        return;
    case UNCIL_ERR_ARG_NOTMOSTRECENT:
        unc0_makeexceptionoroom(w, out, "interface",
            "callex must refer to the most recent stack region");
        return;
    case UNCIL_ERR_ARG_NOPROGRAMLOADED:
        unc0_makeexceptionoroom(w, out, "interface",
            "no program has been loaded");
        return;
    case UNCIL_ERR_ARG_NOTINDEXABLE:
        unc0_makeexceptionoroom(w, out, "type",
            "value does not support indexing");
        return;
    case UNCIL_ERR_ARG_NOTATTRABLE:
        unc0_makeexceptionoroom(w, out, "type",
            "value does not have any attributes");
        return;
    case UNCIL_ERR_ARG_NOTATTRSETTABLE:
        unc0_makeexceptionoroom(w, out, "type",
            "value does not support assigning attributes");
        return;
    case UNCIL_ERR_ARG_MODULENOTFOUND:
        unc0_makeexceptionoroom(w, out, "require", "module not found");
        return;
    case UNCIL_ERR_ARG_UNSUP1:
        unc0_errtoexcept_unsup1(w, e, out);
        return;
    case UNCIL_ERR_ARG_UNSUP2:
        unc0_errtoexcept_unsup2(w, e, out);
        return;
    case UNCIL_ERR_ARG_STRINGTOOLONG:
        unc0_makeexceptionoroom(w, out, "internal", "string too long");
        return;
    case UNCIL_ERR_ARG_INVALIDPROTOTYPE:
        unc0_makeexceptionoroom(w, out, "type", "invalid prototype "
                    "(must be null, table, object or opaque)");
        return;
    case UNCIL_ERR_ARG_CANNOTDELETEINDEX:
        unc0_makeexceptionoroom(w, out, "type",
            "value does not support deleting by index");
    case UNCIL_ERR_ARG_NOTATTRDELETABLE: return;
        unc0_makeexceptionoroom(w, out, "type",
            "value does not support deleting attributes");
        return;
    case UNCIL_ERR_ARG_CANNOTBINDFUNC:
        unc0_makeexceptionoroom(w, out, "type",
            "cannot bind an object of this type");
        return;
    case UNCIL_ERR_ARG_NULLCHAR:
        unc0_makeexceptionoroom(w, out, "value",
            "string may not contain null characters");
        return;
    case UNCIL_ERR_ARG_INTOVERFLOW:
        unc0_makeexceptionoroom(w, out, "value",
            "value too large to fit in int");
        return;
    case UNCIL_ERR_ARG_NOCFUNC:
        unc0_makeexceptionoroom(w, out, "value",
            "cannot call C function here");
        return;
    default:
        unc0_makeexceptionoroom(w, out, "value", "invalid argument");
        return;
    }
}

static void unc0_errtoexcept_cvt(Unc_View *w, int e, Unc_Value *out) {
    switch (e) {
    case UNCIL_ERR_CONVERT_TOINT:
        unc0_makeexceptionoroom(w, out, "type", "cannot convert to integer");
        return;
    case UNCIL_ERR_CONVERT_TOFLOAT:
        unc0_makeexceptionoroom(w, out, "type", "cannot convert to float");
        return;
    default:
        unc0_makeexceptionoroom(w, out, "type", "cannot convert");
        return;
    }
}

static void unc0_errtoexcept_io(Unc_View *w, int e, Unc_Value *out) {
    switch (e) {
    case UNCIL_ERR_IO_INVALIDENCODING:
        unc0_makeexceptionoroom(w, out, "encoding", "invalid encoding");
        return;
    default:
        unc0_makeexceptionoroom(w, out, "io", "I/O error");
        return;
    }
}

static void unc0_errtoexcept_type(Unc_View *w, int e, Unc_Value *out) {
    switch (e) {
    case UNCIL_ERR_TYPE_NOTBOOL:
        unc0_makeexceptionoroom(w, out, "type",
            "value is not a bool");
        return;
    case UNCIL_ERR_TYPE_NOTINT:
        unc0_makeexceptionoroom(w, out, "type",
            "value is not an integer");
        return;
    case UNCIL_ERR_TYPE_NOTFLOAT:
        unc0_makeexceptionoroom(w, out, "type",
            "value is not a float");
        return;
    case UNCIL_ERR_TYPE_NOTOPAQUEPTR:
        unc0_makeexceptionoroom(w, out, "type",
            "value is not a opaqueptr");
        return;
    case UNCIL_ERR_TYPE_NOTSTR:
        unc0_makeexceptionoroom(w, out, "type",
            "value is not a string");
        return;
    case UNCIL_ERR_TYPE_NOTARRAY:
        unc0_makeexceptionoroom(w, out, "type",
            "value is not an array");
        return;
    case UNCIL_ERR_TYPE_NOTDICT:
        unc0_makeexceptionoroom(w, out, "type",
            "value is not a table");
        return;
    case UNCIL_ERR_TYPE_NOTOBJECT:
        unc0_makeexceptionoroom(w, out, "type",
            "value is not an object or opaque");
        return;
    case UNCIL_ERR_TYPE_NOTBLOB:
        unc0_makeexceptionoroom(w, out, "type",
            "value is not a blob");
        return;
    case UNCIL_ERR_TYPE_NOTFUNCTION:
        unc0_makeexceptionoroom(w, out, "type",
            "value is not callable");
        return;
    case UNCIL_ERR_TYPE_NOTOPAQUE:
        unc0_makeexceptionoroom(w, out, "type",
            "value is not an opaque");
        return;
    case UNCIL_ERR_TYPE_NOTWEAKPTR:
        unc0_makeexceptionoroom(w, out, "type",
            "value is not a weak pointer");
        return;
    default:
        unc0_makeexceptionoroom(w, out, "type", "incorrect type");
        return;
    }
}

static void unc0_errtoexcept_logic(Unc_View *w, int e, Unc_Value *out) {
    switch (e) {
    case UNCIL_ERR_LOGIC_UNPACKTOOFEW:
        unc0_makeexceptionoroom(w, out, "value",
            "not enough values to unpack");
        return;
    case UNCIL_ERR_LOGIC_UNPACKTOOMANY:
        unc0_makeexceptionoroom(w, out, "value",
            "too many values to unpack");
        return;
    case UNCIL_ERR_LOGIC_FINISHING:
        unc0_makeexceptionoroom(w, out, "system", "VM is exiting");
        return;
    case UNCIL_ERR_LOGIC_OVLTOOMANY:
        unc0_makeexceptionoroom(w, out, "value",
            "overload should only return one value but returned multiple");
        return;
    case UNCIL_ERR_LOGIC_NOTSUPPORTED:
        unc0_makeexceptionoroom(w, out, "system",
            "not supported on this platform");
        return;
    case UNCIL_ERR_LOGIC_CMPNAN:
        unc0_makeexceptionoroom(w, out, "math",
            "cannot compare NaN values");
        return;
    case UNCIL_ERR_LOGIC_CANNOTLOCK:
        unc0_makeexceptionoroom(w, out, "value",
            "cannot lock this value as it is already locked");
        return;
    default:
        unc0_errtoexcept_unknown(w, e, out);
        return;
    }
}

void unc0_errtoexcept(Unc_View *w, int e, Unc_Value *out) {
    switch (UNCIL_ERR_KIND(e)) {
    case UNCIL_ERR_KIND_FATAL:
        unc0_errtoexcept_fatal(w, e, out);
        return;
    case UNCIL_ERR_KIND_UNCIL:
        VCOPY(w, out, &w->exc);
        return;
    case UNCIL_ERR_KIND_SYNTAX:
        unc0_errtoexcept_syntax(w, e, out);
        return;
    case UNCIL_ERR_KIND_BADARG:
        unc0_errtoexcept_arg(w, e, out);
        return;
    case UNCIL_ERR_KIND_CONVERT:
        unc0_errtoexcept_cvt(w, e, out);
        return;
    case UNCIL_ERR_KIND_IO:
        unc0_errtoexcept_io(w, e, out);
        return;
    case UNCIL_ERR_KIND_TYPE:
        unc0_errtoexcept_type(w, e, out);
        return;
    case UNCIL_ERR_KIND_LOGIC:
        unc0_errtoexcept_logic(w, e, out);
        return;
    default:
        unc0_errtoexcept_unknown(w, e, out);
        return;
    }
}

int unc0_err_unsup1(struct Unc_View *w, int t) {
    w->has_lasterr = 1;
    w->lasterr.i1 = t;
    return UNCIL_ERR_ARG_UNSUP1;
}

int unc0_err_unsup2(struct Unc_View *w, int t1, int t2) {
    w->has_lasterr = 1;
    w->lasterr.i1 = t1;
    w->lasterr.i2 = t2;
    return UNCIL_ERR_ARG_UNSUP2;
}

int unc0_err_withname(struct Unc_View *w, int e, Unc_Size s, const byte *b) {
    w->has_lasterr = 1;
    w->lasterr.s = s;
    w->lasterr.p.c = b;
    return e;
}

int unc0_throwexc(struct Unc_View *w, const char *type, const char *msg) {
    unc0_makeexceptionoroom(w, &w->exc, type, msg);
    return UNCIL_ERR_UNCIL;
}

/* format stack entry */
static int unc0_errstackfmtu(Unc_View *w, Unc_Value *out, Unc_Size lineno) {
    int e;
    const byte *un = w->uncfname;
    Unc_Size fname_n;
    const char *pname = w->program->pname, *fname;
    char linenumber[sizeof(Unc_Size) * 3 + 2];
    if (un) {
        fname_n = unc0_vlqdecz(&un);
        fname = (const char *)un;
    } else {
        fname_n = 9;
        fname = "<unknown>";
    }
    if (!pname)
        pname = "<unknown>";
    if (lineno)
        sprintf(linenumber, ":%"PRIUnc_Int"u", (Unc_UInt)lineno);
    else
        strcpy(linenumber, "");
    if (fname_n > INT_MAX)
        fname_n = INT_MAX;
    e = unc0_usxprintf(w, out, "'%.*S' in %s%s",
                fname_n, fname, pname, linenumber);
    if (!e) unc_incref(w, out);
    return e;
}

static int unc0_errstackfmt(Unc_View *w, Unc_Value *out, int fromc,
                                         Unc_Size lineno) {
    if (!fromc)
        return unc0_errstackfmtu(w, out, lineno);
    else {
        int e;
        e = unc0_usxprintf(w, out, "(C function)");
        if (!e) unc_incref(w, out);
        return e;
    }
}

INLINE int isframec(int ft) {
    switch (ft) {
    case Unc_FrameCallC:
    case Unc_FrameCallCSpew:
        return 1;
    default:
        return 0;
    }
}

void unc0_errstackpush(Unc_View *w, Unc_Size lineno) {
    /* add w to exception stack variable */
    if (!unc_issame(w, &w->exc, &w->world->exc_oom)) {
        int e;
        Unc_Value stack = UNC_BLANK;
        e = unc_getattrc(w, &w->exc, "stack", &stack);
        if (e == UNCIL_ERR_ARG_NOSUCHATTR) {
            e = unc_newarrayempty(w, &stack);
            if (e) return;
            e = unc_setattrc(w, &w->exc, "stack", &stack);
            if (e) return;
        }
        /* if (w->frames.base == w->frames.top
            || !isframec(w->frames.top[-1].type)) */
        {
            Unc_Size an;
            Unc_Value *ap;
            Unc_Value stackline = UNC_BLANK;
            e = unc0_errstackfmt(w, &stackline,
                                 isframec(w->frames.top[-1].type),
                                 lineno);
            if (e) return;

            e = unc_lockarray(w, &stack, &an, &ap);
            if (e) {
                unc_clear(w, &stackline);
                return;
            }
            e = unc_resizearray(w, &stack, an + 1, &ap);
            if (e) {
                unc_clear(w, &stackline);
                return;
            }
            unc_copy(w, &ap[an], &stackline);
            unc_unlock(w, &stack);
            unc_clear(w, &stackline);
        }
        unc_clear(w, &stack);
    }
}

void unc0_errstackpushcoro(struct Unc_View *w) {
    if (!unc_issame(w, &w->exc, &w->world->exc_oom)) {
        int e;
        Unc_Value stack = UNC_BLANK;
        e = unc_getattrc(w, &w->exc, "stack", &stack);
        if (e == UNCIL_ERR_ARG_NOSUCHATTR) {
            e = unc_newarrayempty(w, &stack);
            if (e) return;
            e = unc_setattrc(w, &w->exc, "stack", &stack);
            if (e) return;
        }
        {
            Unc_Size an;
            Unc_Value *ap;
            Unc_Value stackline = UNC_BLANK;
            e = unc_newstringc(w, &stackline, "--- coroutine ---");
            if (e) return;

            e = unc_lockarray(w, &stack, &an, &ap);
            if (e) {
                unc_clear(w, &stackline);
                return;
            }
            e = unc_resizearray(w, &stack, an + 1, &ap);
            if (e) {
                unc_clear(w, &stackline);
                return;
            }
            unc_copy(w, &ap[an], &stackline);
            unc_unlock(w, &stack);
            unc_clear(w, &stackline);
        }
        unc_clear(w, &stack);
    }
}

void unc0_errinfocopyfrom(struct Unc_View *w, struct Unc_View *wc) {
    VCOPY(w, &w->exc, &wc->exc);
    if ((w->has_lasterr = wc->has_lasterr))
        w->lasterr = wc->lasterr;
}
