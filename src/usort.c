/*******************************************************************************
 
Uncil -- sorting impl

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

/* sorting algorithm based on WikiSort by M. McFadden */

#include <setjmp.h>

#define UNCIL_DEFINES

#include "uarithm.h"
#include "ucompdef.h"
#include "ucommon.h"
#include "usort.h"
#include "uval.h"
#include "uvali.h"
#include "uvop.h"
#include "uvm.h"

struct unc0_sortenv {
    Unc_Value *fn;
    Unc_View *w;
    jmp_buf env;
    Unc_RetVal e;
    Unc_Value *buf;
    Unc_Size bufn;
};

static Unc_RetVal unc0_arrcmp(struct unc0_sortenv *s, Unc_Value *a,
                                                      Unc_Value *b) {
    Unc_RetVal e;
    if (LIKELY(!s->fn)) {
        e = unc0_vvcmpe(s->w, a, b, s->e);
        if (UNLIKELY(UNCIL_IS_ERR_CMP(e))) goto unc0_arrcmp_err;
        return e;
    } else {
        Unc_View *w = s->w;
        Unc_Size d = unc0_stackdepth(&w->sval);
        Unc_Pile ret;
        Unc_Tuple tuple;
        if (UNLIKELY(s->e)) return 0;
        if ((e = unc_newpile(w, &ret)))
            return e;
        if ((e = unc0_stackpush(w, &w->sval, 1, a)))
            return e;
        if ((e = unc0_stackpush(w, &w->sval, 1, b))) {
            unc0_restoredepth(w, &w->sval, d);
            return e;
        }
        w->region.base[ret.r] -= 2;
        e = unc0_fcallv(w, s->fn, 2, 1, 1, 1, 0);
        if (!e)
            e = unc0_run(w);
        else if (UNCIL_IS_ERR(e))
            goto unc0_arrcmp_err;
        unc_returnvalues(w, &ret, &tuple);
        if (tuple.count < 1) {
            e = unc_throwexc(w, "value", "comparator did not return a value");
            goto unc0_arrcmp_err;
        }
        switch (tuple.values[0].type) {
        case Unc_TInt:
            e = unc0_cmpint(tuple.values[0].v.i, 0);
            break;
        case Unc_TFloat:
            if (tuple.values[0].v.f != tuple.values[0].v.f) {
                e = unc_throwexc(w, "value", "comparator returned NaN");
                goto unc0_arrcmp_err;
            }
            e = unc0_cmpflt(tuple.values[0].v.f, 0);
            break;
        default:
            e = unc_throwexc(w, "value",
                "comparator did not return a valid numeric value");
            goto unc0_arrcmp_err;
        }
        unc_discard(w, &ret);
        return e;
    }
unc0_arrcmp_err:
    s->e = e;
    return 0;
}

void unc0_sort_fail(struct unc0_sortenv *s) {
    longjmp(s->env, s->e);
}

/* threshold for insertion sort */
#define IBUF 16
/* gallop enable threshold */
#define GBUF 6
/* threshold for simple merge */
#define MBUF 48
/* initial gallop threshold */
#define GALLOP 5
/* smart rotation threshold */
#define ROTBUF 128
/* lazy rotation threshold */
#define LROTBUF 16

#ifdef MIN
#undef MIN
#endif

#define RANGE_LEN(R_) ((R_).end - (R_).beg)
#define RANGE_NEW(R_, i_, j_) ((R_).beg = (i_), (R_).end = (j_))
#define RANGE_NEWI(R_, A_, i_, j_) RANGE_NEW(R_, (A_) + (i_), (A_) + (j_))
#define RANGE_COPY(a_, R_) COPY_N(a_, R_.beg, RANGE_LEN(R_))
#define RANGE_ROTATE(R_, n_) ROTATE_N((R_).beg, (R_).end, n_)
#define RANGE_PASS(R_) (R_).beg, (R_).end

#define CHECKERR(s_) if (UNLIKELY(s_->e)) unc0_sort_fail(s_)
#define MIN(a_, b_) ((a_) < (b_) ? (a_) : (b_))
#define CMP(a_, b_) unc0_arrcmp(s, (a_), (b_))

/* for SWAP_* and COPY_*: a_ and b_ must be evaluated only once!! */
#define SWAP(a_, b_) unc0_arrswap(a_, b_)
#define SWAP_N(a_, b_, n_) unc0_arrswapn(a_, b_, n_)
#define COPY_N(a_, b_, n_) TMEMCPY(Unc_Value, a_, b_, n_)

#define ROTATE_N(a_, b_, n_) unc0_arrrotb((a_), (a_) + (n_), (b_), z, zn)
#define ROTATE_N_NOBUF(a_, b_, n_) unc0_arrrot((a_), (a_) + (n_), (b_))

INLINE void unc0_arrswap(Unc_Value *l, Unc_Value *r) {
    Unc_Value v = *l;
    *l = *r;
    *r = v;
}

