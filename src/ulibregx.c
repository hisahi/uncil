/*******************************************************************************
 
Uncil -- builtin regex library impl

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

#include <string.h>

#define UNCIL_DEFINES

#include "uctype.h"
#include "udef.h"
#include "uncil.h"
#include "uutf.h"
#include "uvsio.h"

struct unc0_varr {
    Unc_Value out;
    Unc_Value *outv;
    Unc_Size outi, outn;
};

static Unc_RetVal unc0_varr_init(Unc_View *w, struct unc0_varr *buf) {
    VINITNULL(&buf->out);
    buf->outi = 0;
    buf->outn = 4;
    return unc_newarray(w, &buf->out, buf->outn, &buf->outv);
}

static Unc_RetVal unc0_varr_push(Unc_View *w, struct unc0_varr *buf,
                                 Unc_Value *v) {
    if (buf->outi == buf->outn) {
        Unc_RetVal e;
        Unc_Size outz = buf->outn + 4;
        e = unc_resizearray(w, &buf->out, outz, &buf->outv);
        if (e) return e;
        buf->outn = outz;
    }
    unc_copy(w, &buf->outv[buf->outi++], &v[0]);
    return 0;
}

static Unc_Value *unc0_varr_done(Unc_View *w, struct unc0_varr *buf) {
    unc_resizearray(w, &buf->out, buf->outi, &buf->outv);
    buf->outn = buf->outi;
    unc_unlock(w, &buf->out);
    return &buf->out;
}

static void unc0_varr_clear(Unc_View *w, struct unc0_varr *buf) {
    unc_unlock(w, &buf->out);
    VCLEAR(w, &buf->out);
}

struct unc_regex_pattern;
static void *unc0_gcxt_make(Unc_View *w);
static Unc_RetVal unc_regex_pattern_destr(Unc_View *w, size_t n, void *data);
static Unc_RetVal unc0_regex_makeerr(Unc_View *w, const char *prefix,
                                     int errcode);
static Unc_RetVal unc0_regex_comp_makeerr(Unc_View *w, const char *prefix,
                                          int errcode, Unc_Size erroffset);
static Unc_RetVal unc0_regex_compile(Unc_View *w, Unc_Value *v,
                                     Unc_Size rgx_n, const char *rgx,
                                     Unc_Size rfl_n, const char *rfl,
                                     void *udata);
static Unc_RetVal uncl_regex_getpat_make(Unc_View *w,
                            struct unc_regex_pattern *pat,
                            Unc_Size rgx_n, const char *rgx,
                            Unc_Size rfl_n, const char *rfl,
                            const char *prefix,
                            void *udata);
static int unc0_regex_match(Unc_View *w, struct unc_regex_pattern *pat,
                            Unc_Size sn, const char *ss,
                            Unc_Value v[2], void *udata);
static Unc_RetVal unc0_regex_find(Unc_View *w, struct unc_regex_pattern *pat,
                                  Unc_Size sn, const char *ss,
                                  Unc_Value v[2], Unc_Size startat,
                                  void *udata);
static Unc_RetVal unc0_regex_findlast(Unc_View *w,
                                      struct unc_regex_pattern *pat,
                                      Unc_Size sn, const char *ss,
                                      Unc_Value v[2], Unc_Size startat,
                                      void *udata);
static Unc_RetVal unc0_regex_findall(Unc_View *w,
                                     struct unc_regex_pattern *pat,
                                     Unc_Size sn, const char *ss,
                                     struct unc0_varr *varr,
                                     Unc_Value v[2], Unc_Size startat,
                                     void *udata);
static Unc_RetVal unc0_regex_split(Unc_View *w,
                                   struct unc_regex_pattern *pat,
                                   Unc_Size sn, const char *ss,
                                   struct unc0_varr *varr, int direction,
                                   Unc_Size startat, Unc_Size splits,
                                   void *udata);
static Unc_RetVal unc0_regex_replace(Unc_View *w,
                                     struct unc_regex_pattern *pat,
                                     Unc_Size sn, const char *ss,
                                     struct unc0_strbuf *q, int direction,
                                     Unc_Size startat, Unc_Size replaces,
                                     int replfunc, Unc_Value *fn,
                                     Unc_Size rpn, const char *rps,
                                     void *udata);

#if UNCIL_LIB_PCRE2
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#ifndef PCRE2_ERROR_MAXLENGTH
#define PCRE2_ERROR_MAXLENGTH 256
#endif

#define REGEX_ENGINE "pcre2"

typedef PCRE2_SIZE *Unc_CaptureVector;

#define UNC_PCRE2_COMPILE_OPTIONS (PCRE2_UCP | PCRE2_UTF | PCRE2_NO_UTF_CHECK  \
                            | PCRE2_NEVER_BACKSLASH_C)
#define UNC_PCRE2_MATCH_OPTIONS (PCRE2_NO_UTF_CHECK)

static void *unc0_gcxt_alloc(PCRE2_SIZE z, void *w) {
    return unc_malloc((Unc_View *)w, z);
}

static void unc0_gcxt_free(void *p, void *w) {
    unc_mfree((Unc_View *)w, p);
}

static void *unc0_gcxt_make(Unc_View *w) {
    return pcre2_general_context_create(&unc0_gcxt_alloc, &unc0_gcxt_free, w);
}

struct unc_regex_pattern {
    pcre2_code *code;
};

static Unc_RetVal unc_regex_pattern_destr(Unc_View *w, size_t n, void *data) {
    struct unc_regex_pattern *p = data;
    pcre2_code_free(p->code);
    return 0;
}

static Unc_RetVal unc0_regex_makeerr(Unc_View *w, const char *prefix,
                                     int errcode) {
    Unc_RetVal e;
    Unc_Value msg = UNC_BLANK;
    PCRE2_UCHAR buffer[PCRE2_ERROR_MAXLENGTH];
    pcre2_get_error_message(errcode, buffer, sizeof(buffer));
    e = unc0_usxprintf(w, &msg, "%s: %s",
        prefix, (const char *)buffer);
    if (e) return UNCIL_ERR_MEM;
    return e ? e : unc_throwext(w, "value", &msg);
}

static Unc_RetVal unc0_regex_comp_makeerr(Unc_View *w, const char *prefix,
                                          int errcode, Unc_Size erroffset) {
    Unc_RetVal e;
    Unc_Value msg = UNC_BLANK;
    PCRE2_UCHAR buffer[PCRE2_ERROR_MAXLENGTH];
    pcre2_get_error_message(errcode, buffer, sizeof(buffer));
    e = unc0_usxprintf(w, &msg, "%s at offset %"PRIUnc_Size"u: %s",
        prefix, erroffset, (const char *)buffer);
    if (e) return UNCIL_ERR_MEM;
    return e ? e : unc_throwext(w, "value", &msg);
}

INLINE Unc_RetVal unc0_regex_parseflags(Unc_View *w, uint32_t *options,
                                        Unc_Size rfl_n, const char *rfl) {
    Unc_Size i;
    for (i = 0; i < rfl_n; ++i) {
        switch (rfl[i]) {
        case 'i':
            *options |= PCRE2_CASELESS;
            break;
        case 'm':
            *options |= PCRE2_MULTILINE;
            break;
        case 's':
            *options |= PCRE2_DOTALL;
            break;
        default:
            return unc_throwexc(w, "value", "unrecognized flag");
        }
    }
    return 0;
}

static Unc_RetVal unc0_regex_compile(Unc_View *w, Unc_Value *v,
                                     Unc_Size rgx_n, const char *rgx,
                                     Unc_Size rfl_n, const char *rfl,
                                     void *udata) {
    Unc_RetVal e;
    int errcode;
    PCRE2_SIZE erroffset;
    uint32_t options = UNC_PCRE2_COMPILE_OPTIONS;
    pcre2_code *code;
    pcre2_compile_context *ccxt;
    struct unc_regex_pattern *data;

    e = unc0_regex_parseflags(w, &options, rfl_n, rfl);
    if (e) return e;
    
    ccxt = pcre2_compile_context_create((pcre2_general_context *)udata);
    if (!ccxt) return UNCIL_ERR_MEM;
    code = pcre2_compile((PCRE2_SPTR)rgx, rgx_n, options,
                            &errcode, &erroffset, ccxt);
    pcre2_compile_context_free(ccxt);
    if (!code)
        return unc0_regex_comp_makeerr(w, "regex.compile()",
                errcode, erroffset);

    e = unc_newopaque(w, v, unc_boundvalue(w, 0),
                      sizeof(struct unc_regex_pattern), (void **)&data,
                      &unc_regex_pattern_destr, 0, NULL, 0, NULL);
    if (e) {
        pcre2_code_free(code);
        return e;
    }
    data->code = code;
    unc_unlock(w, v);
    return e;
}

static Unc_RetVal uncl_regex_getpat_make(Unc_View *w,
                            struct unc_regex_pattern *pat,
                            Unc_Size rgx_n, const char *rgx,
                            Unc_Size rfl_n, const char *rfl,
                            const char *prefix,
                            void *udata) {
    int errcode;
    PCRE2_SIZE erroffset;
    uint32_t options = UNC_PCRE2_COMPILE_OPTIONS;
    pcre2_code *code;
    pcre2_compile_context *ccxt;

    ccxt = pcre2_compile_context_create((pcre2_general_context *)udata);
    if (!ccxt) return UNCIL_ERR_MEM;
    code = pcre2_compile((PCRE2_SPTR)rgx, rgx_n, options,
                            &errcode, &erroffset, ccxt);
    pcre2_compile_context_free(ccxt);
    if (!code)
        return unc0_regex_comp_makeerr(w, prefix, errcode, erroffset);
    pat->code = code;
    return 0;
}

static int unc0_regex_findnext(Unc_Size sn,
                               pcre2_code *code,
                               const char *ss,
                               Unc_Size *startat,
                               pcre2_match_context *mcxt,
                               pcre2_match_data *mdata,
                               PCRE2_SIZE **povec) {
    int rc = pcre2_match(code, (PCRE2_SPTR)ss, sn,
                            *startat, UNC_PCRE2_MATCH_OPTIONS, mdata, mcxt);
    if (rc < 0) {
        if (rc == PCRE2_ERROR_NOMATCH)
            rc = 0;
    } else if (!rc) {
        rc = PCRE2_ERROR_NOMEMORY;
    } else {
        PCRE2_SIZE *ovec = pcre2_get_ovector_pointer(mdata);
        if (!ovec)
            return PCRE2_ERROR_NOMEMORY;
        *povec = ovec;
        *startat = ovec[1];
    }
    return rc;
}

static int unc0_regex_findprev(Unc_Size *psn,
                               pcre2_code *code,
                               const char *ss,
                               Unc_Size *startat,
                               pcre2_match_context *mcxt,
                               pcre2_match_data *mdata,
                               PCRE2_SIZE **povec) {
    Unc_Size sn = *psn;
    Unc_Size sindx = *startat;
    if (!sn) return 0;
    if (sn == sindx)
        sindx = (const char *)unc0_utf8scanbackw(
                    (const byte *)ss, (const byte *)ss + sindx, 0) - ss;
    for (;;) {
        int rc = pcre2_match(code, (PCRE2_SPTR)ss, sn, sindx,
                                UNC_PCRE2_MATCH_OPTIONS | PCRE2_ANCHORED,
                                mdata, mcxt);
        if (rc < 0) {
            if (rc == PCRE2_ERROR_NOMATCH) {
                if (!sindx)
                    rc = 0;
                else {
                    sindx = (const char *)unc0_utf8scanbackw(
                                (const byte *)ss,
                                (const byte *)ss + sindx, 0) - ss;
                    continue;
                }
            }
        } else if (!rc) {
            rc = PCRE2_ERROR_NOMEMORY;
        } else {
            PCRE2_SIZE *ovec = pcre2_get_ovector_pointer(mdata);
            if (!ovec)
                return PCRE2_ERROR_NOMEMORY;
            *povec = ovec;
            *psn = *startat = ovec[0];
        }
        return rc;
    }
}

static int unc0_regex_findbidi(int direction,
                               Unc_Size *psn,
                               pcre2_code *code,
                               const char *ss,
                               Unc_Size *startat,
                               pcre2_match_context *mcxt,
                               pcre2_match_data *mdata,
                               PCRE2_SIZE **povec) {
    return direction
        ? unc0_regex_findprev(psn, code, ss, startat, mcxt, mdata, povec)
        : unc0_regex_findnext(*psn, code, ss, startat, mcxt, mdata, povec);
}

static int unc0_regex_match(Unc_View *w, struct unc_regex_pattern *pat,
                            Unc_Size sn, const char *ss,
                            Unc_Value v[2], void *udata) {
    Unc_RetVal e;
    pcre2_general_context *gcxt = (pcre2_general_context *)udata;
    pcre2_match_data *mdata =
        pcre2_match_data_create_from_pattern(pat->code, gcxt);
    pcre2_match_context *mcxt = pcre2_match_context_create(gcxt);
    if (mdata && mcxt) {
        int rc = pcre2_match(pat->code, (PCRE2_SPTR)ss, sn, 0,
                                UNC_PCRE2_MATCH_OPTIONS
                                | PCRE2_ANCHORED
                                | PCRE2_ENDANCHORED,
                                mdata, mcxt);
        if (rc < 0) {
            if (rc == PCRE2_ERROR_NOMATCH)
                unc_setbool(w, &v[0], 0);
            else
                e = unc0_regex_makeerr(w, "regex.match()", rc);
        } else {
            PCRE2_SIZE *ovec = pcre2_get_ovector_pointer(mdata);
            if (!ovec)
                e = UNCIL_ERR_MEM;
            else {
                Unc_Size i;
                Unc_Value *av;
                unc_setbool(w, &v[0], 1);
                e = unc_newarray(w, &v[1], rc, &av);
                for (i = 0; !e && i < rc; ++i) {
                    Unc_Size z0 = ovec[i * 2];
                    Unc_Size zl = ovec[i * 2 + 1] - z0;
                    e = unc_newstring(w, &av[i], zl, ss + z0);
                }
            }
        }
    } else
        e = UNCIL_ERR_MEM;
    if (mdata) pcre2_match_data_free(mdata);
    if (mcxt) pcre2_match_context_free(mcxt);
    return e;
}

static Unc_RetVal unc0_regex_find(Unc_View *w, struct unc_regex_pattern *pat,
                                  Unc_Size sn, const char *ss,
                                  Unc_Value v[2], Unc_Size startat,
                                  void *udata) {
    Unc_RetVal e;
    pcre2_general_context *gcxt = (pcre2_general_context *)udata;
    pcre2_match_data *mdata =
        pcre2_match_data_create_from_pattern(pat->code, gcxt);
    pcre2_match_context *mcxt = pcre2_match_context_create(gcxt);
    if (mdata && mcxt) {
        PCRE2_SIZE *ovec;
        int rc = unc0_regex_findnext(sn, pat->code, ss, &startat,
                                            mcxt, mdata, &ovec);
        if (!rc)
            unc_setint(w, &v[0], -1);
        else if (rc < 0)
            e = unc0_regex_makeerr(w, "regex.find()", rc);
        else {
            Unc_Size i;
            Unc_Value *av;
            unc_setint(w, &v[0],
                    unc0_utf8unshift((const byte *)ss, ovec[0]));
            e = unc_newarray(w, &v[1], rc, &av);
            for (i = 0; !e && i < rc; ++i) {
                Unc_Size z0 = ovec[i * 2];
                Unc_Size zl = ovec[i * 2 + 1] - z0;
                e = unc_newstring(w, &av[i], zl, ss + z0);
            }
        }
    } else
        e = UNCIL_ERR_MEM;
    if (mdata) pcre2_match_data_free(mdata);
    if (mcxt) pcre2_match_context_free(mcxt);
    return e;
}

static Unc_RetVal unc0_regex_findlast(Unc_View *w,
                                      struct unc_regex_pattern *pat,
                                      Unc_Size sn, const char *ss,
                                      Unc_Value v[2], Unc_Size startat,
                                      void *udata) {
    Unc_RetVal e;
    pcre2_general_context *gcxt = (pcre2_general_context *)udata;
    pcre2_match_data *mdata =
        pcre2_match_data_create_from_pattern(pat->code, gcxt);
    pcre2_match_context *mcxt = pcre2_match_context_create(gcxt);
    if (mdata && mcxt) {
        PCRE2_SIZE *ovec;
        int rc = unc0_regex_findprev(&sn, pat->code, ss, &startat,
                                            mcxt, mdata, &ovec);
        if (!rc)
            unc_setint(w, &v[0], -1);
        else if (rc < 0)
            e = unc0_regex_makeerr(w, "regex.findlast()", rc);
        else {
            Unc_Size i;
            Unc_Value *av;
            unc_setint(w, &v[0],
                    unc0_utf8unshift((const byte *)ss, ovec[0]));
            e = unc_newarray(w, &v[1], rc, &av);
            for (i = 0; !e && i < rc; ++i) {
                Unc_Size z0 = ovec[i * 2];
                Unc_Size zl = ovec[i * 2 + 1] - z0;
                e = unc_newstring(w, &av[i], zl, ss + z0);
            }
        }
    } else
        e = UNCIL_ERR_MEM;
    if (mdata) pcre2_match_data_free(mdata);
    if (mcxt) pcre2_match_context_free(mcxt);
    return e;
}

static Unc_RetVal unc0_regex_findall(Unc_View *w,
                                     struct unc_regex_pattern *pat,
                                     Unc_Size sn, const char *ss,
                                     struct unc0_varr *varr,
                                     Unc_Value v[2], Unc_Size startat,
                                     void *udata) {
    Unc_RetVal e;
    pcre2_general_context *gcxt = (pcre2_general_context *)udata;
    pcre2_match_data *mdata =
        pcre2_match_data_create_from_pattern(pat->code, gcxt);
    pcre2_match_context *mcxt = pcre2_match_context_create(gcxt);
    if (mdata && mcxt) {
        while (!e) {
            PCRE2_SIZE *ovec;
            int rc = unc0_regex_findnext(sn, pat->code, ss, &startat,
                                            mcxt, mdata, &ovec);
            if (!rc)
                break;
            else if (rc < 0)
                e = unc0_regex_makeerr(w, "regex.findall()", rc);
            else {
                Unc_Size i;
                Unc_Value *av;
                unc_setint(w, &v[0],
                        unc0_utf8unshift((const byte *)ss, ovec[0]));
                e = unc_newarray(w, &v[1], rc, &av);
                for (i = 0; !e && i < rc; ++i) {
                    Unc_Size z0 = ovec[i * 2];
                    Unc_Size zl = ovec[i * 2 + 1] - z0;
                    e = unc_newstring(w, &av[i], zl, ss + z0);
                }
                e = unc_newarrayfrom(w, &v[0], 2, v);
                if (!e) e = unc0_varr_push(w, varr, &v[0]);
            }
        }
    } else
        e = UNCIL_ERR_MEM;
    if (mdata) pcre2_match_data_free(mdata);
    if (mcxt) pcre2_match_context_free(mcxt);
    return e;
}

static Unc_RetVal unc0_regex_split(Unc_View *w,
                                   struct unc_regex_pattern *pat,
                                   Unc_Size sn, const char *ss,
                                   struct unc0_varr *varr, int direction,
                                   Unc_Size startat, Unc_Size splits,
                                   void *udata) {
    Unc_RetVal e;
    Unc_Value vs = UNC_BLANK;
    pcre2_general_context *gcxt = (pcre2_general_context *)udata;
        pcre2_match_data *mdata =
        pcre2_match_data_create_from_pattern(pat->code, gcxt);
    pcre2_match_context *mcxt = pcre2_match_context_create(gcxt);
    if (mdata && mcxt) {
        Unc_Size ended = startat;
        while (splits--) {
            Unc_Size started = startat;
            PCRE2_SIZE *ovec;
            int rc = unc0_regex_findbidi(direction, &sn, pat->code,
                                ss, &startat, mcxt, mdata, &ovec);
            if (!rc)
                break;
            else if (rc < 0)
                e = unc0_regex_makeerr(w, "regex.split()", rc);
            else {
                if (!e) e = !direction
                        ? unc_newstring(w, &vs,
                                ovec[0] - started, ss + started)
                        : unc_newstring(w, &vs,
                                started - ovec[1], ss + ovec[1]);
                if (!e) e = unc0_varr_push(w, varr, &vs);
                if (direction) ended = ovec[0];
            }
        }
        if (!e) e = !direction
                ? unc_newstring(w, &vs, sn - startat, ss + startat)
                : unc_newstring(w, &vs, ended, ss);
        if (!e) e = unc0_varr_push(w, varr, &vs);
    } else
        e = UNCIL_ERR_MEM;
    if (mdata) pcre2_match_data_free(mdata);
    if (mcxt) pcre2_match_context_free(mcxt);
    VCLEAR(w, &vs);
    return e;
}

static Unc_RetVal uncl_regex__getrepls(Unc_View *w, const char *ss,
                                       struct unc0_strbuf *outbuf,
                                       Unc_Size rn, const char *rp,
                                       Unc_Size gn,
                                       Unc_CaptureVector ovec);
static Unc_RetVal uncl_regex__getreplf(Unc_View *w, const char *ss,
                                       struct unc0_strbuf *outbuf,
                                       Unc_Value *repl, Unc_Size gn,
                                       Unc_CaptureVector ovec);

static Unc_RetVal unc0_regex_replace(Unc_View *w,
                                     struct unc_regex_pattern *pat,
                                     Unc_Size sn, const char *ss,
                                     struct unc0_strbuf *q, int direction,
                                     Unc_Size startat, Unc_Size replaces,
                                     int replfunc, Unc_Value *fn,
                                     Unc_Size rpn, const char *rps,
                                     void *udata) {
    Unc_RetVal e;
    pcre2_general_context *gcxt = (pcre2_general_context *)udata;
    pcre2_match_data *mdata =
        pcre2_match_data_create_from_pattern(pat->code, gcxt);
    pcre2_match_context *mcxt = pcre2_match_context_create(gcxt);
    if (mdata && mcxt) {
        Unc_Size ended = startat;
        while (replaces--) {
            Unc_Size started = startat;
            PCRE2_SIZE *ovec;
            int rc = unc0_regex_findbidi(direction, &sn, pat->code,
                                ss, &startat, mcxt, mdata, &ovec);
            if (!rc)
                break;
            else if (rc < 0)
                e = unc0_regex_makeerr(w, "regex.replace()", rc);
            else {
                e = !direction
                    ? unc0_strbuf_putn(q,
                        ovec[0] - started, (const byte *)ss + started)
                    : unc0_strbuf_putn_rv(q,
                        started - ovec[1], (const byte *)ss + ovec[1]);
                if (direction) ended = ovec[0];
                if (!e) {
                    Unc_Size qx = q->length;
                    e = replfunc
                        ? uncl_regex__getreplf(w, ss, q, fn, rc, ovec)
                        : uncl_regex__getrepls(w, ss, q, rpn, rps, rc, ovec);
                    if (!e && q->length > qx && direction)
                        unc0_memrev(q->buffer + qx, q->length - qx);
                }
            }
            if (!e) break;
        }
        if (!e) e = !direction
            ? unc0_strbuf_putn(q,
                            sn - startat, (const byte *)ss + startat)
            : unc0_strbuf_putn_rv(q, ended, (const byte *)ss);
    } else
        e = UNCIL_ERR_MEM;
    if (mdata) pcre2_match_data_free(mdata);
    if (mcxt) pcre2_match_context_free(mcxt);
    return e;
}

#else /* stub */

