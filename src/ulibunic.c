/*******************************************************************************
 
Uncil -- builtin unicode library impl

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

#include "uctype.h"
#include "uncil.h"
#include "uutf.h"

#if UNCIL_LIB_ICU
#include "unicode/ubrk.h"
#include "unicode/ucasemap.h"
#include "unicode/uchar.h"
#include "unicode/ucnv.h"
#include "unicode/unorm2.h"
#include "unicode/utext.h"
#include "unicode/utypes.h"

typedef int32_t icu_u8index_t;
#define ICU_U8INDEX_MAX INT32_MAX
#endif

Unc_RetVal unc0_lib_unic_lookup(Unc_View *w, Unc_Tuple args, void *udata) {
#if UNCIL_LIB_ICU
    Unc_Value v = UNC_BLANK;
    int e;
    Unc_Size sn;
    const char *sp;
    UChar32 uc;
    UErrorCode ue = U_ZERO_ERROR;
    char buf[UNC_UTF8_MAX_SIZE];

    e = unc_getstring(w, &args.values[0], &sn, &sp);
    if (e) return e;
    
    uc = u_charFromName(U_UNICODE_CHAR_NAME, sp, &ue);
    if (U_FAILURE(ue))
        uc = u_charFromName(U_EXTENDED_CHAR_NAME, sp, &ue);
    if (U_SUCCESS(ue)) {
        Unc_Size n = unc0_utf8enc(uc, sizeof(buf), (byte *)buf);
        e = unc_newstring(w, &v, n, buf);
    }
    if (!e) e = unc_pushmove(w, &v, NULL);
    return e;
#else
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
}

#if UNCIL_LIB_ICU
Unc_RetVal unc0_lib_unic1char(Unc_View *w, Unc_Tuple args,
                              UChar32 *puc) {
    int e;
    Unc_Size sn;
    const char *sp;

    e = unc_getstring(w, &args.values[0], &sn, &sp);
    if (e) return e;

    if (!sn) return unc_throwexc(w, "value", "no character given");
    *puc = unc0_utf8decd((const byte *)sp);
    return 0;
}
#endif

Unc_RetVal unc0_lib_unic_name(Unc_View *w, Unc_Tuple args, void *udata) {
#if UNCIL_LIB_ICU
    Unc_Value v = UNC_BLANK;
    int e;
    UChar32 uc;
    UErrorCode ue = U_ZERO_ERROR;
    char buf[256];
    int32_t sl;
    
    e = unc0_lib_unic1char(w, args, &uc);
    if (e) return e;
    
    sl = u_charName(uc, U_UNICODE_CHAR_NAME, buf, sizeof(buf), &ue);
    if (U_FAILURE(ue) || !sl)
        return unc_pushmove(w, &v, NULL);
    if (sl + 1 >= sizeof(buf)) {
        char *p = unc_malloc(w, sl + 1);
        if (!p) return UNCIL_ERR_MEM;
        sl = u_charName(uc, U_EXTENDED_CHAR_NAME, p, sl + 1, &ue);
        e = unc_newstringmove(w, &v, sl, p);
        if (e) unc_mfree(w, p);
    } else {
        e = unc_newstring(w, &v, sl, buf);
    }
    if (!e) e = unc_pushmove(w, &v, NULL);
    return e;
#else
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
}

Unc_RetVal unc0_lib_unic_assigned(Unc_View *w, Unc_Tuple args, void *udata) {
#if UNCIL_LIB_ICU
    Unc_Value v = UNC_BLANK;
    int e;
    UChar32 uc;
    
    e = unc0_lib_unic1char(w, args, &uc);
    if (e) return e;
    
    unc_setbool(w, &v, u_charType(uc) != U_UNASSIGNED);
    return unc_pushmove(w, &v, NULL);
#else
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
}

Unc_RetVal unc0_lib_unic_bidi(Unc_View *w, Unc_Tuple args, void *udata) {
#if UNCIL_LIB_ICU
    Unc_Value v = UNC_BLANK;
    int e;
    UChar32 uc;
    const char *pname;
    
    e = unc0_lib_unic1char(w, args, &uc);
    if (e) return e;

    pname = u_getPropertyValueName(UCHAR_BIDI_CLASS,
        u_charDirection(uc), U_SHORT_PROPERTY_NAME);
    if (pname) e = unc_newstringc(w, &v, pname);
    if (!e) unc_pushmove(w, &v, NULL);
    return e;
#else
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
}

Unc_RetVal unc0_lib_unic_block(Unc_View *w, Unc_Tuple args, void *udata) {
#if UNCIL_LIB_ICU
    Unc_Value v = UNC_BLANK;
    int e;
    UChar32 uc;
    const char *pname;
    
    e = unc0_lib_unic1char(w, args, &uc);
    if (e) return e;

    pname = u_getPropertyValueName(UCHAR_BLOCK,
        ublock_getCode(uc), U_LONG_PROPERTY_NAME);
    if (pname) e = unc_newstringc(w, &v, pname);
    if (!e) unc_pushmove(w, &v, NULL);
    return e;
#else
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
}

Unc_RetVal unc0_lib_unic_category(Unc_View *w, Unc_Tuple args, void *udata) {
#if UNCIL_LIB_ICU
    Unc_Value v = UNC_BLANK;
    int e;
    UChar32 uc;
    const char *pname;
    
    e = unc0_lib_unic1char(w, args, &uc);
    if (e) return e;

    pname = u_getPropertyValueName(UCHAR_GENERAL_CATEGORY,
        u_charType(uc), U_SHORT_PROPERTY_NAME);
    if (pname) e = unc_newstringc(w, &v, pname);
    if (!e) unc_pushmove(w, &v, NULL);
    return e;
#else
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
}

Unc_RetVal unc0_lib_unic_combining(Unc_View *w, Unc_Tuple args, void *udata) {
#if UNCIL_LIB_ICU
    Unc_Value v = UNC_BLANK;
    int e;
    UChar32 uc;
    
    e = unc0_lib_unic1char(w, args, &uc);
    if (e) return e;
    unc_setint(w, &v, u_getCombiningClass(uc));
    return unc_pushmove(w, &v, NULL);
#else
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
}

Unc_RetVal unc0_lib_unic_decimal(Unc_View *w, Unc_Tuple args, void *udata) {
#if UNCIL_LIB_ICU
    Unc_Value v = UNC_BLANK;
    int e;
    UChar32 uc;
    int d;
    
    e = unc0_lib_unic1char(w, args, &uc);
    if (e) return e;
    d = u_charDigitValue(uc);
    if (d != -1) unc_setint(w, &v, d);
    return unc_pushmove(w, &v, NULL);
#else
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
}

#if UNCIL_LIB_ICU
INLINE Unc_Size u32charcount(const UChar *buf, Unc_Size n) {
    Unc_Size i, k = 0;
    for (i = 0; i < n; ++i)
        k += (buf[i] & 0xFC00) != 0xDC00;
    return k;
}

INLINE int u32charnext(const UChar **buf, Unc_Size *n, UChar32 *out) {
    if (!*n) return 0;
    if ((**buf & 0xFC00) == 0xD800) {
        UChar32 u;
        u = ((*buf)[0] & 0x3FF) << 10;
        u |= ((*buf)[1] & 0x3FF) << 10;
        u += 0x10000UL;
        *buf += 2;
        *n -= 2;
        *out = u;
        return 1;
    } else {
        *out = *(*buf)++;
        --*n;
        return 1;
    }
}
#endif

Unc_RetVal unc0_lib_unic_decompose(Unc_View *w, Unc_Tuple args, void *udata) {
#if UNCIL_LIB_ICU
    Unc_Value v = UNC_BLANK, *p;
    int e;
    UChar32 uc;
    UChar buf[32];
    const UChar *cbuf = buf;
    int dd;
    Unc_Size d, t;
    UErrorCode ue = U_ZERO_ERROR;
    const UNormalizer2 *np;
    
    e = unc0_lib_unic1char(w, args, &uc);
    if (e) return e;
    np = unorm2_getNFKDInstance(&ue);
    if (!np) return unc_pushmove(w, &v, NULL);
    dd = unorm2_getDecomposition(np, uc, buf, sizeof(buf), &ue);
    if (U_FAILURE(ue) || dd < 0) return unc_pushmove(w, &v, NULL);
    d = dd;
    /* count number of UTF-32 chars */
    t = u32charcount(buf, d);
    e = unc_newarray(w, &v, t, &p);
    if (e) return e;

    t = 0;
    while (u32charnext(&cbuf, &d, &uc))
        unc_setint(w, &p[t++], uc);

    unc_unlock(w, &v);
    return unc_pushmove(w, &v, NULL);
