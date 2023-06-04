/*******************************************************************************
 
Uncil -- builtin unicode library impl

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

#include "uctype.h"
#include "uncil.h"
#include "uutf.h"

struct unc_uchar_result {
    Unc_RetVal e;
    Unc_UChar u;
};

INLINE struct unc_uchar_result uchar_result_err(Unc_RetVal e) {
    struct unc_uchar_result s;
    s.e = e;
    s.u = UNC_UTF8_NO_CHAR;
    return s;
}

INLINE struct unc_uchar_result uchar_result_none(void) {
    return uchar_result_err(0);
}

INLINE struct unc_uchar_result uchar_result_ok(Unc_UChar result) {
    struct unc_uchar_result s;
    s.e = 0;
    s.u = result;
    return s;
}

typedef int Unc_UNormType;
#define UNC_UNORMTYPE_C 0
#define UNC_UNORMTYPE_D 1
#define UNC_UNORMTYPE_K 2

static const char *unc0_unic_version(void);
static struct unc_uchar_result unc0_unic_lookup(const char *name);
static Unc_RetVal unc0_unic_name(Unc_UChar c, size_t sbuf_n, char *sbuf,
                                 size_t *psl);
static int unc0_unic_assigned(Unc_UChar uc);
static const char *unc0_unic_bidi(Unc_UChar uc);
static const char *unc0_unic_block(Unc_UChar uc);
static const char *unc0_unic_category(Unc_UChar uc);
static int unc0_unic_combining(Unc_UChar uc);
static int unc0_unic_decimal(Unc_UChar uc, int *out);
static int unc0_unic_digit(Unc_UChar uc, int *out);
static const char *unc0_unic_eawidth(Unc_UChar uc);
static int unc0_unic_mirrored(Unc_UChar uc);
static int unc0_unic_noncharacter(Unc_UChar uc);
static int unc0_unic_numeric(Unc_UChar uc, double *out);
static int unc0_unic_private(Unc_UChar uc);
static int unc0_unic_surrogate(Unc_UChar uc);

struct unc_uchar_decompose_buf;
static Unc_RetVal unc0_unic_decompose_init(struct unc_uchar_decompose_buf *buf,
                                           Unc_View *w, Unc_UChar c) ;
static int unc0_unic_decompose_next(struct unc_uchar_decompose_buf *buf,
                                    Unc_UChar *outc);
static size_t unc0_unic_decompose_size(struct unc_uchar_decompose_buf *buf);
static void unc0_unic_decompose_free(struct unc_uchar_decompose_buf *buf);

struct unc_uchar_graphemes_buf;
static Unc_RetVal unc0_unic_graphemes_init(struct unc_uchar_graphemes_buf *buf,
                                           Unc_View *w,
                                           Unc_Size sn, const char *sp);
static int unc0_unic_graphemes_next(struct unc_uchar_graphemes_buf *buf,
                                    Unc_Size *outn,
                                    const char **outp);
static void unc0_unic_graphemes_free(struct unc_uchar_graphemes_buf *buf);

struct unc_uchar_casemap;
static Unc_RetVal unc0_unic_casemap_init(struct unc_uchar_casemap *casemap,
                                         Unc_View *w,
                                         const char *locale);
static void unc0_unic_casemap_free(struct unc_uchar_casemap *casemap);
static Unc_RetVal unc0_unic_lower(struct unc_uchar_casemap *casemap,
                                  Unc_View *w, size_t sn, const char *sp,
                                  size_t sbuf_n, char *sbuf, size_t *psl);
static Unc_RetVal unc0_unic_upper(struct unc_uchar_casemap *casemap,
                                  Unc_View *w, size_t sn, const char *sp,
                                  size_t sbuf_n, char *sbuf, size_t *psl);
static Unc_RetVal unc0_unic_title(struct unc_uchar_casemap *casemap,
                                  Unc_View *w, size_t sn, const char *sp,
                                  size_t sbuf_n, char *sbuf, size_t *psl);

static int unc0_unic_isspace(Unc_UChar uc);
static int unc0_unic_icmp(struct unc_uchar_casemap *casemap, Unc_View *w,
                          Unc_Size sn, const char *sp,
                          Unc_Size zn, const char *zp);
static Unc_RetVal unc0_unic_normalize(Unc_View *w,
                                      Unc_UNormType normtype,
                                      size_t sn, const char *sp,
                                      size_t *qn, char **qp);

#define UNICODESUPPORT 1

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

static const char *unc0_unic_version(void) {
    return U_UNICODE_VERSION;
}

static struct unc_uchar_result unc0_unic_lookup(const char *name) {
    UErrorCode ue = U_ZERO_ERROR;
    UChar32 uc = u_charFromName(U_UNICODE_CHAR_NAME, name, &ue);
    if (U_FAILURE(ue))
        uc = u_charFromName(U_EXTENDED_CHAR_NAME, name, &ue);
    if (U_SUCCESS(ue))
        return uchar_result_ok((Unc_UChar)uc);
    return uchar_result_none();
}

static Unc_RetVal unc0_unic_name(Unc_UChar c, size_t sbuf_n, char *sbuf,
                                 size_t *psl) {
    UChar32 uc = (UChar32)c;
    UErrorCode ue = U_ZERO_ERROR;
    size_t sl = u_charName(uc, U_EXTENDED_CHAR_NAME, sbuf, sbuf_n, &ue);
    if (U_FAILURE(ue) || !sl)
        return 0;
    *psl = sl;
    if (sl + 1 >= sbuf_n)
        return UNCIL_ERR_MEM;
    return 0;
}

static int unc0_unic_assigned(Unc_UChar uc) {
    return u_charType((UChar32)uc) != U_UNASSIGNED;
}

static const char *unc0_unic_bidi(Unc_UChar uc) {
    return u_getPropertyValueName(UCHAR_BIDI_CLASS,
        u_charDirection((UChar32)uc), U_SHORT_PROPERTY_NAME);
}

static const char *unc0_unic_block(Unc_UChar uc) {
    return u_getPropertyValueName(UCHAR_BLOCK,
        ublock_getCode((UChar32)uc), U_LONG_PROPERTY_NAME);
}

static const char *unc0_unic_category(Unc_UChar uc) {
    return u_getPropertyValueName(UCHAR_GENERAL_CATEGORY,
        u_charType((UChar32)uc), U_SHORT_PROPERTY_NAME);
}

static int unc0_unic_combining(Unc_UChar uc) {
    return u_getCombiningClass((UChar32)uc);
}

static int unc0_unic_decimal(Unc_UChar uc, int *out) {
    int d = u_charDigitValue((UChar32)uc);
    if (d < 0) return 0;
    *out = d;
    return 1;
}

static int unc0_unic_digit(Unc_UChar uc, int *out) {
    int d = u_digit((UChar32)uc, 10);
    if (d < 0) return 0;
    *out = d;
    return 1;
}

static const char *unc0_unic_eawidth(Unc_UChar uc) {
    return u_getPropertyValueName(UCHAR_EAST_ASIAN_WIDTH,
        u_getIntPropertyValue((UChar32)uc, UCHAR_EAST_ASIAN_WIDTH),
        U_SHORT_PROPERTY_NAME);
}

static int unc0_unic_mirrored(Unc_UChar uc) {
    return u_isMirrored((UChar32)uc);
}

static int unc0_unic_noncharacter(Unc_UChar uc) {
    return (UBool)u_getIntPropertyValue((UChar32)uc,
                                        UCHAR_NONCHARACTER_CODE_POINT);
}

static int unc0_unic_numeric(Unc_UChar uc, double *out) {
    double d;
    d = u_getNumericValue((UChar32)uc);
    if (d == U_NO_NUMERIC_VALUE) return 0;
    *out = d;
    return 1;
}

static int unc0_unic_private(Unc_UChar uc) {
    return u_charType((UChar32)uc) == U_PRIVATE_USE_CHAR;
}

static int unc0_unic_surrogate(Unc_UChar uc) {
    return u_charType((UChar32)uc) == U_SURROGATE;
}

INLINE Unc_Size u32charcount(const UChar *buf, Unc_Size n) {
    Unc_Size i, k = 0;
    for (i = 0; i < n; ++i)
        k += (buf[i] & 0xFC00) != 0xDC00;
    return k;
}

INLINE int u32charnext(const UChar **buf, Unc_Size *n, Unc_UChar *out) {
    if (!*n) return 0;
    if ((**buf & 0xFC00) == 0xD800) {
        Unc_UChar u;
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

struct unc_uchar_decompose_buf {
    const UNormalizer2 *normalizer;
    Unc_Size array_size;
    const UChar *uchar_ptr;
    Unc_Size uchar_size;
    UChar uchar_buf[32];
};

static Unc_RetVal unc0_unic_decompose_init(struct unc_uchar_decompose_buf *buf,
                                           Unc_View *w, Unc_UChar c) {
    UErrorCode ue = U_ZERO_ERROR;
    int dlen;
    buf->normalizer = unorm2_getNFKDInstance(&ue);
    if (!buf->normalizer)
        return unc_throwexc(w, "system", "NFKD normalization not available");
    dlen = unorm2_getDecomposition(buf->normalizer, (UChar32)c, buf->uchar_buf,
                                   ARRAYSIZE(buf->uchar_buf), &ue);
    if (U_FAILURE(ue) || dlen < 0) return 1;
    buf->array_size = u32charcount(buf->uchar_buf, (Unc_Size)dlen);
    buf->uchar_ptr = buf->uchar_buf;
    buf->uchar_size = (Unc_Size)dlen;
    return 0;
}

static int unc0_unic_decompose_next(struct unc_uchar_decompose_buf *buf,
                                    Unc_UChar *outc) {
    return u32charnext(&buf->uchar_ptr, &buf->uchar_size, outc);
}

static size_t unc0_unic_decompose_size(struct unc_uchar_decompose_buf *buf) {
    return buf->array_size;
}

static void unc0_unic_decompose_free(struct unc_uchar_decompose_buf *buf) {
}

struct unc_uchar_graphemes_buf {
    UText ut;
    UBreakIterator *ubi;
    icu_u8index_t op, oc;
    const char *sp;
};

static Unc_RetVal unc0_unic_graphemes_init(struct unc_uchar_graphemes_buf *buf,
                                           Unc_View *w,
                                           Unc_Size sn, const char *sp) {
    UErrorCode ue = U_ZERO_ERROR;
    if (sn > ICU_U8INDEX_MAX)
        return unc_throwexc(w, "system", "string too long for ICU");
    {
        UText temp = UTEXT_INITIALIZER;
        buf->ut = temp;
    }
    
    utext_openUTF8(&buf->ut, sp, sn, &ue);
    if (U_FAILURE(ue))
        return unc_throwexc(w, "system", "utext_openUTF8 failed");

    buf->ubi = ubrk_open(UBRK_CHARACTER, uloc_getDefault(), NULL, 0, &ue);
    if (!buf->ubi) {
        utext_close(&buf->ut);
        return unc_throwexc(w, "system", "ubrk_open failed");
    }
    ubrk_setUText(buf->ubi, &buf->ut, &ue);
    buf->oc = ubrk_first(buf->ubi);
    buf->sp = sp;
    return 0;
}

static int unc0_unic_graphemes_next(struct unc_uchar_graphemes_buf *buf,
                                    Unc_Size *outn,
                                    const char **outp) {
    buf->op = buf->oc;
    buf->oc = ubrk_next(buf->ubi);
    if (buf->oc == UBRK_DONE) return 0;
    *outn = buf->oc - buf->op;
    *outp = buf->sp + buf->op;
    return 1;
}

static void unc0_unic_graphemes_free(struct unc_uchar_graphemes_buf *buf) {
    ubrk_close(buf->ubi);
    utext_close(&buf->ut);
}

struct unc_uchar_casemap {
    UCaseMap *ucm;
};

static Unc_RetVal unc0_unic_casemap_init(struct unc_uchar_casemap *casemap,
                                         Unc_View *w,
                                         const char *locale) {
    UErrorCode ue = U_ZERO_ERROR;
    casemap->ucm = ucasemap_open(locale, U_FOLD_CASE_DEFAULT, &ue);
    if (!casemap->ucm || U_FAILURE(ue)) {
        if (ue == U_BUFFER_OVERFLOW_ERROR)
            return unc_throwexc(w, "value", "invalid locale");
        return unc_throwexc(w, "system", "ucasemap_open failed");
    }
    return 0;
}

static void unc0_unic_casemap_free(struct unc_uchar_casemap *casemap) {
    ucasemap_close(casemap->ucm);
}

static Unc_RetVal unc0_unic_lower(struct unc_uchar_casemap *casemap,
                                  Unc_View *w, size_t sn, const char *sp,
                                  size_t sbuf_n, char *sbuf, size_t *psl) {
    UErrorCode ue = U_ZERO_ERROR;
    icu_u8index_t res;
    if (sn > ICU_U8INDEX_MAX)
        return unc_throwexc(w, "system", "string too long for ICU");
    res = ucasemap_utf8ToLower(casemap->ucm, sbuf, sbuf_n, sp, sn, &ue);
    if (ue == U_BUFFER_OVERFLOW_ERROR)
        return UNCIL_ERR_MEM;
    if (U_FAILURE(ue))
        return unc_throwexc(w, "system", "ucasemap_utf8ToLower failed");
    *psl = res;
    return 0;
}

static Unc_RetVal unc0_unic_upper(struct unc_uchar_casemap *casemap,
                                  Unc_View *w, size_t sn, const char *sp,
                                  size_t sbuf_n, char *sbuf, size_t *psl) {
    UErrorCode ue = U_ZERO_ERROR;
    icu_u8index_t res;
    if (sn > ICU_U8INDEX_MAX)
        return unc_throwexc(w, "system", "string too long for ICU");
    res = ucasemap_utf8ToUpper(casemap->ucm, sbuf, sbuf_n, sp, sn, &ue);
    if (ue == U_BUFFER_OVERFLOW_ERROR)
        return UNCIL_ERR_MEM;
    if (U_FAILURE(ue))
        return unc_throwexc(w, "system", "ucasemap_utf8ToUpper failed");
    *psl = res;
    return 0;
}

static Unc_RetVal unc0_unic_title(struct unc_uchar_casemap *casemap,
                                  Unc_View *w, size_t sn, const char *sp,
                                  size_t sbuf_n, char *sbuf, size_t *psl) {
    UErrorCode ue = U_ZERO_ERROR;
    icu_u8index_t res;
    if (sn > ICU_U8INDEX_MAX)
        return unc_throwexc(w, "system", "string too long for ICU");
    res = ucasemap_utf8ToTitle(casemap->ucm, sbuf, sbuf_n, sp, sn, &ue);
    if (ue == U_BUFFER_OVERFLOW_ERROR)
        return UNCIL_ERR_MEM;
    if (U_FAILURE(ue))
        return unc_throwexc(w, "system", "ucasemap_utf8ToTitle failed");
    *psl = res;
    return 0;
}

static int unc0_unic_isspace(Unc_UChar uc) {
    return u_isspace(uc);
}

static int unc0_u8tou16(Unc_View *w, Unc_Size sn, const char *sp,
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

static int unc0_u16tou8(Unc_View *w, Unc_Size qn, const UChar *qp,
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
    s = unc_malloc(w, z + 1);
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
    *s = 0;
    return 0;
}

static int unc0_unic_icmp(struct unc_uchar_casemap *casemap, Unc_View *w,
                          Unc_Size sn, const char *sp,
                          Unc_Size zn, const char *zp) {
    int cmp = 0;
    char sb[256], *sl = NULL, *si;
    char zb[256], *zl = NULL, *zi;
    UErrorCode ue = U_ZERO_ERROR;
    UCaseMap *ucm;
    icu_u8index_t sq, zq;
    Unc_Size sq1, zq1;
    
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
    return cmp;
}

static Unc_RetVal unc0_unic_normalize(Unc_View *w,
                                      Unc_UNormType normtype,
                                      size_t sn, const char *sp,
                                      size_t *qn, char **qp) {
    Unc_RetVal e;
    Unc_Size tn, bn, cn;
    char *tp;
    UErrorCode ue = U_ZERO_ERROR;
    const UNormalizer2 *norm;
    UChar *bp, *cp;
    
    e = unc0_u8tou16(w, sn, sp, &bn, &bp);
    if (e) return e;

    if (normtype & UNC_UNORMTYPE_K)
        norm = normtype & UNC_UNORMTYPE_D ? unorm2_getNFKDInstance(&ue)
                                          : unorm2_getNFKCInstance(&ue);
    else
        norm = normtype & UNC_UNORMTYPE_D ? unorm2_getNFDInstance(&ue)
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

    *qn = tn;
    *qp = tp;
    return e;
}

#else /* stubs */

