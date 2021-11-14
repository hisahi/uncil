/*******************************************************************************
 
Uncil -- builtin regex library impl

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

#include <string.h>

#define UNCIL_DEFINES

#include "uctype.h"
#include "udef.h"
#include "uncil.h"
#include "uutf.h"
#include "uvsio.h"

#if UNCIL_LIB_PCRE2
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#endif

#ifndef PCRE2_ERROR_MAXLENGTH
#define PCRE2_ERROR_MAXLENGTH 256
#endif

struct unc_regex_pattern {
#if UNCIL_LIB_PCRE2
    pcre2_code *code;
#else
#define NOREGEXSUPPORT 1
    char tmp_;
#endif
};

#if UNCIL_LIB_PCRE2
void *unc__pcre2_alloc(PCRE2_SIZE z, void *w) {
    return unc_malloc((Unc_View *)w, z);
}

void unc__pcre2_free(void *p, void *w) {
    unc_mfree((Unc_View *)w, p);
}
#endif

Unc_RetVal unc_regex_pattern_destr(Unc_View *w, size_t n, void *data) {
    struct unc_regex_pattern *p = data;
#if UNCIL_LIB_PCRE2
    pcre2_code_free(p->code);
#else
    (void)p;
#endif
    return 0;
}

static int unc__regex_makeerr(Unc_View *w, const char *prefix, int errcode) {
#if UNCIL_LIB_PCRE2
    int e;
    Unc_Value msg = UNC_BLANK;
    PCRE2_UCHAR buffer[PCRE2_ERROR_MAXLENGTH];
    pcre2_get_error_message(errcode, buffer, sizeof(buffer));
    e = unc__usxprintf(w, &msg, "%s: %s",
        prefix, (const char *)buffer);
    if (e) return UNCIL_ERR_MEM;
    return e ? e : unc_throwext(w, "value", &msg);
#else
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
}

static int unc__regex_comp_makeerr(Unc_View *w, const char *prefix,
                              int errcode, Unc_Size erroffset) {
#if UNCIL_LIB_PCRE2
    int e;
    Unc_Value msg = UNC_BLANK;
    PCRE2_UCHAR buffer[PCRE2_ERROR_MAXLENGTH];
    pcre2_get_error_message(errcode, buffer, sizeof(buffer));
    e = unc__usxprintf(w, &msg, "%s at offset %"PRIUnc_Size"u: %s",
        prefix, erroffset, (const char *)buffer);
    if (e) return UNCIL_ERR_MEM;
    return e ? e : unc_throwext(w, "value", &msg);
#else
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
}

Unc_RetVal unc__lib_regex_engine(Unc_View *w, Unc_Tuple args, void *udata) {
    int e = 0;
    Unc_Value v = UNC_BLANK;
#if UNCIL_LIB_PCRE2
    e = unc_newstringc(w, &v, "pcre2");
#endif
    if (!e) e = unc_pushmove(w, &v, NULL);
    return e;
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

Unc_RetVal unc__lib_regex_escape(Unc_View *w, Unc_Tuple args, void *udata) {
    int e = 0;
    Unc_Value v = UNC_BLANK;
    Unc_Size sn;
    const char *s, *sx, *se;
    byte *out = NULL;
    Unc_Size out_n = 0, out_c = 0;
    Unc_Allocator *alloc = &w->world->alloc;

    e = unc_getstring(w, &args.values[0], &sn, &s);
    if (e) return e;
    se = s + sn;

    while (s != se) {
        sx = (const char *)unc__utf8nextchar((const byte *)s, &sn);
        if (!sx) sx = se;
        if (!(*s & 0x80) && shouldescape(*s)) {
            e = unc__strpush1(alloc, &out, &out_n, &out_c, 6, '\\');
            if (e) {
                unc__mmfree(alloc, out);
                return e;
            }
        }
        e = unc__strpush(alloc, &out, &out_n, &out_c, 6,
                sx - s, (const byte *)s);
        if (e) {
            unc__mmfree(alloc, out);
            return e;
        }
        s = sx;
    }

    e = unc_newstringmove(w, &v, out_n, (char *)out);
    if (!e) e = unc_pushmove(w, &v, NULL);
    return e;
}

Unc_RetVal unc__lib_regex_escaperepl(Unc_View *w, Unc_Tuple args, void *udata) {
    int e = 0;
    Unc_Value v = UNC_BLANK;
    Unc_Size sn;
    const char *s, *sx, *se;
    byte *out = NULL;
    Unc_Size out_n = 0, out_c = 0;
    Unc_Allocator *alloc = &w->world->alloc;

    e = unc_getstring(w, &args.values[0], &sn, &s);
    if (e) return e;
    se = s + sn;

    while (s != se) {
        sx = (const char *)unc__utf8nextchar((const byte *)s, &sn);
        if (!sx) sx = se;
        if (*s == '$') {
            e = unc__strpush1(alloc, &out, &out_n, &out_c, 6, '$');
            if (e) {
                unc__mmfree(alloc, out);
                return e;
            }
        }
        e = unc__strpush(alloc, &out, &out_n, &out_c, 6,
                sx - s, (const byte *)s);
        if (e) {
            unc__mmfree(alloc, out);
            return e;
        }
        s = sx;
    }

    e = unc_newstringmove(w, &v, out_n, (char *)out);
    if (!e) e = unc_pushmove(w, &v, NULL);
    return e;
}

#define UNC_PCRE2_COMPILE_OPTIONS (PCRE2_UCP | PCRE2_UTF | PCRE2_NO_UTF_CHECK  \
                            | PCRE2_NEVER_BACKSLASH_C)
#define UNC_PCRE2_MATCH_OPTIONS (PCRE2_NO_UTF_CHECK)

Unc_RetVal unc__lib_regex_compile(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    Unc_Size rgx_n, rfl_n = 0;
    const char *rgx, *rfl = NULL;

    e = unc_getstring(w, &args.values[0], &rgx_n, &rgx);
    if (e) return e;

    if (unc_gettype(w, &args.values[1])) {
        e = unc_getstring(w, &args.values[1], &rfl_n, &rfl);
        if (e) return e;
    }
    
    {
#if UNCIL_LIB_PCRE2
        Unc_Value v = UNC_BLANK;
        Unc_Size i;
        int errcode;
        PCRE2_SIZE erroffset;
        uint32_t options = UNC_PCRE2_COMPILE_OPTIONS;
        pcre2_code *code;
        struct unc_regex_pattern *data;
        pcre2_compile_context *ccxt;

        for (i = 0; i < rfl_n; ++i) {
            switch (rfl[i]) {
            case 'i':
                options |= PCRE2_CASELESS;
                break;
            case 'm':
                options |= PCRE2_MULTILINE;
                break;
            case 's':
                options |= PCRE2_DOTALL;
                break;
            default:
                return unc_throwexc(w, "value", "unrecognized flag");
            }
        }
        
        ccxt = pcre2_compile_context_create((pcre2_general_context *)udata);
        if (!ccxt) return UNCIL_ERR_MEM;
        code = pcre2_compile((PCRE2_SPTR)rgx, rgx_n, options,
                             &errcode, &erroffset, ccxt);
        pcre2_compile_context_free(ccxt);
        if (!code)
            return unc__regex_comp_makeerr(w, "regex.compile()",
                    errcode, erroffset);

        e = unc_newopaque(w, &v, unc_boundvalue(w, 0),
                          sizeof(struct unc_regex_pattern), (void **)&data,
                          &unc_regex_pattern_destr, 0, NULL, 0, NULL);
        if (e) {
            pcre2_code_free(code);
            return e;
        }
        data->code = code;
        unc_unlock(w, &v);
        return unc_pushmove(w, &v, NULL);
#else
        return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
    }
}

static Unc_RetVal unc__lib_regex_getpat(Unc_View *w,
                            Unc_Value *tpat, Unc_Value *flags,
                            Unc_Value *prototype,
                            const char *prefix,
                            void *udata,
                            struct unc_regex_pattern *pat, int *temporary) {
#if UNCIL_LIB_PCRE2
    int e;
    Unc_Size i;
    Unc_Size rgx_n, rfl_n = 0;
    const char *rgx, *rfl = NULL;
    int errcode;
    PCRE2_SIZE erroffset;
    uint32_t options = UNC_PCRE2_COMPILE_OPTIONS;
    pcre2_code *code;
    pcre2_compile_context *ccxt;

    if (unc_gettype(w, tpat) == Unc_TOpaque) {
        Unc_Value proto;
        int eq;
        unc_getprototype(w, tpat, &proto);
        eq = unc_issame(w, &proto, prototype);
        unc_clear(w, &proto);
        if (eq) {
            void *opq;
            e = unc_lockopaque(w, tpat, NULL, &opq);
            if (e) return e;
            *temporary = 0;
            *pat = *(struct unc_regex_pattern *)opq;
            return 0;
        }
    }

    e = unc_getstring(w, tpat, &rgx_n, &rgx);
    if (e) return e;

    if (unc_gettype(w, flags)) {
        e = unc_getstring(w, flags, &rfl_n, &rfl);
        if (e) return e;
    }
    
    for (i = 0; i < rfl_n; ++i) {
        switch (rfl[i]) {
        case 'i':
            options |= PCRE2_CASELESS;
            break;
        case 'm':
            options |= PCRE2_MULTILINE;
            break;
        case 's':
            options |= PCRE2_DOTALL;
            break;
        default:
            return unc_throwexc(w, "value", "unrecognized flag");
        }
    }
    
    ccxt = pcre2_compile_context_create((pcre2_general_context *)udata);
    if (!ccxt) return UNCIL_ERR_MEM;
    code = pcre2_compile((PCRE2_SPTR)rgx, rgx_n, options,
                            &errcode, &erroffset, ccxt);
    pcre2_compile_context_free(ccxt);
    if (!code)
        return unc__regex_comp_makeerr(w, prefix, errcode, erroffset);

    *temporary = 1;
    pat->code = code;
    return 0;
#else
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
}

static void unc__lib_regex_unlock(Unc_View *w,
                            Unc_Value *tpat,
                            struct unc_regex_pattern *pat, int temporary) {
    if (temporary)
        unc_regex_pattern_destr(w, sizeof(struct unc_regex_pattern), pat);
    else
        unc_unlock(w, tpat);
}

#if UNCIL_LIB_PCRE2
int unc__lib_regex_findnext(Unc_Size sn,
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

int unc__lib_regex_findprev(Unc_Size *psn,
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
        sindx = (const char *)unc__utf8scanbackw(
                    (const byte *)ss, (const byte *)ss + sindx, 0) - ss;
    for (;;) {
        int rc = pcre2_match(code, (PCRE2_SPTR)ss, sn,
                                sindx, UNC_PCRE2_MATCH_OPTIONS | PCRE2_ANCHORED,
                                mdata, mcxt);
        if (rc < 0) {
            if (rc == PCRE2_ERROR_NOMATCH) {
                if (!sindx)
                    rc = 0;
                else {
                    sindx = (const char *)unc__utf8scanbackw(
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

int unc__lib_regex_findbidi(int direction,
                            Unc_Size *psn,
                            pcre2_code *code,
                            const char *ss,
                            Unc_Size *startat,
                            pcre2_match_context *mcxt,
                            pcre2_match_data *mdata,
                            PCRE2_SIZE **povec) {
    return direction
        ? unc__lib_regex_findprev(psn, code, ss, startat, mcxt, mdata, povec)
        : unc__lib_regex_findnext(*psn, code, ss, startat, mcxt, mdata, povec);
}
#endif

Unc_RetVal unc__lib_regex_match(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    Unc_Size sn;
    const char *ss;
    struct unc_regex_pattern pat;
    int temporary;
    Unc_Value v[2] = UNC_BLANKS;

    e = unc_getstring(w, &args.values[0], &sn, &ss);
    if (e) return e;

    e = unc__lib_regex_getpat(w, &args.values[1], &args.values[2],
        unc_boundvalue(w, 0), "regex.match()", udata, &pat, &temporary);
    if (e) return e;
    
#if UNCIL_LIB_PCRE2
    {
        pcre2_general_context *gcxt = (pcre2_general_context *)udata;
        pcre2_match_data *mdata =
            pcre2_match_data_create_from_pattern(pat.code, gcxt);
        pcre2_match_context *mcxt = pcre2_match_context_create(gcxt);
        if (mdata && mcxt) {
            int rc = pcre2_match(pat.code, (PCRE2_SPTR)ss, sn, 0,
                                 UNC_PCRE2_MATCH_OPTIONS
                                 | PCRE2_ANCHORED
                                 | PCRE2_ENDANCHORED,
                                 mdata, mcxt);
            if (rc < 0) {
                if (rc == PCRE2_ERROR_NOMATCH)
                    unc_setbool(w, &v[0], 0);
                else
                    e = unc__regex_makeerr(w, "regex.match()", rc);
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
    }
#endif

    unc__lib_regex_unlock(w, &args.values[1], &pat, temporary);
    if (!e) unc_push(w, 2, v, NULL);
    unc_clearmany(w, 2, v);
    return e;
}

Unc_RetVal unc__lib_regex_find(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
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
                return unc_push(w, 2, v, NULL);
            }
            startat = ui;
        } else {
            if (ui > sn) {
                unc_setint(w, &v[0], -1);
                return unc_push(w, 2, v, NULL);
            }
            startat = ui;
        }
        startat = unc__utf8reshift((const byte *)ss, sn, startat);
        if (startat == UNC_RESHIFTBAD) {
            unc_setint(w, &v[0], -1);
            return unc_push(w, 2, v, NULL);
        }
    }

    e = unc__lib_regex_getpat(w, &args.values[1], &args.values[2],
        unc_boundvalue(w, 0), "regex.find()", udata, &pat, &temporary);
    if (e) return e;
    
#if UNCIL_LIB_PCRE2
    {
        pcre2_general_context *gcxt = (pcre2_general_context *)udata;
        pcre2_match_data *mdata =
            pcre2_match_data_create_from_pattern(pat.code, gcxt);
        pcre2_match_context *mcxt = pcre2_match_context_create(gcxt);
        if (mdata && mcxt) {
            PCRE2_SIZE *ovec;
            int rc = unc__lib_regex_findnext(sn, pat.code, ss, &startat,
                                             mcxt, mdata, &ovec);
            if (!rc)
                unc_setint(w, &v[0], -1);
            else if (rc < 0)
                e = unc__regex_makeerr(w, "regex.find()", rc);
            else {
                Unc_Size i;
                Unc_Value *av;
                unc_setint(w, &v[0],
                        unc__utf8unshift((const byte *)ss, ovec[0]));
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
    }
#endif

    unc__lib_regex_unlock(w, &args.values[1], &pat, temporary);
    if (!e) unc_push(w, 2, v, NULL);
    unc_clearmany(w, 2, v);
    return e;
}

Unc_RetVal unc__lib_regex_findlast(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
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
                return unc_push(w, 2, v, NULL);
            }
            startat = ui;
        } else {
            if (ui > sn) {
                unc_setint(w, &v[0], -1);
                return unc_push(w, 2, v, NULL);
            }
            startat = ui;
        }
        startat = unc__utf8reshift((const byte *)ss, sn, startat);
        if (startat == UNC_RESHIFTBAD) {
            unc_setint(w, &v[0], -1);
            return unc_push(w, 2, v, NULL);
        }
    } else
        startat = sn;

    e = unc__lib_regex_getpat(w, &args.values[1], &args.values[2],
        unc_boundvalue(w, 0), "regex.findlast()", udata, &pat, &temporary);
    if (e) return e;

#if UNCIL_LIB_PCRE2
    {
        pcre2_general_context *gcxt = (pcre2_general_context *)udata;
        pcre2_match_data *mdata =
            pcre2_match_data_create_from_pattern(pat.code, gcxt);
        pcre2_match_context *mcxt = pcre2_match_context_create(gcxt);
        if (mdata && mcxt) {
            PCRE2_SIZE *ovec;
            int rc = unc__lib_regex_findprev(&sn, pat.code, ss, &startat,
                                             mcxt, mdata, &ovec);
            if (!rc)
                unc_setint(w, &v[0], -1);
            else if (rc < 0)
                e = unc__regex_makeerr(w, "regex.findlast()", rc);
            else {
                Unc_Size i;
                Unc_Value *av;
                unc_setint(w, &v[0],
                        unc__utf8unshift((const byte *)ss, ovec[0]));
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
    }
#endif

    unc__lib_regex_unlock(w, &args.values[1], &pat, temporary);
    if (!e) unc_push(w, 2, v, NULL);
    unc_clearmany(w, 2, v);
    return e;
}

Unc_RetVal unc__lib_regex_findall(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    Unc_Size sn;
    const char *ss;
    struct unc_regex_pattern pat;
    int temporary;
    Unc_Value out = UNC_BLANK, *outv;
    Unc_Value v[2] = UNC_BLANKS;
    Unc_Size outi = 0, outn = 4, startat = 0;

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
                return unc_push(w, 2, v, NULL);
            }
            startat = ui;
        } else {
            if (ui >= sn) {
                unc_setint(w, &v[0], -1);
                return unc_push(w, 2, v, NULL);
            }
            startat = ui;
        }
        startat = unc__utf8reshift((const byte *)ss, sn, startat);
        if (startat == UNC_RESHIFTBAD) {
            unc_setint(w, &v[0], -1);
            return unc_push(w, 2, v, NULL);
        }
    }

    e = unc_newarray(w, &out, outn, &outv);
    if (e) return e;
    
    e = unc__lib_regex_getpat(w, &args.values[1], &args.values[2],
        unc_boundvalue(w, 0), "regex.findall()", udata, &pat, &temporary);
    if (e) {
        unc_unlock(w, &out);
        unc_clear(w, &out);
        return e;
    }
    
#if UNCIL_LIB_PCRE2
    {
        pcre2_general_context *gcxt = (pcre2_general_context *)udata;
        pcre2_match_data *mdata =
            pcre2_match_data_create_from_pattern(pat.code, gcxt);
        pcre2_match_context *mcxt = pcre2_match_context_create(gcxt);
        if (mdata && mcxt) {
            while (!e) {
                PCRE2_SIZE *ovec;
                int rc = unc__lib_regex_findnext(sn, pat.code, ss, &startat,
                                                mcxt, mdata, &ovec);
                if (!rc)
                    break;
                else if (rc < 0)
                    e = unc__regex_makeerr(w, "regex.findall()", rc);
                else {
                    Unc_Size i;
                    Unc_Value *av;
                    unc_setint(w, &v[0],
                            unc__utf8unshift((const byte *)ss, ovec[0]));
                    e = unc_newarray(w, &v[1], rc, &av);
                    for (i = 0; !e && i < rc; ++i) {
                        Unc_Size z0 = ovec[i * 2];
                        Unc_Size zl = ovec[i * 2 + 1] - z0;
                        e = unc_newstring(w, &av[i], zl, ss + z0);
                    }
                    e = unc_newarrayfrom(w, &v[0], 2, v);
                    if (!e && outi == outn) {
                        Unc_Size outz = outn + 4;
                        e = unc_resizearray(w, &out, outz, &outv);
                        if (!e) outn = outz;
                    }
                    if (!e) unc_copy(w, &outv[outi++], &v[0]);
                }
            }
        } else
            e = UNCIL_ERR_MEM;
        if (mdata) pcre2_match_data_free(mdata);
        if (mcxt) pcre2_match_context_free(mcxt);
    }
#endif

    unc_clearmany(w, 2, v);
    unc__lib_regex_unlock(w, &args.values[1], &pat, temporary);
    if (!e) e = unc_resizearray(w, &out, outi, &outv);
    unc_unlock(w, &out);
    if (!e) e = unc_pushmove(w, &out, NULL);
    return e;
}

Unc_RetVal unc__lib_regex_split(Unc_View *w, Unc_Tuple args, void *udata) {
#if NOREGEXSUPPORT
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#else
    int e;
    Unc_Size sn;
    const char *ss;
    struct unc_regex_pattern pat;
    int temporary, direction = 0;
    Unc_Value out = UNC_BLANK, *outv;
    Unc_Size outi = 0, outn = 4, startat = 0, splits = UNC_SIZE_MAX;

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

    e = unc_newarray(w, &out, outn, &outv);
    if (e) return e;
    
    e = unc__lib_regex_getpat(w, &args.values[1], &args.values[2],
        unc_boundvalue(w, 0), "regex.split()", udata, &pat, &temporary);
    if (e) {
        unc_unlock(w, &out);
        unc_clear(w, &out);
        return e;
    }
    
#if UNCIL_LIB_PCRE2
    {
        pcre2_general_context *gcxt = (pcre2_general_context *)udata;
        pcre2_match_data *mdata =
            pcre2_match_data_create_from_pattern(pat.code, gcxt);
        pcre2_match_context *mcxt = pcre2_match_context_create(gcxt);
        if (mdata && mcxt) {
            Unc_Size ended = startat;
            while (splits--) {
                Unc_Size started = startat;
                PCRE2_SIZE *ovec;
                int rc = unc__lib_regex_findbidi(direction, &sn, pat.code,
                                    ss, &startat, mcxt, mdata, &ovec);
                if (!rc)
                    break;
                else if (rc < 0)
                    e = unc__regex_makeerr(w, "regex.split()", rc);
                else {
                    if (outi == outn) {
                        Unc_Size outz = outn + 4;
                        e = unc_resizearray(w, &out, outz, &outv);
                        if (!e) outn = outz;
                    }
                    if (!e) e = !direction
                            ? unc_newstring(w, &outv[outi++],
                                    ovec[0] - started, ss + started)
                            : unc_newstring(w, &outv[outi++],
                                    started - ovec[1], ss + ovec[1]);
                    if (direction) ended = ovec[0];
                }
            }
            if (!e && outi == outn) {
                Unc_Size outz = outn + 4;
                e = unc_resizearray(w, &out, outz, &outv);
                if (!e) outn = outz;
            }
            if (!e) e = !direction
                    ? unc_newstring(w, &outv[outi++],
                            sn - startat, ss + startat)
                    : unc_newstring(w, &outv[outi++],
                            ended, ss);
        } else
            e = UNCIL_ERR_MEM;
        if (mdata) pcre2_match_data_free(mdata);
        if (mcxt) pcre2_match_context_free(mcxt);
    }
#endif

    unc__lib_regex_unlock(w, &args.values[1], &pat, temporary);
    if (!e) e = unc_resizearray(w, &out, outi, &outv);
    if (!e && direction) {
        Unc_Size ia = 0, ib = outi - 1;
        while (ia < ib)
            unc_swap(w, &outv[ia++], &outv[ib--]);
    }
    unc_unlock(w, &out);
    if (!e) unc_pushmove(w, &out, NULL);
    else unc_clear(w, &out);
    return e;
#endif
}

#if UNCIL_LIB_PCRE2
typedef PCRE2_SIZE *Unc_CaptureVector;
#else
typedef int *Unc_CaptureVector;
#endif

#if !NOREGEXSUPPORT
Unc_RetVal unc__lib_regex__getrepls(Unc_View *w, const char *ss,
                                   byte **q, Unc_Size *qn, Unc_Size *qc,
                                   Unc_Size rn, const char *rp,
                                   Unc_Size gn,
                                   Unc_CaptureVector ovec) {
    int e;
    Unc_Allocator *alloc = &w->world->alloc;
    const char *rpp = rp, *rpe = rp + rn;
    for (;;) {
        const char *rpn = unc__memchr(rpp, '$', rpe - rpp);
        int end = !rpn;
        int g_ok = 0;
        Unc_Size g;
        if (end) rpn = rpe;
        e = unc__strpush(alloc, q, qn, qc, 6, rpn - rpp, (const byte *)rpp);
        if (e) return e;
        if (!end) {
            rpp = rpn + 1;
            if (unc__isdigit(*rpp)) {
                /* capture group one digit */
                g_ok = 1;
                g = *rpp++ - '0';
            } else {
                switch (*rpp) {
                case '$':
                    ++rpp;
                    e = unc__strpush1(alloc, q, qn, qc, 6, '$');
                    break;
                case '<':
                    if (!unc__isdigit(*++rpp))
                        return unc_throwexc(w, "value",
                            "invalid dollar syntax in replacement string");
                    g = 0;
                    {
                        Unc_Size pg = g;
                        while (unc__isdigit(*rpp)) {
                            g = g * 10 + (*rpp - '0');
                            if (g < pg) {
                                g = gn;
                                while (unc__isdigit(*rpp))
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
                e = unc__strpush(alloc, q, qn, qc, 6,
                        ovec[g * 2 + 1] - ovec[g * 2],
                        (const byte *)ss + ovec[g * 2]);
            }
        }
        if (e) return e;
        if (end) return 0;
    }
}

Unc_RetVal unc__lib_regex__getreplf(Unc_View *w, const char *ss,
                                   byte **q, Unc_Size *qn, Unc_Size *qc,
                                   Unc_Value *repl,
                                   Unc_Size gn,
                                   Unc_CaptureVector ovec) {
    int e;
    Unc_Allocator *alloc = &w->world->alloc;
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
        unc_clear(w, &arr);
        return e;
    }
    e = unc_pushmove(w, &arr, NULL);
    if (e) {
        unc_clear(w, &arr);
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
    e = unc__strpush(alloc, q, qn, qc, 6, an, (const byte *)ab);
    unc_discard(w, &pile);
    return e;
}
#endif

Unc_RetVal unc__lib_regex_replace(Unc_View *w, Unc_Tuple args, void *udata) {
#if NOREGEXSUPPORT
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#else
    int e, replfunc;
    Unc_Size sn, rpn;
    const char *ss, *rps;
    struct unc_regex_pattern pat;
    int temporary, direction = 0;
    Unc_Value out = UNC_BLANK;
    Unc_Size startat = 0, replaces = UNC_SIZE_MAX;
    byte *q = NULL;
    Unc_Size qn = 0, qc = 0;
    Unc_Allocator *alloc = &w->world->alloc;

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

    e = unc__lib_regex_getpat(w, &args.values[1], &args.values[3],
        unc_boundvalue(w, 0), "regex.replace()", udata, &pat, &temporary);
    if (e) {
        unc_unlock(w, &out);
        unc_clear(w, &out);
        return e;
    }
    
#if UNCIL_LIB_PCRE2
    {
        pcre2_general_context *gcxt = (pcre2_general_context *)udata;
        pcre2_match_data *mdata =
            pcre2_match_data_create_from_pattern(pat.code, gcxt);
        pcre2_match_context *mcxt = pcre2_match_context_create(gcxt);
        if (mdata && mcxt) {
            Unc_Size ended = startat;
            while (replaces--) {
                Unc_Size started = startat;
                PCRE2_SIZE *ovec;
                int rc = unc__lib_regex_findbidi(direction, &sn, pat.code,
                                    ss, &startat, mcxt, mdata, &ovec);
                if (!rc)
                    break;
                else if (rc < 0)
                    e = unc__regex_makeerr(w, "regex.replace()", rc);
                else {
                    e = !direction
                        ? unc__strpush(alloc, &q, &qn, &qc, 6,
                            ovec[0] - started, (const byte *)ss + started)
                        : unc__strpushrv(alloc, &q, &qn, &qc, 6,
                            started - ovec[1], (const byte *)ss + ovec[1]);
                    if (direction) ended = ovec[0];
                    if (!e) {
                        Unc_Size qx = qn;
                        e = replfunc
                            ? unc__lib_regex__getreplf(w, ss, &q, &qn, &qc,
                                             &args.values[2], rc, ovec)
                            : unc__lib_regex__getrepls(w, ss, &q, &qn, &qc,
                                             rpn, rps, rc, ovec);
                        if (!e && qn > qx && direction)
                            unc__memrev(q + qx, qn - qx);
                    }
                }
            }
            if (!e) e = !direction
                ? unc__strpush(alloc, &q, &qn, &qc, 6,
                                sn - startat, (const byte *)ss + startat)
                : unc__strpushrv(alloc, &q, &qn, &qc, 6,
                                ended, (const byte *)ss);
        } else
            e = UNCIL_ERR_MEM;
        if (mdata) pcre2_match_data_free(mdata);
        if (mcxt) pcre2_match_context_free(mcxt);
    }
#endif

    unc__lib_regex_unlock(w, &args.values[1], &pat, temporary);
    if (!e) {
        if (direction) unc__memrev(q, qn);
        e = unc_newstringmove(w, &out, qn, (char *)q);
    }
    unc_unlock(w, &out);
    if (!e) unc_pushmove(w, &out, NULL);
    else unc_clear(w, &out);
    return e;
#endif
}

Unc_RetVal uncilmain_regex(struct Unc_View *w) {
    Unc_RetVal e;
    Unc_Value regex_pattern = UNC_BLANK;
    void *gcxt = NULL;

#if UNCIL_LIB_PCRE2
    gcxt = pcre2_general_context_create(&unc__pcre2_alloc, &unc__pcre2_free, w);
    if (!gcxt) return UNCIL_ERR_MEM;
#endif

    e = unc_newobject(w, &regex_pattern, NULL);
    if (e) return e;

    e = unc_exportcfunction(w, "compile", &unc__lib_regex_compile,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 1, NULL, 1, &regex_pattern, 0, NULL, gcxt);
    if (e) return e;

    e = unc_exportcfunction(w, "engine", &unc__lib_regex_engine,
                            UNC_CFUNC_CONCURRENT,
                            0, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;

    e = unc_exportcfunction(w, "escape", &unc__lib_regex_escape,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;

    e = unc_exportcfunction(w, "escaperepl", &unc__lib_regex_escaperepl,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;

    e = unc_exportcfunction(w, "find", &unc__lib_regex_find,
                            UNC_CFUNC_CONCURRENT,
                            2, 0, 2, NULL, 1, &regex_pattern, 0, NULL, gcxt);
    if (e) return e;

    e = unc_exportcfunction(w, "findall", &unc__lib_regex_findall,
                            UNC_CFUNC_CONCURRENT,
                            2, 0, 2, NULL, 1, &regex_pattern, 0, NULL, gcxt);
    if (e) return e;

    e = unc_exportcfunction(w, "findlast", &unc__lib_regex_findlast,
                            UNC_CFUNC_CONCURRENT,
                            2, 0, 2, NULL, 1, &regex_pattern, 0, NULL, gcxt);
    if (e) return e;

    e = unc_exportcfunction(w, "match", &unc__lib_regex_match,
                            UNC_CFUNC_CONCURRENT,
                            2, 0, 1, NULL, 1, &regex_pattern, 0, NULL, gcxt);
    if (e) return e;

    e = unc_exportcfunction(w, "replace", &unc__lib_regex_replace,
                            UNC_CFUNC_CONCURRENT,
                            3, 0, 2, NULL, 1, &regex_pattern, 0, NULL, gcxt);
    if (e) return e;

    e = unc_exportcfunction(w, "split", &unc__lib_regex_split,
                            UNC_CFUNC_CONCURRENT,
                            2, 0, 2, NULL, 1, &regex_pattern, 0, NULL, gcxt);
    if (e) return e;

    {
        Unc_Value ns = UNC_BLANK;
        e = unc_newstringc(w, &ns, "regex.pattern");
        if (e) return e;
        e = unc_setattrc(w, &regex_pattern, "__name", &ns);
        if (e) return e;
        unc_clear(w, &ns);
    }

    e = unc_setpublicc(w, "pattern", &regex_pattern);
    if (e) return e;

    unc_clear(w, &regex_pattern);
    return 0;
}