#else
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
}

Unc_RetVal unc0_lib_unic_digit(Unc_View *w, Unc_Tuple args, void *udata) {
#if UNCIL_LIB_ICU
    Unc_Value v = UNC_BLANK;
    int e;
    UChar32 uc;
    int d;
    
    e = unc0_lib_unic1char(w, args, &uc);
    if (e) return e;
    d = u_digit(uc, 10);
    if (d != -1) unc_setint(w, &v, d);
    return unc_pushmove(w, &v, NULL);
#else
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
}

Unc_RetVal unc0_lib_unic_eawidth(Unc_View *w, Unc_Tuple args, void *udata) {
#if UNCIL_LIB_ICU
    Unc_Value v = UNC_BLANK;
    int e;
    UChar32 uc;
    const char *pname;
    
    e = unc0_lib_unic1char(w, args, &uc);
    if (e) return e;

    pname = u_getPropertyValueName(UCHAR_EAST_ASIAN_WIDTH,
        u_getIntPropertyValue(uc, UCHAR_EAST_ASIAN_WIDTH),
        U_SHORT_PROPERTY_NAME);
    if (pname) e = unc_newstringc(w, &v, pname);
    if (!e) unc_pushmove(w, &v, NULL);
    return e;
#else
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
}

Unc_RetVal unc0_lib_unic_mirrored(Unc_View *w, Unc_Tuple args, void *udata) {
#if UNCIL_LIB_ICU
    Unc_Value v = UNC_BLANK;
    int e;
    UChar32 uc;
    
    e = unc0_lib_unic1char(w, args, &uc);
    if (e) return e;

    unc_setbool(w, &v, u_isMirrored(uc));
    return unc_pushmove(w, &v, NULL);
#else
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
}