#undef UNICODESUPPORT

static const char *unc0_unic_version(void) {
    return NULL;
}

static struct unc_uchar_result unc0_unic_lookup(const char *name) {
    return uchar_result_err(UNCIL_ERR_LOGIC_NOTSUPPORTED);
}

static Unc_RetVal unc0_unic_name(Unc_UChar c, size_t sbuf_n, char *sbuf,
                                 size_t *psl) {
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
}

static int unc0_unic_assigned(Unc_UChar uc) {
    return 0;
}

static const char *unc0_unic_bidi(Unc_UChar uc) {
    return NULL;    
}

static const char *unc0_unic_block(Unc_UChar uc) {
    return NULL;    
}

static const char *unc0_unic_category(Unc_UChar uc) {
    return NULL;    
}

static int unc0_unic_combining(Unc_UChar uc) {
    return 0;
}

static int unc0_unic_decimal(Unc_UChar uc, int *out) {
    return 0;
}

static int unc0_unic_digit(Unc_UChar uc, int *out) {
    return 0;
}

static const char *unc0_unic_eawidth(Unc_UChar uc) {
    return 0;
}

static int unc0_unic_mirrored(Unc_UChar uc) {
    return 0;
}

static int unc0_unic_noncharacter(Unc_UChar uc) {
    return 0;
}

