/*******************************************************************************
 
Uncil -- Uncil bytecode compiler program

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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define UNCIL_DEFINES

#include "uncil.h"
#include "uncver.h"
#include "uops.h"
#include "uvlq.h"

#if defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))
#define UNCIL_EXIT_OK 0
#define UNCIL_EXIT_FAIL 1
#define UNCIL_EXIT_USE 2
#else
#define UNCIL_EXIT_OK EXIT_SUCCESS
#define UNCIL_EXIT_FAIL EXIT_FAILURE
#define UNCIL_EXIT_USE EXIT_FAILURE
#endif

#define UNCIL_FLAG_S 1

const char *myname;
static char errbuf[512];
Unc_View *uncil_instance;
static int cflags;
static int jumpw;
static const byte *jbegin, *jbase;

static void uncilerr(Unc_View *unc, int e) {
    size_t n = sizeof(errbuf);
    Unc_Value exc = unc_blank;
    unc_getexceptionfromcode(unc, &exc, e);
    if (!unc_exceptiontostring(unc, &exc, &n, errbuf))
        printf("%s\n", errbuf);
    else
        printf("%s: error %04x, could not get exception text\n", myname, e);
    unc_decref(unc, &exc);
}

static void uncil_atexit(void) {
    if (uncil_instance) unc_destroy(uncil_instance);
}

void hexdump(const byte *data, size_t n) {
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

void pdump_reg(const byte **d) {
    printf("R%-5lu\t", (unsigned long)unc__clqdecz(UNCIL_REGW, d));
}

void pdump_us(const byte **d) {
    printf("%-6lu\t", (unsigned long)unc__vlqdecz(d));
}

void pdump_lint(const byte **d) {
    Unc_Int ui = unc__vlqdeci(d);
    printf("I(%+6ld)\t", (long)ui);
}

void pdump_lflt(const byte **d) {
    Unc_Float uf;
    unc__memcpy(&uf, *d, sizeof(Unc_Float));
    *d += sizeof(Unc_Float);
    printf("F(%+6f)\t", (float)uf);
}

void pdump_loff(const char *s, const byte **d) {
    Unc_Size z = unc__vlqdecz(d);
    printf(s, (unsigned long)z);
    putchar('\t');
}

void pdump_lstr(const byte **d) {
    pdump_loff("STR(%06lx)", d);
}

void pdump_jmp(const byte **d) {
    Unc_Size z = unc__clqdecz(jumpw, d);
    printf("$%06lx\t", (unsigned long)(z + (jbase - jbegin)));
    putchar('\t');
}

void pdump_lit(const byte **d) {
    Unc_Int ui = *(*d)++;
    ui |= *(*d)++ << 8;
    if (ui > 0x7FFF) ui -= 0x10000;
    printf("I(%+6ld)\t", (long)ui);
}

int pinstrdump(const byte **d, byte b) {
    switch (b) {
    case UNC_I_NOP:
        printf("%s\t", "NOP");
        break;
    case UNC_I_LDNUM:
        printf("%s\t", "LDNUM");
        pdump_reg(d);
        pdump_lit(d);
        break;
    case UNC_I_LDINT:
        printf("%s\t", "LDINT");
        pdump_reg(d);
        pdump_lint(d);
        break;
    case UNC_I_LDFLT:
        printf("%s\t", "LDFLT");
        pdump_reg(d);
        pdump_lflt(d);
        break;
    case UNC_I_LDBLF:
        printf("%s\t", "LDBLF");
        pdump_reg(d);
        break;
    case UNC_I_LDBLT:
        printf("%s\t", "LDBLT");
        pdump_reg(d);
        break;
    case UNC_I_LDSTR:
        printf("%s\t", "LDSTR");
        pdump_reg(d);
        pdump_lstr(d);
        break;
    case UNC_I_LDNUL:
        printf("%s\t", "LDNUL");
        pdump_reg(d);
        break;
    case UNC_I_LDSTK:
        printf("%s\t", "LDSTK");
        pdump_reg(d);
        pdump_loff("%06lu", d);
        break;
    case UNC_I_LDPUB:
        printf("%s\t", "LDPUB");
        pdump_reg(d);
        pdump_loff("PUB(%06lx)", d);
        break;
    case UNC_I_LDBIND:
        printf("%s\t", "LDBIND");
        pdump_reg(d);
        pdump_reg(d);
        break;
    case UNC_I_LDSTKN:
        printf("%s\t", "LDSTKN");
        pdump_reg(d);
        pdump_loff("%06lu", d);
        break;
    case UNC_I_LDATTR:
        printf("%s\t", "LDATTR");
        pdump_reg(d);
        pdump_reg(d);
        pdump_loff("ID(%06lx)", d);
        break;
    case UNC_I_LDATTRQ:
        printf("%s\t", "LDATTRQ");
        pdump_reg(d);
        pdump_reg(d);
        pdump_loff("ID(%06lx)", d);
        break;
    case UNC_I_LDINDX:
        printf("%s\t", "LDINDX");
        pdump_reg(d);
        pdump_reg(d);
        pdump_reg(d);
        break;
    case UNC_I_LDINDXQ:
        printf("%s\t", "LDINDXQ");
        pdump_reg(d);
        pdump_reg(d);
        pdump_reg(d);
        break;
    case UNC_I_MOV:
        printf("%s\t", "MOV");
        pdump_reg(d);
        pdump_reg(d);
        break;
    case UNC_I_STSTK:
        printf("%s\t", "STSTK");
        pdump_reg(d);
        break;
    case UNC_I_STPUB:
        printf("%s\t", "STPUB");
        pdump_reg(d);
        pdump_loff("PUB(%06lx)", d);
        break;
    case UNC_I_STBIND:
        printf("%s\t", "STBIND");
        pdump_reg(d);
        pdump_reg(d);
        break;
    case UNC_I_STATTR:
        printf("%s\t", "STATTR");
        pdump_reg(d);
        pdump_reg(d);
        pdump_loff("ID(%06lx)", d);
        break;
    case UNC_I_STINDX:
        printf("%s\t", "STINDX");
        pdump_reg(d);
        pdump_reg(d);
        pdump_reg(d);
        break;
    case UNC_I_STWITH:
        printf("%s\t", "STWITH");
        pdump_reg(d);
        break;
    case UNC_I_ADD_RR:
        printf("%s\t", "ADD");
        pdump_reg(d); pdump_reg(d); pdump_reg(d);
        break;
    case UNC_I_SUB_RR:
        printf("%s\t", "SUB");
        pdump_reg(d); pdump_reg(d); pdump_reg(d);
        break;
    case UNC_I_MUL_RR:
        printf("%s\t", "MUL");
        pdump_reg(d); pdump_reg(d); pdump_reg(d);
        break;
    case UNC_I_DIV_RR:
        printf("%s\t", "DIV");
        pdump_reg(d); pdump_reg(d); pdump_reg(d);
        break;
    case UNC_I_IDIV_RR:
        printf("%s\t", "IDIV");
        pdump_reg(d); pdump_reg(d); pdump_reg(d);
        break;
    case UNC_I_MOD_RR:
        printf("%s\t", "MOD");
        pdump_reg(d); pdump_reg(d); pdump_reg(d);
        break;
    case UNC_I_AND_RR:
        printf("%s\t", "AND");
        pdump_reg(d); pdump_reg(d); pdump_reg(d);
        break;
    case UNC_I_BOR_RR:
        printf("%s\t", "BOR");
        pdump_reg(d); pdump_reg(d); pdump_reg(d);
        break;
    case UNC_I_XOR_RR:
        printf("%s\t", "XOR");
        pdump_reg(d); pdump_reg(d); pdump_reg(d);
        break;
    case UNC_I_SHL_RR:
        printf("%s\t", "SHL");
        pdump_reg(d); pdump_reg(d); pdump_reg(d);
        break;
    case UNC_I_SHR_RR:
        printf("%s\t", "SHR");
        pdump_reg(d); pdump_reg(d); pdump_reg(d);
        break;
    case UNC_I_CAT_RR:
        printf("%s\t", "CAT");
        pdump_reg(d); pdump_reg(d); pdump_reg(d);
        break;
    case UNC_I_CEQ_RR:
        printf("%s\t", "CEQ");
        pdump_reg(d); pdump_reg(d); pdump_reg(d);
        break;
    case UNC_I_CLT_RR:
        printf("%s\t", "CLT");
        pdump_reg(d); pdump_reg(d); pdump_reg(d);
        break;
    case UNC_I_ADD_RL:
        printf("%s\t", "ADD");
        pdump_reg(d); pdump_reg(d); pdump_lit(d);
        break;
    case UNC_I_SUB_RL:
        printf("%s\t", "SUB");
        pdump_reg(d); pdump_reg(d); pdump_lit(d);
        break;
    case UNC_I_MUL_RL:
        printf("%s\t", "MUL");
        pdump_reg(d); pdump_reg(d); pdump_lit(d);
        break;
    case UNC_I_DIV_RL:
        printf("%s\t", "DIV");
        pdump_reg(d); pdump_reg(d); pdump_lit(d);
        break;
    case UNC_I_IDIV_RL:
        printf("%s\t", "IDIV");
        pdump_reg(d); pdump_reg(d); pdump_lit(d);
        break;
    case UNC_I_MOD_RL:
        printf("%s\t", "MOD");
        pdump_reg(d); pdump_reg(d); pdump_lit(d);
        break;
    case UNC_I_AND_RL:
        printf("%s\t", "AND");
        pdump_reg(d); pdump_reg(d); pdump_lit(d);
        break;
    case UNC_I_BOR_RL:
        printf("%s\t", "BOR");
        pdump_reg(d); pdump_reg(d); pdump_lit(d);
        break;
    case UNC_I_XOR_RL:
        printf("%s\t", "XOR");
        pdump_reg(d); pdump_reg(d); pdump_lit(d);
        break;
    case UNC_I_SHL_RL:
        printf("%s\t", "SHL");
        pdump_reg(d); pdump_reg(d); pdump_lit(d);
        break;
    case UNC_I_SHR_RL:
        printf("%s\t", "SHR");
        pdump_reg(d); pdump_reg(d); pdump_lit(d);
        break;
    case UNC_I_CAT_RL:
        printf("%s\t", "CAT");
        pdump_reg(d); pdump_reg(d); pdump_lit(d);
        break;
    case UNC_I_CEQ_RL:
        printf("%s\t", "CEQ");
        pdump_reg(d); pdump_reg(d); pdump_lit(d);
        break;
    case UNC_I_CLT_RL:
        printf("%s\t", "CLT");
        pdump_reg(d); pdump_reg(d); pdump_lit(d);
        break;
    case UNC_I_ADD_LR:
        printf("%s\t", "ADD");
        pdump_reg(d); pdump_lit(d); pdump_reg(d);
        break;
    case UNC_I_SUB_LR:
        printf("%s\t", "SUB");
        pdump_reg(d); pdump_lit(d); pdump_reg(d);
        break;
    case UNC_I_MUL_LR:
        printf("%s\t", "MUL");
        pdump_reg(d); pdump_lit(d); pdump_reg(d);
        break;
    case UNC_I_DIV_LR:
        printf("%s\t", "DIV");
        pdump_reg(d); pdump_lit(d); pdump_reg(d);
        break;
    case UNC_I_IDIV_LR:
        printf("%s\t", "IDIV");
        pdump_reg(d); pdump_lit(d); pdump_reg(d);
        break;
    case UNC_I_MOD_LR:
        printf("%s\t", "MOD");
        pdump_reg(d); pdump_lit(d); pdump_reg(d);
        break;
    case UNC_I_AND_LR:
        printf("%s\t", "AND");
        pdump_reg(d); pdump_lit(d); pdump_reg(d);
        break;
    case UNC_I_BOR_LR:
        printf("%s\t", "BOR");
        pdump_reg(d); pdump_lit(d); pdump_reg(d);
        break;
    case UNC_I_XOR_LR:
        printf("%s\t", "XOR");
        pdump_reg(d); pdump_lit(d); pdump_reg(d);
        break;
    case UNC_I_SHL_LR:
        printf("%s\t", "SHL");
        pdump_reg(d); pdump_lit(d); pdump_reg(d);
        break;
    case UNC_I_SHR_LR:
        printf("%s\t", "SHR");
        pdump_reg(d); pdump_lit(d); pdump_reg(d);
        break;
    case UNC_I_CAT_LR:
        printf("%s\t", "CAT");
        pdump_reg(d); pdump_lit(d); pdump_reg(d);
        break;
    case UNC_I_CEQ_LR:
        printf("%s\t", "CEQ");
        pdump_reg(d); pdump_lit(d); pdump_reg(d);
        break;
    case UNC_I_CLT_LR:
        printf("%s\t", "CLT");
        pdump_reg(d); pdump_lit(d); pdump_reg(d);
        break;
    case UNC_I_ADD_LL:
        printf("%s\t", "ADD");
        pdump_reg(d); pdump_lit(d); pdump_lit(d);
        break;
    case UNC_I_SUB_LL:
        printf("%s\t", "SUB");
        pdump_reg(d); pdump_lit(d); pdump_lit(d);
        break;
    case UNC_I_MUL_LL:
        printf("%s\t", "MUL");
        pdump_reg(d); pdump_lit(d); pdump_lit(d);
        break;
    case UNC_I_DIV_LL:
        printf("%s\t", "DIV");
        pdump_reg(d); pdump_lit(d); pdump_lit(d);
        break;
    case UNC_I_IDIV_LL:
        printf("%s\t", "IDIV");
        pdump_reg(d); pdump_lit(d); pdump_lit(d);
        break;
    case UNC_I_MOD_LL:
        printf("%s\t", "MOD");
        pdump_reg(d); pdump_lit(d); pdump_lit(d);
        break;
    case UNC_I_AND_LL:
        printf("%s\t", "AND");
        pdump_reg(d); pdump_lit(d); pdump_lit(d);
        break;
    case UNC_I_BOR_LL:
        printf("%s\t", "BOR");
        pdump_reg(d); pdump_lit(d); pdump_lit(d);
        break;
    case UNC_I_XOR_LL:
        printf("%s\t", "XOR");
        pdump_reg(d); pdump_lit(d); pdump_lit(d);
        break;
    case UNC_I_SHL_LL:
        printf("%s\t", "SHL");
        pdump_reg(d); pdump_lit(d); pdump_lit(d);
        break;
    case UNC_I_SHR_LL:
        printf("%s\t", "SHR");
        pdump_reg(d); pdump_lit(d); pdump_lit(d);
        break;
    case UNC_I_CAT_LL:
        printf("%s\t", "CAT");
        pdump_reg(d); pdump_lit(d); pdump_lit(d);
        break;
    case UNC_I_CEQ_LL:
        printf("%s\t", "CEQ");
        pdump_reg(d); pdump_lit(d); pdump_lit(d);
        break;
    case UNC_I_CLT_LL:
        printf("%s\t", "CLT");
        pdump_reg(d); pdump_lit(d); pdump_lit(d);
        break;
    case UNC_I_LNOT_R:
        printf("%s\t", "LNOT");
        pdump_reg(d); pdump_reg(d);
        break;
    case UNC_I_UPOS_R:
        printf("%s\t", "UPOS");
        pdump_reg(d); pdump_reg(d);
        break;
    case UNC_I_UNEG_R:
        printf("%s\t", "UNEG");
        pdump_reg(d); pdump_reg(d);
        break;
    case UNC_I_UXOR_R:
        printf("%s\t", "UXOR");
        pdump_reg(d); pdump_reg(d);
        break;
    case UNC_I_LNOT_L:
        printf("%s\t", "LNOT");
        pdump_reg(d); pdump_lit(d);
        break;
    case UNC_I_UPOS_L:
        printf("%s\t", "UPOS");
        pdump_reg(d); pdump_lit(d);
        break;
    case UNC_I_UNEG_L:
        printf("%s\t", "UNEG");
        pdump_reg(d); pdump_lit(d);
        break;
    case UNC_I_UXOR_L:
        printf("%s\t", "UXOR");
        pdump_reg(d); pdump_lit(d);
        break;
    case UNC_I_IFF:
        printf("%s\t", "IFF");
        pdump_reg(d); pdump_jmp(d);
        break;
    case UNC_I_IFT:
        printf("%s\t", "IFT");
        pdump_reg(d); pdump_jmp(d);
        break;
    case UNC_I_JMP:
        printf("%s\t", "JMP");
        pdump_jmp(d);
        break;
    case UNC_I_FMAKE:
        printf("%s\t", "FMAKE");
        pdump_reg(d);
        pdump_loff("FUN(%06lx)", d);
        break;
    case UNC_I_IITER:
        printf("%s\t", "IITER");
        pdump_reg(d);
        pdump_reg(d);
        break;
    case UNC_I_INEXT:
        printf("%s\t", "INEXT");
        pdump_reg(d);
        pdump_reg(d);
        pdump_jmp(d);
        break;
    case UNC_I_INEXTS:
        printf("%s\t", "INEXTS");
        pdump_reg(d);
        pdump_jmp(d);
        break;
    case UNC_I_RPUSH:
        printf("%s\t", "RPUSH");
        break;
    case UNC_I_RPOP:
        printf("%s\t", "RPOP");
        break;
    case UNC_I_XPUSH:
        printf("%s\t", "XPUSH");
        pdump_jmp(d);
        break;
    case UNC_I_XPOP:
        printf("%s\t", "XPOP");
        break;
    case UNC_I_EXIT:
        printf("%s\t", "EXIT");
        break;
    case UNC_I_MLIST:
        printf("%s\t", "MLIST");
        pdump_reg(d);
        break;
    case UNC_I_NDICT:
        printf("%s\t", "NDICT");
        pdump_reg(d);
        break;
    case UNC_I_LSPRS:
        printf("%s\t", "LSPRS");
        pdump_reg(d);
        break;
    case UNC_I_LSPR:
        printf("%s\t", "LSPR");
        pdump_reg(d);
        pdump_reg(d);
        break;
    case UNC_I_MLISTP:
        printf("%s\t", "MLISTP");
        pdump_reg(d);
        pdump_us(d);
        pdump_us(d);
        break;
    case UNC_I_CSTK:
        printf("%s\t", "CSTK");
        pdump_us(d);
        break;
    case UNC_I_CSTKG:
        printf("%s\t", "CSTKG");
        pdump_us(d);
        break;
    case UNC_I_FBIND:
        printf("%s\t", "FBIND");
        pdump_reg(d);
        pdump_reg(d);
        pdump_reg(d);
        break;
    case UNC_I_WPUSH:
        printf("%s\t", "WPUSH");
        break;
    case UNC_I_WPOP:
        printf("%s\t", "WPOP");
        break;
    case UNC_I_LDATTRF:
        printf("%s\t", "LDATTRF");
        pdump_reg(d);
        pdump_reg(d);
        pdump_loff("ID(%06lx)", d);
        break;
    case UNC_I_EXIT0:
        printf("%s\t", "EXIT0");
        break;
    case UNC_I_EXIT1:
        printf("%s\t", "EXIT1");
        pdump_reg(d);
        break;
    case UNC_I_FCALL:
        printf("%s\t", "FCALL");
        pdump_reg(d);
        pdump_reg(d);
        break;
    case UNC_I_FCALLS:
        printf("%s\t", "FCALLS");
        pdump_reg(d);
        break;
    case UNC_I_FTAIL:
        printf("%s\t", "FTAIL");
        pdump_reg(d);
        break;
    case UNC_I_DCALL:
        printf("%s\t", "DCALL");
        printf("#%-3u\t", *(*d)++);
        pdump_reg(d);
        pdump_reg(d);
        break;
    case UNC_I_DCALLS:
        printf("%s\t", "DCALLS");
        printf("#%-3u\t", *(*d)++);
        pdump_reg(d);
        break;
    case UNC_I_DTAIL:
        printf("%s\t", "DTAIL");
        printf("#%-3u\t", *(*d)++);
        pdump_reg(d);
        break;
    case UNC_I_DEL:
        printf("%s\t", "---");
        jumpw = *(*d)++;
        jbase = *d;
        break;
    default:
        return 0;
    }
    putchar('\n');
    return 1;
}

void pcodedump(const byte *d, Unc_Size n) {
    byte b;
    const byte *s = d, *e = d + n;
    int k;
    jbegin = jbase = d;
    while (d < e) {
        printf("\t%06lx\t", d - s);
        b = *d++;
        k = pinstrdump(&d, b);
        if (!k) {
            puts("Illegai instruction!");
            break;
        }
    }
}

static int uncilcfile(char* argv[], int fileat, int ofileat) {
    int e;
    FILE *f;
    Unc_View *unc;
    int close;

    if (!strcmp(argv[fileat], "-")) {
        close = 0;
        f = stdin;
    } else {
        close = 1;
        f = fopen(argv[fileat], "r");
        if (!f) {
            printf("%s: cannot open file '%s': %s\n",
                myname, argv[fileat], strerror(errno));
            return UNCIL_EXIT_FAIL;
        }
    }

    unc = unc_createex(NULL, NULL, UNC_MMASK_ALL);
    if (!unc) {
        printf("%s: could not create an Uncil instance (low on memory?)\n",
            myname);
        return UNCIL_EXIT_FAIL;
    }

    atexit(&uncil_atexit);
    uncil_instance = unc;
    e = unc_compilefile(unc, f);
    if (close) fclose(f);
    if (e) {
        uncilerr(unc, e);
        return UNCIL_EXIT_FAIL;
    }

    if (cflags & UNCIL_FLAG_S) {
        Unc_Program *program = unc->program;
        unsigned code_sz = (unsigned)program->code_sz;
        unsigned data_sz = (unsigned)program->data_sz;
        printf("===              Code size %d (%#x) ===\n", code_sz, code_sz);
        hexdump(program->code, code_sz);
        printf("===              Data size %d (%#x) ===\n", data_sz, data_sz);
        hexdump(program->data, data_sz);
        printf("===              Code dump          ===\n");
        pcodedump(program->code, code_sz);
        return 0;
    }
    
    if (!ofileat) {
        close = 1;
        f = fopen("uncilc.out", "wb");
        if (!f) {
            printf("%s: cannot open file '%s': %s\n",
                myname, argv[fileat], strerror(errno));
            return UNCIL_EXIT_FAIL;
        }
    } else if (!strcmp(argv[ofileat], "-")) {
        close = 0;
        f = stdout;
    } else {
        close = 1;
        f = fopen(argv[ofileat], "wb");
        if (!f) {
            printf("%s: cannot open file '%s': %s\n",
                myname, argv[fileat], strerror(errno));
            return UNCIL_EXIT_FAIL;
        }
    }

    e = unc_dumpfile(unc, f);
    if (close) fclose(f);
    if (e) {
        uncilerr(unc, e);
        return UNCIL_EXIT_FAIL;
    }

    return UNCIL_EXIT_OK;
}

int print_help(int err) {
    if (!err)
        printf("uncilc - Uncil bytecode compiler (%s)\n", UNCIL_VER_STRING);
    puts("Usage: uncilc [OPTION]... file.unc");
    if (err) {
        printf("Try '%s --help' for more information.\n", myname);
    } else {
        puts("Compiles Uncil source code files (.unc) into");
        puts("bytecode files (.cnu)");
        puts("Options:");
        puts("  -?, --help");
        puts("\t\tprints this message");
        puts("  --version");
        puts("\t\tprints version information");
        puts("  -o [out]");
        puts("\t\tsets the output file name (by default, the source file name");
        puts("\t\twith the extension .cnu)");
        puts("  -S");
        puts("\t\tdumps Uncil code into the console instead of compiling");
    }
    return err;
}

int main(int argc, char* argv[]) {
    int i;
    int argindex = 1, fileindex = 0, ofileindex = 0;
    int flagok = 1;
    myname = argv[0];

    for (i = 1; i < argc; ++i) {
        char *arg = argv[i];
        if (flagok && arg[0] == '-') {
            if (!strcmp(arg, "--")) {
                flagok = 0;
            } else if (!strcmp(arg, "-o")) {
                ++i;
                if (i < argc) {
                    ofileindex = i;
                } else {
                    return print_help(UNCIL_EXIT_USE);
                }
            } else if (!strcmp(arg, "-S")) {
                cflags |= UNCIL_FLAG_S;
            } else if (!strcmp(arg, "-?") || !strcmp(arg, "--help")) {
                return print_help(UNCIL_EXIT_OK);
            } else if (!strcmp(arg, "--version")) {
                uncil_printversion();
                return 0;
            } else {
                /* unrecognized flag */
                return print_help(UNCIL_EXIT_USE);
            }
        } else {
            if (!fileindex) fileindex = argindex;
            argv[argindex++] = arg;
        }
    }

    if (!argindex)
        return print_help(UNCIL_EXIT_OK);
    return uncilcfile(argv, fileindex, ofileindex);
}
