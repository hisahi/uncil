
#include <stdio.h>

#define UNCIL_DEFINES

#include "ucomp.h"
#include "udebug.h"
#include "ulex.h"
#include "uncil.h"
#include "uoptim.h"
#include "uparse.h"
#include "uvlq.h"

#define NOOPTIMIZE 0

int getch(void *p) {
    return getchar();
}

int fgetch(void *p) {
    return fgetc((FILE *)p);
}

static const byte *jbegin, *jbase;
static int jumpw = 1;

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

int qopcodedump(byte op) {
    switch (op) {
    case UNC_QINSTR_OP_DELETE:     printf("%s\t", "DELETE"); return 0;
    case UNC_QINSTR_OP_MOV:        printf("%s\t", "MOV");    return 2;
    case UNC_QINSTR_OP_ADD:        printf("%s\t", "ADD");    return 3;
    case UNC_QINSTR_OP_SUB:        printf("%s\t", "SUB");    return 3;
    case UNC_QINSTR_OP_MUL:        printf("%s\t", "MUL");    return 3;
    case UNC_QINSTR_OP_DIV:        printf("%s\t", "DIV");    return 3;
    case UNC_QINSTR_OP_IDIV:       printf("%s\t", "IDIV");   return 3;
    case UNC_QINSTR_OP_MOD:        printf("%s\t", "MOD");    return 3;
    case UNC_QINSTR_OP_SHL:        printf("%s\t", "SHL");    return 3;
    case UNC_QINSTR_OP_SHR:        printf("%s\t", "SHR");    return 3;
    case UNC_QINSTR_OP_CAT:        printf("%s\t", "CAT");    return 3;
    case UNC_QINSTR_OP_AND:        printf("%s\t", "AND");    return 3;
    case UNC_QINSTR_OP_OR:         printf("%s\t", "OR");     return 3;
    case UNC_QINSTR_OP_XOR:        printf("%s\t", "XOR");    return 3;
    case UNC_QINSTR_OP_CEQ:        printf("%s\t", "CEQ");    return 3;
    case UNC_QINSTR_OP_CLT:        printf("%s\t", "CLT");    return 3;
    case UNC_QINSTR_OP_JMP:        printf("%s\t", "JMP");    return -2;
    case UNC_QINSTR_OP_IFT:        printf("%s\t", "IFT");    return 2;
    case UNC_QINSTR_OP_IFF:        printf("%s\t", "IFF");    return 2;
    case UNC_QINSTR_OP_PUSH:       printf("%s\t", "PUSH");   return 2;
    case UNC_QINSTR_OP_UPOS:       printf("%s\t", "UPOS");   return 2;
    case UNC_QINSTR_OP_UNEG:       printf("%s\t", "UNEG");   return 2;
    case UNC_QINSTR_OP_UXOR:       printf("%s\t", "UXOR");   return 2;
    case UNC_QINSTR_OP_LNOT:       printf("%s\t", "LNOT");   return 2;
    case UNC_QINSTR_OP_EXPUSH:     printf("%s\t", "EXPUSH"); return -2;
    case UNC_QINSTR_OP_EXPOP:      printf("%s\t", "EXPOP");  return 0;
    case UNC_QINSTR_OP_GATTR:      printf("%s\t", "GATTR");  return 3;
    case UNC_QINSTR_OP_GATTRQ:     printf("%s\t", "GATTRQ"); return 3;
    case UNC_QINSTR_OP_SATTR:      printf("%s\t", "SATTR");  return 3;
    case UNC_QINSTR_OP_GINDX:      printf("%s\t", "GINDX");  return 3;
    case UNC_QINSTR_OP_GINDXQ:     printf("%s\t", "GINDXQ"); return 3;
    case UNC_QINSTR_OP_SINDX:      printf("%s\t", "SINDX");  return 3;
    case UNC_QINSTR_OP_PUSHF:      printf("%s\t", "PUSHF");  return 0;
    case UNC_QINSTR_OP_POPF:       printf("%s\t", "POPF");   return 0;
    case UNC_QINSTR_OP_FCALL:      printf("%s\t", "FCALL");  return 2;
    case UNC_QINSTR_OP_GPUB:       printf("%s\t", "GPUB");   return 2;
    case UNC_QINSTR_OP_SPUB:       printf("%s\t", "SPUB");   return 2;
    case UNC_QINSTR_OP_IITER:      printf("%s\t", "IITER");  return 2;
    case UNC_QINSTR_OP_INEXT:      printf("%s\t", "INEXT");  return 3;
    case UNC_QINSTR_OP_FMAKE:      printf("%s\t", "FMAKE");  return 2;
    case UNC_QINSTR_OP_MLIST:      printf("%s\t", "MLIST");  return 1;
    case UNC_QINSTR_OP_NDICT:      printf("%s\t", "NDICT");  return 1;
    case UNC_QINSTR_OP_NOP:        printf("%s\t", "NOP");    return 0;
    case UNC_QINSTR_OP_END:        printf("%s\t", "END");    return 0;
    case UNC_QINSTR_OP_GBIND:      printf("%s\t", "GBIND");  return 2;
    case UNC_QINSTR_OP_SBIND:      printf("%s\t", "SBIND");  return 2;
    case UNC_QINSTR_OP_SPREAD:     printf("%s\t", "SPREAD"); return 2;
    case UNC_QINSTR_OP_MLISTP:     printf("%s\t", "MLISTP"); return 3;
    case UNC_QINSTR_OP_STKEQ:      printf("%s\t", "STKEQ");  return -2;
    case UNC_QINSTR_OP_STKGE:      printf("%s\t", "STKGE");  return -2;
    case UNC_QINSTR_OP_GATTRF:     printf("%s\t", "GATTRF"); return 3;
    case UNC_QINSTR_OP_INEXTS:     printf("%s\t", "INEXTS"); return -3;
    case UNC_QINSTR_OP_DPUB:       printf("%s\t", "DPUB");   return -2;
    case UNC_QINSTR_OP_DATTR:      printf("%s\t", "DATTR");  return -3;
    case UNC_QINSTR_OP_DINDX:      printf("%s\t", "DINDX");  return -3;
    case UNC_QINSTR_OP_FBIND:      printf("%s\t", "FBIND");  return 3;
    case UNC_QINSTR_OP_WPUSH:      printf("%s\t", "WPUSH");  return 0;
    case UNC_QINSTR_OP_WPOP:       printf("%s\t", "WPOP");   return 0;
    case UNC_QINSTR_OP_PUSHW:      printf("%s\t", "PUSHW");  return 2;
    case UNC_QINSTR_OP_FTAIL:      printf("%s\t", "FTAIL");  return 2;
    case UNC_QINSTR_OP_DCALL:      printf("%s\t", "DCALL");  return 3;
    case UNC_QINSTR_OP_DTAIL:      printf("%s\t", "DTAIL");  return 3;
    case UNC_QINSTR_OP_EXIT0:      printf("%s\t", "EXIT0");  return 0;
    case UNC_QINSTR_OP_EXIT1:      printf("%s\t", "EXIT1");  return 1;
    }
    return -1;
}