Unc_RetVal unc0_lib_unic_noncharacter(Unc_View *w,
                                      Unc_Tuple args, void *udata) {
#if UNCIL_LIB_ICU
    Unc_Value v = UNC_BLANK;
    int e;
    UChar32 uc;
    
    e = unc0_lib_unic1char(w, args, &uc);
    if (e) return e;

    unc_setbool(w, &v, (UBool)u_getIntPropertyValue(uc,
                            UCHAR_NONCHARACTER_CODE_POINT));
    return unc_pushmove(w, &v, NULL);
#else
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
}

Unc_RetVal unc0_lib_unic_numeric(Unc_View *w, Unc_Tuple args, void *udata) {
#if UNCIL_LIB_ICU
    Unc_Value v = UNC_BLANK;
    int e;
    UChar32 uc;
    double d;
    
    e = unc0_lib_unic1char(w, args, &uc);
    if (e) return e;
    d = u_getNumericValue(uc);
    if (d != U_NO_NUMERIC_VALUE) unc_setfloat(w, &v, d);
    return unc_pushmove(w, &v, NULL);
#else
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
}

Unc_RetVal unc0_lib_unic_private(Unc_View *w, Unc_Tuple args, void *udata) {
#if UNCIL_LIB_ICU
    Unc_Value v = UNC_BLANK;
    int e;
    UChar32 uc;
    
    e = unc0_lib_unic1char(w, args, &uc);
    if (e) return e;

    unc_setbool(w, &v, u_charType(uc) == U_PRIVATE_USE_CHAR);
    return unc_pushmove(w, &v, NULL);
#else
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
}

Unc_RetVal unc0_lib_unic_surrogate(Unc_View *w, Unc_Tuple args, void *udata) {
#if UNCIL_LIB_ICU
    Unc_Value v = UNC_BLANK;
    int e;
    UChar32 uc;
    
    e = unc0_lib_unic1char(w, args, &uc);
    if (e) return e;

    unc_setbool(w, &v, u_charType(uc) == U_SURROGATE);
    return unc_pushmove(w, &v, NULL);
#else
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
}

