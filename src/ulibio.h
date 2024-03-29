/*******************************************************************************
 
Uncil -- builtin io library header

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

#ifndef UNCIL_ULIBIO_H
#define UNCIL_ULIBIO_H

#include <stdio.h>

#include "udef.h"

struct ulib_io_file {
    FILE *f;
    int encoding;
    int encodingtext;
    int flags;
    char *tfn;
};

struct Unc_Allocator;
struct Unc_Value;
struct Unc_View;

Unc_RetVal unc0_io_init(struct Unc_View *w);

int unc0_io_feof(struct ulib_io_file *file);
int unc0_io_ferror(struct ulib_io_file *file);
/*
int unc0_io_fread(struct ulib_io_file *file, Unc_Byte *b,
                  Unc_Size o, Unc_Size n);
int unc0_io_fwrite(struct ulib_io_file *file, const Unc_Byte *b, Unc_Size n);
int unc0_io_fflush(struct ulib_io_file *file);
int unc0_io_fclose(struct Unc_Allocator *alloc, struct ulib_io_file *file);
*/
Unc_RetVal unc0_io_makeerr(struct Unc_View *w, const char *prefix, int err);
Unc_RetVal unc0_io_makeerr_errno(struct Unc_View *w, const char *prefix);

int unc0_io_fread_p(struct Unc_View *w, struct ulib_io_file *file, Unc_Byte *b,
                  Unc_Size o, Unc_Size n);
int unc0_io_fwrite_p(struct Unc_View *w, struct ulib_io_file *file,
                  const Unc_Byte *b, Unc_Size n);
int unc0_io_fflush_p(struct Unc_View *w, struct ulib_io_file *file);
int unc0_io_fclose_p(struct Unc_View *w,
                  struct Unc_Allocator *alloc, struct ulib_io_file *file);

#define UNC_IO_GETC_BUFFER 16
int unc0_io_fgetc_text(struct Unc_View *w, struct ulib_io_file *file,
                        char *buffer);
int unc0_io_fwrite_text(struct Unc_View *w, struct ulib_io_file *file,
                        const Unc_Byte *b, Unc_Size n);

Unc_RetVal unc0_io_fwrap(struct Unc_View *w, struct Unc_Value *v,
                         FILE *file, int wr);
Unc_RetVal unc0_io_lockfile(struct Unc_View *w, struct Unc_Value *v,
                            struct ulib_io_file **file, int ignoreerr);
void unc0_io_unlockfile(struct Unc_View *w, struct Unc_Value *v);

int unc0_io_fgetc(struct ulib_io_file *file);
/*int unc0_io_fputc(int c, struct ulib_io_file *file);*/

int unc0_io_fgetc_p(struct Unc_View *w, struct ulib_io_file *file);
int unc0_io_fputc_p(struct Unc_View *w, int c, struct ulib_io_file *file);

#endif /* UNCIL_ULIBIO_H */