static int unc0_unic_numeric(Unc_UChar uc, double *out) {
    return 0;
}

static int unc0_unic_private(Unc_UChar uc) {
    return 0;
}

static int unc0_unic_surrogate(Unc_UChar uc) {
    return 0;
}

struct unc_uchar_decompose_buf {
    char c_;
};

static Unc_RetVal unc0_unic_decompose_init(struct unc_uchar_decompose_buf *buf,
                                           Unc_View *w, Unc_UChar c) {
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
}

static int unc0_unic_decompose_next(struct unc_uchar_decompose_buf *buf,
                                    Unc_UChar *outc) {
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
}

static size_t unc0_unic_decompose_size(struct unc_uchar_decompose_buf *buf) {
    return 0;
}

static void unc0_unic_decompose_free(struct unc_uchar_decompose_buf *buf) {
}

struct unc_uchar_graphemes_buf { char c_; };

static Unc_RetVal unc0_unic_graphemes_init(struct unc_uchar_graphemes_buf *buf,
                                           Unc_View *w,
                                           Unc_Size sn, const char *sp) {
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
}

static int unc0_unic_graphemes_next(struct unc_uchar_graphemes_buf *buf,
                                    Unc_Size *outn,
                                    const char **outp) {
    return 0;
}

static void unc0_unic_graphemes_free(struct unc_uchar_graphemes_buf *buf) {
}