Unc_RetVal unc0_lib_unic_glength(Unc_View *w, Unc_Tuple args, void *udata) {
#if UNCIL_LIB_ICU
    Unc_Value v = UNC_BLANK;
    int e;
    Unc_Size sn;
    const char *sp;
    UText ut = UTEXT_INITIALIZER;
    UErrorCode ue = U_ZERO_ERROR;
    UBreakIterator *ubi;
    icu_u8index_t oc;
    Unc_Size ai = 0;

    e = unc_getstring(w, &args.values[0], &sn, &sp);
    if (e) return e;

    if (sn > ICU_U8INDEX_MAX || sn > UNC_INT_MAX) {
        return unc_throwexc(w, "internal", "string too long for ICU");
    }

    utext_openUTF8(&ut, sp, sn, &ue);
    if (U_FAILURE(ue)) {
        return unc_throwexc(w, "internal", "utext_openUTF8 failed");
    }
    ubi = ubrk_open(UBRK_CHARACTER, uloc_getDefault(), NULL, 0, &ue);
    if (!ubi) {
        utext_close(&ut);
        return unc_throwexc(w, "internal", "ubrk_open failed");
    }
    ubrk_setUText(ubi, &ut, &ue);
    oc = ubrk_first(ubi);

    while (oc != UBRK_DONE) {
        oc = ubrk_next(ubi);
        if (oc == UBRK_DONE)
            break;
        ++ai;
    }

    ubrk_close(ubi);
    utext_close(&ut);
    unc_setint(w, &v, ai);
    return unc_pushmove(w, &v, NULL);
#else
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
}

Unc_RetVal unc0_lib_unic_graphemes(Unc_View *w, Unc_Tuple args, void *udata) {
#if UNCIL_LIB_ICU
    Unc_Value v = UNC_BLANK, tmp = UNC_BLANK;
    int e;
    Unc_Size sn;
    const char *sp;
    UText ut = UTEXT_INITIALIZER;
    UErrorCode ue = U_ZERO_ERROR;
    UBreakIterator *ubi;
    icu_u8index_t op, oc;
    Unc_Size an, ai = 0;
    Unc_Value *ap;

    e = unc_getstring(w, &args.values[0], &sn, &sp);
    if (e) return e;

    if (sn > ICU_U8INDEX_MAX)
        return unc_throwexc(w, "internal", "string too long for ICU");

    utext_openUTF8(&ut, sp, sn, &ue);
    if (U_FAILURE(ue)) {
        return unc_throwexc(w, "internal", "utext_openUTF8 failed");
    }
    ubi = ubrk_open(UBRK_CHARACTER, uloc_getDefault(), NULL, 0, &ue);
    if (!ubi) {
        utext_close(&ut);
        return unc_throwexc(w, "internal", "ubrk_open failed");
    }
    ubrk_setUText(ubi, &ut, &ue);
    oc = ubrk_first(ubi);

    an = 0;
    e = unc_newarray(w, &v, 0, &ap);
    if (e) {
        ubrk_close(ubi);
        utext_close(&ut);
        return e;
    }

    while (oc != UBRK_DONE) {
        op = oc;
        oc = ubrk_next(ubi);
        if (oc == UBRK_DONE)
            break;
        if (ai >= an)
            e = unc_resizearray(w, &v, (an = an + 32), &ap);
        if (!e) {
            e = unc_newstring(w, &tmp, oc - op, sp + op);
            if (!e) unc_copy(w, &ap[ai++], &tmp);
        }
        
        if (e) {
            ubrk_close(ubi);
            utext_close(&ut);
            unc_unlock(w, &v);
            unc_clear(w, &tmp);
            unc_clear(w, &v);
            return e;
        }
    }

    e = unc_resizearray(w, &v, ai, &ap);
    unc_unlock(w, &v);
    unc_clear(w, &tmp);
    ubrk_close(ubi);
    utext_close(&ut);
    if (!e) e = unc_pushmove(w, &v, NULL);
    return e;
#else
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
}

