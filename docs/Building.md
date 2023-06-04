
# Building documentation

Documentation for building Uncil.

Uncil comes with a Makefile that automatically compiles an interpreter `uncil`
and byte-code compiler `uncilc`. The minimum requirements are a compiler that
meets the C89 (ANSI C, ISO/IEC 9899:1990) standard and which meets stricter
external identifier requirements (at least 31 significant initial characters).
Uncil does not currently support being compiled as C++, but `uncil.h`
has an automatic `extern "C"` for the external API when compiling C++.
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
  * this precludes "fast math" optimizations!
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
* jemalloc / TCMalloc / mimalloc
  define **only one** of `UNCIL_LIB_JEMALLOC`, `UNCIL_LIB_TCMALLOC`,
  `UNCIL_LIB_MIMALLOC`.

## Platforms

Uncil has very few platform dependencies besides those listed above in the
Architectures section. Some modules may however have platform-dependent
implementations for their functionality.

In particular, functionality in the modules may be platform-dependent;
see below.

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

## Sandboxing

Define `UNCIL_SANDBOXED` to enable a sandboxed mode. The following features
are disabled automatically:

* The modules `fs`, `gc`, `io`, `os`, `process`, `sys` and `thread`.
  * The files may still be compiled, but the modules cannot be accessed.
    The corresponding `ulib*.c` can be safely left out when compiling
    a sandboxed version.
* I/O functionality in the modules `cbor` and `json`.
* The builtins `print` and `input`.
* Printing of memory addresses.
* `@` and `=` in Convert encode/decode; both are mapped to `<` instead.

The standalone interpreter nor compiler cannot be compiled in sandboxed mode.

## Freestanding mode

Some effort has been put into making Uncil work in freestanding mode without
a hosted C environment. However, this is not yet completely possible, although
a compilation flag `UNCIL_NOLIBC` exists.

The following hosted C headers are needed by each Uncil source file:
* `uarithm.c`
  * `math.h` (various functions)
* `uimpl.c`
  * `stdio.h` for `FILE *` support
* `umem.c`
  * `stdlib.h` (default allocator: `realloc`, `free`)
    * can be dsiabled with `UNCIL_NOSTDALLOC`. however, doing so means
      `unc_create`(`ex`) requires that an allocator be provided
* `umodule.c`
  * `stdio.h` (`fopen`, `fclose`; for checking whether a file can be found)
* `usort.c`
  * `setjmp.h`
* `uvm.c`
  * `setjmp.h`
* `uvsio.c`:
  * `string.h` (`strerror`)
* `ulib.c` (standard Uncil library)
  * `stdio.h` (various; for `input`/`print`)
* `ulibfs.c` (Uncil `fs`)
  * `stdio.h` (various)
  * `errno.h` (to handle errors from I/O functions)
* `ulibio.c` (Uncil `io`)
  * `stdio.h` (various)
  * `errno.h` (to handle errors from I/O functions)
* `ulibmath.c` (Uncil `math`)
  * `math.h` (various)
  * `errno.h` (to handle errors from math functions)
* `ulibos.c` (Uncil `os`)
  * `stdlib.h` (`getenv`, `system`)
  * `time.h` (`time`, `difftime`)
* `ulibproc.c` (Uncil `process`)
  * `stdio.h` (various)
* `ulibrand.c` (Uncil `random`)
  * `stdlib.h` (`rand`)
  * `time.h` (`time`)
* `ulibsys.c` (Uncil `sys`)
  * `stdlib.h` (`getenv`, `system`)
  * `time.h` (`time`, `difftime`)
* `ulibthrd.c` (Uncil `thread`)
  * `time.h` (`time`, `difftime`)
* `ulibtime.c` (Uncil `time`)
  * `time.h` (various)

Another way to look at this is to see which standard C headers are needed:
* `errno.h`: `ulibfs.c` (`fs`), `ulibio.c` (`io`), `ulibmath.c` (`math`)
* `math.h`: `uarithm.c`, `ulibmath.c` (`math`)
* `setjmp.h`: `usort.c`, `uvm.c`
* `stdio.h`: `uimpl.c`, `umodule.c`, `ulib.c`, `ulibfs.c` (`fs`),
  `ulibio.c` (`io`), `ulibproc.c` (`process`)
* `stdlib.h`: `umem.c`, `ulibos.c` (`os`), `ulibrand.c` (`random`),
  `ulibsys.c` (`sys`)
* `string.h`: `uvsio.c`
* `time.h`: `ulibos.c` (`os`), `ulibrand.c` (`random`),
  `ulibsys.c` (`sys`), `ulibthrd.c` (`thread`), `ulibtime.c` (`time`)

The standalone Uncil interpreter and compiler binaries need
`errno.h`, `stdio.h`, `stdlib.h` and `string.h`.

In debug mode, `assert.h`, `signal.h` and `stdio.h` are also needed.


