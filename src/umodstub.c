/*******************************************************************************
 
Uncil -- module stubs

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

/* this file can be used to stub out builtin modules. */

#define UNCIL_DEFINES

#include "udef.h"
#include "uncil.h"

#define UNC_STUB_MODULE0(mod)
#define UNC_STUB_MODULE1(mod)                                                  \
    Unc_RetVal uncilmain_## mod ##(struct Unc_View *w) {                       \
        return UNCIL_ERR_ARG_MODULENOTFOUND;                                   \
    }

#define UNC_STUB_MODULE_ENABLE(flag, mod) UNC_STUB_MODULE##flag(mod)

UNC_STUB_MODULE_ENABLE(0, cbor)
UNC_STUB_MODULE_ENABLE(0, convert)
UNC_STUB_MODULE_ENABLE(0, coroutine)
UNC_STUB_MODULE_ENABLE(0, fs)
UNC_STUB_MODULE_ENABLE(0, gc)
UNC_STUB_MODULE_ENABLE(0, io)
UNC_STUB_MODULE_ENABLE(0, json)
UNC_STUB_MODULE_ENABLE(0, math)
UNC_STUB_MODULE_ENABLE(0, os)
UNC_STUB_MODULE_ENABLE(0, process)
UNC_STUB_MODULE_ENABLE(0, random)
UNC_STUB_MODULE_ENABLE(0, regex)
UNC_STUB_MODULE_ENABLE(0, sys)
UNC_STUB_MODULE_ENABLE(0, thread)
UNC_STUB_MODULE_ENABLE(0, time)
UNC_STUB_MODULE_ENABLE(0, unicode)
