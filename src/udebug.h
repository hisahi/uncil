/*******************************************************************************
 
Uncil -- debug definitions (for use inside Uncil only)

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

#ifndef UNCIL_UDEBUG_H
#define UNCIL_UDEBUG_H

#include <assert.h>

#include "uerr.h"

#ifndef NDEBUG
#define DEBUGPRINTENABLE 1
#endif

#define DEBUGPRINT_ALLOC 0
#define DEBUGPRINT_PARSE0 0
#define DEBUGPRINT_PARSE1 0
#define DEBUGPRINT_INSTRS 0
#define DEBUGPRINT_PUBLIC 0
#define DEBUGPRINT_REFS 0
#define DEBUGPRINT_GC 0
#define DEBUGPRINT_CORO 0

#if DEBUGPRINTENABLE
#include <stdio.h>
#define DEBUGPRINT(flag, c) (void)(DEBUGPRINT_##flag && printf c \
                                        && fflush(stdout))
#else
#define DEBUGPRINT(flag, c)
#endif

#if NDEBUG
#define BREAKPOINT()
#else
#include <signal.h>
#define BREAKPOINT() raise(SIGTRAP)
void uncil__hexdump(const unsigned char *data, size_t n);
#endif

#define ASSERT assert
#define NEVER_() ASSERT(0)
#define NEVER() do { NEVER_(); return UNCIL_ERR_INTERNAL; } while (0)

#endif /* UNCIL_UDEBUG_H */
