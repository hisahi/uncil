/*******************************************************************************
 
Uncil -- character/text conversion header

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

#ifndef UNCIL_UTXT_H
#define UNCIL_UTXT_H

#include "udef.h"
#include "uhash.h"
#include "umem.h"

/* enc reads UTF-8 text and emits a byte stream in that encoding */
/* dec reads a byte stream in some encoding and emits UTF-8 text.
   n is the max number of characters to write */

/* Unc_CConv_In returns -1 on EOF or error */
/* Unc_CConv_Out returns <0 in case of error, >0 if the encoder
   should stop but there was no error */
/* Enc/Dec should return <>0 in case of error  (if Out fails or if
                                                encoding is invalid).
   enc can assume the UTF-8 text is valid, but it may contain
   individual surrogate pair characters, etc. */

typedef int (*Unc_CConv_In)(void *data);
typedef int (*Unc_CConv_Out)(void *data, Unc_Size n, const byte *b);
typedef int (*Unc_CConv_Enc)(Unc_CConv_In in, void *in_data,
                             Unc_CConv_Out out, void *out_data);
typedef int (*Unc_CConv_Dec)(Unc_CConv_In in, void *in_data,
                             Unc_CConv_Out out, void *out_data,
                             Unc_Size n);

typedef struct Unc_EncodingEntry {
    Unc_CConv_Enc enc;
    Unc_CConv_Dec dec;
} Unc_EncodingEntry;

typedef struct Unc_EncodingTable {
    Unc_Size entries;
    Unc_EncodingEntry *data;
    Unc_HTblS names;
} Unc_EncodingTable;

void unc__initenctable(Unc_Allocator *alloc, Unc_EncodingTable *table);
int unc__adddefaultencs(struct Unc_View *w, Unc_EncodingTable *table);
void unc__dropenctable(struct Unc_View *w, Unc_EncodingTable *table);
int unc__resolveencindex(struct Unc_View *w, Unc_Size name_n, const byte *name);
Unc_EncodingEntry *unc__getbyencindex(struct Unc_View *w, int index);

int unc__cconv_passthru(Unc_CConv_In in, void *in_data,
                        Unc_CConv_Out out, void *out_data);

#endif /* UNCIL_UTXT_H */
