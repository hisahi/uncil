/*******************************************************************************
 
Uncil -- platform-specific defines

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

#ifndef UNCIL_UOSDEF_H
#define UNCIL_UOSDEF_H

#include "ucstd.h"

#if (HAVE_UNISTD_H || defined(__unix__) || defined(__unix) ||                  \
     defined(__QNX__) || (defined(__APPLE__) && defined(__MACH__)))
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include <unistd.h>
#endif

#ifdef __ANDROID__
#define UNCIL_IS_ANDROID 1
#endif

#if defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))
#if defined(_POSIX_VERSION)
#define UNCIL_IS_POSIX 1
#endif
#if defined(__unix__)
#define UNCIL_IS_UNIX 1
#include <sys/param.h>
#if defined(__linux__)
#define UNCIL_IS_LINUX 1
#elif defined(_AIX)
#define UNCIL_IS_AIX 1
#elif defined(__sun) || defined(sun)
#define UNCIL_IS_SUNOS 1
#elif defined(BSD)
#define UNCIL_IS_BSD 1
#if __OpenBSD__
#define UNCIL_IS_OPENBSD 1
#elif __FreeBSD__
#define UNCIL_IS_FREEBSD 1
#elif __NetBSD__
#define UNCIL_IS_NETBSD 1
#elif __DragonFly__
#define UNCIL_IS_DRAGONFLY_BSD 1
#endif
#endif

#elif defined(__APPLE__)
#define UNCIL_IS_APPLE 1
#if defined(_POSIX_VERSION)
#define UNCIL_IS_POSIX 1
#endif
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE
#define UNCIL_IS_IOS 1
#elif TARGET_OS_MAC
#define UNCIL_IS_MACOS 1
#endif
#endif

#define UNCIL_CPU_NOCHECK 1
#if TARGET_CPU_PPC
#define UNCIL_CPU_IS_PPC 1
#elif TARGET_CPU_PPC64
#define UNCIL_CPU_IS_PPC64 1
#elif TARGET_CPU_68K
#define UNCIL_CPU_IS_M68K 1
#elif TARGET_CPU_X86
#define UNCIL_CPU_IS_X86 1
#elif TARGET_CPU_X86_64
#define UNCIL_CPU_IS_AMD64 1
#elif TARGET_CPU_ARM
#define UNCIL_CPU_IS_ARM 1
#elif TARGET_CPU_ARM64
#define UNCIL_CPU_IS_AARCH64 1
#elif TARGET_CPU_MIPS
#define UNCIL_CPU_IS_MIPS 1
#elif TARGET_CPU_SPARC
#define UNCIL_CPU_IS_SPARC 1
#elif TARGET_CPU_ALPHA
#define UNCIL_CPU_IS_ALPHA 1
#else
#undef UNCIL_CPU_NOCHECK
#endif

#elif defined(_WIN32) || defined(__CYGWIN__)
#define UNCIL_IS_WINDOWS 1
#if defined(__CYGWIN__)
#define UNCIL_IS_CYGWIN 1
#endif

#elif defined(_POSIX_VERSION)
#define UNCIL_IS_POSIX 1

#elif defined(MSDOS) || defined(__MSDOS__)
#define UNCIL_IS_MSDOS 1
#endif

#if !UNCIL_CPU_NOCHECK
#if defined(_MSC_VER)

#if defined(_M_PPC)
#define UNCIL_CPU_IS_PPC 1
#elif defined(_M_IA64)
#define UNCIL_CPU_IS_IA64 1
#elif defined(_M_IX86)
#define UNCIL_CPU_IS_X86 1
#elif defined(_M_I86)
#define UNCIL_CPU_IS_8086 1
#elif defined(_M_AMD64)
#define UNCIL_CPU_IS_AMD64 1
#elif defined(_M_ARM64)
#define UNCIL_CPU_IS_AARCH64 1
#elif defined(_M_ARM)
#define UNCIL_CPU_IS_ARM 1
#elif defined(_M_ALPHA)
#define UNCIL_CPU_IS_ALPHA 1
#endif

#elif defined(__INTEL_COMPILER)
#if defined(__x86_64__)
#define UNCIL_CPU_IS_AMD64 1
#elif defined(__i386__)
#define UNCIL_CPU_IS_X86 1
#endif

#elif defined(__GNUC__) || defined(__clang__)
#if defined(__amd64__) || defined(__x86_64__)
#define UNCIL_CPU_IS_AMD64 1
#elif defined(__i386__)
#define UNCIL_CPU_IS_X86 1
#elif defined(__ia64__)
#define UNCIL_CPU_IS_IA64 1
#elif defined(__aarch64__)
#define UNCIL_CPU_IS_AARCH64 1
#elif defined(__arm__)
#define UNCIL_CPU_IS_ARM 1
#elif defined(__powerpc64__)
#define UNCIL_CPU_IS_PPC64 1
#elif defined(__powerpc__)
#define UNCIL_CPU_IS_PPC 1
#elif defined(__alpha__)
#define UNCIL_CPU_IS_ALPHA 1
#elif defined(__m68k__)
#define UNCIL_CPU_IS_M68K 1
#elif defined(__mips__) && _MIPSEL
#define UNCIL_CPU_IS_MIPSEL 1
#elif defined(__mips__)
#define UNCIL_CPU_IS_MIPS 1
#elif defined(__sparc__) && __arch64__
#define UNCIL_CPU_IS_SPARC64 1
#elif defined(__sparc__)
#define UNCIL_CPU_IS_SPARC 1
#elif defined(__riscv)
#define UNCIL_CPU_IS_RISCV 1
#elif defined(__s390__) || defined(__s390x__)
#define UNCIL_CPU_IS_S390 1
#elif defined(__SH4__)
#define UNCIL_CPU_IS_SH4 1
#endif

#endif
#endif

#ifdef DIRSEP
#define UNCIL_DIRSEP DIRSEP
#else
#if UNCIL_IS_WINDOWS
#define UNCIL_DIRSEP '\\'
#else
#define UNCIL_DIRSEP '/'
#endif
#endif

#ifdef PATHSEP
#define UNCIL_PATHSEP PATHSEP
#else
#if UNCIL_IS_WINDOWS
#define UNCIL_PATHSEP ';'
#else
#define UNCIL_PATHSEP ':'
#endif
#endif

#if UNCIL_CPU_IS_PPC64 | UNCIL_CPU_IS_AARCH64 |                                  \
    UNCIL_CPU_IS_IA64 | UNCIL_CPU_IS_AMD64
#define UNCIL_64BIT 1
#endif

#if UNCIL_IS_WINDOWS
#define UNCIL_TARGET "windows"
#elif UNCIL_IS_ANDROID
#define UNCIL_TARGET "android"
#elif UNCIL_IS_LINUX
#define UNCIL_TARGET "linux"
#elif UNCIL_IS_OPENBSD
#define UNCIL_TARGET "openbsd"
#elif UNCIL_IS_FREEBSD
#define UNCIL_TARGET "freebsd"
#elif UNCIL_IS_NETBSD
#define UNCIL_TARGET "netbsd"
#elif UNCIL_IS_DRAGONFLY_BSD
#define UNCIL_TARGET "dragonflybsd"
#elif UNCIL_IS_BSD
#define UNCIL_TARGET "bsd"
#elif UNCIL_IS_IOS
#define UNCIL_TARGET "ios"
#elif UNCIL_IS_MACOS
#define UNCIL_TARGET "macos"
#elif UNCIL_IS_SUNOS
#define UNCIL_TARGET "sunos"
#elif UNCIL_IS_AIX
#define UNCIL_TARGET "aix"
#elif UNCIL_IS_UNIX
#define UNCIL_TARGET "unix"
#elif UNCIL_IS_POSIX
#define UNCIL_TARGET "posix"
#elif UNCIL_IS_MSDOS
#define UNCIL_TARGET "msdos"
#else
#define UNCIL_TARGET "other"
#endif

#if UNCIL_CPU_IS_PPC64
#define UNCIL_CPU_ARCH "ppc64"
#elif UNCIL_CPU_IS_PPC
#define UNCIL_CPU_ARCH "ppc"
#elif UNCIL_CPU_IS_M68K
#define UNCIL_CPU_ARCH "m68k"
#elif UNCIL_CPU_IS_IA64
#define UNCIL_CPU_ARCH "ia64"
#elif UNCIL_CPU_IS_X86
#define UNCIL_CPU_ARCH "x86"
#elif UNCIL_CPU_IS_8086
#define UNCIL_CPU_ARCH "8086"
#elif UNCIL_CPU_IS_AMD64
#define UNCIL_CPU_ARCH "amd64"
#elif UNCIL_CPU_IS_AARCH64
#define UNCIL_CPU_ARCH "aarch64"
#elif UNCIL_CPU_IS_ARM
#define UNCIL_CPU_ARCH "arm"
#elif UNCIL_CPU_IS_RISCV
#define UNCIL_CPU_ARCH "riscv"
#elif UNCIL_CPU_IS_MIPSEL
#define UNCIL_CPU_ARCH "mipsel"
#elif UNCIL_CPU_IS_MIPS
#define UNCIL_CPU_ARCH "mips"
#elif UNCIL_CPU_IS_SPARC64
#define UNCIL_CPU_ARCH "sparc64"
#elif UNCIL_CPU_IS_SPARC
#define UNCIL_CPU_ARCH "sparc"
#elif UNCIL_CPU_IS_ALPHA
#define UNCIL_CPU_ARCH "alpha"
#elif UNCIL_CPU_IS_S390
#define UNCIL_CPU_ARCH "s390"
#elif UNCIL_CPU_IS_SH4
#define UNCIL_CPU_ARCH "sh4"
#else
#define UNCIL_CPU_ARCH "other"
#endif

#if UNCIL_LIB_PTHREAD
#define UNCIL_MT_PROVIDER "pthread"
#define UNCIL_MT_PTHREAD 1
#define UNCIL_MT_OK 1
#elif UNCIL_C11 && !__STDC_NO_ATOMICS__ && !__STDC_NO_THREADS__
#define UNCIL_MT_PROVIDER "C11"
#define UNCIL_MT_C11 1
#define UNCIL_MT_OK 1
#else
#define UNCIL_MT_OK 0
#endif

#ifdef UNCIL_SINGLETHREADED
#undef UNCIL_MT_OK
#define UNCIL_MT_OK 0
#endif

#if UNCIL_IS_WINDOWS
#define UNCIL_CONVERT_CRLF 1
#endif

/* add new requirec impls here and implement in umodule.c */

#ifndef HAVE_DLOPEN
#if UNCIL_IS_LINUX || UNCIL_IS_BSD
#define HAVE_DLOPEN 1
#endif
#endif

#if HAVE_DLOPEN
#define UNCIL_REQUIREC_IMPL 1
#define UNCIL_REQUIREC_IMPL_DLOPEN 1
#endif

#endif /* UNCIL_UOSDEF_H */
