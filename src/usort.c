/*******************************************************************************
 
Uncil -- sorting impl

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

#include <math.h>
#include <setjmp.h>
#include <string.h>

#define UNCIL_DEFINES

#include "uarithm.h"
#include "ucommon.h"
#include "usort.h"
#include "uval.h"
#include "uvali.h"
#include "uvop.h"
#include "uvm.h"

struct unc0_arrlt_buf {
    Unc_Value *buf;
    Unc_Value *fn;
    Unc_View *w;
    jmp_buf env;
};

static int unc0_arrcmp(struct unc0_arrlt_buf *s, Unc_Value *a, Unc_Value *b) {
    int e;
    if (!s->fn) {
        e = unc0_vvcmp(s->w, a, b);
        if (UNCIL_IS_ERR_CMP(e))
            longjmp(s->env, e);
        return e;
    } else {
        Unc_View *w = s->w;
        Unc_Size d = unc0_stackdepth(&w->sval);
        Unc_Pile ret;
        Unc_Tuple tuple;
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
            longjmp(s->env, e);
        unc_returnvalues(w, &ret, &tuple);
        if (tuple.count < 1)
            longjmp(s->env, unc_throwexc(w, "value",
                "comparator did not return a value"));
        switch (tuple.values[0].type) {
        case Unc_TInt:
            e = unc0_cmpint(tuple.values[0].v.i, 0);
            break;
        case Unc_TFloat:
            if (tuple.values[0].v.f != tuple.values[0].v.f)
                longjmp(s->env, unc_throwexc(w, "value",
                    "comparator returned NaN"));
            e = unc0_cmpflt(tuple.values[0].v.f, 0);
            break;
        default:
            longjmp(s->env, unc_throwexc(w, "value",
                "comparator did not return a valid numeric value"));
        }
        unc_discard(w, &ret);
        return e;
    }
}

/* threshold for insertion sort */
#define IBUF 32
/* threshold for scanning for monotonic subsequences */
#define SCANL 256

INLINE void unc0_arrsorti(struct unc0_arrlt_buf *s,
                          Unc_Value *l, Unc_Value *r) {
    /* insertion sort */
    Unc_Value *i = l + 1, *j;
    while (i != r) {
        Unc_Value p = *i;
        j = i - 1;
        while (j >= l) {
            int c = unc0_arrcmp(s, j, &p);
            if (c <= 0) break;
            j[1] = j[0];
            --j;
        }
        j[1] = p;
        ++i;
    }
}

INLINE void unc0_arrrev(Unc_Value *l, Unc_Value *r) {
    --r;
    while (r > l) {
        Unc_Value t = *l;
        *l++ = *r;
        *r-- = t;
    }
}

static Unc_Value *unc0_arrsortmf(struct unc0_arrlt_buf *s,
                           Unc_Value *l, Unc_Value *m, Unc_Value *r) {
    /* merge forward */
    Unc_Value *a = s->buf, *b = m, *d = l, *o = a + (m - l);
    int c;
    if (unc0_arrcmp(s, &m[-1], &m[0]) <= 0) return r;
    if (unc0_arrcmp(s, &r[-1], &l[0]) < 0) {
        unc0_arrrev(l, m);
        unc0_arrrev(m, r);
        unc0_arrrev(l, r);
        return r;
    }
    memcpy(a, l, (m - l) * sizeof(Unc_Value));
    ASSERT((m - l) <= (r - m));
    while (a < o && b < r) {
        c = unc0_arrcmp(s, a, b);
        if (c <= 0) *d++ = *a++;
        else        *d++ = *b++;
    }
    if (a < o)
        memcpy(d, a, (o - a) * sizeof(Unc_Value));
    return r;
}

static Unc_Value *unc0_arrsortmb(struct unc0_arrlt_buf *s,
                           Unc_Value *l, Unc_Value *m, Unc_Value *r) {
    /* merge backward */
    Unc_Value *o = s->buf, *a = m - 1, *d = r, *b = o + (r - m) - 1;
    int c;
    if (unc0_arrcmp(s, &m[-1], &m[0]) <= 0) return r;
    if (unc0_arrcmp(s, &r[-1], &l[0]) < 0) {
        unc0_arrrev(l, m);
        unc0_arrrev(m, r);
        unc0_arrrev(l, r);
        return r;
    }
    memcpy(o, m, (r - m) * sizeof(Unc_Value));
    ASSERT((m - l) >= (r - m));
    while (a >= l && b >= o) {
        c = unc0_arrcmp(s, a, b);
        if (c > 0)  *--d = *a--;
        else        *--d = *b--;
    }
    if (b >= o) {
        Unc_Size q = b - o + 1;
        memcpy(d - q, o, q * sizeof(Unc_Value));
    }
    return r;
}

