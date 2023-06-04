
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

void qopcode_instr(const char *s) {
    printf("%s\t", s);
}

int qopcodedump(byte op) {
    switch (op) {
    case UNC_QINSTR_OP_DELETE:     qopcode_instr("DELETE"); return 0;
    case UNC_QINSTR_OP_MOV:        qopcode_instr("MOV");    return 2;
    case UNC_QINSTR_OP_ADD:        qopcode_instr("ADD");    return 3;
    case UNC_QINSTR_OP_SUB:        qopcode_instr("SUB");    return 3;
    case UNC_QINSTR_OP_MUL:        qopcode_instr("MUL");    return 3;
    case UNC_QINSTR_OP_DIV:        qopcode_instr("DIV");    return 3;
    case UNC_QINSTR_OP_IDIV:       qopcode_instr("IDIV");   return 3;
    case UNC_QINSTR_OP_MOD:        qopcode_instr("MOD");    return 3;
    case UNC_QINSTR_OP_SHL:        qopcode_instr("SHL");    return 3;
    case UNC_QINSTR_OP_SHR:        qopcode_instr("SHR");    return 3;
    case UNC_QINSTR_OP_CAT:        qopcode_instr("CAT");    return 3;
    case UNC_QINSTR_OP_AND:        qopcode_instr("AND");    return 3;
    case UNC_QINSTR_OP_OR:         qopcode_instr("OR");     return 3;
    case UNC_QINSTR_OP_XOR:        qopcode_instr("XOR");    return 3;
    case UNC_QINSTR_OP_CEQ:        qopcode_instr("CEQ");    return 3;
    case UNC_QINSTR_OP_CLT:        qopcode_instr("CLT");    return 3;
    case UNC_QINSTR_OP_JMP:        qopcode_instr("JMP");    return -2;
    case UNC_QINSTR_OP_IFT:        qopcode_instr("IFT");    return 2;
    case UNC_QINSTR_OP_IFF:        qopcode_instr("IFF");    return 2;
    case UNC_QINSTR_OP_PUSH:       qopcode_instr("PUSH");   return 2;
    case UNC_QINSTR_OP_UPOS:       qopcode_instr("UPOS");   return 2;
    case UNC_QINSTR_OP_UNEG:       qopcode_instr("UNEG");   return 2;
    case UNC_QINSTR_OP_UXOR:       qopcode_instr("UXOR");   return 2;
    case UNC_QINSTR_OP_LNOT:       qopcode_instr("LNOT");   return 2;
    case UNC_QINSTR_OP_EXPUSH:     qopcode_instr("EXPUSH"); return -2;
    case UNC_QINSTR_OP_EXPOP:      qopcode_instr("EXPOP");  return 0;
    case UNC_QINSTR_OP_GATTR:      qopcode_instr("GATTR");  return 3;
    case UNC_QINSTR_OP_GATTRQ:     qopcode_instr("GATTRQ"); return 3;
    case UNC_QINSTR_OP_SATTR:      qopcode_instr("SATTR");  return 3;
    case UNC_QINSTR_OP_GINDX:      qopcode_instr("GINDX");  return 3;
    case UNC_QINSTR_OP_GINDXQ:     qopcode_instr("GINDXQ"); return 3;
    case UNC_QINSTR_OP_SINDX:      qopcode_instr("SINDX");  return 3;
    case UNC_QINSTR_OP_PUSHF:      qopcode_instr("PUSHF");  return 0;
    case UNC_QINSTR_OP_POPF:       qopcode_instr("POPF");   return 0;
    case UNC_QINSTR_OP_FCALL:      qopcode_instr("FCALL");  return 2;
    case UNC_QINSTR_OP_GPUB:       qopcode_instr("GPUB");   return 2;
    case UNC_QINSTR_OP_SPUB:       qopcode_instr("SPUB");   return 2;
    case UNC_QINSTR_OP_IITER:      qopcode_instr("IITER");  return 2;
    case UNC_QINSTR_OP_INEXT:      qopcode_instr("INEXT");  return 3;
    case UNC_QINSTR_OP_FMAKE:      qopcode_instr("FMAKE");  return 2;
    case UNC_QINSTR_OP_MLIST:      qopcode_instr("MLIST");  return 1;
    case UNC_QINSTR_OP_NDICT:      qopcode_instr("NDICT");  return 1;
    case UNC_QINSTR_OP_NOP:        qopcode_instr("NOP");    return 0;
    case UNC_QINSTR_OP_END:        qopcode_instr("END");    return 0;
    case UNC_QINSTR_OP_GBIND:      qopcode_instr("GBIND");  return 2;
    case UNC_QINSTR_OP_SBIND:      qopcode_instr("SBIND");  return 2;
    case UNC_QINSTR_OP_SPREAD:     qopcode_instr("SPREAD"); return 2;
    case UNC_QINSTR_OP_MLISTP:     qopcode_instr("MLISTP"); return 3;
    case UNC_QINSTR_OP_STKEQ:      qopcode_instr("STKEQ");  return -2;
    case UNC_QINSTR_OP_STKGE:      qopcode_instr("STKGE");  return -2;
    case UNC_QINSTR_OP_GATTRF:     qopcode_instr("GATTRF"); return 3;
    case UNC_QINSTR_OP_INEXTS:     qopcode_instr("INEXTS"); return -3;
    case UNC_QINSTR_OP_DPUB:       qopcode_instr("DPUB");   return -2;
    case UNC_QINSTR_OP_DATTR:      qopcode_instr("DATTR");  return -3;
    case UNC_QINSTR_OP_DINDX:      qopcode_instr("DINDX");  return -3;
    case UNC_QINSTR_OP_FBIND:      qopcode_instr("FBIND");  return 3;
    case UNC_QINSTR_OP_WPUSH:      qopcode_instr("WPUSH");  return 0;
    case UNC_QINSTR_OP_WPOP:       qopcode_instr("WPOP");   return 0;
    case UNC_QINSTR_OP_PUSHW:      qopcode_instr("PUSHW");  return 2;
    case UNC_QINSTR_OP_FTAIL:      qopcode_instr("FTAIL");  return 2;
    case UNC_QINSTR_OP_DCALL:      qopcode_instr("DCALL");  return 3;
    case UNC_QINSTR_OP_DTAIL:      qopcode_instr("DTAIL");  return 3;
    case UNC_QINSTR_OP_EXIT0:      qopcode_instr("EXIT0");  return 0;
    case UNC_QINSTR_OP_EXIT1:      qopcode_instr("EXIT1");  return 1;
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

    if ((e = unc0_initalloc(&alloc, NULL, NULL, NULL))) {
        puts("Alloc failure");
        return 1;
    }
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