static void unc0_sortins(struct unc0_sortenv *s, Unc_Value *l, Unc_Value *r) {
    /* insertion sort */
    Unc_Value *i = l + 1, *j;
    if (l == r) return;
    while (i != r) {
        Unc_Value p = *i;
        j = i - 1;
        while (j >= l) {
            if (CMP(j, &p) <= 0) break;
            j[1] = j[0];
            --j;
        }
        j[1] = p;
        ++i;
    }
    CHECKERR(s);
}

INLINE void unc0_arrswapn(Unc_Value *l, Unc_Value *r, Unc_Size n) {
    Unc_Value t;
    while (n--) {
        t = *l;
        *l++ = *r;
        *r++ = t;
    }
}

INLINE void unc0_arrrev(Unc_Value *l, Unc_Value *r) {
    Unc_Value t;
    --r;
    while (r > l) {
        t = *l;
        *l++ = *r;
        *r-- = t;
    }
}

INLINE void unc0_arrrot_i(Unc_Value *l, Unc_Value *m, Unc_Value *r) {
    if (r - l < ROTBUF) {
        unc0_arrrev(l, m);
        unc0_arrrev(m, r);
        unc0_arrrev(l, r);
    } else {
        /* "trinity" (triple conjoined reversal) rotation */
        Unc_Size q, ql, qr;
        Unc_Value *a = l, *b = m - 1, *c = m, *d = r - 1;

        ql = b - a, qr = d - c;
        q = (MIN(ql, qr) + 1) / 2;
        while (q--) {
            SWAP(a  , b--);
            SWAP(c++, d  );
            SWAP(a++, d--);
        }
        
        if (ql >= qr) {
            q = (ql + 1) / 2;
            while (q--) {
                SWAP(a  , b--);
                SWAP(a++, d--);
            }
        } else {
            q = (qr + 1) / 2;
            while (q--) {
                SWAP(c++, d  );
                SWAP(a++, d--);
            }
        }

        q = (d - a + 1) / 2;
        while (q--) SWAP(a++, d--);
    }
}

static void unc0_arrrot(Unc_Value *l, Unc_Value *m, Unc_Value *r) {
    Unc_Size ln = m - l, rn = r - m;
    if (ln <= LROTBUF) {
        Unc_Value b[LROTBUF];
        if (!ln) return;
        COPY_N(b, l, ln);
        TMEMMOVE(Unc_Value, l, m, r - m);
        COPY_N(r - ln, b, ln);
        return;
    } else if (rn <= LROTBUF) {
        Unc_Value b[LROTBUF];
        if (!rn) return;
        COPY_N(b, r - rn, rn);
        TMEMMOVE(Unc_Value, l + rn, l, m - l);
        COPY_N(l, b, rn);
        return;
    } else
        unc0_arrrot_i(l, m, r);
}

static void unc0_arrrotb(Unc_Value *l, Unc_Value *m, Unc_Value *r,
                         Unc_Value *z, Unc_Size zn) {
    Unc_Size ln = m - l, rn;
    if (ln <= LROTBUF) {
        Unc_Value b[LROTBUF];
        if (!ln) return;
        COPY_N(b, l, ln);
        TMEMMOVE(Unc_Value, l, m, r - m);
        COPY_N(r - ln, b, ln);
        return;
    } else if (ln <= zn) {
        COPY_N(z, l, ln);
        TMEMMOVE(Unc_Value, l, m, r - m);
        COPY_N(r - ln, z, ln);
        return;
    }
    rn = r - m;
    if (rn <= LROTBUF) {
        Unc_Value b[LROTBUF];
        if (!rn) return;
        COPY_N(b, r - rn, rn);
        TMEMMOVE(Unc_Value, l + rn, l, m - l);
        COPY_N(l, b, rn);
        return;
    } else if (rn <= zn) {
        COPY_N(z, r - rn, rn);
        TMEMMOVE(Unc_Value, l + rn, l, m - l);
        COPY_N(l, z, rn);
        return;
    }
    unc0_arrrot_i(l, m, r);
}

struct wiki_range {
    Unc_Value *beg, *end;
};

static Unc_Value *unc0_wiki_findbf_i(struct unc0_sortenv *s,
                                     Unc_Value *l, Unc_Value *r,
                                     Unc_Value *v) {
    if (l < r) {
        Unc_Value *q = r - 1;
        while (l < r) {
            Unc_Value *m = l + (r - l) / 2;
            if (CMP(v, m) <= 0)
                r = m;
            else
                l = m + 1;
        }
        if (l == q && CMP(l, v) < 0) ++l;
    }
    return l;
}

static Unc_Value *unc0_wiki_findbl_i(struct unc0_sortenv *s,
                                     Unc_Value *l, Unc_Value *r,
                                     Unc_Value *v) {
    if (l < r) {
        Unc_Value *q = r - 1;
        while (l < r) {
            Unc_Value *m = l + (r - l) / 2;
            if (CMP(v, m) < 0)
                r = m;
            else
                l = m + 1;
        }
        if (r == q && CMP(r, v) <= 0) ++r;
    }
    return r;
}