INLINE Unc_Value *unc0_arrsortmm(struct unc0_arrlt_buf *s,
                           Unc_Value *l, Unc_Value *m, Unc_Value *r) {
    /* merge forward or backward */
    if (l == m || m == r)
        return r;
    else if (m - l <= r - m)
        return unc0_arrsortmf(s, l, m, r);
    else
        return unc0_arrsortmb(s, l, m, r);
}

static Unc_Value *unc0_arrsortmr(struct unc0_arrlt_buf *s,
                                 Unc_Value *l, Unc_Value *r,
                                 int depth) {
    if (depth) {
        /* merge run */
        Unc_Value *p = unc0_arrsortmr(s, l, r, depth - 1), *p2;
        if (p == r) return p;
        p2 = unc0_arrsortmr(s, p, r, depth - 1);
        return unc0_arrsortmm(s, l, p, p2);
    } else if (r - l > 2) {
        /* find next longest run and possibly reverse it */
        int d = 0;
        Unc_Value *p = l + 1;
        /* reversed run? */
        d = unc0_arrcmp(s, &p[-1], &p[0]) > 0;
        /* find end of run */
        while (++p < r)
            if (d != (unc0_arrcmp(s, &p[-1], &p[0]) > 0))
                break;
        if (d)
            unc0_arrrev(l, p);
        return p;
    }
    return r;
}

void unc0_arrsortm(struct unc0_arrlt_buf *s, Unc_Value *l, Unc_Value *r) {
    Unc_Size z = r - l, i;
    if (z < 2) return;

    if (z > SCANL) {
        /* scan the number of runs in linear time */
        int c, d = 0, rl = 0;
        Unc_Size maxRunsOrig = (Unc_Size)sqrt(z) + 1, maxRuns = maxRunsOrig;
        for (i = 1; i < z && maxRuns; ++i) {
            c = unc0_arrcmp(s, &l[i], &l[i - 1]);
            if (!d && c < 0)
                d = 1, maxRuns -= rl, rl = 0;
            else if (d && c >= 0)
                d = 0, maxRuns -= rl, rl = 0;
            else
                rl = 1;
        }
        /* if <= sqrt(N) runs, try to merge them directly instead */
        if (maxRuns) {
            int rl2;
            maxRuns = maxRunsOrig - maxRuns;
            if (!maxRuns) {
                if (d) unc0_arrrev(l, r);
                return;
            }
            maxRuns = (maxRuns << 1) - 1;
            rl2 = 0;
            do {
                ++rl2;
            } while (maxRuns >>= 1);
            (void)unc0_arrsortmr(s, l, r, rl2);
            return;
        }
    }

    if (IBUF > 1) {
        /* insertion sort the smaller lists */
        Unc_Value *a = l, *b;
        /* the first block should be the shortest because we
           merge forwards at the end */
        b = a + z % IBUF;
        if (a == b) b += IBUF;
        while (a < r) {
            unc0_arrsorti(s, a, b);
            a = b;
            b = a + IBUF;
            if (b > r) b = r;
        }
    }

    { /* merge from bottom up */
        Unc_Size b, m, e;
        for (b = IBUF; b < z; b <<= 1) {
            e = z;
            while (e > b) {
                m = e - b;
                i = m < b ? 0 : m - b;
                (void)unc0_arrsortmf(s, l + i, l + m, l + e);
                e = i;
            }
        }
    }
}

int unc0_arrsort(Unc_View *w, Unc_Value *fn, Unc_Size n, Unc_Value *arr) {
    int e;
    struct unc0_arrlt_buf s;
    s.fn = fn;
    s.w = w;
    s.buf = TMALLOC(Unc_Value, &w->world->alloc, Unc_AllocExternal, n / 2);
    if (!s.buf)
        return UNCIL_ERR_MEM;
    if (!(e = setjmp(s.env)))
        unc0_arrsortm(&s, arr, arr + n);
    TMFREE(Unc_Value, &w->world->alloc, s.buf, n / 2);
    return e;
}
