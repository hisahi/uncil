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

static const char *endianness[3] = { "other", "little", "big" };

#define FIELDWIDTH 24
#define UNCVER_PROP(name, spec, value) printf(                                 \
                "\t%-"UNCIL_STRINGIFY(FIELDWIDTH)"s\t"spec"\n", name, value);

#define YES "yes"
#define NO "no"
#define YESNO(x) ((x) ? YES : NO)

#if NDEBUG
#define IS_DEBUG_BUILD 0
#else
#define IS_DEBUG_BUILD 1
#endif

#if UNCIL_C23
#define UNCIL_CSTD "C23"
#elif UNCIL_C11
#define UNCIL_CSTD "C11"
#elif UNCIL_C99
#define UNCIL_CSTD "C99"
#else
#define UNCIL_CSTD "C89"
#endif

#if !UNCIL_MT_OK
#undef UNCIL_MT_PROVIDER
#define UNCIL_MT_PROVIDER "(none, single-threaded build)"
#endif

const char UNCIL_COPYRIGHT[] = "(C) 2021-2023 hisahi & Uncil Team";

#if __clang__
const char UNCIL_COMPILED_WITH[] = "Clang " __clang_version__;
#elif __GNUC__
const char UNCIL_COMPILED_WITH[] = "gcc " __VERSION__;
#elif defined(__INTEL_COMPILER)
#if 0
#if __INTEL_COMPILER >= 2000
#define UNCIL_COMPILED_WITH "Intel C/C++ "                                     \
              UNCIL_STRINGIFY(__INTEL_COMPILER)                                \
          "." UNCIL_STRINGIFY(__INTEL_COMPILER_UPDATE) ".*"
#elif defined(__INTEL_COMPILER_UPDATE)
#define UNCIL_COMPILED_WITH "Intel C/C++ "                                     \
              UNCIL_STRINGIFY(__INTEL_COMPILER / 100)                          \
          "." UNCIL_STRINGIFY(__INTEL_COMPILER % 100)                          \
          "." UNCIL_STRINGIFY(__INTEL_COMPILER_UPDATE)
#else
#define UNCIL_COMPILED_WITH "Intel C/C++ "                                     \
              UNCIL_STRINGIFY(__INTEL_COMPILER / 100)                          \
          "." UNCIL_STRINGIFY(__INTEL_COMPILER % 100)
#endif
#endif
const char UNCIL_COMPILED_WITH[] = "Intel C/C++";
#elif defined(_MSC_VER)
#if 0
#ifdef _MSC_FULL_VER
#define UNCIL_VER_MSVC_MAJOR _MSC_FULL_VER / 10000000
#define UNCIL_VER_MSVC_MINOR (_MSC_FULL_VER / 100000) % 100
#define UNCIL_VER_MSVC_REVISION _MSC_FULL_VER % 100000

#define UNCIL_VER_MSVC_MAJOR_STR UNCIL_STRINGIFY(UNCIL_VER_MSVC_MAJOR)
#if UNCIL_VER_MSVC_MINOR < 10
#define UNCIL_VER_MSVC_MINOR_STR "0" UNCIL_STRINGIFY(UNCIL_VER_MSVC_MINOR)
#else
#define UNCIL_VER_MSVC_MINOR_STR UNCIL_STRINGIFY(UNCIL_VER_MSVC_MINOR)
#endif
#define UNCIL_VER_MSVC_REVISION_STR UNCIL_STRINGIFY(UNCIL_VER_MSVC_REVISION)

#define UNCIL_STRINGIFY_PAD2(s) ((s) < 10 ? ("0" UNCIL_STRINGIFY(s))           \
                                          : UNCIL_STRINGIFY(s))
#define UNCIL_COMPILED_WITH "Microsoft C Compiler "                            \
              UNCIL_VER_MSVC_MAJOR_STR                                         \
          "." UNCIL_VER_MSVC_MINOR_STR                                         \
          "." UNCIL_VER_MSVC_REVISION_STR ".*"
#elif defined(_MSC_FULL_VER)
#define UNCIL_VER_MSVC_MAJOR _MSC_FULL_VER / 10000000
#define UNCIL_VER_MSVC_REVISION _MSC_FULL_VER % 100000
#define UNCIL_VER_MSVC_MINOR (_MSC_FULL_VER / 100000) % 100

