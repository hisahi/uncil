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

#include "uarithm.h"
#include "udef.h"
#include "uncil.h"

#define _USE_MATH_DEFINES
#include <errno.h>
#if UNCIL_C99
#include <tgmath.h>
#else
#include <math.h>
#endif

#ifdef M_PI
#define PI M_PI
#elif 0
#define PI acos(-1)
#else
#define PI ((Unc_Float)3.141592653589793238462643383279502884197169399375105821)
#endif

#if !UNCIL_C99
static Unc_Float unc0_math_trunc(Unc_Float x) {
    return x < 0 ? ceil(x) : floor(x);
}

static Unc_Float unc0_math_round(Unc_Float x) {
    return x > 0 ? ceil(x - 0.5) : floor(x + 0.5);
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
        frexp(x, &xexp);
        xexp = 5 * xexp / 8;
        x = ldexp(x, -xexp);
        y = ldexp(y, -xexp);
        return ldexp(sqrt(x * x + y * y), xexp);
    }
    if (y < y * DBL_MIN) {
        /* underflow */
        int yexp;
        frexp(y, &yexp);
        yexp = 5 * yexp / 8;
        x = ldexp(x, -yexp);
        y = ldexp(y, -yexp);
        return ldexp(sqrt(x * x + y * y), yexp);
    }
    return sqrt(x * x + y * y);
}

static Unc_Float unc0_math_asinh(Unc_Float x) {
    return log(x + sqrt(x * x + 1));
}

static Unc_Float unc0_math_acosh(Unc_Float x) {
    Unc_Float y;
    errno = 0;
    y = sqrt(x * x - 1);
    if (errno == EDOM) return -HUGE_VAL;
    return log(x + y);
}

static Unc_Float unc0_math_atanh(Unc_Float x) {
    Unc_Float y;
    errno = 0;
    y = log((1 + x) / (1 - x));
    if (errno == EDOM) return x < 0 ? -HUGE_VAL : HUGE_VAL;
    return y / 2;
}
#endif

Unc_RetVal unc0_lib_math_abs(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    Unc_Value v = UNC_BLANK;
    Unc_Int ui;
    Unc_Float uf;

    e = unc_getint(w, &args.values[0], &ui);
    if (!e && !unc0_negovf(ui)) {
        unc_setint(w, &v, ui < 0 ? -ui : ui);
        return unc_push(w, 1, &v, NULL);
    }
    e = unc_getfloat(w, &args.values[0], &uf);
    if (e) return e;
    errno = 0;
    uf = fabs(uf);
    unc_setfloat(w, &v, uf);
    return unc_push(w, 1, &v, NULL);
}

Unc_RetVal unc0_lib_math_sign(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    Unc_Value v = UNC_BLANK;
    Unc_Float uf;

    e = unc_getfloat(w, &args.values[0], &uf);
    if (e) return e;
    if (uf != uf)
        unc_setfloat(w, &v, uf);
    else
        unc_setint(w, &v, unc0_cmpflt(uf, 0));
    return unc_push(w, 1, &v, NULL);
}

Unc_RetVal unc0_lib_math_sqrt(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    Unc_Value v = UNC_BLANK;
    Unc_Float uf;

    e = unc_getfloat(w, &args.values[0], &uf);
    if (e) return e;
    errno = 0;
    uf = sqrt(uf);
    if (errno == EDOM) return unc_throwexc(w, "math", "domain error");
    unc_setfloat(w, &v, uf);
    return unc_push(w, 1, &v, NULL);
}

Unc_RetVal unc0_lib_math_exp(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    Unc_Value v = UNC_BLANK;
    Unc_Float uf;

    e = unc_getfloat(w, &args.values[0], &uf);
    if (e) return e;
    errno = 0;
    uf = exp(uf);
    if (errno == EDOM) return unc_throwexc(w, "math", "domain error");
    unc_setfloat(w, &v, uf);
    return unc_push(w, 1, &v, NULL);
}

Unc_RetVal unc0_lib_math_log(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    Unc_Value v = UNC_BLANK;
    Unc_Float uf;

    e = unc_getfloat(w, &args.values[0], &uf);
    if (e) return e;
    errno = 0;
    uf = log(uf);
    if (errno == EDOM) return unc_throwexc(w, "math", "domain error");
    unc_setfloat(w, &v, uf);
    return unc_push(w, 1, &v, NULL);
}

Unc_RetVal unc0_lib_math_log2(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    Unc_Value v = UNC_BLANK;
    Unc_Float uf;

    e = unc_getfloat(w, &args.values[0], &uf);
    if (e) return e;
    errno = 0;
#if UNCIL_C99
    uf = log2(uf);
#else
    uf = log(uf) / log(2);
#endif
    if (errno == EDOM) return unc_throwexc(w, "math", "domain error");
    unc_setfloat(w, &v, uf);
    return unc_push(w, 1, &v, NULL);
}

