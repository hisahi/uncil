/*******************************************************************************
 
Uncil -- builtin math library impl

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

#define UNCIL_DEFINES

#include <float.h>

#include "uarithm.h"
#include "udef.h"
#include "uncil.h"

#define _USE_MATH_DEFINES
#include <errno.h>
#include <math.h>

#ifndef M_PI
#define M_PI ((Unc_Float)3.14159265358979323846264338327950288419716939937511)
#endif

#ifndef M_E
/* non-constant is fine here, but not for M_PI */
#define M_E UNC_FLOAT_FUNC(exp)(1)
#endif

#if UNCIL_C99
#define unc0_math_trunc trunc
#define unc0_math_round round
#define unc0_math_hypot hypot
#define unc0_math_asinh asinh
#define unc0_math_acosh acosh
#define unc0_math_atanh atanh
#define unc0_math_log2 log2
#else
static Unc_Float unc0_math_trunc(Unc_Float x) {
    return x < 0 ? UNC_FLOAT_FUNC(ceil)(x)
                 : UNC_FLOAT_FUNC(floor)(x);
}

static Unc_Float unc0_math_round(Unc_Float x) {
    return x > 0 ? UNC_FLOAT_FUNC(ceil)(x - 0.5)
                 : UNC_FLOAT_FUNC(floor)(x + 0.5);
}

static Unc_Float unc0_math_hypot(Unc_Float x, Unc_Float y) {
    if (x != x) return x; /* NAN */
    if (y != y) return y; /* NAN */
    if (x < 0) x = -x; /* abs */
    if (y < 0) y = -y; /* abs */
    if (x < y) {
        Unc_Float t = x;
        x = y;
        y = t;
    }
    /* infinities */
    if (x > +DBL_MAX) return x;
    /* now x >= y */
    if (x + y * 2 == x) {
        /* y too small to register */
        return x;
    }
    if (x * x > DBL_MAX) {
        /* overflow */
        int xexp;
        UNC_FLOAT_FUNC(frexp)(x, &xexp);
        xexp = 5 * xexp / 8;
        x = UNC_FLOAT_FUNC(ldexp)(x, -xexp);
        y = UNC_FLOAT_FUNC(ldexp)(y, -xexp);
        return UNC_FLOAT_FUNC(ldexp)(UNC_FLOAT_FUNC(sqrt)(x * x + y * y),
                              xexp);
    }
    if (y < y * DBL_MIN) {
        /* underflow */
        int yexp;
        UNC_FLOAT_FUNC(frexp)(y, &yexp);
        yexp = 5 * yexp / 8;
        x = UNC_FLOAT_FUNC(ldexp)(x, -yexp);
        y = UNC_FLOAT_FUNC(ldexp)(y, -yexp);
        return UNC_FLOAT_FUNC(ldexp)(UNC_FLOAT_FUNC(sqrt)(x * x + y * y),
                              yexp);
    }
    return UNC_FLOAT_FUNC(sqrt)(x * x + y * y);
}

static Unc_Float unc0_math_asinh(Unc_Float x) {
    return UNC_FLOAT_FUNC(log)(x + UNC_FLOAT_FUNC(sqrt)(x * x + 1));
}

static Unc_Float unc0_math_acosh(Unc_Float x) {
    Unc_Float y;
    errno = 0;
    y = UNC_FLOAT_FUNC(sqrt)(x * x - 1);
    if (errno == EDOM) return -HUGE_VAL;
    return UNC_FLOAT_FUNC(log)(x + y);
}

static Unc_Float unc0_math_atanh(Unc_Float x) {
    Unc_Float y;
    errno = 0;
    y = UNC_FLOAT_FUNC(log)((1 + x) / (1 - x));
    if (errno == EDOM) return x < 0 ? -HUGE_VAL : HUGE_VAL;
    return y / 2;
}

static Unc_Float unc0_math_log2(Unc_Float x) {
    return UNC_FLOAT_FUNC(log)(x) / UNC_FLOAT_FUNC(log)(2);
}
#endif

static Unc_Float unc0_math_deg(Unc_Float f) {
    return f * 180 / M_PI;
}

static Unc_Float unc0_math_rad(Unc_Float f) {
    return f * M_PI / 180;
}

