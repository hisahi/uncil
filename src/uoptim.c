/*******************************************************************************
 
Uncil -- Q-code optimizer impl

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

#define UNCIL_DEFINES

#include "udebug.h"
#include "uoptim.h"
#include "uparse.h"
#include "uvlq.h"

#define MUST(val) do { int e; if ((e = (val))) return e; } while (0)

#define MARK(cd) ((cd)->o0type |= 0x80, (cd)->o1type &= 0x7F)
#define UNMARK(cd) ((cd)->o0type &= 0x7F, (cd)->o1type &= 0x7F)
#define ISMARKED(cd) ((cd)->o0type & 0x80)

#define PMARK(cd) ((cd)->o1type |= 0x80)
#define UNPMARK(cd) ((cd)->o1type &= 0x7F)
#define ISPMARKED(cd) ((cd)->o1type & 0x80)

INLINE Unc_Dst reducetmp_mapreg(Unc_Size *trd, Unc_Dst reg,
        Unc_Dst *count, Unc_QInstr *cd, Unc_Size cdsz, Unc_Size offset,
        int remap, Unc_Dst dst) {
    /* get next free temp register */
    Unc_Size lr = offset, fr = offset;
    if (remap) {
        Unc_Dst oc = *count;
        dst = 0;
        while (dst < oc) {
            if (offset > trd[dst])
                break;
            ++dst;
        }
        if (dst >= oc)
            *count = dst + 1;
    } else
        --dst;

    ++offset;
    cd += offset;
    while (offset < cdsz) {
        int opc = unc__qcode_operandcount(cd->op);
        if (opc < 0) {
            opc = -opc;
        } else if (unc__qcode_isread0op(cd->op)
                && cd->o0type == UNC_QOPER_TYPE_TMP && cd->o0data == reg)
            lr = offset;
        /* assume ops 1-2 are only ever read, which is currently true
            (even with SATTR/SINDX, we are mutating an existing object,
            not changing the value) */
        else if (opc > 1 && cd->o1type == UNC_QOPER_TYPE_TMP
                         && cd->o1data.o == reg)
            lr = offset;
        else if (opc > 2 && cd->o2type == UNC_QOPER_TYPE_TMP
                         && cd->o2data.o == reg)
            lr = offset;
        if ((opc > 1 && cd->o1type == UNC_QOPER_TYPE_JUMP)
                || (opc > 2 && cd->o2type == UNC_QOPER_TYPE_JUMP)) {
            /* deal with jumps */
            Unc_Size jd = (opc > 2 && cd->o2type == UNC_QOPER_TYPE_JUMP)
                                ? cd->o2data.o : cd->o1data.o;
            /* if we might jump backwards, update lr */
            if (jd < offset && jd >= fr)
                lr = offset;
        }
        if (unc__qcode_iswrite0op(cd->op)
                && cd->o0type == UNC_QOPER_TYPE_TMP && cd->o0data == reg) {
            break;
        }
        ++cd, ++offset;
    }
    trd[dst] = lr;

    return dst + 1;
}