struct unc_uchar_casemap { char c_; };

static Unc_RetVal unc0_unic_casemap_init(struct unc_uchar_casemap *casemap,
                                         Unc_View *w,
                                         const char *locale) {
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
}

static void unc0_unic_casemap_free(struct unc_uchar_casemap *casemap) {
}

static Unc_RetVal unc0_unic_lower(struct unc_uchar_casemap *casemap,
                                  Unc_View *w, size_t sn, const char *sp,
                                  size_t sbuf_n, char *sbuf, size_t *psl) {
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
}

static Unc_RetVal unc0_unic_upper(struct unc_uchar_casemap *casemap,
                                  Unc_View *w, size_t sn, const char *sp,
                                  size_t sbuf_n, char *sbuf, size_t *psl) {
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
}

static Unc_RetVal unc0_unic_title(struct unc_uchar_casemap *casemap,
                                  Unc_View *w, size_t sn, const char *sp,
                                  size_t sbuf_n, char *sbuf, size_t *psl) {
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
}

static int unc0_unic_isspace(Unc_UChar uc) {
    return 0;
}

static int unc0_unic_icmp(struct unc_uchar_casemap *casemap, Unc_View *w,
                          Unc_Size sn, const char *sp,
                          Unc_Size zn, const char *zp) {
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
}