struct unc_regex_pattern {
    char tmp_;
};

static void *unc0_gcxt_make(Unc_View *w) {
    return NULL;
}

static Unc_RetVal unc_regex_pattern_destr(Unc_View *w, size_t n, void *data) {
    return 0;
}

static Unc_RetVal unc0_regex_makeerr(Unc_View *w, const char *prefix,
                                     int errcode) {
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
}

static Unc_RetVal unc0_regex_comp_makeerr(Unc_View *w, const char *prefix,
                                          int errcode, Unc_Size erroffset) {
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
}

static Unc_RetVal unc0_regex_compile(Unc_View *w, Unc_Value *v,
                                     Unc_Size rgx_n, const char *rgx,
                                     Unc_Size rfl_n, const char *rfl,
                                     void *udata) {
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
}

static Unc_RetVal uncl_regex_getpat_make(Unc_View *w,
                            struct unc_regex_pattern *pat,
                            Unc_Size rgx_n, const char *rgx,
                            Unc_Size rfl_n, const char *rfl,
                            const char *prefix,
                            void *udata) {
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
}

static int unc0_regex_match(Unc_View *w, struct unc_regex_pattern *pat,
                            Unc_Size sn, const char *ss,
                            Unc_Value v[2], void *udata) {
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
}

static Unc_RetVal unc0_regex_find(Unc_View *w, struct unc_regex_pattern *pat,
                                  Unc_Size sn, const char *ss,
                                  Unc_Value v[2], Unc_Size startat,
                                  void *udata) {
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
}