#define UNCIL_VER_MSVC_MAJOR_STR UNCIL_STRINGIFY(UNCIL_VER_MSVC_MAJOR)
#define UNCIL_VER_MSVC_MINOR_STR UNCIL_STRINGIFY(UNCIL_VER_MSVC_MINOR)
#define UNCIL_VER_MSVC_REVISION_STR UNCIL_STRINGIFY(UNCIL_VER_MSVC_REVISION)

#define UNCIL_COMPILED_WITH "Microsoft C Compiler "                            \
              UNCIL_VER_MSVC_MAJOR_STR "." UNCIL_VER_MSVC_MINOR_STR            \                                       \
          "." UNCIL_VER_MSVC_REVISION_STR
#else
#define UNCIL_VER_MSVC_MAJOR _MSC_VER / 100
#define UNCIL_VER_MSVC_MINOR _MSC_VER % 100
#define UNCIL_VER_MSVC_MAJOR_STR UNCIL_STRINGIFY(UNCIL_VER_MSVC_MAJOR)
#define UNCIL_VER_MSVC_MINOR_STR UNCIL_STRINGIFY(UNCIL_VER_MSVC_MINOR)
#define UNCIL_COMPILED_WITH "Microsoft C Compiler "                            \
              UNCIL_VER_MSVC_MAJOR_STR "." UNCIL_VER_MSVC_MINOR_STR
#endif
#endif
const char UNCIL_COMPILED_WITH[] = "Microsoft C Compiler";
#elif defined(__IAR_SYSTEMS_ICC__)
#if 0
#define UNCIL_VER_ICC_MAJOR __VER__ / 100
#define UNCIL_VER_ICC_MINOR __VER__ % 100
#define UNCIL_VER_ICC_MAJOR_STR UNCIL_STRINGIFY(UNCIL_VER_ICC_MAJOR)
#define UNCIL_VER_ICC_MINOR_STR UNCIL_STRINGIFY(UNCIL_VER_ICC_MINOR)
#define UNCIL_COMPILED_WITH "IAR C/C++ "                                       \
              UNCIL_VER_ICC_MAJOR_STR "." UNCIL_VER_ICC_MINOR_STR
#endif
const char UNCIL_COMPILED_WITH[] = "IAR C/C++";
#elif defined(__TINYC__)
const char UNCIL_COMPILED_WITH[] = "Tiny C Compiler";
#else
const char UNCIL_COMPILED_WITH[] = "(unknown standard compiler)";
#endif

static void uncil_printlibraries(void) {
    unsigned col = 0, nl = 0;
#define MAX_COLUMNS 40
#define PRINT_LIB(flag)                                                        \
        if (UNCIL_LIB_##flag) {                                                \
            if (nl && col + sizeof(UNCIL_STRINGIFY(flag)) >= MAX_COLUMNS) {    \
                col = 0;                                                       \
                printf( "\n\t%-"UNCIL_STRINGIFY(FIELDWIDTH)"s\t", "");         \
            }                                                                  \
            printf("%s ", UNCIL_STRINGIFY(flag));                              \
            col += sizeof(UNCIL_STRINGIFY(flag));                              \
            nl = 1;                                                            \
        }

    printf("\t%-"UNCIL_STRINGIFY(FIELDWIDTH)"s\t", "Libraries");

    PRINT_LIB(PTHREAD);
    PRINT_LIB(PCRE2);
    PRINT_LIB(ICU);
    PRINT_LIB(READLINE);
    PRINT_LIB(JEMALLOC);
    PRINT_LIB(TCMALLOC);
    PRINT_LIB(MIMALLOC);

    putchar('\n');
}

void uncil_printversion(int detail) {
    printf("Uncil %s\n", UNCIL_VER_STRING);
    UNCVER_PROP("Copyright", "%s", UNCIL_COPYRIGHT);
    if (detail >= 1) {
        UNCVER_PROP("Byte code version", "%u", UNCIL_PROGRAM_VER);
        UNCVER_PROP("Target platform", "%s", UNCIL_TARGET);
        UNCVER_PROP("Target architecture", "%s", UNCIL_CPU_ARCH);
        UNCVER_PROP("Endianness", "%s", endianness[unc0_getendianness()]);
        UNCVER_PROP("Compiled with", "%s", UNCIL_COMPILED_WITH);
        UNCVER_PROP("Compiled on", "%s", __DATE__ ", " __TIME__);
        UNCVER_PROP("C standard level", "%s", UNCIL_CSTD);
        UNCVER_PROP("Threading backend", "%s", UNCIL_MT_PROVIDER);
        UNCVER_PROP("Debug build", "%s", YESNO(IS_DEBUG_BUILD));
        uncil_printlibraries();
    }
}