Unc_RetVal unc0_lib_math_log10(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    Unc_Value v = UNC_BLANK;
    Unc_Float uf;

    e = unc_getfloat(w, &args.values[0], &uf);
    if (e) return e;
    errno = 0;
    uf = log10(uf);
    if (errno == EDOM) return unc_throwexc(w, "math", "domain error");
    unc_setfloat(w, &v, uf);
    return unc_push(w, 1, &v, NULL);
}

Unc_RetVal unc0_lib_math_floor(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    Unc_Value v = UNC_BLANK;
    Unc_Float uf;

    e = unc_getfloat(w, &args.values[0], &uf);
    if (e) return e;
    errno = 0;
    uf = floor(uf);
    if (errno == EDOM) return unc_throwexc(w, "math", "domain error");
    unc_setfloat(w, &v, uf);
    return unc_push(w, 1, &v, NULL);
}

Unc_RetVal unc0_lib_math_ceil(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    Unc_Value v = UNC_BLANK;
    Unc_Float uf;

    e = unc_getfloat(w, &args.values[0], &uf);
    if (e) return e;
    errno = 0;
    uf = ceil(uf);
    if (errno == EDOM) return unc_throwexc(w, "math", "domain error");
    unc_setfloat(w, &v, uf);
    return unc_push(w, 1, &v, NULL);
}

Unc_RetVal unc0_lib_math_round(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    Unc_Value v = UNC_BLANK;
    Unc_Float uf;

    e = unc_getfloat(w, &args.values[0], &uf);
    if (e) return e;
    errno = 0;
#if UNCIL_C99
    uf = round(uf);
#else
    uf = unc0_math_round(uf);
#endif
    if (errno == EDOM) return unc_throwexc(w, "math", "domain error");
    unc_setfloat(w, &v, uf);
    return unc_push(w, 1, &v, NULL);
}

Unc_RetVal unc0_lib_math_trunc(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    Unc_Value v = UNC_BLANK;
    Unc_Float uf;

    e = unc_getfloat(w, &args.values[0], &uf);
    if (e) return e;
    errno = 0;
#if UNCIL_C99
    uf = trunc(uf);
#else
    uf = unc0_math_trunc(uf);
#endif
    if (errno == EDOM) return unc_throwexc(w, "math", "domain error");
    unc_setfloat(w, &v, uf);
    return unc_push(w, 1, &v, NULL);
}

Unc_RetVal unc0_lib_math_deg(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    Unc_Value v = UNC_BLANK;
    Unc_Float uf;

    e = unc_getfloat(w, &args.values[0], &uf);
    if (e) return e;
    uf = uf * 180 / PI;
    unc_setfloat(w, &v, uf);
    return unc_push(w, 1, &v, NULL);
}

Unc_RetVal unc0_lib_math_rad(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    Unc_Value v = UNC_BLANK;
    Unc_Float uf;

    e = unc_getfloat(w, &args.values[0], &uf);
    if (e) return e;
    uf = uf * PI / 180;
    unc_setfloat(w, &v, uf);
    return unc_push(w, 1, &v, NULL);
}

Unc_RetVal unc0_lib_math_sin(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    Unc_Value v = UNC_BLANK;
    Unc_Float uf;

    e = unc_getfloat(w, &args.values[0], &uf);
    if (e) return e;
    errno = 0;
    uf = sin(uf);
    if (errno == EDOM) return unc_throwexc(w, "math", "domain error");
    unc_setfloat(w, &v, uf);
    return unc_push(w, 1, &v, NULL);
}

Unc_RetVal unc0_lib_math_cos(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    Unc_Value v = UNC_BLANK;
    Unc_Float uf;

    e = unc_getfloat(w, &args.values[0], &uf);
    if (e) return e;
    errno = 0;
    uf = cos(uf);
    if (errno == EDOM) return unc_throwexc(w, "math", "domain error");
    unc_setfloat(w, &v, uf);
    return unc_push(w, 1, &v, NULL);
}

Unc_RetVal unc0_lib_math_tan(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    Unc_Value v = UNC_BLANK;
    Unc_Float uf;

    e = unc_getfloat(w, &args.values[0], &uf);
    if (e) return e;
    errno = 0;
    uf = tan(uf);
    if (errno == EDOM) return unc_throwexc(w, "math", "domain error");
    unc_setfloat(w, &v, uf);
    return unc_push(w, 1, &v, NULL);
}

Unc_RetVal unc0_lib_math_asin(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    Unc_Value v = UNC_BLANK;
    Unc_Float uf;

    e = unc_getfloat(w, &args.values[0], &uf);
    if (e) return e;
    errno = 0;
    uf = asin(uf);
    if (errno == EDOM) return unc_throwexc(w, "math", "domain error");
    unc_setfloat(w, &v, uf);
    return unc_push(w, 1, &v, NULL);
}

