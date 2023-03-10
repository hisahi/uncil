/*******************************************************************************
 
Uncil -- P-code instruction table

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

#ifndef UNCIL_UOPS_H
#define UNCIL_UOPS_H

/* make sure to also update uvm.c INSTRLIST */
#define UNC_I_NOP               0x00
#define UNC_I_LDNUM             0x01
#define UNC_I_LDINT             0x02
#define UNC_I_LDFLT             0x03
#define UNC_I_LDBLF             0x04
#define UNC_I_LDBLT             0x05
#define UNC_I_LDSTR             0x06
#define UNC_I_LDNUL             0x07
#define UNC_I_LDSTK             0x08
#define UNC_I_LDPUB             0x09
#define UNC_I_LDBIND            0x0A
#define UNC_I_LDSTKN            0x0B
#define UNC_I_LDATTR            0x0C
#define UNC_I_LDATTRQ           0x0D
#define UNC_I_LDINDX            0x0E
#define UNC_I_LDINDXQ           0x0F
#define UNC_I_MOV               0x10
#define UNC_I_STPUB             0x11
#define UNC_I_STATTR            0x14
#define UNC_I_STWITH            0x15
#define UNC_I_STINDX            0x16
#define UNC_I_STSTK             0x18
#define UNC_I_STBIND            0x1A
#define UNC_I_DEPUB             0x21
#define UNC_I_DEATTR            0x24
#define UNC_I_DEINDX            0x26

#define UNC_I_LDATTRF           0x3C

#define UNC_I_ADD_RR            0x40
#define UNC_I_SUB_RR            0x41
#define UNC_I_MUL_RR            0x42
#define UNC_I_DIV_RR            0x43
#define UNC_I_IDIV_RR           0x44
#define UNC_I_MOD_RR            0x45
#define UNC_I_AND_RR            0x46
#define UNC_I_BOR_RR            0x47
#define UNC_I_XOR_RR            0x48
#define UNC_I_SHL_RR            0x49
#define UNC_I_SHR_RR            0x4A
#define UNC_I_CAT_RR            0x4B
#define UNC_I_CEQ_RR            0x4C
#define UNC_I_CLT_RR            0x4D

#define UNC_I_ADD_RL            0x50
#define UNC_I_SUB_RL            0x51
#define UNC_I_MUL_RL            0x52
#define UNC_I_DIV_RL            0x53
#define UNC_I_IDIV_RL           0x54
#define UNC_I_MOD_RL            0x55
#define UNC_I_AND_RL            0x56
#define UNC_I_BOR_RL            0x57
#define UNC_I_XOR_RL            0x58
#define UNC_I_SHL_RL            0x59
#define UNC_I_SHR_RL            0x5A
#define UNC_I_CAT_RL            0x5B
#define UNC_I_CEQ_RL            0x5C
#define UNC_I_CLT_RL            0x5D

#define UNC_I_ADD_LR            0x60
#define UNC_I_SUB_LR            0x61
#define UNC_I_MUL_LR            0x62
#define UNC_I_DIV_LR            0x63
#define UNC_I_IDIV_LR           0x64
#define UNC_I_MOD_LR            0x65
#define UNC_I_AND_LR            0x66
#define UNC_I_BOR_LR            0x67
#define UNC_I_XOR_LR            0x68
#define UNC_I_SHL_LR            0x69
#define UNC_I_SHR_LR            0x6A
#define UNC_I_CAT_LR            0x6B
#define UNC_I_CEQ_LR            0x6C
#define UNC_I_CLT_LR            0x6D

#define UNC_I_ADD_LL            0x70
#define UNC_I_SUB_LL            0x71
#define UNC_I_MUL_LL            0x72
#define UNC_I_DIV_LL            0x73
#define UNC_I_IDIV_LL           0x74
#define UNC_I_MOD_LL            0x75
#define UNC_I_AND_LL            0x76
#define UNC_I_BOR_LL            0x77
#define UNC_I_XOR_LL            0x78
#define UNC_I_SHL_LL            0x79
#define UNC_I_SHR_LL            0x7A
#define UNC_I_CAT_LL            0x7B
#define UNC_I_CEQ_LL            0x7C
#define UNC_I_CLT_LL            0x7D

#define UNC_I_LNOT_R            0x80
#define UNC_I_UPOS_R            0x81
#define UNC_I_UNEG_R            0x82
#define UNC_I_UXOR_R            0x83

#define UNC_I_LNOT_L            0x90
#define UNC_I_UPOS_L            0x91
#define UNC_I_UNEG_L            0x92
#define UNC_I_UXOR_L            0x93

/* jumps are absolute within a function but relative to function base */
#define UNC_I_IFF               0xC0
#define UNC_I_IFT               0xC1
#define UNC_I_JMP               0xC2
#define UNC_I_EXIT              0xC3
#define UNC_I_EXIT0             0xC4
#define UNC_I_EXIT1             0xC5
#define UNC_I_WPUSH             0xC6
#define UNC_I_WPOP              0xC7
#define UNC_I_RPUSH             0xC8
#define UNC_I_RPOP              0xC9
#define UNC_I_XPUSH             0xCA
#define UNC_I_XPOP              0xCB
#define UNC_I_LSPRS             0xCC
#define UNC_I_LSPR              0xCD
#define UNC_I_CSTK              0xCE
#define UNC_I_CSTKG             0xCF
#define UNC_I_MLIST             0xD0
#define UNC_I_NDICT             0xD1
#define UNC_I_MLISTP            0xD2
#define UNC_I_IITER             0xD3
#define UNC_I_FMAKE             0xD4
#define UNC_I_FBIND             0xD5
#define UNC_I_INEXTS            0xD6
#define UNC_I_INEXT             0xD7
#define UNC_I_DCALLS            0xD8
#define UNC_I_DCALL             0xD9
#define UNC_I_DTAIL             0xDA
#define UNC_I_FCALLS            0xDC
#define UNC_I_FCALL             0xDD
#define UNC_I_FTAIL             0xDE

/* this instruction will never be valid, but treat as NOP */
#define UNC_I_DEL               0xFF

#define UNC_LIT_NUL             0
#define UNC_LIT_INT             1
#define UNC_LIT_FLT             2
#define UNC_LIT_STR             3

#define UNC_BYTES_IN_FCODEADDR 8

#endif /* UNCIL_UOPS_H */