Unc_RetVal unc0_lib_unic_lower(Unc_View *w, Unc_Tuple args, void *udata) {
#if UNCIL_LIB_ICU
    int e;
    Unc_Value v = UNC_BLANK;
    Unc_Size sn;
    const char *sp, *locale = NULL;
    char buf[256], *sl = NULL;
    UErrorCode ue = U_ZERO_ERROR;
    UCaseMap *ucm;
    icu_u8index_t res;

    e = unc_getstring(w, &args.values[0], &sn, &sp);
    if (e) return e;

    if (unc_gettype(w, &args.values[1])) {
        Unc_Size locn;
        e = unc_getstring(w, &args.values[1], &locn, &locale);
        if (e) return e;
    }

    if (sn > ICU_U8INDEX_MAX)
        return unc_throwexc(w, "internal", "string too long for ICU");

    ucm = ucasemap_open(locale, U_FOLD_CASE_DEFAULT, &ue);
    if (!ucm || U_FAILURE(ue)) {
        if (ue == U_BUFFER_OVERFLOW_ERROR)
            return unc_throwexc(w, "value", "invalid locale");
        return unc_throwexc(w, "internal", "ucasemap_open failed");
    }

    res = ucasemap_utf8ToLower(ucm, buf, sizeof(buf), sp, sn, &ue);
    if (ue == U_BUFFER_OVERFLOW_ERROR) {
        ue = U_ZERO_ERROR;
        sl = unc_malloc(w, res);
        if (!sl) {
            ucasemap_close(ucm);
            return UNCIL_ERR_MEM;
        }
        res = ucasemap_utf8ToLower(ucm, sl, res, sp, sn, &ue);
    }
    if (U_FAILURE(ue)) {
        if (sl) unc_mfree(w, sl);
        return unc_throwexc(w, "internal", "ucasemap_utf8ToLower failed");
    }
    
    if (sl) {
        e = unc_newstringmove(w, &v, res, sl);
        if (e) unc_mfree(w, sl);
    } else
        e = unc_newstring(w, &v, res, buf);

    ucasemap_close(ucm);
    if (!e) e = unc_pushmove(w, &v, NULL);
    return e;
#else
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
}

Unc_RetVal unc0_lib_unic_upper(Unc_View *w, Unc_Tuple args, void *udata) {
#if UNCIL_LIB_ICU
    int e;
    Unc_Value v = UNC_BLANK;
    Unc_Size sn;
    const char *sp, *locale = NULL;
    char buf[256], *sl = NULL;
    UErrorCode ue = U_ZERO_ERROR;
    UCaseMap *ucm;
    icu_u8index_t res;

    e = unc_getstring(w, &args.values[0], &sn, &sp);
    if (e) return e;

    if (unc_gettype(w, &args.values[1])) {
        Unc_Size locn;
        e = unc_getstring(w, &args.values[1], &locn, &locale);
        if (e) return e;
    }

    if (sn > ICU_U8INDEX_MAX)
        return unc_throwexc(w, "internal", "string too long for ICU");

    ucm = ucasemap_open(locale, U_FOLD_CASE_DEFAULT, &ue);
    if (!ucm || U_FAILURE(ue)) {
        if (ue == U_BUFFER_OVERFLOW_ERROR)
            return unc_throwexc(w, "value", "invalid locale");
        return unc_throwexc(w, "internal", "ucasemap_open failed");
    }

    res = ucasemap_utf8ToUpper(ucm, buf, sizeof(buf), sp, sn, &ue);
    if (ue == U_BUFFER_OVERFLOW_ERROR) {
        ue = U_ZERO_ERROR;
        sl = unc_malloc(w, res);
        if (!sl) {
            ucasemap_close(ucm);
            return UNCIL_ERR_MEM;
        }
        res = ucasemap_utf8ToUpper(ucm, sl, res, sp, sn, &ue);
    }
    if (U_FAILURE(ue)) {
        if (sl) unc_mfree(w, sl);
        return unc_throwexc(w, "internal", "ucasemap_utf8ToUpper failed");
    }
    
    if (sl){
        e = unc_newstringmove(w, &v, res, sl);
        if (e) unc_mfree(w, sl);
    } else
        e = unc_newstring(w, &v, res, buf);

    ucasemap_close(ucm);
    if (!e) e = unc_pushmove(w, &v, NULL);
    return e;
#else
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
}

Unc_RetVal unc0_lib_unic_title(Unc_View *w, Unc_Tuple args, void *udata) {
#if UNCIL_LIB_ICU
    int e;
    Unc_Value v = UNC_BLANK;
    Unc_Size sn;
    const char *sp, *locale = NULL;
    char buf[256], *sl = NULL;
    UErrorCode ue = U_ZERO_ERROR;
    UCaseMap *ucm;
    icu_u8index_t res;

    e = unc_getstring(w, &args.values[0], &sn, &sp);
    if (e) return e;

    if (unc_gettype(w, &args.values[1])) {
        Unc_Size locn;
        e = unc_getstring(w, &args.values[1], &locn, &locale);
        if (e) return e;
    }

    if (sn > ICU_U8INDEX_MAX)
        return unc_throwexc(w, "internal", "string too long for ICU");

    ucm = ucasemap_open(locale, U_FOLD_CASE_DEFAULT, &ue);
    if (!ucm || U_FAILURE(ue)) {
        if (ue == U_BUFFER_OVERFLOW_ERROR)
            return unc_throwexc(w, "value", "invalid locale");
        return unc_throwexc(w, "internal", "ucasemap_open failed");
    }

    res = ucasemap_utf8ToTitle(ucm, buf, sizeof(buf), sp, sn, &ue);
    if (ue == U_BUFFER_OVERFLOW_ERROR) {
        ue = U_ZERO_ERROR;
        sl = unc_malloc(w, res);
        if (!sl) {
            ucasemap_close(ucm);
            return UNCIL_ERR_MEM;
        }
        res = ucasemap_utf8ToTitle(ucm, sl, res, sp, sn, &ue);
    }
    if (U_FAILURE(ue)) {
        if (sl) unc_mfree(w, sl);
        return unc_throwexc(w, "internal", "ucasemap_utf8ToTitle failed");
    }
    
    if (sl){
        e = unc_newstringmove(w, &v, res, sl);
        if (e) unc_mfree(w, sl);
    } else
        e = unc_newstring(w, &v, res, buf);

    ucasemap_close(ucm);
    if (!e) e = unc_pushmove(w, &v, NULL);
    return e;
#else
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
}