INLINE Unc_RetVal uncl_math_f1(Unc_View *w, Unc_Tuple args,
                               Unc_Float (*fn)(Unc_Float)) {
    Unc_RetVal e;
    Unc_Value v = UNC_BLANK;
    Unc_Float uf;

    e = unc_getfloat(w, &args.values[0], &uf);
    if (e) return e;
    errno = 0;
    uf = (*fn)(uf);
    if (errno == EDOM) return unc_throwexc(w, "math", "domain error");
    unc_setfloat(w, &v, uf);
    return unc_returnlocal(w, 0, &v);
}

INLINE Unc_RetVal uncl_math_f2(Unc_View *w, Unc_Tuple args,
                               Unc_Float (*fn)(Unc_Float, Unc_Float)) {
    Unc_RetVal e;
    Unc_Value v = UNC_BLANK;
    Unc_Float uf1, uf2;

    e = unc_getfloat(w, &args.values[0], &uf1);
    if (e) return e;
    e = unc_getfloat(w, &args.values[1], &uf2);
    if (e) return e;
    errno = 0;
    uf1 = (*fn)(uf1, uf2);
    if (errno == EDOM) return unc_throwexc(w, "math", "domain error");
    unc_setfloat(w, &v, uf1);
    return unc_returnlocal(w, 0, &v);
}

Unc_RetVal uncl_math_abs(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Value v = UNC_BLANK;
    Unc_Int ui;
    Unc_Float uf;

    e = unc_getint(w, &args.values[0], &ui);
    if (!e && !unc0_negovf(ui)) {
        unc_setint(w, &v, ui < 0 ? -ui : ui);
        return unc_returnlocal(w, 0, &v);
    }
    e = unc_getfloat(w, &args.values[0], &uf);
    if (e) return e;
    uf = UNC_FLOAT_FUNC(fabs)(uf);
    unc_setfloat(w, &v, uf);
    return unc_returnlocal(w, 0, &v);
}

Unc_RetVal uncl_math_sign(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Value v = UNC_BLANK;
    Unc_Float uf;

    e = unc_getfloat(w, &args.values[0], &uf);
    if (e) return e;
    if (uf != uf)
        unc_setfloat(w, &v, uf);
    else
        unc_setint(w, &v, unc0_cmpflt(uf, 0));
    return unc_returnlocal(w, 0, &v);
}

#define MATH_F1(name, func)                                                    \
    Unc_RetVal uncl_math_##name(Unc_View *w, Unc_Tuple args, void *udata) {    \
        return uncl_math_f1(w, args, UNC_FLOAT_FUNC(func));                    \
    }

#define MATH_F2(name, func)                                                    \
    Unc_RetVal uncl_math_##name(Unc_View *w, Unc_Tuple args, void *udata) {    \
        return uncl_math_f2(w, args, UNC_FLOAT_FUNC(func));                    \
    }

MATH_F1(sqrt, sqrt)
MATH_F1(exp, exp)
MATH_F1(log, log)
MATH_F1(log2, unc0_math_log2)
MATH_F1(log10, log10)
MATH_F1(floor, floor)
MATH_F1(ceil, ceil)
MATH_F1(round, unc0_math_round)
MATH_F1(trunc, unc0_math_trunc)
MATH_F1(deg, unc0_math_deg)
MATH_F1(rad, unc0_math_rad)
MATH_F1(sin, sin)
MATH_F1(cos, cos)
MATH_F1(tan, tan)
MATH_F1(asin, asin)
MATH_F1(acos, acos)
MATH_F1(atan, atan)
MATH_F1(sinh, sinh)
MATH_F1(cosh, cosh)
MATH_F1(tanh, tanh)
MATH_F1(asinh, unc0_math_asinh)
MATH_F1(acosh, unc0_math_acosh)
MATH_F1(atanh, unc0_math_atanh)

MATH_F2(pow, pow)
MATH_F2(atan2, atan2)
MATH_F2(hypot, unc0_math_hypot)

Unc_RetVal uncl_math_min(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Size i;
    Unc_RetVal e;
    Unc_Value v = UNC_BLANK;
    Unc_Int ui1, ui2;
    Unc_Float uf1, uf2;
    int floated = 0;

    e = unc_getint(w, &args.values[0], &ui1);
    if (e)
        floated = 1;
    else {
        e = unc_getfloat(w, &args.values[0], &uf1);
        if (e) return e;
    }
    for (i = 1; i < args.count; ++i) {
        if (!floated) {
            e = unc_getint(w, &args.values[i], &ui2);
            if (e) floated = 1;
            else {
                if (ui1 > ui2) ui1 = ui2;
                continue;
            }
        }
        e = unc_getfloat(w, &args.values[i], &uf2);
        if (e) return e;
        if (uf1 > uf2) uf1 = uf2;
    }
    if (floated)
        unc_setfloat(w, &v, uf1);
    else
        unc_setint(w, &v, ui1);
    return unc_returnlocal(w, 0, &v);
}