static Unc_RetVal unc0_regex_findlast(Unc_View *w,
                                      struct unc_regex_pattern *pat,
                                      Unc_Size sn, const char *ss,
                                      Unc_Value v[2], Unc_Size startat,
                                      void *udata) {
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
}

static Unc_RetVal unc0_regex_findall(Unc_View *w,
                                     struct unc_regex_pattern *pat,
                                     Unc_Size sn, const char *ss,
                                     struct unc0_varr *varr,
                                     Unc_Value v[2], Unc_Size startat,
                                     void *udata) {
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
}

static Unc_RetVal unc0_regex_split(Unc_View *w,
                                   struct unc_regex_pattern *pat,
                                   Unc_Size sn, const char *ss,
                                   struct unc0_varr *varr, int direction,
                                   Unc_Size startat, Unc_Size splits,
                                   void *udata) {
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
}

static Unc_RetVal unc0_regex_replace(Unc_View *w,
                                     struct unc_regex_pattern *pat,
                                     Unc_Size sn, const char *ss,
                                     struct unc0_strbuf *q, int direction,
                                     Unc_Size startat, Unc_Size replaces,
                                     int replfunc, Unc_Value *fn,
                                     Unc_Size rpn, const char *rps,
                                     void *udata) {
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
}

#endif