static Unc_Value *unc0_wiki_findff_i(struct unc0_sortenv *s,
                                     Unc_Value *l, Unc_Value *r,
                                     Unc_Value *v, Unc_Size u) {
    Unc_Size k;
    Unc_Value *p;
    if (l == r) return l;
    k = (r - l) / u;
    if (!k) k = 1;
    for (p = l + k; CMP(v, p - 1) > 0; p += k) {
        if (p >= r - k) return unc0_wiki_findbf_i(s, p, r, v);
    }
    return unc0_wiki_findbf_i(s, p - k, p, v);
}

static Unc_Value *unc0_wiki_findlf_i(struct unc0_sortenv *s,
                                     Unc_Value *l, Unc_Value *r,
                                     Unc_Value *v, Unc_Size u) {
    Unc_Size k;
    Unc_Value *p;
    if (l == r) return l;
    k = (r - l) / u;
    if (!k) k = 1;
    for (p = l + k; CMP(v, p - 1) >= 0; p += k) {
        if (p >= r - k) return unc0_wiki_findbl_i(s, p, r, v);
    }
    return unc0_wiki_findbl_i(s, p - k, p, v);
}

static Unc_Value *unc0_wiki_findfb_i(struct unc0_sortenv *s,
                                     Unc_Value *l, Unc_Value *r,
                                     Unc_Value *v, Unc_Size u) {
    Unc_Size k;
    Unc_Value *p;
    if (l == r) return l;
    k = (r - l) / u;
    if (!k) k = 1;
    for (p = r - k; p > l && CMP(v, p - 1) <= 0; p -= k) {
        if (p < l + k) return unc0_wiki_findbf_i(s, l, p, v);
    }
    return unc0_wiki_findbf_i(s, p, p + k, v);
}

static Unc_Value *unc0_wiki_findlb_i(struct unc0_sortenv *s,
                                     Unc_Value *l, Unc_Value *r,
                                     Unc_Value *v, Unc_Size u) {
    Unc_Size k;
    Unc_Value *p;
    if (l == r) return l;
    k = (r - l) / u;
    if (!k) k = 1;
    for (p = r - k; p > l && CMP(v, p - 1) < 0; p -= k) {
        if (p < l + k) return unc0_wiki_findbl_i(s, l, p, v);
    }
    return unc0_wiki_findbl_i(s, p, p + k, v);
}

INLINE Unc_Value *unc0_wiki_findbf(struct unc0_sortenv *s,
                                   Unc_Value *l, Unc_Value *r,
                                   Unc_Value *vv) {
    Unc_Value v = *vv;
    return unc0_wiki_findbf_i(s, l, r, &v);
}

INLINE Unc_Value *unc0_wiki_findbl(struct unc0_sortenv *s,
                                   Unc_Value *l, Unc_Value *r,
                                   Unc_Value *vv) {
    Unc_Value v = *vv;
    return unc0_wiki_findbl_i(s, l, r, &v);
}

INLINE Unc_Value *unc0_wiki_findff(struct unc0_sortenv *s,
                                   Unc_Value *l, Unc_Value *r,
                                   Unc_Value *vv, Unc_Size u) {
    Unc_Value v = *vv;
    return unc0_wiki_findff_i(s, l, r, &v, u);
}

INLINE Unc_Value *unc0_wiki_findlf(struct unc0_sortenv *s,
                                   Unc_Value *l, Unc_Value *r,
                                   Unc_Value *vv, Unc_Size u) {
    Unc_Value v = *vv;
    return unc0_wiki_findlf_i(s, l, r, &v, u);
}

INLINE Unc_Value *unc0_wiki_findfb(struct unc0_sortenv *s,
                                   Unc_Value *l, Unc_Value *r,
                                   Unc_Value *vv, Unc_Size u) {
    Unc_Value v = *vv;
    return unc0_wiki_findfb_i(s, l, r, &v, u);
}

INLINE Unc_Value *unc0_wiki_findlb(struct unc0_sortenv *s,
                                   Unc_Value *l, Unc_Value *r,
                                   Unc_Value *vv, Unc_Size u) {
    Unc_Value v = *vv;
    return unc0_wiki_findlb_i(s, l, r, &v, u);
}

struct unc0_merge_result {
    Unc_Value *r, *a, *b;
};

INLINE struct unc0_merge_result unc0_merge_result_is(Unc_Value *r,
                                       Unc_Value *a, Unc_Value *b) {
    struct unc0_merge_result m;
    m.r = r;
    m.a = a;
    m.b = b;
    return m;
}

INLINE int unc0_merge_gallop_e(int d, int allow_eq) {
    return allow_eq ? d <= 0 : d < 0;
}

INLINE Unc_Size unc0_merge_gallop_i(struct unc0_sortenv *s, int allow_eq,
                                    Unc_Value *a0, Unc_Value *a1,
                                    Unc_Value *b) {
    Unc_Value *s0 = a0, *s1 = a0;
    Unc_Size step = 1;

    while (s1 < a1 && unc0_merge_gallop_e(unc0_arrcmp(s, s1, b), allow_eq)) {
        s0 = s1;
        s1 = s1 + step;
        step <<= 1;
    }
    if (s1 > a1) s1 = a1;

    if (s0 == s1) return 0;
    if (allow_eq)
        s0 = unc0_wiki_findbl(s, s0, s1, b);
    else
        s0 = unc0_wiki_findbf(s, s0, s1, b);
    return s0 - a0;
}