static Unc_RetVal unc0_unic_normalize(Unc_View *w,
                                      Unc_UNormType normtype,
                                      size_t sn, const char *sp,
                                      size_t *qn, char **qp) {
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
}

#endif

Unc_RetVal uncl_unic_lookup(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value v = UNC_BLANK;
    Unc_RetVal e;
    const char *sp;
    struct unc_uchar_result result;
    char buf[UNC_UTF8_MAX_SIZE];

    e = unc_getstringc(w, &args.values[0], &sp);
    if (e) return e;
    
    result = unc0_unic_lookup(sp);
    if (result.e) return result.e;
    if (result.u != UNC_UTF8_NO_CHAR) {
        Unc_Size n = unc0_utf8enc(result.u, sizeof(buf), (byte *)buf);
        e = unc_newstring(w, &v, n, buf);
    }
    return unc_returnlocal(w, e, &v);
}

static Unc_RetVal uncl_unic1char(Unc_View *w, Unc_Tuple args, Unc_UChar *puc) {
#if UNICODESUPPORT
    Unc_RetVal e;
    Unc_Size sn;
    const char *sp;

    e = unc_getstring(w, &args.values[0], &sn, &sp);
    if (e) return e;

    if (!sn) return unc_throwexc(w, "value", "no character given");
    *puc = unc0_utf8decd((const byte *)sp);
    return 0;
#else
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
}

Unc_RetVal uncl_unic_name(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value v = UNC_BLANK;
    Unc_RetVal e;
    Unc_UChar uc;
    char buf[256];
    size_t sl;
    
    e = uncl_unic1char(w, args, &uc);
    if (e) return e;
    
    e = unc0_unic_name(uc, sizeof(buf), buf, &sl);
    if (e == UNCIL_ERR_MEM) {
        char *p = unc_malloc(w, sl + 1);
        if (!p) return UNCIL_ERR_MEM;
        unc0_unic_name(uc, sl + 1, p, &sl);
        e = unc_newstringmove(w, &v, sl, p);
        if (e) unc_mfree(w, p);
    } else {
        e = unc_newstring(w, &v, sl, buf);
    }
    return unc_returnlocal(w, e, &v);
}

Unc_RetVal uncl_unic_assigned(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value v = UNC_BLANK;
    Unc_RetVal e;
    Unc_UChar uc;
    
    e = uncl_unic1char(w, args, &uc);
    if (e) return e;
    unc_setbool(w, &v, unc0_unic_assigned(uc));
    return unc_returnlocal(w, 0, &v);
}

Unc_RetVal uncl_unic_bidi(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value v = UNC_BLANK;
    Unc_RetVal e;
    Unc_UChar uc;
    const char *p;
    
    e = uncl_unic1char(w, args, &uc);
    if (e) return e;
    p = unc0_unic_bidi(uc);
    if (!p) e = unc_newstringc(w, &v, p);
    return unc_returnlocal(w, e, &v);
}

Unc_RetVal uncl_unic_block(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value v = UNC_BLANK;
    Unc_RetVal e;
    Unc_UChar uc;
    const char *p;
    
    e = uncl_unic1char(w, args, &uc);
    if (e) return e;
    p = unc0_unic_block(uc);
    if (!p) e = unc_newstringc(w, &v, p);
    return unc_returnlocal(w, e, &v);
}