#ifdef REGEX_ENGINE
static Unc_RetVal uncl_regex__getrepls(Unc_View *w, const char *ss,
                                       struct unc0_strbuf *outbuf,
                                       Unc_Size rn, const char *rp,
                                       Unc_Size gn,
                                       Unc_CaptureVector ovec) {
    Unc_RetVal e;
    const char *rpp = rp, *rpe = rp + rn;
    for (;;) {
        const char *rpn = unc0_memchr(rpp, '$', rpe - rpp);
        int end = !rpn;
        int g_ok = 0;
        Unc_Size g;
        if (end) rpn = rpe;
        e = unc0_strbuf_putn(outbuf, rpn - rpp, (const byte *)rpp);
        if (e) return e;
        if (!end) {
            rpp = rpn + 1;
            if (unc0_isdigit(*rpp)) {
                /* capture group one digit */
                g_ok = 1;
                g = *rpp++ - '0';
            } else {
                switch (*rpp) {
                case '$':
                    ++rpp;
                    e = unc0_strbuf_put1(outbuf, '$');
                    break;
                case '<':
                    if (!unc0_isdigit(*++rpp))
                        return unc_throwexc(w, "value",
                            "invalid dollar syntax in replacement string");
                    g = 0;
                    {
                        Unc_Size pg = g;
                        while (unc0_isdigit(*rpp)) {
                            g = g * 10 + (*rpp - '0');
                            if (g < pg) {
                                g = gn;
                                while (unc0_isdigit(*rpp))
                                    ++rpp;
                                break;
                            }
                            ++rpp;
                            pg = g;
                        }
                        g_ok = 1;
                    }
                    if (*rpp++ != '>')
                        return unc_throwexc(w, "value",
                            "invalid dollar syntax in replacement string");
                    break;
                default:
                    return unc_throwexc(w, "value",
                        "invalid dollar syntax in replacement string");
                }
            }
            if (!e && g_ok) {
                if (g >= gn)
                    return unc_throwexc(w, "value",
                        "capture group reference out of bounds in "
                        "replacement string");
                e = unc0_strbuf_putn(outbuf,
                        ovec[g * 2 + 1] - ovec[g * 2],
                        (const byte *)ss + ovec[g * 2]);
            }
        }
        if (e) return e;
        if (end) return 0;
    }
}

