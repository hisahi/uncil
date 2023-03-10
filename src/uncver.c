/*******************************************************************************
 
Uncil -- Uncil version information formatter

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

#include <stdio.h>

#define UNCIL_DEFINES

#include "udef.h"
#include "umt.h"
#include "uncil.h"
#include "uosdef.h"
#include "uvlq.h"

const char *endianness[3] = { "other", "little", "big" };

void uncil_printversion(void) {
    printf("Uncil version %s\n", UNCIL_VER_STRING);
    printf("\tByte code version  \t%u\n", UNCIL_PROGRAM_VER);
    printf("\tTarget platform    \t%s\n", UNCIL_TARGET);
    printf("\tTarget architecture\t%s\n", UNCIL_CPU_ARCH);
    printf("\tEndianness         \t%s\n", endianness[unc0_getendianness()]);
    printf("\tCompiled with      \t%s\n", UNCIL_COMPILED_WITH);
    printf("\tCompiled on        \t%s\n", __DATE__ ", " __TIME__);
    printf("\tHas multithreading \t%s\n",
#if UNCIL_MT_OK
                                        "yes"
#else
                                        "no"
#endif
                                        );
#if UNCIL_MT_OK
    printf("\tThreading backend  \t%s\n", UNCIL_MT_PROVIDER);
#endif
    printf("\tDebug build        \t%s\n",
#ifndef NDEBUG
                                        "yes"
#else
                                        "no"
#endif
                                        );
}