static int reducetmp(Unc_Context *cxt, Unc_QFunc *fn) {
    /* minimize temp registers by merging ones where the lifetimes do not 
        overlap. ignore TMP(0) though, that one is special */
    Unc_Dst *trs;
    Unc_Size *trd;
    Unc_Dst newcnt = 0;
    Unc_Size i, e = fn->cd_sz;
    Unc_QInstr *cd = fn->cd, *cr = cd;

    if (fn->cnt_tmp < 2)
        return 0;
    
    trs = unc__mallocz(cxt->alloc, 0,
                        (fn->cnt_tmp - 1) * sizeof(Unc_Dst));
    if (!trs) goto reducetmp_fail0;
    
    trd = unc__mallocz(cxt->alloc, 0,
                        (fn->cnt_tmp - 1) * sizeof(Unc_Size));
    if (!trd) goto reducetmp_fail1;

    for (i = 0; i < e; ++i) {
        int opc = unc__qcode_operandcount(cr->op);
        if (opc < 0) {
            opc = -opc;
        } else if (unc__qcode_isread0op(cr->op)
                && cr->o0type == UNC_QOPER_TYPE_TMP && cr->o0data > 0) {
            /* make sure we are not reading a tmp register before
                we write to it */
            Unc_Dst d = trs[cr->o0data - 1];
            ASSERT(d != 0);
            cr->o0data = d;
        } else if (unc__qcode_iswrite0op(cr->op)
                && cr->o0type == UNC_QOPER_TYPE_TMP && cr->o0data > 0) {
            int remap = 1;
            /* don't attempt to remap if we are reading the same value */
            if (opc > 1 && cr->o1type == UNC_QOPER_TYPE_TMP
                        && cr->o0data == cr->o1data.o)
                remap = 0;
            if (opc > 2 && cr->o2type == UNC_QOPER_TYPE_TMP
                        && cr->o0data == cr->o2data.o)
                remap = 0;
            trs[cr->o0data - 1] = reducetmp_mapreg(trd, cr->o0data,
                &newcnt, cd, e, i, remap, trs[cr->o0data - 1]);
            cr->o0data = trs[cr->o0data - 1];
        }
        /* assume ops 1-2 are only ever read, which is currently true
            (even with SATTR/SINDX, we are mutating an existing object,
            not changing the value) */
        if (opc > 1 && cr->o1type == UNC_QOPER_TYPE_TMP && cr->o1data.o > 0) {
            /* make sure we are not reading a tmp register before
                we write to it */
            Unc_Dst d = trs[cr->o1data.o - 1];
            ASSERT(d != 0);
            cr->o1data.o = d;
        }
        if (opc > 2 && cr->o2type == UNC_QOPER_TYPE_TMP && cr->o2data.o > 0) {
            /* make sure we are not reading a tmp register before
                we write to it */
            Unc_Dst d = trs[cr->o2data.o - 1];
            ASSERT(d != 0);
            cr->o2data.o = d;
        }
        ++cr;
    }
    unc__mfree(cxt->alloc, trd, (fn->cnt_tmp - 1)
                                * sizeof(Unc_Dst));
reducetmp_fail1:
    unc__mfree(cxt->alloc, trs, (fn->cnt_tmp - 1)
                                * sizeof(Unc_Size));
reducetmp_fail0:
    fn->cnt_tmp = newcnt + 1;
    return 0;
}

static int tailcalls(Unc_Context *cxt, Unc_QFunc *fn) {
    Unc_Size n = fn->cd_sz, i = n;
    Unc_QInstr *cd = fn->cd + n;
    int exited = 0;
    do {
        --i;
        --cd;
        if (exited && cd->op == UNC_QINSTR_OP_FCALL
                   && cd->o0type == UNC_QOPER_TYPE_STACK) {
            cd->op = UNC_QINSTR_OP_FTAIL;
            cd[1].op = UNC_QINSTR_OP_DELETE;
            exited = 0;
        } else if (exited && cd->op == UNC_QINSTR_OP_DCALL
                          && cd->o0type == UNC_QOPER_TYPE_STACK) {
            cd->op = UNC_QINSTR_OP_DTAIL;
            cd[1].op = UNC_QINSTR_OP_DELETE;
            exited = 0;
        } else if (cd->op == UNC_QINSTR_OP_END) {
            exited = 1;
        } else {
            exited = 0;
        }
    } while (i);
    return 0;
}

static void optimcleanup(Unc_Context *cxt, Unc_QFunc *fn) {
    Unc_Size i, n = fn->cd_sz;
    for (i = 0; i < n; ++i)
        UNMARK(&fn->cd[i]);
}