static Unc_RetVal uncl_regex__getreplf(Unc_View *w, const char *ss,
                                       struct unc0_strbuf *outbuf,
                                       Unc_Value *repl, Unc_Size gn,
                                       Unc_CaptureVector ovec) {
    Unc_RetVal e;
    Unc_Value arr = UNC_BLANK;
    Unc_Size i, an;
    const char *ab;
    Unc_Value *av;
    Unc_Pile pile;
    Unc_Tuple tuple;
    e = unc_newarray(w, &arr, gn, &av);
    if (e) return e;
    for (i = 0; !e && i < gn; ++i) {
        Unc_Size z0 = ovec[i * 2];
        Unc_Size zl = ovec[i * 2 + 1] - z0;
        e = unc_newstring(w, &av[i], zl, ss + z0);
    }
    unc_unlock(w, &arr);
    if (e) {
        VCLEAR(w, &arr);
        return e;
    }
    e = unc_pushmove(w, &arr);
    if (e) {
        VCLEAR(w, &arr);
        return e;
    }
    e = unc_call(w, repl, 1, &pile);
    if (e) return e;
    unc_returnvalues(w, &pile, &tuple);
    if (tuple.count != 1) {
        unc_discard(w, &pile);
        return unc_throwexc(w, "value", "replacement function did not "
                                        "return exactly one value");
    }
    if (unc_getstring(w, &tuple.values[0], &an, &ab)) {
        unc_discard(w, &pile);
        return unc_throwexc(w, "value", "replacement function did not "
                                        "return a string");
    }
    e = unc0_strbuf_putn(outbuf, an, (const byte *)ab);
    unc_discard(w, &pile);
    return e;
}
#endif /* REGEX_ENGINE */