static Unc_Size unc0_merge_gallop_a(struct unc0_sortenv *s,
                                    Unc_Value *a0, Unc_Value *a1,
                                    Unc_Value *b) {
    return unc0_merge_gallop_i(s, 1, a0, a1, b);
}

static Unc_Size unc0_merge_gallop_b(struct unc0_sortenv *s,
                                    Unc_Value *b0, Unc_Value *b1,
                                    Unc_Value *a) {
    return unc0_merge_gallop_i(s, 0, b0, b1, a);
}

/* returns the output pointer and A and B input pointers
   caller is responsible to check that a < a1 && b < b1 */
INLINE struct unc0_merge_result unc0_merge_impl(
                                     struct unc0_sortenv *s, int swap,
                                     Unc_Value *a, Unc_Value *a1,
                                     Unc_Value *b, Unc_Value *b1,
                                     Unc_Value *z) {
#define MERGE_OUT(x)                                                           \
    if (swap)                                                                  \
        SWAP(z++, x++);                                                        \
    else                                                                       \
        *z++ = *x++;
#define MERGE_OUTN(x, n)                                                       \
    if (swap)                                                                  \
        SWAP_N(z, x, n), z += n, x += n;                                       \
    else                                                                       \
        COPY_N(z, x, n), z += n, x += n;
#define MERGE_END(x, x1) if (x == x1) return unc0_merge_result_is(z, a, b);

    Unc_Size an = a1 - a, bn = b1 - b, n = an + bn;
    
    if (n > MBUF && an > GBUF && bn > GBUF) {
        int gcond;
        if (GBUF > 0) {
            if (unc0_arrcmp(s, a, b) <= 0) {
                gcond = unc0_arrcmp(s, a + GBUF, b) <= 0;
            } else {
                gcond = unc0_arrcmp(s, b + GBUF, a) < 0;
            }
        } else {
            gcond = 1;
        }
        if (gcond) {
            /* merge with possible gallop */
            Unc_Size run_gallop = GALLOP;
            int gallop_b = 0;
            Unc_Size galloped = 0;
            Unc_Size g;

            for (;;) {
                if (CMP(a, b) <= 0) {
found_a:
                    MERGE_OUT(a);
                    MERGE_END(a, a1);
                    if (gallop_b) {
                        gallop_b = 0;
                        galloped = 1;
                    } else if (++galloped > run_gallop) {
                        g = unc0_merge_gallop_a(s, a, a1, b);
                        MERGE_OUTN(a, g);
                        MERGE_END(a, a1);
                        if (g > run_gallop) {
                            if (run_gallop > 3) --run_gallop;
                        } else {
                            ++run_gallop;
                        }
                        goto found_b;
                    }
                } else {
found_b:
                    MERGE_OUT(b);
                    MERGE_END(b, b1);
                    if (!gallop_b) {
                        gallop_b = 1;
                        galloped = 1;
                    } else if (++galloped > run_gallop) {
                        g = unc0_merge_gallop_b(s, b, b1, a);
                        MERGE_OUTN(b, g);
                        MERGE_END(b, b1);
                        if (g > run_gallop) {
                            if (run_gallop > 3) --run_gallop;
                        } else {
                            ++run_gallop;
                        }
                        goto found_a;
                    }
                }
            }
        }
    }
    
    /* simple merge */
    for (;;) {
        if (CMP(a, b) <= 0) {
            MERGE_OUT(a);
            MERGE_END(a, a1);
        } else {
            MERGE_OUT(b);
            MERGE_END(b, b1);
        }
    }
#undef MERGE_OUT
#undef MERGE_OUTN
#undef MERGE_END
}

static void unc0_wiki_merge_internal(struct unc0_sortenv *s,
                                     Unc_Value *a0, Unc_Value *a1,
                                     Unc_Value *b0, Unc_Value *b1,
                                     Unc_Value *z) {
    Unc_Value *r;
    struct unc0_merge_result m;
    if (a0 == a1) return;

    /* A is in z */
    r = a0;
    a1 = &z[a1 - a0];
    a0 = z;
    
    if (b0 < b1) {
        m = unc0_merge_impl(s, 1, a0, a1, b0, b1, r);
        SWAP_N(m.r, m.a, a1 - m.a);
    } else
        SWAP_N(r, a0, a1 - a0);
    CHECKERR(s);
}

static void unc0_wiki_merge_external(struct unc0_sortenv *s,
                                    Unc_Value *a0, Unc_Value *a1,
                                    Unc_Value *b0, Unc_Value *b1,
                                    Unc_Value *z) {
    Unc_Value *r;
    struct unc0_merge_result m;
    if (a0 == a1) return;

    /* A is in z */
    r = a0;
    a1 = &z[a1 - a0];
    a0 = z;

    if (b0 < b1) {
        m = unc0_merge_impl(s, 1, a0, a1, b0, b1, r);
        COPY_N(m.r, m.a, a1 - m.a);
    } else
        COPY_N(r, a0, a1 - a0);
    CHECKERR(s);
}