static Unc_Size mergejmps_i(Unc_QInstr *qc, Unc_Size qi, int rec) {
    Unc_QInstr *q = &qc[qi];
    if (rec < 256 && (rec ? q->op == UNC_QINSTR_OP_JMP
                          : unc__qcode_isjump(q->op))) {
        int d = unc__qcode_getjumpd(q->op);
        Unc_Size target;
        switch (d) {
        case 1:
            target = q->o1data.o;
            break;
        case 2:
            target = q->o2data.o;
            break;
        }
        target = mergejmps_i(qc, target, rec + 1);
        switch (d) {
        case 1:
            q->o1data.o = target;
            break;
        case 2:
            q->o2data.o = target;
            break;
        }
        return target;
    }
    return qi;
}

static int mergejmps(Unc_Context *cxt, Unc_QFunc *fn) {
    Unc_Size i, n = fn->cd_sz;
    for (i = 0; i < n; ++i) {
        if (unc__qcode_isjump(fn->cd[i].op)) {
            mergejmps_i(fn->cd, i, 0);
        }
    }
    return 0;
}

static int nodeadcode_i(Unc_Context *cxt, Unc_QFunc *fn) {
    Unc_Size i, n = fn->cd_sz;
    int ret = 0;
    for (i = 0; i < n; ++i)
        UNMARK(&fn->cd[i]);
    for (;;) {
        for (i = 0; i < n; ++i) {
            Unc_QInstr *q;
nodeadcode_again:
            q = &fn->cd[i];
            MARK(q);
            if (unc__qcode_isjump(q->op)) {
                int cond = q->op != UNC_QINSTR_OP_JMP;
                Unc_Size target;
                switch (unc__qcode_getjumpd(q->op)) {
                case 1:
                    target = q->o1data.o;
                    break;
                case 2:
                    target = q->o2data.o;
                    break;
                }
                if (cond && !ISMARKED(&fn->cd[target]))
                    PMARK(&fn->cd[target]);
                else if (!cond && target > i) {
                    i = target;
                    goto nodeadcode_again;
                }
            } else if (unc__qcode_isexit(q->op)) {
                break;
            }
        }
        for (i = 0; i < n; ++i)
            if (ISPMARKED(&fn->cd[i]))
                goto nodeadcode_again;
        break;
    }
    for (i = 0; i < n; ++i)
        if (ISMARKED(&fn->cd[i]))
            UNMARK(&fn->cd[i]);
        else
            ret |= fn->cd[i].op != UNC_QINSTR_OP_DELETE,
                   fn->cd[i].op = UNC_QINSTR_OP_DELETE;
    return ret;
}

static int nodeadcode(Unc_Context *cxt, Unc_QFunc *fn) {
    int cycles = 8;
    while (--cycles && nodeadcode_i(cxt, fn))
        ;
    return 0;
}

static Unc_Size roundup_pow2(Unc_Size v) {
    Unc_Size j = 1;
    --v;
    /* while x+1 popcount would be > 1... */
    while (v & (v + 1))
        v |= v >> j, j <<= 1;
    return v + 1;
}

static Unc_Size ilog2(Unc_Size v) {
    Unc_Size j = 0;
    while (v >>= 1)
        ++j;
    return v;
}

int unc__optqcode(Unc_Context *cxt, Unc_QCode *out) {
    Unc_Size i, fs = out->fn_sz;
    /* intra-function optimizations */
    for (i = 0; i < fs; ++i) {
        Unc_QFunc *fn = &out->fn[i];
        MUST(reducetmp(cxt, fn));       /* tmp register minimization */
        MUST(tailcalls(cxt, fn));       /* tail calls */
        MUST(mergejmps(cxt, fn));       /* merge JMP chains */
        /* TODO: dealing with constants? */
        MUST(nodeadcode(cxt, fn));      /* dead code elimination */
    }
    /* inter-function optimizations? */

    /* post-optimization cleanup */
    for (i = 0; i < fs; ++i) {
        Unc_QFunc *fn = &out->fn[i];
        optimcleanup(cxt, fn);
    }
    return 0;
}
