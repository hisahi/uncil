/*******************************************************************************
 
Uncil -- program impl

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

#define UNCIL_DEFINES

#include "udebug.h"
#include "uerr.h"
#include "umt.h"
#include "uprog.h"

Unc_Program *unc0_newprogram(Unc_Allocator *alloc) {
    Unc_Program *p = TMALLOC(Unc_Program, alloc, 0, 1);
    if (p) unc0_initprogram(p);
    return p;
}

void unc0_initprogram(Unc_Program *program) {
    program->uncil_version = UNCIL_PROGRAM_VER;
    ATOMICLSET(program->refcount, 0);
    program->code_sz = program->data_sz = 0;
    program->code = program->data = NULL;
    program->main_doff = 0;
    program->next = NULL;
    program->pname = NULL;
}

Unc_RetVal unc0_upgradeprogram(Unc_Program *program, Unc_Allocator *alloc) {
    if (program->uncil_version == UNCIL_PROGRAM_VER)
        return 0;
    if (program->uncil_version > UNCIL_PROGRAM_VER)
        return UNCIL_ERR_PROGRAM_INCOMPATIBLE;
    /* upgrade program here if necessary */
    if (program->uncil_version != UNCIL_PROGRAM_VER)
        return UNCIL_ERR_PROGRAM_INCOMPATIBLE;
    return 0;
}

void unc0_dropprogram(Unc_Program *program, Unc_Allocator *alloc) {
    unc0_mfree(alloc, program->code, program->code_sz);
    unc0_mfree(alloc, program->data, program->data_sz);
    if (program->pname) unc0_mmfree(alloc, program->pname);
    unc0_initprogram(program);
}

void unc0_freeprogram(Unc_Program *program, Unc_Allocator *alloc) {
    unc0_dropprogram(program, alloc);
    TMFREE(Unc_Program, alloc, program, 1);
}

Unc_Program *unc0_progincref(Unc_Program *program) {
    ATOMICLINC(program->refcount);
    ASSERT(program->refcount);
    return program;
}

Unc_Program *unc0_progdecref(Unc_Program *program, Unc_Allocator *alloc) {
    if (!ATOMICLDEC(program->refcount))
        unc0_freeprogram(program, alloc);
    return NULL;
}