Unc_RetVal uncl_math_max(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Size i;
    Unc_RetVal e;
    Unc_Value v = UNC_BLANK;
    Unc_Int ui1, ui2;
    Unc_Float uf1, uf2;
    int floated = 0;

    e = unc_getint(w, &args.values[0], &ui1);
    if (e)
        floated = 1;
    else {
        e = unc_getfloat(w, &args.values[0], &uf1);
        if (e) return e;
    }
    for (i = 1; i < args.count; ++i) {
        if (!floated) {
            e = unc_getint(w, &args.values[i], &ui2);
            if (e) floated = 1;
            else {
                if (ui1 < ui2) ui1 = ui2;
                continue;
            }
        }
        e = unc_getfloat(w, &args.values[i], &uf2);
        if (e) return e;
        if (uf1 < uf2) uf1 = uf2;
    }
    if (floated)
        unc_setfloat(w, &v, uf1);
    else
        unc_setint(w, &v, ui1);
    return unc_returnlocal(w, 0, &v);
}

Unc_RetVal uncl_math_clamp(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Value v = UNC_BLANK;
    Unc_Int ui0, ui1, ui2;
    Unc_Float uf0, uf1, uf2;

    e = unc_getint(w, &args.values[0], &ui0);
    if (e) goto uncl_math_clamp_float;
    e = unc_getint(w, &args.values[1], &ui1);
    if (e) goto uncl_math_clamp_float;
    e = unc_getint(w, &args.values[2], &ui2);
    if (e) goto uncl_math_clamp_float;
    if (ui1 > ui2) {
        Unc_Int t = ui1;
        ui1 = ui2;
        ui2 = t;
    }
    if (ui0 < ui1) ui0 = ui1;
    if (ui0 > ui2) ui0 = ui2;

    unc_setint(w, &v, ui0);
    return unc_returnlocal(w, 0, &v);

uncl_math_clamp_float:
    e = unc_getfloat(w, &args.values[0], &uf0);
    if (e) return e;
    e = unc_getfloat(w, &args.values[1], &uf1);
    if (e) return e;
    e = unc_getfloat(w, &args.values[2], &uf2);
    if (e) return e;
    if (uf1 > uf2) {
        Unc_Float t = uf1;
        uf1 = uf2;
        uf2 = t;
    }
    if (uf0 < uf1) uf0 = uf1;
    if (uf0 > uf2) uf0 = uf2;

    unc_setfloat(w, &v, uf0);
    return unc_returnlocal(w, 0, &v);
}

#define FN(x) &uncl_math_##x, #x
static const Unc_ModuleCFunc lib[] = {
    { FN(abs),      1, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(sign),     1, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(sqrt),     1, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(exp),      1, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(log),      1, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(log2),     1, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(log10),    1, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(floor),    1, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(ceil),     1, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(round),    1, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(trunc),    1, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(deg),      1, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(rad),      1, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(sin),      1, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(cos),      1, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(tan),      1, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(asin),     1, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(acos),     1, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(atan),     1, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(sinh),     1, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(cosh),     1, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(tanh),     1, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(asinh),    1, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(acosh),    1, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(atanh),    1, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(atan2),    2, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(pow),      2, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(hypot),    2, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(min),      1, 0, 1, UNC_CFUNC_CONCURRENT },
    { FN(max),      1, 0, 1, UNC_CFUNC_CONCURRENT },
    { FN(clamp),    3, 0, 0, UNC_CFUNC_CONCURRENT },
};

INLINE Unc_RetVal setflt_(struct Unc_View *w, const char *name, Unc_Float f) {
    Unc_Value v = UNC_BLANK;
    unc_setfloat(w, &v, f);
    return unc_setpublicc(w, name, &v);
}

Unc_RetVal uncilmain_math(struct Unc_View *w) {
    Unc_RetVal e;

    if ((e = setflt_(w, "PI",   M_PI            ))) return e;
    if ((e = setflt_(w, "TAU",  M_PI * 2        ))) return e;
    if ((e = setflt_(w, "E",    M_E             ))) return e;

    return unc_exportcfunctions(w, PASSARRAY(lib), 0, NULL, NULL);
}