void qsourcedump(byte type, union Unc_QInstr_Data data, int sink) {
    switch (type) {
    case UNC_QOPER_TYPE_NONE:
        printf("???      ");
        break;
    case UNC_QOPER_TYPE_TMP:
        printf("TEMP(%3lu)", (unsigned long)(data.o));
        break;
    case UNC_QOPER_TYPE_LOCAL:
        printf("LOCL(%3lu)", (unsigned long)(data.o));
        break;
    case UNC_QOPER_TYPE_EXHALE:
        printf("EXHL(%3lu)", (unsigned long)(data.o));
        break;
    case UNC_QOPER_TYPE_INHALE:
        printf("INHL(%3lu)", (unsigned long)(data.o));
        break;
    case UNC_QOPER_TYPE_PUBLIC:
        printf("PUB(%4lu)", (unsigned long)(data.o));
        break;
    case UNC_QOPER_TYPE_INT:
        printf("I(%+6ld)", (long)(data.ui));
        break;
    case UNC_QOPER_TYPE_FLOAT:
        printf("F(%+6f)", (float)(data.uf));
        break;
    case UNC_QOPER_TYPE_NULL:
        printf("NULL     ");
        break;
    case UNC_QOPER_TYPE_STR:
        printf("STR(%4lu)", (unsigned long)(data.o));
        break;
    case UNC_QOPER_TYPE_JUMP:
        printf("J(%6lu)", (unsigned long)(data.o));
        break;
    case UNC_QOPER_TYPE_STACK:
        if (sink)
            printf("STK      ");
        else
            printf("STK(%4lu)", (unsigned long)(data.o));
        break;
    case UNC_QOPER_TYPE_IDENT:
        printf("ID(%5lu)", (unsigned long)(data.o));
        break;
    case UNC_QOPER_TYPE_FALSE:
        printf("FALSE    ");
        break;
    case UNC_QOPER_TYPE_TRUE:
        printf("TRUE     ");
        break;
    case UNC_QOPER_TYPE_FUNCTION:
        printf("FUN(%4lu)", (unsigned long)(data.o));
        break;
    case UNC_QOPER_TYPE_STACKNEG:
        printf("STK(-%3lu)", (unsigned long)(data.o));
        break;
    case UNC_QOPER_TYPE_UNSIGN:
        printf("%6lu", (unsigned long)(data.o));
        break;
    case UNC_QOPER_TYPE_WSTACK:
        printf("WSTK      ");
        break;
    }
}