static Unc_RetVal uncl_regex_getpat(Unc_View *w,
                            Unc_Value *tpat, Unc_Value *flags,
                            Unc_Value *prototype,
                            const char *prefix,
                            void *udata,
                            struct unc_regex_pattern *pat, int *temporary) {
    Unc_RetVal e;
    Unc_Size rgx_n, rfl_n = 0;
    const char *rgx, *rfl = NULL;

    if (unc_gettype(w, tpat) == Unc_TOpaque) {
        Unc_Value proto;
        int eq;
        unc_getprototype(w, tpat, &proto);
        eq = unc_issame(w, &proto, prototype);
        VCLEAR(w, &proto);
        if (eq) {
            void *opq;
            e = unc_lockopaque(w, tpat, NULL, &opq);
            if (e) return e;
            *temporary = 0;
            *pat = *(struct unc_regex_pattern *)opq;
            return 0;
        }
        return unc_throwexc(w, "type", "argument is not a valid pattern");
    }

    e = unc_getstring(w, tpat, &rgx_n, &rgx);
    if (e) return e;

    if (unc_gettype(w, flags)) {
        e = unc_getstring(w, flags, &rfl_n, &rfl);
        if (e) return e;
    }

    *temporary = 1;
    return uncl_regex_getpat_make(w, pat, rgx_n, rgx, rfl_n, rfl,
                                  prefix, udata);
}

Unc_RetVal uncl_regex_engine(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e = 0;
    Unc_Value v = UNC_BLANK;
#ifdef REGEX_ENGINE
    e = unc_newstringc(w, &v, REGEX_ENGINE);
#endif
    return unc_returnlocal(w, e, &v);
}

INLINE int shouldescape(int c) {
    switch (c) {
    case '\\':
    case '#':
    case '&':
    case '\'':
    case '-':
    case '?':
    case '*':
    case '+':
    case '^':
    case '$':
    case '.':
    case '|':
    case '(':
    case ')':
    case '[':
    case ']':
    case '{':
    case '}':
    case '~':
        return 1;
    default:
        return 0;
    }
}

Unc_RetVal uncl_regex_escape(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e = 0;
    Unc_Value v = UNC_BLANK;
    Unc_Size sn;
    const char *s, *sx, *se;
    struct unc0_strbuf out;

    e = unc_getstring(w, &args.values[0], &sn, &s);
    if (e) return e;
    se = s + sn;

    unc0_strbuf_init(&out, &w->world->alloc, Unc_AllocString);
    while (s != se) {
        sx = (const char *)unc0_utf8nextchar((const byte *)s, &sn);
        if (!sx) sx = se;
        if (!(*s & 0x80) && shouldescape(*s)) {
            e = unc0_strbuf_put1(&out, '\\');
            if (e) goto fail;
        }
        e = unc0_strbuf_putn(&out, sx - s, (const byte *)s);
        if (e) goto fail;
        s = sx;
    }

    e = unc0_buftostring(w, &v, &out);
    e = unc_returnlocal(w, e, &v);
fail:
    unc0_strbuf_free(&out);
    return e;
}

Unc_RetVal uncl_regex_escaperepl(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e = 0;
    Unc_Value v = UNC_BLANK;
    Unc_Size sn;
    const char *s, *sx, *se;
    struct unc0_strbuf out;

    e = unc_getstring(w, &args.values[0], &sn, &s);
    if (e) return e;
    se = s + sn;
    
    unc0_strbuf_init(&out, &w->world->alloc, Unc_AllocString);
    while (s != se) {
        sx = (const char *)unc0_utf8nextchar((const byte *)s, &sn);
        if (!sx) sx = se;
        if (*s == '$') {
            e = unc0_strbuf_put1(&out, '$');
            if (e) goto fail;
        }
        e = unc0_strbuf_putn(&out, sx - s, (const byte *)s);
        if (e) goto fail;
        s = sx;
    }

    e = unc0_buftostring(w, &v, &out);
    e = unc_returnlocal(w, e, &v);
fail:
    unc0_strbuf_free(&out);
    return e;
}

