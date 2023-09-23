/*******************************************************************************
 
Uncil -- Uncil bytecode compiler program

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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define UNCIL_DEFINES

#include "uncil.h"
#include "uncver.h"
#include "uops.h"
#include "uvlq.h"

#if UNCIL_IS_POSIX || UNCIL_IS_UNIX
#define UNCIL_EXIT_OK 0
#define UNCIL_EXIT_FAIL 1
#define UNCIL_EXIT_USE 2
#else
#define UNCIL_EXIT_OK EXIT_SUCCESS
#define UNCIL_EXIT_FAIL EXIT_FAILURE
#define UNCIL_EXIT_USE EXIT_FAILURE
#endif

#define UNCIL_FLAG_S 1

#if UNCIL_SANDBOXED
#error The standalone Uncil compiler cannot be compiled with UNCIL_SANDBOXED
#endif

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

void pdump_instr(const char *s) {
    printf("%s\t", s);
}

void pdump_reg(const byte **d) {
    printf("R%-5lu\t", (unsigned long)unc0_clqdecz(UNCIL_REGW, d));
}

void pdump_us(const byte **d) {
    printf("%-6lu\t", (unsigned long)unc0_vlqdecz(d));
}

void pdump_lint(const byte **d) {
    Unc_Int ui = unc0_vlqdeci(d);
    printf("I(%+6ld)\t", (long)ui);
}

void pdump_lflt(const byte **d) {
    Unc_Float uf;
    unc0_memcpy(&uf, *d, sizeof(Unc_Float));
    *d += sizeof(Unc_Float);
    printf("F(%+6f)\t", (float)uf);
}

void pdump_loff(const char *s, const byte **d) {
    Unc_Size z = unc0_vlqdecz(d);
    printf(s, (unsigned long)z);
    putchar('\t');
}

void pdump_lstr(const byte **d) {
    pdump_loff("STR(%06lx)", d);
}

void pdump_jmp(const byte **d) {
    Unc_Size z = unc0_clqdecz(jumpw, d);
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
        pdump_instr("NOP");
        break;
    case UNC_I_LDNUM:
        pdump_instr("LDNUM");
        pdump_reg(d);
        pdump_lit(d);
        break;
    case UNC_I_LDINT:
        pdump_instr("LDINT");
        pdump_reg(d);
        pdump_lint(d);
        break;
    case UNC_I_LDFLT:
        pdump_instr("LDFLT");
        pdump_reg(d);
        pdump_lflt(d);
        break;
    case UNC_I_LDBLF:
        pdump_instr("LDBLF");
        pdump_reg(d);
        break;
    case UNC_I_LDBLT:
        pdump_instr("LDBLT");
        pdump_reg(d);
        break;
    case UNC_I_LDSTR:
        pdump_instr("LDSTR");
        pdump_reg(d);
        pdump_lstr(d);
        break;
    case UNC_I_LDNUL:
        pdump_instr("LDNUL");
        pdump_reg(d);
        break;
    case UNC_I_LDSTK:
        pdump_instr("LDSTK");
        pdump_reg(d);
        pdump_loff("%06lu", d);
        break;
    case UNC_I_LDPUB:
        pdump_instr("LDPUB");
        pdump_reg(d);
        pdump_loff("PUB(%06lx)", d);
        break;
    case UNC_I_LDBIND:
        pdump_instr("LDBIND");
        pdump_reg(d);
        pdump_reg(d);
        break;
    case UNC_I_LDSTKN:
        pdump_instr("LDSTKN");
        pdump_reg(d);
        pdump_loff("%06lu", d);
        break;
    case UNC_I_LDATTR:
        pdump_instr("LDATTR");
        pdump_reg(d);
        pdump_reg(d);
        pdump_loff("ID(%06lx)", d);
        break;
    case UNC_I_LDATTRQ:
        pdump_instr("LDATTRQ");
        pdump_reg(d);
        pdump_reg(d);
        pdump_loff("ID(%06lx)", d);
        break;
    case UNC_I_LDINDX:
        pdump_instr("LDINDX");
        pdump_reg(d);
        pdump_reg(d);
        pdump_reg(d);
        break;
    case UNC_I_LDINDXQ:
        pdump_instr("LDINDXQ");
        pdump_reg(d);
        pdump_reg(d);
        pdump_reg(d);
        break;
    case UNC_I_MOV:
        pdump_instr("MOV");
        pdump_reg(d);
        pdump_reg(d);
        break;
    case UNC_I_STSTK:
        pdump_instr("STSTK");
        pdump_reg(d);
        break;
    case UNC_I_STPUB:
        pdump_instr("STPUB");
        pdump_reg(d);
        pdump_loff("PUB(%06lx)", d);
        break;
    case UNC_I_STBIND:
        pdump_instr("STBIND");
        pdump_reg(d);
        pdump_reg(d);
        break;
    case UNC_I_STATTR:
        pdump_instr("STATTR");
        pdump_reg(d);
        pdump_reg(d);
        pdump_loff("ID(%06lx)", d);
        break;
    case UNC_I_STINDX:
        pdump_instr("STINDX");
        pdump_reg(d);
        pdump_reg(d);
        pdump_reg(d);
        break;
    case UNC_I_STWITH:
        pdump_instr("STWITH");
        pdump_reg(d);
        break;
    case UNC_I_ADD_RR:
        pdump_instr("ADD");
        pdump_reg(d); pdump_reg(d); pdump_reg(d);
        break;
    case UNC_I_SUB_RR:
        pdump_instr("SUB");
        pdump_reg(d); pdump_reg(d); pdump_reg(d);
        break;
    case UNC_I_MUL_RR:
        pdump_instr("MUL");
        pdump_reg(d); pdump_reg(d); pdump_reg(d);
        break;
    case UNC_I_DIV_RR:
        pdump_instr("DIV");
        pdump_reg(d); pdump_reg(d); pdump_reg(d);
        break;
    case UNC_I_IDIV_RR:
        pdump_instr("IDIV");
        pdump_reg(d); pdump_reg(d); pdump_reg(d);
        break;
    case UNC_I_MOD_RR:
        pdump_instr("MOD");
        pdump_reg(d); pdump_reg(d); pdump_reg(d);
        break;
    case UNC_I_AND_RR:
        pdump_instr("AND");
        pdump_reg(d); pdump_reg(d); pdump_reg(d);
        break;
    case UNC_I_BOR_RR:
        pdump_instr("BOR");
        pdump_reg(d); pdump_reg(d); pdump_reg(d);
        break;
    case UNC_I_XOR_RR:
        pdump_instr("XOR");
        pdump_reg(d); pdump_reg(d); pdump_reg(d);
        break;
    case UNC_I_SHL_RR:
        pdump_instr("SHL");
        pdump_reg(d); pdump_reg(d); pdump_reg(d);
        break;
    case UNC_I_SHR_RR:
        pdump_instr("SHR");
        pdump_reg(d); pdump_reg(d); pdump_reg(d);
        break;
    case UNC_I_CAT_RR:
        pdump_instr("CAT");
        pdump_reg(d); pdump_reg(d); pdump_reg(d);
        break;
    case UNC_I_CEQ_RR:
        pdump_instr("CEQ");
        pdump_reg(d); pdump_reg(d); pdump_reg(d);
        break;
    case UNC_I_CLT_RR:
        pdump_instr("CLT");
        pdump_reg(d); pdump_reg(d); pdump_reg(d);
        break;
    case UNC_I_ADD_RL:
        pdump_instr("ADD");
        pdump_reg(d); pdump_reg(d); pdump_lit(d);
        break;
    case UNC_I_SUB_RL:
        pdump_instr("SUB");
        pdump_reg(d); pdump_reg(d); pdump_lit(d);
        break;
    case UNC_I_MUL_RL:
        pdump_instr("MUL");
        pdump_reg(d); pdump_reg(d); pdump_lit(d);
        break;
    case UNC_I_DIV_RL:
        pdump_instr("DIV");
        pdump_reg(d); pdump_reg(d); pdump_lit(d);
        break;
    case UNC_I_IDIV_RL:
        pdump_instr("IDIV");
        pdump_reg(d); pdump_reg(d); pdump_lit(d);
        break;
    case UNC_I_MOD_RL:
        pdump_instr("MOD");
        pdump_reg(d); pdump_reg(d); pdump_lit(d);
        break;
    case UNC_I_AND_RL:
        pdump_instr("AND");
        pdump_reg(d); pdump_reg(d); pdump_lit(d);
        break;
    case UNC_I_BOR_RL:
        pdump_instr("BOR");
        pdump_reg(d); pdump_reg(d); pdump_lit(d);
        break;
    case UNC_I_XOR_RL:
        pdump_instr("XOR");
        pdump_reg(d); pdump_reg(d); pdump_lit(d);
        break;
    case UNC_I_SHL_RL:
        pdump_instr("SHL");
        pdump_reg(d); pdump_reg(d); pdump_lit(d);
        break;
    case UNC_I_SHR_RL:
        pdump_instr("SHR");
        pdump_reg(d); pdump_reg(d); pdump_lit(d);
        break;
    case UNC_I_CAT_RL:
        pdump_instr("CAT");
        pdump_reg(d); pdump_reg(d); pdump_lit(d);
        break;
    case UNC_I_CEQ_RL:
        pdump_instr("CEQ");
        pdump_reg(d); pdump_reg(d); pdump_lit(d);
        break;
    case UNC_I_CLT_RL:
        pdump_instr("CLT");
        pdump_reg(d); pdump_reg(d); pdump_lit(d);
        break;
    case UNC_I_ADD_LR:
        pdump_instr("ADD");
        pdump_reg(d); pdump_lit(d); pdump_reg(d);
        break;
    case UNC_I_SUB_LR:
        pdump_instr("SUB");
        pdump_reg(d); pdump_lit(d); pdump_reg(d);
        break;
    case UNC_I_MUL_LR:
        pdump_instr("MUL");
        pdump_reg(d); pdump_lit(d); pdump_reg(d);
        break;
    case UNC_I_DIV_LR:
        pdump_instr("DIV");
        pdump_reg(d); pdump_lit(d); pdump_reg(d);
        break;
    case UNC_I_IDIV_LR:
        pdump_instr("IDIV");
        pdump_reg(d); pdump_lit(d); pdump_reg(d);
        break;
    case UNC_I_MOD_LR:
        pdump_instr("MOD");
        pdump_reg(d); pdump_lit(d); pdump_reg(d);
        break;
    case UNC_I_AND_LR:
        pdump_instr("AND");
        pdump_reg(d); pdump_lit(d); pdump_reg(d);
        break;
    case UNC_I_BOR_LR:
        pdump_instr("BOR");
        pdump_reg(d); pdump_lit(d); pdump_reg(d);
        break;
    case UNC_I_XOR_LR:
        pdump_instr("XOR");
        pdump_reg(d); pdump_lit(d); pdump_reg(d);
        break;
    case UNC_I_SHL_LR:
        pdump_instr("SHL");
        pdump_reg(d); pdump_lit(d); pdump_reg(d);
        break;
    case UNC_I_SHR_LR:
        pdump_instr("SHR");
        pdump_reg(d); pdump_lit(d); pdump_reg(d);
        break;
    case UNC_I_CAT_LR:
        pdump_instr("CAT");
        pdump_reg(d); pdump_lit(d); pdump_reg(d);
        break;
    case UNC_I_CEQ_LR:
        pdump_instr("CEQ");
        pdump_reg(d); pdump_lit(d); pdump_reg(d);
        break;
    case UNC_I_CLT_LR:
        pdump_instr("CLT");
        pdump_reg(d); pdump_lit(d); pdump_reg(d);
        break;
    case UNC_I_ADD_LL:
        pdump_instr("ADD");
        pdump_reg(d); pdump_lit(d); pdump_lit(d);
        break;
    case UNC_I_SUB_LL:
        pdump_instr("SUB");
        pdump_reg(d); pdump_lit(d); pdump_lit(d);
        break;
    case UNC_I_MUL_LL:
        pdump_instr("MUL");
        pdump_reg(d); pdump_lit(d); pdump_lit(d);
        break;
    case UNC_I_DIV_LL:
        pdump_instr("DIV");
        pdump_reg(d); pdump_lit(d); pdump_lit(d);
        break;
    case UNC_I_IDIV_LL:
        pdump_instr("IDIV");
        pdump_reg(d); pdump_lit(d); pdump_lit(d);
        break;
    case UNC_I_MOD_LL:
        pdump_instr("MOD");
        pdump_reg(d); pdump_lit(d); pdump_lit(d);
        break;
    case UNC_I_AND_LL:
        pdump_instr("AND");
        pdump_reg(d); pdump_lit(d); pdump_lit(d);
        break;
    case UNC_I_BOR_LL:
        pdump_instr("BOR");
        pdump_reg(d); pdump_lit(d); pdump_lit(d);
        break;
    case UNC_I_XOR_LL:
        pdump_instr("XOR");
        pdump_reg(d); pdump_lit(d); pdump_lit(d);
        break;
    case UNC_I_SHL_LL:
        pdump_instr("SHL");
        pdump_reg(d); pdump_lit(d); pdump_lit(d);
        break;
    case UNC_I_SHR_LL:
        pdump_instr("SHR");
        pdump_reg(d); pdump_lit(d); pdump_lit(d);
        break;
    case UNC_I_CAT_LL:
        pdump_instr("CAT");
        pdump_reg(d); pdump_lit(d); pdump_lit(d);
        break;
    case UNC_I_CEQ_LL:
        pdump_instr("CEQ");
        pdump_reg(d); pdump_lit(d); pdump_lit(d);
        break;
    case UNC_I_CLT_LL:
        pdump_instr("CLT");
        pdump_reg(d); pdump_lit(d); pdump_lit(d);
        break;
    case UNC_I_LNOT_R:
        pdump_instr("LNOT");
        pdump_reg(d); pdump_reg(d);
        break;
    case UNC_I_UPOS_R:
        pdump_instr("UPOS");
        pdump_reg(d); pdump_reg(d);
        break;
    case UNC_I_UNEG_R:
        pdump_instr("UNEG");
        pdump_reg(d); pdump_reg(d);
        break;
    case UNC_I_UXOR_R:
        pdump_instr("UXOR");
        pdump_reg(d); pdump_reg(d);
        break;
    case UNC_I_LNOT_L:
        pdump_instr("LNOT");
        pdump_reg(d); pdump_lit(d);
        break;
    case UNC_I_UPOS_L:
        pdump_instr("UPOS");
        pdump_reg(d); pdump_lit(d);
        break;
    case UNC_I_UNEG_L:
        pdump_instr("UNEG");
        pdump_reg(d); pdump_lit(d);
        break;
    case UNC_I_UXOR_L:
        pdump_instr("UXOR");
        pdump_reg(d); pdump_lit(d);
        break;
    case UNC_I_IFF:
        pdump_instr("IFF");
        pdump_reg(d); pdump_jmp(d);
        break;
    case UNC_I_IFT:
        pdump_instr("IFT");
        pdump_reg(d); pdump_jmp(d);
        break;
    case UNC_I_JMP:
        pdump_instr("JMP");
        pdump_jmp(d);
        break;
    case UNC_I_FMAKE:
        pdump_instr("FMAKE");
        pdump_reg(d);
        pdump_loff("FUN(%06lx)", d);
        break;
    case UNC_I_IITER:
        pdump_instr("IITER");
        pdump_reg(d);
        pdump_reg(d);
        break;
    case UNC_I_INEXT:
        pdump_instr("INEXT");
        pdump_reg(d);
        pdump_reg(d);
        pdump_jmp(d);
        break;
    case UNC_I_INEXTS:
        pdump_instr("INEXTS");
        pdump_reg(d);
        pdump_jmp(d);
        break;
    case UNC_I_RPUSH:
        pdump_instr("RPUSH");
        break;
    case UNC_I_RPOP:
        pdump_instr("RPOP");
        break;
    case UNC_I_XPUSH:
        pdump_instr("XPUSH");
        pdump_jmp(d);
        break;
    case UNC_I_XPOP:
        pdump_instr("XPOP");
        break;
    case UNC_I_EXIT:
        pdump_instr("EXIT");
        break;
    case UNC_I_MLIST:
        pdump_instr("MLIST");
        pdump_reg(d);
        break;
    case UNC_I_NDICT:
        pdump_instr("NDICT");
        pdump_reg(d);
        break;
    case UNC_I_LSPRS:
        pdump_instr("LSPRS");
        pdump_reg(d);
        break;
    case UNC_I_LSPR:
        pdump_instr("LSPR");
        pdump_reg(d);
        pdump_reg(d);
        break;
    case UNC_I_MLISTP:
        pdump_instr("MLISTP");
        pdump_reg(d);
        pdump_us(d);
        pdump_us(d);
        break;
    case UNC_I_CSTK:
        pdump_instr("CSTK");
        pdump_us(d);
        break;
    case UNC_I_CSTKG:
        pdump_instr("CSTKG");
        pdump_us(d);
        break;
    case UNC_I_FBIND:
        pdump_instr("FBIND");
        pdump_reg(d);
        pdump_reg(d);
        pdump_reg(d);
        break;
    case UNC_I_WPUSH:
        pdump_instr("WPUSH");
        break;
    case UNC_I_WPOP:
        pdump_instr("WPOP");
        break;
    case UNC_I_LDATTRF:
        pdump_instr("LDATTRF");
        pdump_reg(d);
        pdump_reg(d);
        pdump_loff("ID(%06lx)", d);
        break;
    case UNC_I_EXIT0:
        pdump_instr("EXIT0");
        break;
    case UNC_I_EXIT1:
        pdump_instr("EXIT1");
        pdump_reg(d);
        break;
    case UNC_I_FCALL:
        pdump_instr("FCALL");
        pdump_reg(d);
        pdump_reg(d);
        break;
    case UNC_I_FCALLS:
        pdump_instr("FCALLS");
        pdump_reg(d);
        break;
    case UNC_I_FTAIL:
        pdump_instr("FTAIL");
        pdump_reg(d);
        break;
    case UNC_I_DCALL:
        pdump_instr("DCALL");
        printf("#%-3u\t", *(*d)++);
        pdump_reg(d);
        pdump_reg(d);
        break;
    case UNC_I_DCALLS:
        pdump_instr("DCALLS");
        printf("#%-3u\t", *(*d)++);
        pdump_reg(d);
        break;
    case UNC_I_DTAIL:
        pdump_instr("DTAIL");
        printf("#%-3u\t", *(*d)++);
        pdump_reg(d);
        break;
    case UNC_I_DEL:
        pdump_instr("---");
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

static int uncilcfile(char *argv[], int fileat, int ofileat) {
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
    
    if (ofileat && !strcmp(argv[ofileat], "-")) {
        close = 0;
        f = stdout;
    } else {
        close = 1;
        f = fopen(ofileat ? argv[ofileat] : "uncilc.out", "wb");
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
        puts("uncilc - Uncil bytecode compiler");
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
        puts("\t\tsets the output file name (by default, the source ");
        puts("\t\tfile name with the extension .cnu)");
        puts("  -S");
        puts("\t\tdumps Uncil code into the console instead of compiling");
    }
    return err;
}

int main(int argc, char *argv[]) {
    int i;
    int argindex = 1, fileindex = 0, ofileindex = 0;
    int flagok = 1, version_query = 0;
    myname = argv[0];

    for (i = 1; i < argc; ++i) {
        char *arg = argv[i];
        if (flagok && arg[0] == '-' && argv[i][1]) {
            char *arg = argv[i];
            char buf[2], fchr;
            const char *fptr;

            if (argv[i][1] == '-') {
                const char *fstr = &argv[i][2];
                buf[1] = 0;
                if (!*fstr) {
                    flagok = 0;
                } else if (!strcmp(fstr, "help")) {
                    buf[0] = '?';  /* --help => -? */
                } else if (!strcmp(fstr, "version")) {
                    buf[0] = 'v';  /* --version => -v */
                } else {
                    /* unrecognized flag */
                    return print_help(UNCIL_EXIT_USE);
                }
                fptr = &buf[0];
            } else {
                fptr = &argv[i][1];
            }

            while ((fchr = *fptr++)) {
                if (fchr == 'o') {
                    ++i;
                    if (i < argc) {
                        ofileindex = i;
                    } else {
                        return print_help(UNCIL_EXIT_USE);
                    }
                } else if (fchr == 'S') {
                    cflags |= UNCIL_FLAG_S;
                } else if (fchr == '?') {
                    return print_help(UNCIL_EXIT_OK);
                } else if (fchr == 'v') {
                    if (version_query < INT_MAX) ++version_query;
                } else {
                    /* unrecognized flag */
                    return print_help(UNCIL_EXIT_USE);
                }
            }
        } else {
            if (!fileindex) fileindex = argindex;
            argv[argindex++] = arg;
        }
    }

    if (version_query) {
        uncil_printversion(version_query - 1);
        return 0;
    }

    if (!fileindex)
        return print_help(UNCIL_EXIT_OK);
    return uncilcfile(argv, fileindex, ofileindex);
}