Unc_RetVal unc0_lib_math_acos(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    Unc_Value v = UNC_BLANK;
    Unc_Float uf;

    e = unc_getfloat(w, &args.values[0], &uf);
    if (e) return e;
    errno = 0;
    uf = acos(uf);
    if (errno == EDOM) return unc_throwexc(w, "math", "domain error");
    unc_setfloat(w, &v, uf);
    return unc_push(w, 1, &v, NULL);
}

Unc_RetVal unc0_lib_math_atan(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    Unc_Value v = UNC_BLANK;
    Unc_Float uf;

    e = unc_getfloat(w, &args.values[0], &uf);
    if (e) return e;
    errno = 0;
    uf = atan(uf);
    if (errno == EDOM) return unc_throwexc(w, "math", "domain error");
    unc_setfloat(w, &v, uf);
    return unc_push(w, 1, &v, NULL);
}

Unc_RetVal unc0_lib_math_sinh(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    Unc_Value v = UNC_BLANK;
    Unc_Float uf;

    e = unc_getfloat(w, &args.values[0], &uf);
    if (e) return e;
    errno = 0;
    uf = sinh(uf);
    if (errno == EDOM) return unc_throwexc(w, "math", "domain error");
    unc_setfloat(w, &v, uf);
    return unc_push(w, 1, &v, NULL);
}

Unc_RetVal unc0_lib_math_cosh(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    Unc_Value v = UNC_BLANK;
    Unc_Float uf;

    e = unc_getfloat(w, &args.values[0], &uf);
    if (e) return e;
    errno = 0;
    uf = cosh(uf);
    if (errno == EDOM) return unc_throwexc(w, "math", "domain error");
    unc_setfloat(w, &v, uf);
    return unc_push(w, 1, &v, NULL);
}

Unc_RetVal unc0_lib_math_tanh(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    Unc_Value v = UNC_BLANK;
    Unc_Float uf;

    e = unc_getfloat(w, &args.values[0], &uf);
    if (e) return e;
    errno = 0;
    uf = tanh(uf);
    if (errno == EDOM) return unc_throwexc(w, "math", "domain error");
    unc_setfloat(w, &v, uf);
    return unc_push(w, 1, &v, NULL);
}

Unc_RetVal unc0_lib_math_asinh(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    Unc_Value v = UNC_BLANK;
    Unc_Float uf;

    e = unc_getfloat(w, &args.values[0], &uf);
    if (e) return e;
    errno = 0;
#if UNCIL_C99
    uf = asinh(uf);
#else
    uf = unc0_math_asinh(uf);
#endif
    if (errno == EDOM) return unc_throwexc(w, "math", "domain error");
    unc_setfloat(w, &v, uf);
    return unc_push(w, 1, &v, NULL);
}

Unc_RetVal unc0_lib_math_acosh(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    Unc_Value v = UNC_BLANK;
    Unc_Float uf;

    e = unc_getfloat(w, &args.values[0], &uf);
    if (e) return e;
    errno = 0;
#if UNCIL_C99
    uf = acosh(uf);
#else
    uf = unc0_math_acosh(uf);
#endif
    if (errno == EDOM) return unc_throwexc(w, "math", "domain error");
    unc_setfloat(w, &v, uf);
    return unc_push(w, 1, &v, NULL);
}

Unc_RetVal unc0_lib_math_atanh(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    Unc_Value v = UNC_BLANK;
    Unc_Float uf;

    e = unc_getfloat(w, &args.values[0], &uf);
    if (e) return e;
    errno = 0;
#if UNCIL_C99
    uf = atanh(uf);
#else
    uf = unc0_math_atanh(uf);
#endif
    if (errno == EDOM) return unc_throwexc(w, "math", "domain error");
    unc_setfloat(w, &v, uf);
    return unc_push(w, 1, &v, NULL);
}

Unc_RetVal unc0_lib_math_pow(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    Unc_Value v = UNC_BLANK;
    Unc_Float uf1, uf2;

    e = unc_getfloat(w, &args.values[0], &uf1);
    if (e) return e;
    e = unc_getfloat(w, &args.values[1], &uf2);
    if (e) return e;
    errno = 0;
    uf1 = pow(uf1, uf2);
    if (errno == EDOM) return unc_throwexc(w, "math", "domain error");
    unc_setfloat(w, &v, uf1);
    return unc_push(w, 1, &v, NULL);
}

Unc_RetVal unc0_lib_math_atan2(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    Unc_Value v = UNC_BLANK;
    Unc_Float uf1, uf2;

    e = unc_getfloat(w, &args.values[0], &uf1);
    if (e) return e;
    e = unc_getfloat(w, &args.values[1], &uf2);
    if (e) return e;
    errno = 0;
    uf1 = atan2(uf1, uf2);
    if (errno == EDOM) return unc_throwexc(w, "math", "domain error");
    unc_setfloat(w, &v, uf1);
    return unc_push(w, 1, &v, NULL);
}