static void unc0_wiki_merge_into(struct unc0_sortenv *s,
                                 Unc_Value *a0, Unc_Value *a1,
                                 Unc_Value *b0, Unc_Value *b1,
                                 Unc_Value *z) {
    struct unc0_merge_result m;
    if (UNLIKELY(a0 == a1)) {
        COPY_N(z, b0, b1 - b0);
        return;
    }
    if (UNLIKELY(b0 == b1)){
        COPY_N(z, a0, a1 - a0);
        return;
    }
    m = unc0_merge_impl(s, 0, a0, a1, b0, b1, z);
    if (m.a < a1)
        COPY_N(m.r, m.a, a1 - m.a);
    else if (m.b < b1)
        COPY_N(m.r, m.b, b1 - m.b);
    CHECKERR(s);
}

static void unc0_wiki_merge_inplace(struct unc0_sortenv *s,
                                    Unc_Value *a0, Unc_Value *a1,
                                    Unc_Value *b0, Unc_Value *b1,
                                    Unc_Value *z, Unc_Size zn) {
    if (b0 == b1 || a0 == a1) return;
    for (;;) {
        Unc_Value *m = unc0_wiki_findbf(s, b0, b1, a0);
        Unc_Size n = m - a1;
        ROTATE_N(a0, m, a1 - a0);
        if (b1 == m) break;
        b0 = m;
        a0 += n;
        a1 = b0;
        a0 = unc0_wiki_findbl(s, a0, a1, a0);
        if (a0 == a1) break;
    }
    CHECKERR(s);
}

INLINE Unc_Size rounddown_pow2(Unc_Size v) {
    Unc_Size j = 1;
    while (v & (v - 1)) v &= v - 1, j <<= 1;
    return v;
}

INLINE Unc_Size ilog2(Unc_Size v) {
    Unc_Size n = 0;
    while ((v >>= 1)) ++n;
    return n;
}

INLINE Unc_Size sqrtz(Unc_Size v) {
    Unc_Size l = 1 << (ilog2(v) >> 1);
    Unc_Size r = l << 1;
    while (l < r) {
        Unc_Size m = l + (r - l) / 2;
        Unc_Size p = m * m;
        if (v == p) return m;
        else if (v < p) r = m;
        else l = m + 1;
    }
    if (l * l > v) --l;
    return l;
}

struct wiki_iter {
    Unc_Value *arr;
    Unc_Size s;
    Unc_Size q, dq;
    Unc_Size n, dn;
    Unc_Size d;
};

#define ITER_LEN(it) (it).dq
#define ITER_RESET(it) (it).n = (it).q = 0
#define ITER_DONE(it) ((it).q >= (it).s)

INLINE void unc0_wiki_iter_next(struct wiki_iter *it, struct wiki_range *r) {
    Unc_Size s = it->q;
    it->q += it->dq;
    if ((it->n += it->dn) >= it->d) {
        it->n -= it->d;
        ++it->q;
    }
    RANGE_NEW(*r, &it->arr[s], &it->arr[it->q]);
}

INLINE int unc0_wiki_iter_up(struct wiki_iter *it) {
    it->dq += it->dq;
    if ((it->dn += it->dn) >= it->d) {
        it->dn -= it->d;
        ++it->dq;
    }
    return it->dq < it->s;
}

struct wiki_pull {
    Unc_Value *beg, *end;
    struct wiki_range r;
    Unc_Size cnt;
};