Unc_RetVal uncl_unic_category(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value v = UNC_BLANK;
    Unc_RetVal e;
    Unc_UChar uc;
    const char *p;
    
    e = uncl_unic1char(w, args, &uc);
    if (e) return e;
    p = unc0_unic_category(uc);
    if (!p) e = unc_newstringc(w, &v, p);
    return unc_returnlocal(w, e, &v);
}

Unc_RetVal uncl_unic_combining(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value v = UNC_BLANK;
    Unc_RetVal e;
    Unc_UChar uc;
    
    e = uncl_unic1char(w, args, &uc);
    if (e) return e;
    unc_setint(w, &v, unc0_unic_combining(uc));
    return unc_returnlocal(w, 0, &v);
}

Unc_RetVal uncl_unic_decimal(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value v = UNC_BLANK;
    Unc_RetVal e;
    Unc_UChar uc;
    int result;

    e = uncl_unic1char(w, args, &uc);
    if (e) return e;
    if (unc0_unic_decimal(uc, &result))
        unc_setint(w, &v, result);
    return unc_returnlocal(w, 0, &v);
}

Unc_RetVal uncl_unic_decompose(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value v = UNC_BLANK;
    Unc_RetVal e;
    Unc_UChar uc;
    struct unc_uchar_decompose_buf buf;
    
    e = uncl_unic1char(w, args, &uc);
    if (e) return e;
    e = unc0_unic_decompose_init(&buf, w, uc);
    if (UNCIL_IS_ERR(e)) return e;
    if (!e) {
        Unc_Value *p;
        e = unc_newarray(w, &v, unc0_unic_decompose_size(&buf), &p);
        if (e) {
            unc0_unic_decompose_free(&buf);
            return e;
        }

        while (unc0_unic_decompose_next(&buf, &uc))
            unc_setint(w, p++, uc);

        unc_unlock(w, &v);
    }
    unc0_unic_decompose_free(&buf);
    return unc_returnlocal(w, 0, &v);
}

Unc_RetVal uncl_unic_digit(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value v = UNC_BLANK;
    Unc_RetVal e;
    Unc_UChar uc;
    int result;

    e = uncl_unic1char(w, args, &uc);
    if (e) return e;
    if (unc0_unic_digit(uc, &result))
        unc_setint(w, &v, result);
    return unc_returnlocal(w, 0, &v);
}

Unc_RetVal uncl_unic_eawidth(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value v = UNC_BLANK;
    Unc_RetVal e;
    Unc_UChar uc;
    const char *p;
    
    e = uncl_unic1char(w, args, &uc);
    if (e) return e;
    p = unc0_unic_eawidth(uc);
    if (!p) e = unc_newstringc(w, &v, p);
    return unc_returnlocal(w, e, &v);
}

Unc_RetVal uncl_unic_mirrored(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value v = UNC_BLANK;
    Unc_RetVal e;
    Unc_UChar uc;
    
    e = uncl_unic1char(w, args, &uc);
    if (e) return e;
    unc_setbool(w, &v, unc0_unic_mirrored(uc));
    return unc_returnlocal(w, 0, &v);
}

Unc_RetVal uncl_unic_noncharacter(Unc_View *w,
                                      Unc_Tuple args, void *udata) {
    Unc_Value v = UNC_BLANK;
    Unc_RetVal e;
    Unc_UChar uc;
    
    e = uncl_unic1char(w, args, &uc);
    if (e) return e;
    unc_setbool(w, &v, unc0_unic_noncharacter(uc));
    return unc_returnlocal(w, 0, &v);
}

Unc_RetVal uncl_unic_numeric(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value v = UNC_BLANK;
    Unc_RetVal e;
    Unc_UChar uc;
    double result;

    e = uncl_unic1char(w, args, &uc);
    if (e) return e;
    if (unc0_unic_numeric(uc, &result))
        unc_setfloat(w, &v, result);
    return unc_returnlocal(w, 0, &v);
}

Unc_RetVal uncl_unic_private(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value v = UNC_BLANK;
    Unc_RetVal e;
    Unc_UChar uc;
    
    e = uncl_unic1char(w, args, &uc);
    if (e) return e;
    unc_setbool(w, &v, unc0_unic_private(uc));
    return unc_returnlocal(w, 0, &v);
}

Unc_RetVal uncl_unic_surrogate(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value v = UNC_BLANK;
    Unc_RetVal e;
    Unc_UChar uc;
    
    e = uncl_unic1char(w, args, &uc);
    if (e) return e;
    unc_setbool(w, &v, unc0_unic_surrogate(uc));
    return unc_returnlocal(w, 0, &v);
}

Unc_RetVal uncl_unic_glength(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value v = UNC_BLANK;
    Unc_RetVal e;
    Unc_Size sn;
    const char *sp;
    Unc_Size ai = 0;
    struct unc_uchar_graphemes_buf buf;

    e = unc_getstring(w, &args.values[0], &sn, &sp);
    if (e) return e;

    e = unc0_unic_graphemes_init(&buf, w, sn, sp);
    if (e) return e;

    while (unc0_unic_graphemes_next(&buf, &sn, &sp))
        ++ai;

    unc0_unic_graphemes_free(&buf);
    unc_setint(w, &v, ai);
    return unc_returnlocal(w, e, &v);
}

