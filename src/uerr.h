/*******************************************************************************
 
Uncil -- error codes

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

#ifndef UNCIL_UERR_H
#define UNCIL_UERR_H

#include <stddef.h>

#include "udef.h"

/* error codes */
/* note that 0x01 - 0xFF are "valid" return values depending on context!
   they can never be valid error codes! -1 is also never a valid error code */
#define UNCIL_ERR_MEM 0x0100
#define UNCIL_ERR_INTERNAL 0x0101
#define UNCIL_ERR_UNCIL 0x0200
#define UNCIL_ERR_SYNTAX 0x0300
#define UNCIL_ERR_SYNTAX_UNTERMSTR 0x0301
#define UNCIL_ERR_SYNTAX_BADESCAPE 0x0302
#define UNCIL_ERR_SYNTAX_BADUESCAPE 0x0303
#define UNCIL_ERR_SYNTAX_TRAILING 0x0304
#define UNCIL_ERR_SYNTAX_STRAYEND 0x0305
#define UNCIL_ERR_SYNTAX_TOODEEP 0x0306
#define UNCIL_ERR_SYNTAX_BADBREAK 0x0307
#define UNCIL_ERR_SYNTAX_BADCONTINUE 0x0308
#define UNCIL_ERR_SYNTAX_INLINEIFMUSTELSE 0x0309
#define UNCIL_ERR_SYNTAX_NOFOROP 0x030A
#define UNCIL_ERR_SYNTAX_CANNOTPUBLICLOCAL 0x030B
#define UNCIL_ERR_SYNTAX_OPTAFTERREQ 0x030C
#define UNCIL_ERR_SYNTAX_UNPACKLAST 0x030D
#define UNCIL_ERR_SYNTAX_NODEFAULTUNPACK 0x030E
#define UNCIL_ERR_SYNTAX_ONLYONEELLIPSIS 0x030F
#define UNCIL_ERR_SYNTAX_FUNCTABLEUNNAMED 0x0310
#define UNCIL_ERR_SYNTAX_ELLIPSISCOMPOUND 0x0311
#define UNCIL_ERR_SYNTAX_PUBLICONLYONE 0x0312
#define UNCIL_ERR_PROGRAM_INCOMPATIBLE 0x0401
#define UNCIL_ERR_UNHASHABLE 0x0402
#define UNCIL_ERR_ARG_OUTOFBOUNDS 0x0403
#define UNCIL_ERR_ARG_REFCOPYOUTOFBOUNDS 0x0404
#define UNCIL_ERR_ARG_INDEXOUTOFBOUNDS 0x0405
#define UNCIL_ERR_ARG_INDEXNOTINTEGER 0x0406
#define UNCIL_ERR_ARG_CANNOTWEAK 0x0407
#define UNCIL_ERR_TOODEEP 0x0408
#define UNCIL_ERR_ARG_NOTENOUGHARGS 0x0409
#define UNCIL_ERR_ARG_TOOMANYARGS 0x040A
#define UNCIL_ERR_ARG_NOTINCFUNC 0x040B
#define UNCIL_ERR_ARG_NOSUCHATTR 0x040C
#define UNCIL_ERR_ARG_NOSUCHINDEX 0x040D
#define UNCIL_ERR_ARG_CANNOTSETINDEX 0x040E
#define UNCIL_ERR_ARG_NOSUCHNAME 0x040F
#define UNCIL_ERR_ARG_DIVBYZERO 0x0410
#define UNCIL_ERR_ARG_NOTITERABLE 0x0411
#define UNCIL_ERR_ARG_NOTMOSTRECENT 0x0412
#define UNCIL_ERR_ARG_NOPROGRAMLOADED 0x0413
#define UNCIL_ERR_ARG_NOTINDEXABLE 0x0414
#define UNCIL_ERR_ARG_NOTATTRABLE 0x0415
#define UNCIL_ERR_ARG_NOTATTRSETTABLE 0x0416
#define UNCIL_ERR_ARG_MODULENOTFOUND 0x0417
#define UNCIL_ERR_ARG_UNSUP1 0x0418
#define UNCIL_ERR_ARG_UNSUP2 0x0419
#define UNCIL_ERR_ARG_STRINGTOOLONG 0x041A
#define UNCIL_ERR_ARG_INVALIDPROTOTYPE 0x041B
#define UNCIL_ERR_ARG_CANNOTDELETEINDEX 0x041C
#define UNCIL_ERR_ARG_NOTATTRDELETABLE 0x041D
#define UNCIL_ERR_ARG_CANNOTBINDFUNC 0x041E
#define UNCIL_ERR_ARG_NULLCHAR 0x041F
#define UNCIL_ERR_ARG_INTOVERFLOW 0x0420
#define UNCIL_ERR_ARG_NOCFUNC 0x0421
#define UNCIL_ERR_CONVERT_TOINT 0x0502
#define UNCIL_ERR_CONVERT_TOFLOAT 0x0503
#define UNCIL_ERR_IO_GENERIC 0x0600
#define UNCIL_ERR_IO_INVALIDENCODING 0x0601
#define UNCIL_ERR_IO_UNDERLYING 0x0602
#define UNCIL_ERR_TYPE_NOTBOOL 0x0701
#define UNCIL_ERR_TYPE_NOTINT 0x0702
#define UNCIL_ERR_TYPE_NOTFLOAT 0x0703
#define UNCIL_ERR_TYPE_NOTOPAQUEPTR 0x0704
#define UNCIL_ERR_TYPE_NOTSTR 0x0705
#define UNCIL_ERR_TYPE_NOTARRAY 0x0706
#define UNCIL_ERR_TYPE_NOTDICT 0x0707
#define UNCIL_ERR_TYPE_NOTOBJECT 0x0708
#define UNCIL_ERR_TYPE_NOTBLOB 0x0709
#define UNCIL_ERR_TYPE_NOTFUNCTION 0x070A
#define UNCIL_ERR_TYPE_NOTOPAQUE 0x070B
#define UNCIL_ERR_TYPE_NOTWEAKPTR 0x070C
#define UNCIL_ERR_LOGIC_UNPACKTOOFEW 0x0801
#define UNCIL_ERR_LOGIC_UNPACKTOOMANY 0x0802
#define UNCIL_ERR_LOGIC_FINISHING 0x0803
#define UNCIL_ERR_LOGIC_OVLTOOMANY 0x0804
#define UNCIL_ERR_LOGIC_NOTSUPPORTED 0x0805
#define UNCIL_ERR_LOGIC_CMPNAN 0x0806
#define UNCIL_ERR_LOGIC_CANNOTLOCK 0x0807
#define UNCIL_ERR_TRAMPOLINE 0x0900
#define UNCIL_ERR_HALT 0x0A00