Unc_RetVal unc0_lib_unic_icmp(Unc_View *w, Unc_Tuple args, void *udata) {
#if UNCIL_LIB_ICU
    int e, cmp = 0;
    Unc_Value v = UNC_BLANK;
    Unc_Size sn, zn;
    const char *sp, *zp, *locale = NULL;
    char sb[256], *sl = NULL, *si;
    char zb[256], *zl = NULL, *zi;
    UErrorCode ue = U_ZERO_ERROR;
    UCaseMap *ucm;
    icu_u8index_t sq, zq;
    Unc_Size sq1, zq1;

    e = unc_getstring(w, &args.values[0], &sn, &sp);
    if (e) return e;

    e = unc_getstring(w, &args.values[1], &zn, &zp);
    if (e) return e;

    if (unc_gettype(w, &args.values[2])) {
        Unc_Size locn;
        e = unc_getstring(w, &args.values[2], &locn, &locale);
        if (e) return e;
    }

    if (sn > ICU_U8INDEX_MAX || zn > ICU_U8INDEX_MAX)
        return unc_throwexc(w, "internal", "string too long for ICU");

    ucm = ucasemap_open(locale, U_FOLD_CASE_DEFAULT, &ue);
    if (!ucm || U_FAILURE(ue)) {
        if (ue == U_BUFFER_OVERFLOW_ERROR)
            return unc_throwexc(w, "value", "invalid locale");
        return unc_throwexc(w, "internal", "ucasemap_open failed");
    }

    /* TODO: if ICU adds u_utf8strCaseCompare one day */
    sq = ucasemap_utf8FoldCase(ucm, sb, sizeof(sb), sp, sn, &ue);
    if (ue == U_BUFFER_OVERFLOW_ERROR) {
        ue = U_ZERO_ERROR;
        sl = unc_malloc(w, sq);
        if (!sl) {
            ucasemap_close(ucm);
            return UNCIL_ERR_MEM;
        }
        sq = ucasemap_utf8FoldCase(ucm, sl, sq, sp, sn, &ue);
    }
    if (U_FAILURE(ue)) {
        if (sl) unc_mfree(w, sl);
        return unc_throwexc(w, "internal", "ucasemap_utf8FoldCase failed");
    }
    
    zq = ucasemap_utf8FoldCase(ucm, zb, sizeof(zb), zp, zn, &ue);
    if (ue == U_BUFFER_OVERFLOW_ERROR) {
        ue = U_ZERO_ERROR;
        zl = unc_malloc(w, zq);
        if (!zl) {
            ucasemap_close(ucm);
            return UNCIL_ERR_MEM;
        }
        zq = ucasemap_utf8FoldCase(ucm, zl, zq, zp, zn, &ue);
    }
    if (U_FAILURE(ue)) {
        if (zl) unc_mfree(w, zl);
        if (sl) unc_mfree(w, sl);
        return unc_throwexc(w, "internal", "ucasemap_utf8FoldCase failed");
    }

    ucasemap_close(ucm);
    si = sl ? sl : sb, sq1 = sq;
    zi = zl ? zl : zb, zq1 = zq;

    while (sq1 && zq1) {
        Unc_UChar us = unc0_utf8decx(&sq1, (const byte **)&si);
        Unc_UChar uz = unc0_utf8decx(&zq1, (const byte **)&zi);
        if (us != uz) {
            if (us < uz) {
                cmp = -1;
                break;
            } else {
                cmp = 1;
                break;
            }
        }
    }

    if (!cmp) {
        if (sq1 && !zq1)
            cmp = 1;
        else if (!sq1 && zq1)
            cmp = -1;
    }
    
    if (zl) unc_mfree(w, zl);
    if (sl) unc_mfree(w, sl);
    unc_setint(w, &v, cmp);
    return unc_pushmove(w, &v, NULL);
#else
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
}