Unc_RetVal uncl_regex_compile(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Value v = UNC_BLANK;
    Unc_Size rgx_n, rfl_n = 0;
    const char *rgx, *rfl = NULL;

    e = unc_getstring(w, &args.values[0], &rgx_n, &rgx);
    if (e) return e;

    if (unc_gettype(w, &args.values[1])) {
        e = unc_getstring(w, &args.values[1], &rfl_n, &rfl);
        if (e) return e;
    }
    
    e = unc0_regex_compile(w, &v, rgx_n, rgx, rfl_n, rfl, udata);
    return unc_returnlocal(w, e, &v);
}

static void uncl_regex_unlock(Unc_View *w,
                            Unc_Value *tpat,
                            struct unc_regex_pattern *pat, int temporary) {
    if (temporary)
        unc_regex_pattern_destr(w, sizeof(struct unc_regex_pattern), pat);
    else
        unc_unlock(w, tpat);
}

Unc_RetVal uncl_regex_match(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Size sn;
    const char *ss;
    struct unc_regex_pattern pat;
    int temporary;
    Unc_Value v[2] = UNC_BLANKS;

    e = unc_getstring(w, &args.values[0], &sn, &ss);
    if (e) return e;

    e = uncl_regex_getpat(w, &args.values[1], &args.values[2],
        unc_boundvalue(w, 0), "regex.match()", udata, &pat, &temporary);
    if (e) return e;
    
    e = unc0_regex_match(w, &pat, sn, ss, v, udata);
    uncl_regex_unlock(w, &args.values[1], &pat, temporary);
    return unc_returnlocalarray(w, e, 2, v);
}

Unc_RetVal uncl_regex_find(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Size sn;
    const char *ss;
    struct unc_regex_pattern pat;
    int temporary;
    Unc_Value v[2] = UNC_BLANKS;
    Unc_Size startat = 0;

    e = unc_getstring(w, &args.values[0], &sn, &ss);
    if (e) return e;

    if (unc_gettype(w, &args.values[3])) {
        Unc_Int ui;
        e = unc_getint(w, &args.values[3], &ui);
        if (e) return e;
        if (ui < 0) {
            ui = sn - ui;
            if (ui < 0 || ui > sn) {
                unc_setint(w, &v[0], -1);
                return unc_push(w, 2, v);
            }
            startat = ui;
        } else {
            if (ui > sn) {
                unc_setint(w, &v[0], -1);
                return unc_push(w, 2, v);
            }
            startat = ui;
        }
        startat = unc0_utf8reshift((const byte *)ss, sn, startat);
        if (startat == UNC_RESHIFTBAD) {
            unc_setint(w, &v[0], -1);
            return unc_push(w, 2, v);
        }
    }

    e = uncl_regex_getpat(w, &args.values[1], &args.values[2],
        unc_boundvalue(w, 0), "regex.find()", udata, &pat, &temporary);
    if (e) return e;
    
    e = unc0_regex_find(w, &pat, sn, ss, v, startat, udata);
    uncl_regex_unlock(w, &args.values[1], &pat, temporary);
    return unc_returnlocalarray(w, e, 2, v);
}

Unc_RetVal uncl_regex_findlast(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Size sn;
    const char *ss;
    struct unc_regex_pattern pat;
    int temporary;
    Unc_Value v[2] = UNC_BLANKS;
    Unc_Size startat = 0;

    e = unc_getstring(w, &args.values[0], &sn, &ss);
    if (e) return e;

    if (unc_gettype(w, &args.values[3])) {
        Unc_Int ui;
        e = unc_getint(w, &args.values[3], &ui);
        if (e) return e;
        if (ui < 0) {
            ui = sn - ui;
            if (ui < 0 || ui >= sn) {
                unc_setint(w, &v[0], -1);
                return unc_push(w, 2, v);
            }
            startat = ui;
        } else {
            if (ui > sn) {
                unc_setint(w, &v[0], -1);
                return unc_push(w, 2, v);
            }
            startat = ui;
        }
        startat = unc0_utf8reshift((const byte *)ss, sn, startat);
        if (startat == UNC_RESHIFTBAD) {
            unc_setint(w, &v[0], -1);
            return unc_push(w, 2, v);
        }
    } else
        startat = sn;

    e = uncl_regex_getpat(w, &args.values[1], &args.values[2],
        unc_boundvalue(w, 0), "regex.findlast()", udata, &pat, &temporary);
    if (e) return e;

    e = unc0_regex_findlast(w, &pat, sn, ss, v, startat, udata);
    uncl_regex_unlock(w, &args.values[1], &pat, temporary);
    return unc_returnlocalarray(w, e, 2, v);
}

Unc_RetVal uncl_regex_findall(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Size sn;
    const char *ss;
    struct unc_regex_pattern pat;
    int temporary;
    struct unc0_varr buf;
    Unc_Value v[2] = UNC_BLANKS;
    Unc_Size startat = 0;

    e = unc_getstring(w, &args.values[0], &sn, &ss);
    if (e) return e;

    if (unc_gettype(w, &args.values[3])) {
        Unc_Int ui;
        e = unc_getint(w, &args.values[3], &ui);
        if (e) return e;
        if (ui < 0) {
            ui = sn - ui;
            if (ui < 0 || ui >= sn) {
                unc_setint(w, &v[0], -1);
                return unc_push(w, 2, v);
            }
            startat = ui;
        } else {
            if (ui >= sn) {
                unc_setint(w, &v[0], -1);
                return unc_push(w, 2, v);
            }
            startat = ui;
        }
        startat = unc0_utf8reshift((const byte *)ss, sn, startat);
        if (startat == UNC_RESHIFTBAD) {
            unc_setint(w, &v[0], -1);
            return unc_push(w, 2, v);
        }
    }

    e = unc0_varr_init(w, &buf);
    if (e) return e;

    e = uncl_regex_getpat(w, &args.values[1], &args.values[2],
        unc_boundvalue(w, 0), "regex.findall()", udata, &pat, &temporary);
    if (e) {
        unc0_varr_clear(w, &buf);
        return e;
    }
    
    e = unc0_regex_findall(w, &pat, sn, ss, &buf, v, startat, udata);
    unc_clearmany(w, 2, v);
    uncl_regex_unlock(w, &args.values[1], &pat, temporary);
    if (!e) {
        e = unc_returnlocal(w, 0, unc0_varr_done(w, &buf));
    } else {
        unc0_varr_clear(w, &buf);
    }
    return e;
}

