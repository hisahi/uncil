/*******************************************************************************
 
Uncil -- debug code

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

#include "udebug.h"
#include "ucomp.h"
#include "uncil.h"
#include "uparse.h"
#include "uvlq.h"

void unc0_dbghexdump(const byte *data, size_t n) {
    size_t i = 0;
    if (n) {
        int j;
        printf("          |");
        for (j = 0; j < 16; ++j)
            printf("  %02x", (int)j);
        printf("\n----------+");
        for (j = 0; j < 16; ++j)
            printf("----");
        puts("--");
    }
    while (i < n) {
        if (!(i & 15))
            printf("%08x  |", (unsigned)i);
        printf("  %02x", *data++);
        ++i;
        if (!(i & 15))
            putchar('\n');
    }
    putchar('\n');
}

static void pdump_reg(Unc_View *w, const byte **d) {
    printf("R%-5lu\t", (unsigned long)unc0_clqdecz(UNCIL_REGW, d));
}

static void pdump_us(Unc_View *w, const byte **d) {
    printf("%-6lu\t", (unsigned long)unc0_vlqdecz(d));
}

static void pdump_lint(Unc_View *w, const byte **d) {
    Unc_Int ui = unc0_vlqdeci(d);
    printf("I(%+6ld)\t", (long)ui);
}

static void pdump_lflt(Unc_View *w, const byte **d) {
    Unc_Float uf;
    unc0_memcpy(&uf, *d, sizeof(Unc_Float));
    *d += sizeof(Unc_Float);
    printf("F(%+6f)\t", (float)uf);
}

static void pdump_loff(Unc_View *w, const char *s, const byte **d) {
    Unc_Size z = unc0_vlqdecz(d);
    printf(s, (unsigned long)z);
    putchar('\t');
}

static void pdump_lstr(Unc_View *w, const byte **d) {
    pdump_loff(w, "STR(%06lx)", d);
}

static void pdump_jmp(Unc_View *w, const byte **d) {
    Unc_Size z = unc0_clqdecz(w->jumpw, d);
    printf("$%06lx\t", (unsigned long)(z + (w->jbase - w->bcode)));
    putchar('\t');
}

static void pdump_lit(Unc_View *w, const byte **d) {
    Unc_Int ui = *(*d)++;
    ui |= *(*d)++ << 8;
    if (ui > 0x7FFF) ui -= 0x10000;
    printf("I(%+6ld)\t", (long)ui);
}

static int pinstrdump(Unc_View *w, const byte **d, byte b) {
    switch (b) {
    case UNC_I_NOP:
        printf("%s\t", "NOP");
        break;
    case UNC_I_LDNUM:
        printf("%s\t", "LDNUM");
        pdump_reg(w, d);
        pdump_lit(w, d);
        break;
    case UNC_I_LDINT:
        printf("%s\t", "LDINT");
        pdump_reg(w, d);
        pdump_lint(w, d);
        break;
    case UNC_I_LDFLT:
        printf("%s\t", "LDFLT");
        pdump_reg(w, d);
        pdump_lflt(w, d);
        break;
    case UNC_I_LDBLF:
        printf("%s\t", "LDBLF");
        pdump_reg(w, d);
        break;
    case UNC_I_LDBLT:
        printf("%s\t", "LDBLT");
        pdump_reg(w, d);
        break;
    case UNC_I_LDSTR:
        printf("%s\t", "LDSTR");
        pdump_reg(w, d);
        pdump_lstr(w, d);
        break;
    case UNC_I_LDNUL:
        printf("%s\t", "LDNUL");
        pdump_reg(w, d);
        break;
    case UNC_I_LDSTK:
        printf("%s\t", "LDSTK");
        pdump_reg(w, d);
        pdump_loff(w, "%06lu", d);
        break;
    case UNC_I_LDPUB:
        printf("%s\t", "LDPUB");
        pdump_reg(w, d);
        pdump_loff(w, "PUB(%06lx)", d);
        break;
    case UNC_I_LDBIND:
        printf("%s\t", "LDBIND");
        pdump_reg(w, d);
        pdump_reg(w, d);
        break;
    case UNC_I_LDSTKN:
        printf("%s\t", "LDSTKN");
        pdump_reg(w, d);
        pdump_loff(w, "%06lu", d);
        break;
    case UNC_I_LDATTR:
        printf("%s\t", "LDATTR");
        pdump_reg(w, d);
        pdump_reg(w, d);
        pdump_loff(w, "ID(%06lx)", d);
        break;
    case UNC_I_LDATTRQ:
        printf("%s\t", "LDATTRQ");
        pdump_reg(w, d);
        pdump_reg(w, d);
        pdump_loff(w, "ID(%06lx)", d);
        break;
    case UNC_I_LDINDX:
        printf("%s\t", "LDINDX");
        pdump_reg(w, d);
        pdump_reg(w, d);
        pdump_reg(w, d);
        break;
    case UNC_I_LDINDXQ:
        printf("%s\t", "LDINDXQ");
        pdump_reg(w, d);
        pdump_reg(w, d);
        pdump_reg(w, d);
        break;
    case UNC_I_MOV:
        printf("%s\t", "MOV");
        pdump_reg(w, d);
        pdump_reg(w, d);
        break;
    case UNC_I_STSTK:
        printf("%s\t", "STSTK");
        pdump_reg(w, d);
        break;
    case UNC_I_STPUB:
        printf("%s\t", "STPUB");
        pdump_reg(w, d);
        pdump_loff(w, "PUB(%06lx)", d);
        break;
    case UNC_I_STBIND:
        printf("%s\t", "STBIND");
        pdump_reg(w, d);
        pdump_reg(w, d);
        break;
    case UNC_I_STATTR:
        printf("%s\t", "STATTR");
        pdump_reg(w, d);
        pdump_reg(w, d);
        pdump_loff(w, "ID(%06lx)", d);
        break;
    case UNC_I_STINDX:
        printf("%s\t", "STINDX");
        pdump_reg(w, d);
        pdump_reg(w, d);
        pdump_reg(w, d);
        break;
    case UNC_I_STWITH:
        printf("%s\t", "STWITH");
        pdump_reg(w, d);
        break;
    case UNC_I_DEPUB:
        printf("%s\t", "DEPUB");
        pdump_loff(w, "PUB(%06lx)", d);
        break;
    case UNC_I_DEATTR:
        printf("%s\t", "DEATTR");
        pdump_reg(w, d);
        pdump_loff(w, "ID(%06lx)", d);
        break;
    case UNC_I_DEINDX:
        printf("%s\t", "DEINDX");
        pdump_reg(w, d);
        pdump_reg(w, d);
        break;
    case UNC_I_ADD_RR:
        printf("%s\t", "ADD");
        pdump_reg(w, d); pdump_reg(w, d); pdump_reg(w, d);
        break;
    case UNC_I_SUB_RR:
        printf("%s\t", "SUB");
        pdump_reg(w, d); pdump_reg(w, d); pdump_reg(w, d);
        break;
    case UNC_I_MUL_RR:
        printf("%s\t", "MUL");
        pdump_reg(w, d); pdump_reg(w, d); pdump_reg(w, d);
        break;
    case UNC_I_DIV_RR:
        printf("%s\t", "DIV");
        pdump_reg(w, d); pdump_reg(w, d); pdump_reg(w, d);
        break;
    case UNC_I_IDIV_RR:
        printf("%s\t", "IDIV");
        pdump_reg(w, d); pdump_reg(w, d); pdump_reg(w, d);
        break;
    case UNC_I_MOD_RR:
        printf("%s\t", "MOD");
        pdump_reg(w, d); pdump_reg(w, d); pdump_reg(w, d);
        break;
    case UNC_I_AND_RR:
        printf("%s\t", "AND");
        pdump_reg(w, d); pdump_reg(w, d); pdump_reg(w, d);
        break;
    case UNC_I_BOR_RR:
        printf("%s\t", "BOR");
        pdump_reg(w, d); pdump_reg(w, d); pdump_reg(w, d);
        break;
    case UNC_I_XOR_RR:
        printf("%s\t", "XOR");
        pdump_reg(w, d); pdump_reg(w, d); pdump_reg(w, d);
        break;
    case UNC_I_SHL_RR:
        printf("%s\t", "SHL");
        pdump_reg(w, d); pdump_reg(w, d); pdump_reg(w, d);
        break;
    case UNC_I_SHR_RR:
        printf("%s\t", "SHR");
        pdump_reg(w, d); pdump_reg(w, d); pdump_reg(w, d);
        break;
    case UNC_I_CAT_RR:
        printf("%s\t", "CAT");
        pdump_reg(w, d); pdump_reg(w, d); pdump_reg(w, d);
        break;
    case UNC_I_CEQ_RR:
        printf("%s\t", "CEQ");
        pdump_reg(w, d); pdump_reg(w, d); pdump_reg(w, d);
        break;
    case UNC_I_CLT_RR:
        printf("%s\t", "CLT");
        pdump_reg(w, d); pdump_reg(w, d); pdump_reg(w, d);
        break;
    case UNC_I_ADD_RL:
        printf("%s\t", "ADD");
        pdump_reg(w, d); pdump_reg(w, d); pdump_lit(w, d);
        break;
    case UNC_I_SUB_RL:
        printf("%s\t", "SUB");
        pdump_reg(w, d); pdump_reg(w, d); pdump_lit(w, d);
        break;
    case UNC_I_MUL_RL:
        printf("%s\t", "MUL");
        pdump_reg(w, d); pdump_reg(w, d); pdump_lit(w, d);
        break;
    case UNC_I_DIV_RL:
        printf("%s\t", "DIV");
        pdump_reg(w, d); pdump_reg(w, d); pdump_lit(w, d);
        break;
    case UNC_I_IDIV_RL:
        printf("%s\t", "IDIV");
        pdump_reg(w, d); pdump_reg(w, d); pdump_lit(w, d);
        break;
    case UNC_I_MOD_RL:
        printf("%s\t", "MOD");
        pdump_reg(w, d); pdump_reg(w, d); pdump_lit(w, d);
        break;
    case UNC_I_AND_RL:
        printf("%s\t", "AND");
        pdump_reg(w, d); pdump_reg(w, d); pdump_lit(w, d);
        break;
    case UNC_I_BOR_RL:
        printf("%s\t", "BOR");
        pdump_reg(w, d); pdump_reg(w, d); pdump_lit(w, d);
        break;
    case UNC_I_XOR_RL:
        printf("%s\t", "XOR");
        pdump_reg(w, d); pdump_reg(w, d); pdump_lit(w, d);
        break;
    case UNC_I_SHL_RL:
        printf("%s\t", "SHL");
        pdump_reg(w, d); pdump_reg(w, d); pdump_lit(w, d);
        break;
    case UNC_I_SHR_RL:
        printf("%s\t", "SHR");
        pdump_reg(w, d); pdump_reg(w, d); pdump_lit(w, d);
        break;
    case UNC_I_CAT_RL:
        printf("%s\t", "CAT");
        pdump_reg(w, d); pdump_reg(w, d); pdump_lit(w, d);
        break;
    case UNC_I_CEQ_RL:
        printf("%s\t", "CEQ");
        pdump_reg(w, d); pdump_reg(w, d); pdump_lit(w, d);
        break;
    case UNC_I_CLT_RL:
        printf("%s\t", "CLT");
        pdump_reg(w, d); pdump_reg(w, d); pdump_lit(w, d);
        break;
    case UNC_I_ADD_LR:
        printf("%s\t", "ADD");
        pdump_reg(w, d); pdump_lit(w, d); pdump_reg(w, d);
        break;
    case UNC_I_SUB_LR:
        printf("%s\t", "SUB");
        pdump_reg(w, d); pdump_lit(w, d); pdump_reg(w, d);
        break;
    case UNC_I_MUL_LR:
        printf("%s\t", "MUL");
        pdump_reg(w, d); pdump_lit(w, d); pdump_reg(w, d);
        break;
    case UNC_I_DIV_LR:
        printf("%s\t", "DIV");
        pdump_reg(w, d); pdump_lit(w, d); pdump_reg(w, d);
        break;
    case UNC_I_IDIV_LR:
        printf("%s\t", "IDIV");
        pdump_reg(w, d); pdump_lit(w, d); pdump_reg(w, d);
        break;
    case UNC_I_MOD_LR:
        printf("%s\t", "MOD");
        pdump_reg(w, d); pdump_lit(w, d); pdump_reg(w, d);
        break;
    case UNC_I_AND_LR:
        printf("%s\t", "AND");
        pdump_reg(w, d); pdump_lit(w, d); pdump_reg(w, d);
        break;
    case UNC_I_BOR_LR:
        printf("%s\t", "BOR");
        pdump_reg(w, d); pdump_lit(w, d); pdump_reg(w, d);
        break;
    case UNC_I_XOR_LR:
        printf("%s\t", "XOR");
        pdump_reg(w, d); pdump_lit(w, d); pdump_reg(w, d);
        break;
    case UNC_I_SHL_LR:
        printf("%s\t", "SHL");
        pdump_reg(w, d); pdump_lit(w, d); pdump_reg(w, d);
        break;
    case UNC_I_SHR_LR:
        printf("%s\t", "SHR");
        pdump_reg(w, d); pdump_lit(w, d); pdump_reg(w, d);
        break;
    case UNC_I_CAT_LR:
        printf("%s\t", "CAT");
        pdump_reg(w, d); pdump_lit(w, d); pdump_reg(w, d);
        break;
    case UNC_I_CEQ_LR:
        printf("%s\t", "CEQ");
        pdump_reg(w, d); pdump_lit(w, d); pdump_reg(w, d);
        break;
    case UNC_I_CLT_LR:
        printf("%s\t", "CLT");
        pdump_reg(w, d); pdump_lit(w, d); pdump_reg(w, d);
        break;
    case UNC_I_ADD_LL:
        printf("%s\t", "ADD");
        pdump_reg(w, d); pdump_lit(w, d); pdump_lit(w, d);
        break;
    case UNC_I_SUB_LL:
        printf("%s\t", "SUB");
        pdump_reg(w, d); pdump_lit(w, d); pdump_lit(w, d);
        break;
    case UNC_I_MUL_LL:
        printf("%s\t", "MUL");
        pdump_reg(w, d); pdump_lit(w, d); pdump_lit(w, d);
        break;
    case UNC_I_DIV_LL:
        printf("%s\t", "DIV");
        pdump_reg(w, d); pdump_lit(w, d); pdump_lit(w, d);
        break;
    case UNC_I_IDIV_LL:
        printf("%s\t", "IDIV");
        pdump_reg(w, d); pdump_lit(w, d); pdump_lit(w, d);
        break;
    case UNC_I_MOD_LL:
        printf("%s\t", "MOD");
        pdump_reg(w, d); pdump_lit(w, d); pdump_lit(w, d);
        break;
    case UNC_I_AND_LL:
        printf("%s\t", "AND");
        pdump_reg(w, d); pdump_lit(w, d); pdump_lit(w, d);
        break;
    case UNC_I_BOR_LL:
        printf("%s\t", "BOR");
        pdump_reg(w, d); pdump_lit(w, d); pdump_lit(w, d);
        break;
    case UNC_I_XOR_LL:
        printf("%s\t", "XOR");
        pdump_reg(w, d); pdump_lit(w, d); pdump_lit(w, d);
        break;
    case UNC_I_SHL_LL:
        printf("%s\t", "SHL");
        pdump_reg(w, d); pdump_lit(w, d); pdump_lit(w, d);
        break;
    case UNC_I_SHR_LL:
        printf("%s\t", "SHR");
        pdump_reg(w, d); pdump_lit(w, d); pdump_lit(w, d);
        break;
    case UNC_I_CAT_LL:
        printf("%s\t", "CAT");
        pdump_reg(w, d); pdump_lit(w, d); pdump_lit(w, d);
        break;
    case UNC_I_CEQ_LL:
        printf("%s\t", "CEQ");
        pdump_reg(w, d); pdump_lit(w, d); pdump_lit(w, d);
        break;
    case UNC_I_CLT_LL:
        printf("%s\t", "CLT");
        pdump_reg(w, d); pdump_lit(w, d); pdump_lit(w, d);
        break;
    case UNC_I_LNOT_R:
        printf("%s\t", "LNOT");
        pdump_reg(w, d); pdump_reg(w, d);
        break;
    case UNC_I_UPOS_R:
        printf("%s\t", "UPOS");
        pdump_reg(w, d); pdump_reg(w, d);
        break;
    case UNC_I_UNEG_R:
        printf("%s\t", "UNEG");
        pdump_reg(w, d); pdump_reg(w, d);
        break;
    case UNC_I_UXOR_R:
        printf("%s\t", "UXOR");
        pdump_reg(w, d); pdump_reg(w, d);
        break;
    case UNC_I_LNOT_L:
        printf("%s\t", "LNOT");
        pdump_reg(w, d); pdump_lit(w, d);
        break;
    case UNC_I_UPOS_L:
        printf("%s\t", "UPOS");
        pdump_reg(w, d); pdump_lit(w, d);
        break;
    case UNC_I_UNEG_L:
        printf("%s\t", "UNEG");
        pdump_reg(w, d); pdump_lit(w, d);
        break;
    case UNC_I_UXOR_L:
        printf("%s\t", "UXOR");
        pdump_reg(w, d); pdump_lit(w, d);
        break;
    case UNC_I_IFF:
        printf("%s\t", "IFF");
        pdump_reg(w, d); pdump_jmp(w, d);
        break;
    case UNC_I_IFT:
        printf("%s\t", "IFT");
        pdump_reg(w, d); pdump_jmp(w, d);
        break;
    case UNC_I_JMP:
        printf("%s\t", "JMP");
        pdump_jmp(w, d);
        break;
    case UNC_I_FMAKE:
        printf("%s\t", "FMAKE");
        pdump_reg(w, d);
        pdump_loff(w, "FUN(%06lx)", d);
        break;
    case UNC_I_IITER:
        printf("%s\t", "IITER");
        pdump_reg(w, d);
        pdump_reg(w, d);
        break;
    case UNC_I_INEXT:
        printf("%s\t", "INEXT");
        pdump_reg(w, d);
        pdump_reg(w, d);
        pdump_jmp(w, d);
        break;
    case UNC_I_INEXTS:
        printf("%s\t", "INEXTS");
        pdump_reg(w, d);
        pdump_jmp(w, d);
        break;
    case UNC_I_RPUSH:
        printf("%s\t", "RPUSH");
        break;
    case UNC_I_RPOP:
        printf("%s\t", "RPOP");
        break;
    case UNC_I_XPUSH:
        printf("%s\t", "XPUSH");
        pdump_jmp(w, d);
        break;
    case UNC_I_XPOP:
        printf("%s\t", "XPOP");
        break;
    case UNC_I_EXIT:
        printf("%s\t", "EXIT");
        break;
    case UNC_I_MLIST:
        printf("%s\t", "MLIST");
        pdump_reg(w, d);
        break;
    case UNC_I_NDICT:
        printf("%s\t", "NDICT");
        pdump_reg(w, d);
        break;
    case UNC_I_LSPRS:
        printf("%s\t", "LSPRS");
        pdump_reg(w, d);
        break;
    case UNC_I_LSPR:
        printf("%s\t", "LSPR");
        pdump_reg(w, d);
        pdump_reg(w, d);
        break;
    case UNC_I_MLISTP:
        printf("%s\t", "MLISTP");
        pdump_reg(w, d);
        pdump_us(w, d);
        pdump_us(w, d);
        break;
    case UNC_I_CSTK:
        printf("%s\t", "CSTK");
        pdump_us(w, d);
        break;
    case UNC_I_CSTKG:
        printf("%s\t", "CSTKG");
        pdump_us(w, d);
        break;
    case UNC_I_FBIND:
        printf("%s\t", "FBIND");
        pdump_reg(w, d);
        pdump_reg(w, d);
        pdump_reg(w, d);
        break;
    case UNC_I_WPUSH:
        printf("%s\t", "WPUSH");
        break;
    case UNC_I_WPOP:
        printf("%s\t", "WPOP");
        break;
    case UNC_I_LDATTRF:
        printf("%s\t", "LDATTRF");
        pdump_reg(w, d);
        pdump_reg(w, d);
        pdump_loff(w, "ID(%06lx)", d);
        break;
    case UNC_I_EXIT0:
        printf("%s\t", "EXIT0");
        break;
    case UNC_I_EXIT1:
        printf("%s\t", "EXIT1");
        pdump_reg(w, d);
        break;
    case UNC_I_FCALL:
        printf("%s\t", "FCALL");
        pdump_reg(w, d);
        pdump_reg(w, d);
        break;
    case UNC_I_FCALLS:
        printf("%s\t", "FCALLS");
        pdump_reg(w, d);
        break;
    case UNC_I_FTAIL:
        printf("%s\t", "FTAIL");
        pdump_reg(w, d);
        break;
    case UNC_I_DCALL:
        printf("%s\t", "DCALL");
        printf("#%-3u\t", *(*d)++);
        pdump_reg(w, d);
        pdump_reg(w, d);
        break;
    case UNC_I_DCALLS:
        printf("%s\t", "DCALLS");
        printf("#%-3u\t", *(*d)++);
        pdump_reg(w, d);
        break;
    case UNC_I_DTAIL:
        printf("%s\t", "DTAIL");
        printf("#%-3u\t", *(*d)++);
        pdump_reg(w, d);
        break;
    default:
        return 0;
    }
    putchar('\n');
    return 1;
}

void pcurinstrdump(Unc_View *w, const byte *pc) {
    int k;
    byte b;
    printf("%p\t", (void *)w);
    for (k = 0; k < w->frames.top - w->frames.base; ++k)
        printf("  ");
    printf("    %06lx          ", (unsigned long)(pc - w->bcode));
    fflush(stdout);
    b = *pc++;
    k = pinstrdump(w, &pc, b);
    if (!k) {
        puts("Illegai instruction!");
        NEVER_();
    }
}