Unc_RetVal uncl_unic_graphemes(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value v = UNC_BLANK, tmp = UNC_BLANK;
    Unc_RetVal e;
    Unc_Size sn;
    const char *sp;
    Unc_Size an, ai = 0;
    Unc_Value *ap;
    struct unc_uchar_graphemes_buf buf;

    e = unc_getstring(w, &args.values[0], &sn, &sp);
    if (e) return e;

    e = unc0_unic_graphemes_init(&buf, w, sn, sp);
    if (e) return e;

    an = 0;
    e = unc_newarray(w, &v, 0, &ap);
    if (e) goto fail;

    while (unc0_unic_graphemes_next(&buf, &sn, &sp)) {
        if (ai >= an)
            e = unc_resizearray(w, &v, (an = an + 32), &ap);
        if (!e) {
            e = unc_newstring(w, &tmp, sn, sp);
            if (!e) unc_copy(w, &ap[ai++], &tmp);
        }
        if (e) break;
    }

    VCLEAR(w, &tmp);
    unc_unlock(w, &v);
    if (!e) e = unc_resizearray(w, &v, ai, &ap);
    e = unc_returnlocal(w, e, &v);
fail:
    unc0_unic_graphemes_free(&buf);
    return e;
}

Unc_RetVal uncl_unic_lower(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Value v = UNC_BLANK;
    Unc_Size sn;
    const char *sp, *locale = NULL;
    struct unc_uchar_casemap ucm;
    char buf[256];
    Unc_Size ql;

    e = unc_getstring(w, &args.values[0], &sn, &sp);
    if (e) return e;

    if (unc_gettype(w, &args.values[1])) {
        e = unc_getstringc(w, &args.values[1], &locale);
        if (e) return e;
    }

    e = unc0_unic_casemap_init(&ucm, w, locale);
    if (e) return e;

    e = unc0_unic_lower(&ucm, w, sn, sp, sizeof(buf), buf, &ql);
    if (e == UNCIL_ERR_MEM) {
        char *qp = unc_malloc(w, ql + 1);
        if (!qp) goto fail;
        e = unc0_unic_lower(&ucm, w, sn, sp, ql, qp, &ql);
        if (!e) e = unc_newstringmove(w, &v, ql, qp);
        if (e) unc_mfree(w, qp);
    } else if (!e)
        e = unc_newstring(w, &v, ql, buf);

    e = unc_returnlocal(w, e, &v);
fail:
    unc0_unic_casemap_free(&ucm);
    return e;
}

Unc_RetVal uncl_unic_upper(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Value v = UNC_BLANK;
    Unc_Size sn;
    const char *sp, *locale = NULL;
    struct unc_uchar_casemap ucm;
    char buf[256];
    Unc_Size ql;

    e = unc_getstring(w, &args.values[0], &sn, &sp);
    if (e) return e;

    if (unc_gettype(w, &args.values[1])) {
        e = unc_getstringc(w, &args.values[1], &locale);
        if (e) return e;
    }

    e = unc0_unic_casemap_init(&ucm, w, locale);
    if (e) return e;

    e = unc0_unic_upper(&ucm, w, sn, sp, sizeof(buf), buf, &ql);
    if (e == UNCIL_ERR_MEM) {
        char *qp = unc_malloc(w, ql + 1);
        if (!qp) goto fail;
        e = unc0_unic_upper(&ucm, w, sn, sp, ql, qp, &ql);
        if (!e) e = unc_newstringmove(w, &v, ql, qp);
        if (e) unc_mfree(w, qp);
    } else if (!e)
        e = unc_newstring(w, &v, ql, buf);

    e = unc_returnlocal(w, e, &v);
fail:
    unc0_unic_casemap_free(&ucm);
    return e;
}

Unc_RetVal uncl_unic_title(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Value v = UNC_BLANK;
    Unc_Size sn;
    const char *sp, *locale = NULL;
    struct unc_uchar_casemap ucm;
    char buf[256];
    Unc_Size ql;

    e = unc_getstring(w, &args.values[0], &sn, &sp);
    if (e) return e;

    if (unc_gettype(w, &args.values[1])) {
        e = unc_getstringc(w, &args.values[1], &locale);
        if (e) return e;
    }

    e = unc0_unic_casemap_init(&ucm, w, locale);
    if (e) return e;

    e = unc0_unic_title(&ucm, w, sn, sp, sizeof(buf), buf, &ql);
    if (e == UNCIL_ERR_MEM) {
        char *qp = unc_malloc(w, ql + 1);
        if (!qp) goto fail;
        e = unc0_unic_title(&ucm, w, sn, sp, ql, qp, &ql);
        if (!e) e = unc_newstringmove(w, &v, ql, qp);
        if (e) unc_mfree(w, qp);
    } else if (!e)
        e = unc_newstring(w, &v, ql, buf);

    e = unc_returnlocal(w, e, &v);
fail:
    unc0_unic_casemap_free(&ucm);
    return e;
}

