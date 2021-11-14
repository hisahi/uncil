
# Building documentation

Documentation for building Uncil.

Uncil comes with a Makefile that automatically compiles both `uncil` and
`uncilc`. The minimum requirements are a compiler that meets the C89
(ANSI C, ISO/IEC 9899:1990) standard and which meets stricter external
identifier requirements (at least 31 significant initial characters).
Uncil does not currently support being compiled as C++.
The Makefile that is included should work on most GNU Make versions and
compatible programs.

The `config.inc` file can be used to provide extra flags for the build.
It comes with builtin configurations and a set of flags that can be used to
toggle between them.

## Architectures

Uncil currently requires the following in order to be built for an
architecture (in addition to the requirements to run C89-compatible code):

* that `char` is an 8-bit type (`CHAR_BIT == 8`)
* the use of two's complement representation for signed integers
* the use of IEEE 754 compatible floating-point formats
* that the target machine uses an ASCII-compatible character encoding

These are sufficient to compile and run a single-threaded version of Uncil.

The Uncil source code file `uosdef.h` contains definitions specific to
certain architectures and/or operating systems.

## Multithreading

Uncil has some form of multithreading support, but support for both atomics
as well as a library for synchronization primitives (such as locks) is required.

For atomics, the following options are currently supported:
* standard C11 atomics
* GNU C atomics with `__atomic` builtins

For locks etc., the following options are currently supported:
* standard C11 `<threads.h>`
  * on many platforms, this is a wrapper for pthread. in such a case, it
    only needs to be linked in and `UNCIL_LIB_PTHREAD` need not be defined
* POSIX Threads (pthread)

Multithreading primitives are defined in `umt.h`.

## Libraries

`config.inc` can be used to add libraries. The version that comes with the
Uncil source code distribution includes some examples which are supported
by Uncil:

* pthread (for multithreading)
  define `UNCIL_LIB_PTHREAD`
* ICU (International Components for Unicode, for the `unicode` module)
  define `UNCIL_LIB_ICU`
* PCRE2 (for the `regex` module)
  define `UNCIL_LIB_PCRE2`
* GNU readline (for improved `input()` and REPL input capabilities)
  define `UNCIL_LIB_READLINE`
  (note that readline is by default only used with `uncil`; it is not used
   by default when Uncil is embedded into other programs)

## Platforms

Uncil has very few platform dependencies besides those listed above in the
Architectures section. Some modules may however have platform-dependent
implementations.

## Modules

The following modules have dependencies:

* While Uncil itself is not platform-dependent, parts of the standard
  library have platform-specific functionality. These include, but are not
  limited to, the Time, Process, IO, FS and Thread modules.
* Regex
  * Requires PCRE2, but there are plans to implement a custom fallback library.
* Unicode
  * Requires ICU (International Components for Unicode), but there are plans
    to implement a custom library that only needs a specially formatted blob
    of data that could be generated from the UCD (Unicode
    Character Database). If not available, the `unicode` module will
    be a stub that does not support any of the function calls.

