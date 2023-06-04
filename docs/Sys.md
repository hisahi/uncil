
# Uncil system library

Documentation for the builtin system library that is used to access
internal details in the Uncil interpreter.

This module can usually be accessed with `require("sys")`.

This module is disabled by default when Uncil is used as a library, but
available on the standalone interpreter; see `mmask` in `API.md`. This module
is not available if Uncil is compiled in sandboxed mode.

## sys.arch
`sys.arch`

A string value that identifies the CPU architecture Uncil was compiled for.
Possible values are:
* `"x86"` (32-bit Intel x86 or compatible)
* `"amd64"` (AMD64/x86_64 or compatible)
* `"arm"` (ARM)
* `"aarch64"` (ARM64/AArch64)
* `"riscv"` (RISC-V)
* `"ppc"` (32-bit PowerPC)
* `"ppc64"` (64-bit PowerPC)
* `"ia64"` (Intel Itanium)
* `"mips"` (MIPS)
* `"mipsel"` (MIPSel)
* `"s390"` (IBM S/390)
* `"sh4"` (Hitachi Super-H, SH-4)
* `"sparc"` (SPARC)
* `"sparc64"` (SPARC64)
* `"alpha"` (DEC Alpha)
* `"m68k"` (Motorola 68000)
* `"8086"` (16-bit 8086 real mode)
* `"other"` (also used if the actual architecture could not be recognized)

Some of these values may be purely theoretical.

## sys.canloaddl
`sys.canloaddl`

Whether `sys.loaddl` is supported on the platform. If false, `sys.loaddl` will
always fail.

## sys.compiler
`sys.compiler`

A textual representation of the compiler that was used to compile the Uncil
interpreter, if recognized.

## sys.compiletime
`sys.compiletime`

A representation of the date and time when the Uncil interpreter was compiled.

## sys.dlpath
`sys.dlpath`

An array of directories that will be searched when looking for a dynamic-link
library upon a call to `sys.dlopen()`. See Modules for more information.

## sys.endian
`sys.endian`

Returns the current endianness detected from the system; `little`, `big`
or `other`.

## sys.forget
`sys.forget(name)`

Removes the module called `name` from the cache. Returns `true` if the module
was found from the cache and successfully removed, or else `false`.

## sys.loaddl
`sys.loaddl(filename, [callname])`

Loads a dynamic-link library called `filename` (which must be a string that
includes any possible file extensions) and calls its symbol `callname`, which
is assumed to be a Uncil-compatible entry point. For more information,
see Modules.

Returns an object containing the public members defined by the Uncil-compatible
entry point.

## sys.path
`sys.path`

An array of directories that will be searched when looking for a module
upon a call to `require()`. See Modules for more information.

## sys.platform
`sys.platform`

A string value that identifies the platform Uncil was compiled for.
Possible values are:
* `"windows"` (Microsoft Windows)
* `"macos"` (Apple macOS)
* `"linux"` (Linux)
* `"bsd"` (BSD)
* `"openbsd"` (OpenBSD)
* `"freebsd"` (FreeBSD)
* `"netbsd"` (NetBSD)
* `"dragonflybsd"` (Dragonfly BSD)
* `"ios"` (Apple iOS)
* `"android"` (Android)
* `"sunos"` (SunOS)
* `"aix"` (IBM AIX)
* `"unix"` (some other Unix-compatible system)
* `"posix"` (some other POSIX-compatible system)
* `"msdos"` (MS-DOS)
* `"other"` (also used if the actual platform could not be recognized)

## sys.threader
`sys.threader`

Returns the name of the current multithreading library, or `null` if none of
them were compiled in.

## sys.version
`sys.version()`

A function that returns three integers that represent the version of the Uncil
interpreter; the major version, minor version and the patch version.
Same as `getversion`.

## sys.versiontext
`sys.versiontext`

A textual representation of the version of the Uncil interpreter.