Unc_RetVal unc0_lib_math_hypot(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    Unc_Value v = UNC_BLANK;
    Unc_Float uf1, uf2;

    e = unc_getfloat(w, &args.values[0], &uf1);
    if (e) return e;
    e = unc_getfloat(w, &args.values[1], &uf2);
    if (e) return e;
    errno = 0;
#if UNCIL_C99
    uf1 = hypot(uf1, uf2);
#else
    uf1 = unc0_math_hypot(uf1, uf2);
#endif
    if (errno == EDOM) return unc_throwexc(w, "math", "domain error");
    unc_setfloat(w, &v, uf1);
    return unc_push(w, 1, &v, NULL);
}

Unc_RetVal unc0_lib_math_min(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Size i;
    int e;
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
    return unc_push(w, 1, &v, NULL);
}

Unc_RetVal unc0_lib_math_max(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Size i;
    int e;
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
    return unc_push(w, 1, &v, NULL);
}

Unc_RetVal unc0_lib_math_clamp(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    Unc_Value v = UNC_BLANK;
    Unc_Int ui0, ui1, ui2;
    Unc_Float uf0, uf1, uf2;

    e = unc_getint(w, &args.values[0], &ui0);
    if (e) goto unc0_lib_math_clamp_float;
    e = unc_getint(w, &args.values[1], &ui1);
    if (e) goto unc0_lib_math_clamp_float;
    e = unc_getint(w, &args.values[2], &ui2);
    if (e) goto unc0_lib_math_clamp_float;
    if (ui1 > ui2) {
        Unc_Int t = ui1;
        ui1 = ui2;
        ui2 = t;
    }
    if (ui0 < ui1) ui0 = ui1;
    if (ui0 > ui2) ui0 = ui2;

    unc_setint(w, &v, ui0);
    return unc_push(w, 1, &v, NULL);
unc0_lib_math_clamp_float:
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
    return unc_push(w, 1, &v, NULL);
}

Unc_RetVal uncilmain_math(struct Unc_View *w) {
    Unc_RetVal e;
    e = unc_exportcfunction(w, "abs", &unc0_lib_math_abs,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "sign", &unc0_lib_math_sign,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "sqrt", &unc0_lib_math_sqrt,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "exp", &unc0_lib_math_exp,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "log", &unc0_lib_math_log,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "log2", &unc0_lib_math_log2,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "log10", &unc0_lib_math_log10,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "floor", &unc0_lib_math_floor,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "ceil", &unc0_lib_math_ceil,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "round", &unc0_lib_math_round,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "trunc", &unc0_lib_math_trunc,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "deg", &unc0_lib_math_deg,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "rad", &unc0_lib_math_rad,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "sin", &unc0_lib_math_sin,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "cos", &unc0_lib_math_cos,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "tan", &unc0_lib_math_tan,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "asin", &unc0_lib_math_asin,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "acos", &unc0_lib_math_acos,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "atan", &unc0_lib_math_atan,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "sinh", &unc0_lib_math_sinh,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "cosh", &unc0_lib_math_cosh,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "tanh", &unc0_lib_math_tanh,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "asinh", &unc0_lib_math_asinh,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "acosh", &unc0_lib_math_acosh,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "atanh", &unc0_lib_math_atanh,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "atan2", &unc0_lib_math_atan2,
                            UNC_CFUNC_CONCURRENT,
                            2, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "pow", &unc0_lib_math_pow,
                            UNC_CFUNC_CONCURRENT,
                            2, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "hypot", &unc0_lib_math_hypot,
                            UNC_CFUNC_CONCURRENT,
                            2, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "min", &unc0_lib_math_min,
                            UNC_CFUNC_CONCURRENT,
                            1, 1, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "max", &unc0_lib_math_max,
                            UNC_CFUNC_CONCURRENT,
                            1, 1, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "clamp", &unc0_lib_math_clamp,
                            UNC_CFUNC_CONCURRENT,
                            3, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    {
        Unc_Value v = UNC_BLANK;
        unc_setfloat(w, &v, PI);
        e = unc_setpublicc(w, "PI", &v);
        if (e) return e;
    }
    
    {
        Unc_Value v = UNC_BLANK;
        unc_setfloat(w, &v, PI * 2);
        e = unc_setpublicc(w, "TAU", &v);
        if (e) return e;
    }
    
    {
        Unc_Value v = UNC_BLANK;
        unc_setfloat(w, &v, exp(1));
        e = unc_setpublicc(w, "E", &v);
        if (e) return e;
    }

    return 0;
}