Unc_RetVal unc0_lib_unic_trim(Unc_View *w, Unc_Tuple args, void *udata) {
#if UNCIL_LIB_ICU
    int e;
    Unc_Value v = UNC_BLANK;
    Unc_Size sn;
    const char *sp;
    
    e = unc_getstring(w, &args.values[0], &sn, &sp);
    if (e) return e;

    while (sn) {
        Unc_UChar u = unc0_utf8decd((const byte *)sp);
        if (!u_isspace(u)) break;
        sp = (const char *)unc0_utf8nextchar((const byte *)sp, &sn);
    }

    while (sn) {
        Unc_UChar u;
        const char *su = (const char *)unc0_utf8lastchar((const byte *)sp, sn);
        if (!su) break;
        u = unc0_utf8decd((const byte *)su);
        if (!u_isspace(u)) break;
        sn = su - sp;
    }

    e = unc_newstring(w, &v, sn, sp);
    if (!e) e = unc_pushmove(w, &v, NULL);
    return e;
#else
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
}

#if UNCIL_LIB_ICU
int unc0_u8tou16(Unc_View *w, Unc_Size sn, const char *sp,
                              Unc_Size *qn, UChar **qp) {
    Unc_Size i, z = 0;
    Unc_UChar u;
    UChar *q;
    for (i = 0; i < sn; ++i) {
        z += ((byte)sp[i] & 0xC0) != 0x80;
        z += ((byte)sp[i] & 0xF0) == 0xF0;
    }
    q = unc_malloc(w, sizeof(UChar) * z);
    if (!q) return UNCIL_ERR_MEM;
    *qn = z;
    *qp = q;
    while (sn) {
        u = unc0_utf8decx(&sn, (const byte **)&sp);
        if (u >= 0x10000) {
            u -= 0x10000;
            *q++ = (UChar)(0xD800 | ((u >> 10) & 0x3FF));
            *q++ = (UChar)(0xDC00 | (u & 0x3FF));
        } else {
            *q++ = (UChar)u;
        }
    }
    return 0;
}

int unc0_u16tou8(Unc_View *w, Unc_Size qn, const UChar *qp,
                              Unc_Size *sn, char **sp) {
    Unc_Size i, z = 0;
    Unc_UChar u;
    char *s, *se;
    for (i = 0; i < qn; ++i) {
        if ((qp[i] & 0xF800) == 0xD800)
            z += 2; /* called twice for each surrogate, total z += 4 */
        else if (qp[i] & 0xF800)
            z += 3;
        else
            z += qp[i] & 0xFF80 ? 2 : 1;
    }
    s = unc_malloc(w, z);
    if (!s) return UNCIL_ERR_MEM;
    *sn = z;
    *sp = s;
    se = s + z;
    for (i = 0; i < qn; ++i) {
        u = qp[i];
        if ((u & 0xF800) == 0xD800) {
            u = (u & 0x3FF) << 10;
            u |= (qp[++i] & 0x3FF);
            u += 0x10000;
        }
        s += unc0_utf8enc(u, se - s, (byte *)s);
    }
    return 0;
}
#endif