Unc_RetVal uncl_regex_split(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Size sn;
    const char *ss;
    struct unc_regex_pattern pat;
    int temporary, direction = 0;
    struct unc0_varr buf;
    Unc_Size startat = 0, splits = UNC_SIZE_MAX;

    e = unc_getstring(w, &args.values[0], &sn, &ss);
    if (e) return e;

    if (unc_gettype(w, &args.values[3])) {
        Unc_Int ui;
        e = unc_getint(w, &args.values[3], &ui);
        if (e) return e;
        if (ui < 0) {
            direction = 1;
            startat = sn;
            splits = -ui;
        } else {
            splits = ui;
        }
    }

    e = unc0_varr_init(w, &buf);
    if (e) return e;

    e = uncl_regex_getpat(w, &args.values[1], &args.values[2],
        unc_boundvalue(w, 0), "regex.split()", udata, &pat, &temporary);
    if (e) {
        unc0_varr_clear(w, &buf);
        return e;
    }
    
    e = unc0_regex_split(w, &pat, sn, ss, &buf,
                         direction, startat, splits, udata);
    uncl_regex_unlock(w, &args.values[1], &pat, temporary);
    if (!e) {
        if (direction) {
            Unc_Size ia = 0, ib = buf.outi - 1;
            while (ia < ib)
                unc_swap(w, &buf.outv[ia++], &buf.outv[ib--]);
        }
        e = unc_returnlocal(w, 0, unc0_varr_done(w, &buf));
    } else {
        unc0_varr_clear(w, &buf);
    }
    return e;
}

Unc_RetVal uncl_regex_replace(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    int replfunc;
    Unc_Size sn, rpn;
    const char *ss, *rps;
    struct unc_regex_pattern pat;
    int temporary, direction = 0;
    Unc_Size startat = 0, replaces = UNC_SIZE_MAX;
    struct unc0_strbuf q;

    e = unc_getstring(w, &args.values[0], &sn, &ss);
    if (e) return e;

    if (unc_gettype(w, &args.values[4])) {
        Unc_Int ui;
        e = unc_getint(w, &args.values[4], &ui);
        if (e) return e;
        if (ui < 0) {
            direction = 1;
            startat = sn;
            replaces = -ui;
        } else {
            replaces = ui;
        }
    }

    switch (unc_gettype(w, &args.values[2])) {
    case Unc_TString:
        e = unc_getstring(w, &args.values[2], &rpn, &rps);
        if (e) return e;
        replfunc = 0;
        break;
    default:
        if (unc_iscallable(w, &args.values[2])) {
            replfunc = 1;
            break;
        }
        return unc_throwexc(w, "type",
            "replacement must be a string or function");
    }

    e = uncl_regex_getpat(w, &args.values[1], &args.values[3],
        unc_boundvalue(w, 0), "regex.replace()", udata, &pat, &temporary);
    if (e) return e;
    
    unc0_strbuf_init(&q, &w->world->alloc, Unc_AllocString);

    e = unc0_regex_replace(w, &pat, sn, ss, &q, direction, startat, replaces,
                           replfunc, &args.values[2], rpn, rps, udata);
    uncl_regex_unlock(w, &args.values[1], &pat, temporary);
    if (!e) {
        Unc_Value out = UNC_BLANK;
        if (direction) unc0_memrev(q.buffer, q.length);
        e = unc0_buftostring(w, &out, &q);
        e = unc_returnlocal(w, e, &out);
    }
    unc0_strbuf_free(&q);
    return e;
}

#define FN(x) &uncl_regex_##x, #x

static const Unc_ModuleCFunc lib[] = {
    { FN(engine),       0, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(escape),       1, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(escaperepl),   1, 0, 0, UNC_CFUNC_CONCURRENT },
};

static const Unc_ModuleCFunc lib_regex[] = {
    { FN(compile),      1, 1, 0, UNC_CFUNC_CONCURRENT },
    { FN(find),         2, 2, 0, UNC_CFUNC_CONCURRENT },
    { FN(findall),      2, 2, 0, UNC_CFUNC_CONCURRENT },
    { FN(findlast),     2, 2, 0, UNC_CFUNC_CONCURRENT },
    { FN(match),        2, 1, 0, UNC_CFUNC_CONCURRENT },
    { FN(replace),      3, 2, 0, UNC_CFUNC_CONCURRENT },
    { FN(split),        2, 2, 0, UNC_CFUNC_CONCURRENT },
};

Unc_RetVal uncilmain_regex(struct Unc_View *w) {
    Unc_RetVal e;
    Unc_Value regex_pattern = UNC_BLANK;
    void *gcxt = unc0_gcxt_make(w);
#ifdef REGEX_ENGINE
    if (!gcxt) return UNCIL_ERR_MEM;
#endif

    e = unc_newobject(w, &regex_pattern, NULL);
    if (e) return e;

    e = unc_exportcfunctions(w, PASSARRAY(lib), 0, NULL, NULL);
    if (e) return e;

    e = unc_exportcfunctions(w, PASSARRAY(lib_regex), 1, &regex_pattern, gcxt);
    if (e) return e;

    {
        Unc_Value ns = UNC_BLANK;
        e = unc_newstringc(w, &ns, "regex.pattern");
        if (e) return e;
        e = unc_setattrc(w, &regex_pattern, "__name", &ns);
        if (e) return e;
        VCLEAR(w, &ns);
    }

    e = unc_setpublicc(w, "pattern", &regex_pattern);
    if (e) return e;

    VCLEAR(w, &regex_pattern);
    return 0;
}