Unc_RetVal uncl_unic_icmp(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    int cmp;
    Unc_Value v = UNC_BLANK;
    Unc_Size sn, zn;
    const char *sp, *zp, *locale = NULL;
    struct unc_uchar_casemap ucm;

    e = unc_getstring(w, &args.values[0], &sn, &sp);
    if (e) return e;

    e = unc_getstring(w, &args.values[1], &zn, &zp);
    if (e) return e;

    if (unc_gettype(w, &args.values[2])) {
        Unc_Size locn;
        e = unc_getstring(w, &args.values[2], &locn, &locale);
        if (e) return e;
    }

    e = unc0_unic_casemap_init(&ucm, w, locale);
    if (e) return e;

    cmp = unc0_unic_icmp(&ucm, w, sn, sp, zn, zp);
    if (!UNCIL_IS_ERR_CMP(cmp)) {
        unc_setint(w, &v, cmp);
        e = unc_returnlocal(w, 0, &v);
    }

    unc0_unic_casemap_free(&ucm);
    return e;
}

Unc_RetVal uncl_unic_trim(Unc_View *w, Unc_Tuple args, void *udata) {
#if UNICODESUPPORT
    Unc_RetVal e;
    Unc_Value v = UNC_BLANK;
    Unc_Size sn;
    const char *sp;

    e = unc_getstring(w, &args.values[0], &sn, &sp);
    if (e) return e;

    while (sn) {
        Unc_UChar u = unc0_utf8decd((const byte *)sp);
        if (!unc0_unic_isspace(u)) break;
        sp = (const char *)unc0_utf8nextchar((const byte *)sp, &sn);
    }

    while (sn) {
        Unc_UChar u;
        const char *su = (const char *)unc0_utf8lastchar((const byte *)sp, sn);
        if (!su) break;
        u = unc0_utf8decd((const byte *)su);
        if (!unc0_unic_isspace(u)) break;
        sn = su - sp;
    }

    e = unc_newstring(w, &v, sn, sp);
    return unc_returnlocal(w, e, &v);
#else
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
}

Unc_RetVal uncl_unic_normalize(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Value v = UNC_BLANK;
    Unc_UNormType nm = 0;
    Unc_Size sn, tn;
    const char *sp, *mode;
    char *tp;

    e = unc_getstring(w, &args.values[0], &sn, &sp);
    if (e) return e;

    e = unc_getstringc(w, &args.values[1], &mode);
    if (e) return e;

    if (unc0_tolower(*mode) == 'n') {
        ++mode;
        if (unc0_tolower(*mode) == 'f') {
            ++mode;
        }
    }

    if (unc0_tolower(*mode) == 'k') {
        ++mode;
        nm |= UNC_UNORMTYPE_K;
    }

    switch (unc0_tolower(*mode)) {
    case 'c':
        nm |= UNC_UNORMTYPE_C;
        break;
    case 'd':
        nm |= UNC_UNORMTYPE_D;
        break;
    default:
        return unc_throwexc(w, "value", "invalid normalization mode");
    }

    e = unc0_unic_normalize(w, nm, sn, sp, &tn, &tp);
    if (!e) {
        e = unc_newstringmove(w, &v, tn, tp);
        if (e) unc_mfree(w, tp);
    }
    return unc_returnlocal(w, e, &v);
}

#define FN(x) &uncl_unic_##x, #x

#define ONECHARFUNC(x)                                                         \
    { FN(x),            1, 0, 0, UNC_CFUNC_CONCURRENT },

static const Unc_ModuleCFunc lib[] = {
    { FN(lookup),       1, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(glength),      1, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(graphemes),    1, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(lower),        1, 1, 0, UNC_CFUNC_CONCURRENT },
    { FN(upper),        1, 1, 0, UNC_CFUNC_CONCURRENT },
    { FN(title),        1, 1, 0, UNC_CFUNC_CONCURRENT },
    { FN(icmp),         2, 1, 0, UNC_CFUNC_CONCURRENT },
    { FN(trim),         1, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(normalize),    2, 0, 0, UNC_CFUNC_CONCURRENT },
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
};

Unc_RetVal uncilmain_unicode(Unc_View *w) {
    Unc_RetVal e = unc_exportcfunctions(w, PASSARRAY(lib), 0, NULL, NULL);
    if (e) return e;

    {
        Unc_Value v = UNC_BLANK;
        const char *version = unc0_unic_version();
        if (version) {
            e = unc_newstringc(w, &v, version);
            if (e) return e;
        }
        e = unc_setpublicc(w, "version", &v);
        if (e) return e;
        VCLEAR(w, &v);
    }
    return 0;
}