#define UNCIL_ERR_KIND(x) ((x) >> 8)
#define UNCIL_ERR_KIND_FATAL 1
#define UNCIL_ERR_KIND_UNCIL 2
#define UNCIL_ERR_KIND_SYNTAX 3
#define UNCIL_ERR_KIND_BADARG 4
#define UNCIL_ERR_KIND_CONVERT 5
#define UNCIL_ERR_KIND_IO 6
#define UNCIL_ERR_KIND_TYPE 7
#define UNCIL_ERR_KIND_LOGIC 8
#define UNCIL_ERR_KIND_TRAMPOLINE 9
#define UNCIL_ERR_KIND_HALT 10

#define UNCIL_IS_ERR(x) (UNCIL_ERR_KIND(x) != 0)
#define UNCIL_IS_ERR_CMP(x) (UNCIL_IS_ERR(x) && ((x) != -1))

struct Unc_View;
struct Unc_Frame;
int unc__err_unsup1(struct Unc_View *w, int t);
int unc__err_unsup2(struct Unc_View *w, int t1, int t2);
int unc__err_unsup2(struct Unc_View *w, int t1, int t2);
int unc__err_withname(struct Unc_View *w, int e, Unc_Size s,
                      const unsigned char *b);
void unc__errstackpush(struct Unc_View *w, Unc_Size lineno);
void unc__errstackpushcoro(struct Unc_View *w);
void unc__errinfocopyfrom(struct Unc_View *w, struct Unc_View *wc);

#endif /* UNCIL_UERR_H */