static void unc0_wiki_sort(struct unc0_sortenv *s,
                           Unc_Value *l, Unc_Value *r) {
#if IBUF > 8
#define IBUF2 IBUF
#else
#define IBUF2 8
#endif
    Unc_Size n = r - l, np2 = rounddown_pow2(n);
    struct wiki_iter it;
    Unc_Size zn;
    Unc_Value *z;

    if (n <= IBUF2) {
        unc0_sortins(s, l, r);
        return;
    }

    zn = n / 2 + 1;
    while (zn >= 32) {
        z = TMALLOC(Unc_Value, &s->w->world->alloc, Unc_AllocInternal, zn);
        if (z) goto cache_ok;
        zn >>= 1;
        zn += zn >> 1;
    }
    
    z = NULL, zn = 0;
cache_ok:
    s->buf = z, s->bufn = zn;
    
    it.arr = l;
    it.s = n;
    it.d = np2 / (IBUF2 >> 1);
    it.dq = n / it.d;
    it.dn = n % it.d;

    ITER_RESET(it);
    while (!ITER_DONE(it)) {
        struct wiki_range r;
        unc0_wiki_iter_next(&it, &r);
        unc0_sortins(s, RANGE_PASS(r));
    }

    do {
        Unc_Size itl = ITER_LEN(it);
        Unc_Size bs;
        Unc_Size fs;
        struct wiki_range f1, f2, a, b;
        struct wiki_pull pulls[2];
        Unc_Size cnt, ff;
        Unc_Value *p, *p1;
        unsigned pull;
        int separate;
#define PULL (pulls[pull])

        if (itl < zn) {
            if ((itl + 1) * 4 <= zn && itl * 4 <= n) {
                struct wiki_range a1, a2, b1, b2;
#define a3 a
#define b3 b
                ITER_RESET(it);
                while (!ITER_DONE(it)) {
                    Unc_Value *zz;
                    unc0_wiki_iter_next(&it, &a1);
                    unc0_wiki_iter_next(&it, &b1);
                    unc0_wiki_iter_next(&it, &a2);
                    unc0_wiki_iter_next(&it, &b2);

                    if (CMP(a1.beg, b1.end - 1) > 0) {
                        RANGE_COPY(z, b1);
                        RANGE_COPY(z + RANGE_LEN(b1), a1);
                    } else if (CMP(b1.beg, a1.end - 1) < 0) {
                        unc0_wiki_merge_into(s, RANGE_PASS(a1),
                                                RANGE_PASS(b1), z);
                    } else if (CMP(b1.end - 1, a2.beg) <= 0
                            && CMP(a2.end - 1, b2.beg) <= 0) {
                        continue;
                    } else {
                        RANGE_COPY(z, a1);
                        RANGE_COPY(z + RANGE_LEN(a1), b1);
                    }
                    a1.end = b1.end;

                    zz = z + RANGE_LEN(a1);
                    if (CMP(a2.beg, b2.end - 1) > 0) {
                        RANGE_COPY(zz, b2);
                        RANGE_COPY(zz + RANGE_LEN(b2), a2);
                    } else if (CMP(b2.beg, a2.end - 1) < 0) {
                        unc0_wiki_merge_into(s, RANGE_PASS(a2),
                                                RANGE_PASS(b2), zz);
                    } else {
                        RANGE_COPY(zz, a2);
                        RANGE_COPY(zz + RANGE_LEN(a2), b2);
                    }
                    a2.end = b2.end;

                    a3.beg = z;
                    a3.end = a3.beg + RANGE_LEN(a1);
                    b3.beg = a3.end;
                    b3.end = b3.beg + RANGE_LEN(a2);

                    if (CMP(a3.beg, b3.end - 1) > 0) {
                        RANGE_COPY(a1.beg, b3);
                        RANGE_COPY(a1.beg + RANGE_LEN(a2), a3);
                    } else if (CMP(b3.beg, a3.end - 1) < 0) {
                        unc0_wiki_merge_into(s, RANGE_PASS(a3),
                                                RANGE_PASS(b3), a1.beg);
                    } else {
                        RANGE_COPY(a1.beg, a3);
                        RANGE_COPY(a1.beg + RANGE_LEN(a1), b3);
                    }
                    CHECKERR(s);
                }
#undef a3
#undef b3
                unc0_wiki_iter_up(&it);
            } else {
                ITER_RESET(it);
                while (!ITER_DONE(it)) {
                    unc0_wiki_iter_next(&it, &a);
                    unc0_wiki_iter_next(&it, &b);

                    if (CMP(a.beg, b.end - 1) > 0) {
                        ROTATE_N(a.beg, b.end, RANGE_LEN(a));
                    } else {
                        COPY_N(z, a.beg, RANGE_LEN(a));
                        unc0_wiki_merge_external(s, RANGE_PASS(a),
                                                RANGE_PASS(b), z);
                    }

                    CHECKERR(s);
                }
            }
            continue;
        }

        bs = sqrtz(itl);
        fs = itl / bs + 1;
        pull = 0;
        separate = 0;
        
        RANGE_NEW(pulls[0], l, l);
        RANGE_NEW(pulls[0].r, l, l);
        RANGE_NEW(pulls[1], l, l);
        RANGE_NEW(pulls[1].r, l, l);
        RANGE_NEW(f1, l, l);
        RANGE_NEW(f2, l, l);
        pulls[0].cnt = pulls[1].cnt = 0;

        ff = fs << 1;
        if (ff <= zn) {
            ff = fs;
        } else if (ff > itl) {
            ff = fs;
            separate = 1;
        }

        ITER_RESET(it);
        while (!ITER_DONE(it)) {
            unc0_wiki_iter_next(&it, &a);
            unc0_wiki_iter_next(&it, &b);

            #define DO_PULL(_qe) (                                             \
                RANGE_NEW(PULL, p, _qe),                                       \
                RANGE_NEW(PULL.r, a.beg, b.end),                               \
                PULL.cnt = cnt)
            
            for (p1 = a.beg, cnt = 1; cnt < ff; p1 = p, ++cnt) {
                p = unc0_wiki_findlf(s, p1 + 1, a.end, p1, ff - cnt);
                if (p == a.end) break;
            }
            p = p1;

            if (cnt >= fs) {
                DO_PULL(a.beg);
                pull = 1;
                if (cnt == fs + fs) {
                    RANGE_NEW(f1, a.beg, a.beg + fs);
                    RANGE_NEW(f2, a.beg + fs, a.beg + cnt);
                    break;
                } else if (ff == fs + fs) {
                    RANGE_NEW(f1, a.beg, a.beg + cnt);
                    ff = fs;
                } else if (bs <= zn) {
                    RANGE_NEW(f1, a.beg, a.beg + cnt);
                    break;
                } else if (separate) {
                    RANGE_NEW(f1, a.beg, a.beg + cnt);
                    separate = 0;
                } else {
                    RANGE_NEW(f2, a.beg, a.beg + cnt);
                    break;
                }
            } else if (!pull && cnt > RANGE_LEN(f1)) {
                RANGE_NEW(f1, a.beg, a.beg + cnt);
                DO_PULL(a.beg);
            }

            for (p1 = b.end - 1, cnt = 1; cnt < ff; p1 = p - 1, ++cnt) {
                p = unc0_wiki_findfb(s, b.beg, p1, p1, ff - cnt);
                if (p == b.beg) break;
            }
            p = p1;

            if (cnt >= fs) {
                DO_PULL(b.end);
                pull = 1;
                if (cnt == fs + fs) {
                    RANGE_NEW(f1, b.end - cnt, b.end - fs);
                    RANGE_NEW(f2, b.end - fs, b.end);
                    break;
                } else if (ff == fs + fs) {
                    RANGE_NEW(f1, b.end - cnt, b.end);
                    ff = fs;
                } else if (bs <= zn) {
                    RANGE_NEW(f1, b.end - cnt, b.end);
                    break;
                } else if (separate) {
                    RANGE_NEW(f1, b.end - cnt, b.end);
                    separate = 0;
                } else {
                    if (pulls[0].r.beg == a.beg) {
                        pulls[0].r.end -= pulls[1].cnt;
                    }
                    RANGE_NEW(f2, b.end - cnt, b.end);
                    break;
                }
            } else if (!pull && cnt > RANGE_LEN(f1)) {
                RANGE_NEW(f1, b.end - cnt, b.end);
                DO_PULL(b.end);
            }
            CHECKERR(s);
        }

        for (pull = 0; pull < 2; ++pull) {
            struct wiki_range tmp;
            Unc_Size ll = PULL.cnt;

            if (PULL.end < PULL.beg) {
                p = PULL.beg;
                for (cnt = 1; cnt < ll; ++cnt) {
                    p = unc0_wiki_findfb(s, PULL.end,
                            PULL.beg - cnt + 1, p - 1, ll - cnt);
                    RANGE_NEW(tmp, p + 1, PULL.beg + 1);
                    RANGE_ROTATE(tmp, RANGE_LEN(tmp) - cnt);
                    PULL.beg = p + cnt;
                }
            } else if (PULL.end > PULL.beg) {
                p = PULL.beg + 1;
                for (cnt = 1; cnt < ll; ++cnt) {
                    p = unc0_wiki_findlf(s, p, PULL.end, p, ll - cnt);
                    RANGE_NEW(tmp, PULL.beg, p - 1);
                    RANGE_ROTATE(tmp, cnt);
                    PULL.beg = p - cnt - 1;
                }
            }
        }
        CHECKERR(s);

        fs = RANGE_LEN(f1);
        bs = itl / fs + 1;

        ITER_RESET(it);
        while (!ITER_DONE(it)) {
            Unc_Value *p0;

            unc0_wiki_iter_next(&it, &a);
            unc0_wiki_iter_next(&it, &b);

            p0 = a.beg;
            if (p0 == pulls[0].r.beg) {
                if (pulls[0].beg > pulls[0].end) {
                    a.beg += pulls[0].cnt;
                    if (!RANGE_LEN(a)) continue;
                } else if (pulls[0].beg < pulls[0].end) {
                    b.end -= pulls[0].cnt;
                    if (!RANGE_LEN(b)) continue;
                }
            }
            if (p0 == pulls[1].r.beg) {
                if (pulls[1].beg > pulls[1].end) {
                    a.beg += pulls[1].cnt;
                    if (!RANGE_LEN(a)) continue;
                } else if (pulls[1].beg < pulls[1].end) {
                    b.end -= pulls[1].cnt;
                    if (!RANGE_LEN(b)) continue;
                }
            }

            if (CMP(b.end - 1, a.beg) < 0) {
                ROTATE_N(a.beg, b.end, RANGE_LEN(a));
            } else if (CMP(a.end - 1, a.end) > 0) {
                struct wiki_range aa, bb, a0, a1, b1;
                Unc_Value *pa;

                aa = a;
                RANGE_NEW(a0, a.beg, a.beg + RANGE_LEN(aa) % bs);

                for (pa = f1.beg, p = a0.end; p < aa.end; ++pa, p += bs)
                    SWAP(pa, p);
                
                a1 = a0;
                RANGE_NEW(b1, l, l);
                RANGE_NEW(bb, b.beg, b.beg + MIN(bs, RANGE_LEN(b)));
                aa.beg += RANGE_LEN(a0);
                pa = f1.beg;

                if (RANGE_LEN(a1) <= zn)
                    COPY_N(z, a1.beg, RANGE_LEN(a1));
                else if (RANGE_LEN(f2))
                    SWAP_N(a1.beg, f2.beg, RANGE_LEN(a1));

                if (RANGE_LEN(aa)) {
                    for (;;) {
                        if ((RANGE_LEN(b1) && unc0_arrcmp(s,
                                b1.end - 1, pa) >= 0) || !RANGE_LEN(bb)) {
                            Unc_Value *bp = unc0_wiki_findbf(s,
                                                RANGE_PASS(b1), pa);
                            Unc_Size br = b1.end - bp;
                            Unc_Value *af, *am = aa.beg;

                            for (af = am + bs; af < aa.end; af += bs)
                                if (CMP(af, am) < 0)
                                    am = af;
                            SWAP_N(aa.beg, am, bs);
                            SWAP(aa.beg, pa);
                            ++pa;

                            if (RANGE_LEN(a1) <= zn)
                                unc0_wiki_merge_external(s, RANGE_PASS(a1),
                                                        a1.end, bp, z);
                            else if (RANGE_LEN(f2))
                                unc0_wiki_merge_internal(s, RANGE_PASS(a1),
                                                        a1.end, bp, f2.beg);
                            else
                                unc0_wiki_merge_inplace(s, RANGE_PASS(a1),
                                                        a1.end, bp, z, zn);
                            
                            if (bs <= zn) {
                                COPY_N(z, aa.beg, bs);
                                SWAP_N(bp, aa.beg + bs - br, br);
                            } else if (RANGE_LEN(f2)) {
                                SWAP_N(aa.beg, f2.beg, bs);
                                SWAP_N(bp, aa.beg + bs - br, br);
                            } else {
                                ROTATE_N(bp, aa.beg + bs, aa.beg - bp);
                            }

                            RANGE_NEW(a1, aa.beg - br, aa.beg - br + bs);
                            RANGE_NEW(b1, a1.end, a1.end + br);

                            aa.beg += bs;
                            if (!RANGE_LEN(aa)) break;
                        } else if (RANGE_LEN(bb) < bs) {
                            Unc_Size bbl = RANGE_LEN(bb);
                            ROTATE_N_NOBUF(aa.beg, bb.end, bb.beg - aa.beg);
                            RANGE_NEW(b1, aa.beg, aa.beg + bbl);
                            aa.beg += bbl;
                            aa.end += bbl;
                            bb.end = bb.beg;
                        } else {
                            SWAP_N(aa.beg, bb.beg, bs);
                            RANGE_NEW(b1, aa.beg, aa.beg + bs);
                            aa.beg += bs;
                            aa.end += bs;
                            bb.beg += bs;
                            if ((bb.end += bs) > b.end)
                                bb.end = b.end;
                        }
                    }
                }

                if (RANGE_LEN(a1) <= zn)
                    unc0_wiki_merge_external(s, RANGE_PASS(a1),
                                            a1.end, b.end, z);
                else if (RANGE_LEN(f2))
                    unc0_wiki_merge_internal(s, RANGE_PASS(a1),
                                             a1.end, b.end, f2.beg);
                else
                    unc0_wiki_merge_inplace(s, RANGE_PASS(a1),
                                            a1.end, b.end, z, zn);
            }
            CHECKERR(s);
        }

        unc0_sortins(s, RANGE_PASS(f2));

        for (pull = 0; pull < 2; ++pull) {
            Unc_Size am, u = PULL.cnt << 1;
            if (PULL.end < PULL.beg) {
                struct wiki_range tmp;
                RANGE_NEW(tmp, PULL.r.beg, PULL.r.beg + PULL.cnt);
                while (RANGE_LEN(tmp)) {
                    p = unc0_wiki_findff(s, tmp.end, PULL.r.end, tmp.beg, u);
                    am = p - tmp.end;
                    ROTATE_N(tmp.beg, p, RANGE_LEN(tmp));
                    tmp.beg += am + 1;
                    tmp.end += am;
                    u -= 2;
                }
            } else if (PULL.end > PULL.beg) {
                struct wiki_range tmp;
                RANGE_NEW(tmp, PULL.r.end - PULL.cnt, PULL.r.end);
                while (RANGE_LEN(tmp)) {
                    p = unc0_wiki_findlb(s,
                        PULL.r.beg, tmp.beg, tmp.end - 1, u);
                    am = tmp.beg - p;
                    ROTATE_N(p, tmp.end, am);
                    tmp.beg -= am;
                    tmp.end -= am + 1;
                    u -= 2;
                }
            }
        }
        CHECKERR(s);
    } while (unc0_wiki_iter_up(&it));
#undef PULL
#undef IBUF2
}

INLINE Unc_RetVal unc0_arrsort_i(struct unc0_sortenv *s,
                          Unc_Value *l, Unc_Value *r) {
    Unc_RetVal e;
    if (!(e = setjmp(s->env)))
        unc0_wiki_sort(s, l, r);
    return e;
}

Unc_RetVal unc0_arrsort(Unc_View *w, Unc_Value *fn,
                        Unc_Size n, Unc_Value *arr) {
    Unc_RetVal e;
    struct unc0_sortenv s;
    if (n < 2) return 0;

    s.buf = NULL;
    s.bufn = 0;
    s.fn = fn;
    s.w = w;
    s.e = 0;
    
    e = unc0_arrsort_i(&s, arr, arr + n);

    if (s.buf) TMFREE(Unc_Value, &w->world->alloc, s.buf, s.bufn);
    return e;
}