Unc_RetVal unc0_lib_unic_normalize(Unc_View *w, Unc_Tuple args, void *udata) {
#if UNCIL_LIB_ICU
#define NORMC 0
#define NORMD 1
#define NORMK 2
    int e;
    Unc_Value v = UNC_BLANK;
    Unc_Size sn, tn, bn, cn;
    const char *sp, *mode;
    char *tp;
    UErrorCode ue = U_ZERO_ERROR;
    int nm = 0;
    const UNormalizer2 *norm;
    UChar *bp, *cp;

    e = unc_getstring(w, &args.values[0], &sn, &sp);
    if (e) return e;

    if (sn > ICU_U8INDEX_MAX)
        return unc_throwexc(w, "internal", "string too long for ICU");

    {
        Unc_Size moden;
        e = unc_getstring(w, &args.values[1], &moden, &mode);
        if (e) return e;
    }

    if (unc0_tolower(*mode) == 'n') {
        ++mode;
        if (unc0_tolower(*mode) == 'f') {
            ++mode;
        }
    }

    if (unc0_tolower(*mode) == 'k') {
        ++mode;
        nm |= NORMK;
    }

    switch (unc0_tolower(*mode)) {
    case 'c':
        nm |= NORMC;
        break;
    case 'd':
        nm |= NORMD;
        break;
    default:
        return unc_throwexc(w, "value", "invalid normalization mode");
    }

    e = unc0_u8tou16(w, sn, sp, &bn, &bp);
    if (e) return e;

    if (nm & NORMK)
        norm = nm & NORMD ? unorm2_getNFKDInstance(&ue)
                          : unorm2_getNFKCInstance(&ue);
    else
        norm = nm & NORMD ? unorm2_getNFDInstance(&ue)
                          : unorm2_getNFCInstance(&ue);
    if (U_FAILURE(ue))
        return unc_throwexc(w, "internal", "normalization failure");

    cn = unorm2_normalize(norm, bp, bn, NULL, 0, &ue);
    cp = unc_malloc(w, sizeof(UChar) * cn);
    if (!cp) {
        unc_mfree(w, bp);
        return UNCIL_ERR_MEM;
    }
    ue = U_ZERO_ERROR;
    cn = unorm2_normalize(norm, bp, bn, cp, cn, &ue);
    unc_mfree(w, bp);
    if (U_FAILURE(ue)) {
        unc_mfree(w, cp);
        return unc_throwexc(w, "internal", "unorm2_normalize failed");
    }
    
    e = unc0_u16tou8(w, cn, cp, &tn, &tp);
    unc_mfree(w, cp);
    if (e) {
        return e;
    }

    e = unc_newstringmove(w, &v, tn, tp);
    if (!e) e = unc_pushmove(w, &v, NULL);
    else unc_mfree(w, tp);
    return e;
#else
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
}

#define ONECHARFUNC(x)                                                         \
    e = unc_exportcfunction(w, #x, &unc0_lib_unic_##x,                         \
                            UNC_CFUNC_CONCURRENT,                              \
                            1, 0, 0, NULL, 0, NULL, 0, NULL, NULL);            \
    if (e) return e;                                                           \

Unc_RetVal uncilmain_unicode(Unc_View *w) {
    Unc_RetVal e;

    e = unc_exportcfunction(w, "lookup", &unc0_lib_unic_lookup,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;

    ONECHARFUNC(name)
    ONECHARFUNC(assigned)
    ONECHARFUNC(bidi)
    ONECHARFUNC(block)
    ONECHARFUNC(category)
    ONECHARFUNC(combining)
    ONECHARFUNC(decimal)
    ONECHARFUNC(decompose)
    ONECHARFUNC(digit)
    ONECHARFUNC(eawidth)
    ONECHARFUNC(mirrored)
    ONECHARFUNC(noncharacter)
    ONECHARFUNC(numeric)
    ONECHARFUNC(private)
    ONECHARFUNC(surrogate)

    e = unc_exportcfunction(w, "glength", &unc0_lib_unic_glength,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;

    e = unc_exportcfunction(w, "graphemes", &unc0_lib_unic_graphemes,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;

    e = unc_exportcfunction(w, "lower", &unc0_lib_unic_lower,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 1, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;

    e = unc_exportcfunction(w, "upper", &unc0_lib_unic_upper,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 1, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;

    e = unc_exportcfunction(w, "title", &unc0_lib_unic_title,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 1, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;

    e = unc_exportcfunction(w, "icmp", &unc0_lib_unic_icmp,
                            UNC_CFUNC_CONCURRENT,
                            2, 0, 1, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;

    e = unc_exportcfunction(w, "trim", &unc0_lib_unic_trim,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;

    e = unc_exportcfunction(w, "normalize", &unc0_lib_unic_normalize,
                            UNC_CFUNC_CONCURRENT,
                            2, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;

    {
        Unc_Value v = UNC_BLANK;
#if UNCIL_LIB_ICU
        e = unc_newstringc(w, &v, U_UNICODE_VERSION);
        if (e) return e;
#else
        unc_setnull(w, &v);
#endif
        e = unc_setpublicc(w, "version", &v);
        if (e) return e;
        unc_clear(w, &v);
    }
    return 0;
}