void qsinkdump(byte type, unsigned data) {
    union Unc_QInstr_Data d;
    d.o = data;
    qsourcedump(type, d, 1);
}

void qcodedump(const Unc_QInstr *d, Unc_Size n) {
    Unc_Size s;
    for (s = 0; s < n; ++s) {
        const Unc_QInstr *q = &d[s];
        int o;
        printf("%6lu   ", s);
        printf("L%6lu   ", q->lineno);
        o = qopcodedump(q->op);
        if (o == -1) {
            puts("=== Invalid or unrecognized q-code instruction");
            ASSERT(0);
            return;
        }
        if (o > 0)
            qsinkdump(q->o0type, q->o0data);
        if (o < 0)
            o = -o;
        if (o > 1) {
            printf("\t\t <= ");
            qsourcedump(q->o1type, q->o1data, 0);
        }
        if (o > 2) {
            printf(",\t");
            qsourcedump(q->o2type, q->o2data, 0);
        }
        putchar('\n');
    }
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

void qfuncdump(const Unc_QFunc *f) {
    Unc_Size cd_sz = f->cd_sz;
    printf("===   Temporary count %u ===\n", f->cnt_tmp);
    printf("===       Local count %u ===\n", f->cnt_loc);
    printf("===      Exhale count %u ===\n", f->cnt_exh);
    printf("===       Line number %lu ===\n", f->lineno);
    printf("===         Code size %lu (%#lx) ===\n", cd_sz, cd_sz);
    qcodedump(f->cd, cd_sz);
    if (f->cnt_inh) {
        Unc_Size i;
        printf("===      Inhale count %u ===\n", f->cnt_inh);
        for (i = 0; i < f->cnt_inh; ++i) {
            printf("\t%5lu\t", i);
            qsourcedump(f->inhales[i].type, f->inhales[i].data, 0);
            printf("\n");
        }
    }
}

int main(int argc, char* argv[]) {
    Unc_Allocator alloc;
    Unc_Context cxt;
    Unc_LexOut lexout;
    Unc_QCode qcode;
    Unc_Program program;
    int e;
    FILE *f;

    f = fopen("test.unc", "r");
    if (!f)
        return 1;

    unc0_initalloc(&alloc, NULL, NULL, NULL);
    if ((e = unc0_newcontext(&cxt, &alloc))) {
        puts("Alloc failure");
        return 1;
    }
    unc0_initprogram(&program);
    puts("======= Lexer output");
    if ((e = unc0_lexcode(&cxt, &lexout, &fgetch, f))) {
        unc0_dropcontext(&cxt);
        printf("Returned error code %04x from line %d\n",
                e, (int)lexout.lineno);
        return 1;
    }

    {
        unsigned lc_sz = (unsigned)lexout.lc_sz;
        unsigned st_sz = (unsigned)lexout.st_sz;
        unsigned id_sz = (unsigned)lexout.id_sz;
        unsigned id_n = (unsigned)lexout.id_n;
        printf("===              Code size %d (%#x) ===\n", lc_sz, lc_sz);
        hexdump(lexout.lc, lexout.lc_sz);
        printf("=== Permanent s-table size %d (%#x) ===\n", st_sz, st_sz);
        hexdump(lexout.st, lexout.st_sz);
        printf("=== Transient s-table size %d (%#x) ===\n", id_sz, id_sz);
        hexdump(lexout.id, lexout.id_sz);
        printf("===       Identifier count %d (%#x) ===\n", id_n, id_n);
    }

    puts("======= Parser Q-code output");
    if ((e = unc0_parsec1(&cxt, &qcode, &lexout))) {
        unc0_dropcontext(&cxt);
        printf("Returned error code %04x from line %d\n",
                e, (int)qcode.lineno);
        return 1;
    }

    {
        unsigned fn_sz = (unsigned)qcode.fn_sz, i;
        unsigned st_sz = (unsigned)qcode.st_sz;
        printf("===    Function count %d (%#x) ===\n", fn_sz, fn_sz);
        for (i = 0; i < fn_sz; ++i) {
            printf("================= FUNCTION %u\n", i);
            qfuncdump(&qcode.fn[i]);
        }
        printf("=== String table size %d (%#x) ===\n", st_sz, st_sz);
        hexdump(qcode.st, qcode.st_sz);
    }

#if !NOOPTIMIZE
    puts("======= Optimized Q-code output");
    if ((e = unc0_optqcode(&cxt, &qcode))) {
        unc0_dropcontext(&cxt);
        printf("Returned error code %04x\n", e);
        return 1;
    }

    {
        unsigned fn_sz = (unsigned)qcode.fn_sz, i;
        unsigned st_sz = (unsigned)qcode.st_sz;
        printf("===    Function count %d (%#x) ===\n", fn_sz, fn_sz);
        for (i = 0; i < fn_sz; ++i) {
            printf("================= FUNCTION %u\n", i);
            qfuncdump(&qcode.fn[i]);
        }
        printf("=== String table size %d (%#x) ===\n", st_sz, st_sz);
        hexdump(qcode.st, qcode.st_sz);
    }
#endif

    puts("======= Compiled bytecode (P-code) output");
    if ((e = unc0_parsec2(&cxt, &program, &qcode))) {
        unc0_dropcontext(&cxt);
        printf("Returned error code %04x\n", e);
        return 1;
    }

    {
        unsigned code_sz = (unsigned)program.code_sz;
        unsigned data_sz = (unsigned)program.data_sz;
        printf("===              Code size %d (%#x) ===\n", code_sz, code_sz);
        hexdump(program.code, code_sz);
        printf("===              Data size %d (%#x) ===\n", data_sz, data_sz);
        hexdump(program.data, data_sz);
        printf("===       Code disassembly          ===\n");
        pcodedump(program.code, code_sz);
    }

    unc0_dropprogram(&program, &alloc);
    unc0_dropcontext(&cxt);
    return 0;
}
