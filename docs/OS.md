
# Operating system functionality library

Documentation for the builtin operating system functionality library that is
used to access functionality specific to the operating system or platform that
code is executing on.

This module can usually be accessed with `require("os")`.

This module is disabled by default when Uncil is used as a library, but
available on the standalone interpreter; see `mmask` in `API.md`.

## os.difftime
`os.difftime(t1, t0)`

Returns the difference in seconds (as a floating-point number) between the
two points in time `t1` and `t0` which should be values obtained from
`os.time()`.

## os.freemem
`os.freemem()`

Returns the approximate number of free memory on the system, or
`null` if not available.

## os.getenv
`os.getenv([env])`

If `env` is not `null`, gets the value of the environment variable `env`
(string),.Returns the value as a string, or `null` if no such environment
variable was found.

If `env` is `null` or not given, returns all environment variables as a table.

## os.nprocs
`os.nprocs()`

Returns the number of logical processors on the current system,
or `null` if the information is not available.

## os.system
`os.system([cmd])`

This is a wrapper around the C standard library call `system()`.

If `cmd` is `null` (or not given), return `true` if `os.system` supports
running commands and `false` if not.

If `cmd` is a string, runs it as a command and returns the exit code. The
meaning of the exit code is platform-dependent.

## os.time
`os.time()`

Returns the time in a platform-specific format. The timestamp is only
guaranteed to give meaningful results given to `os.difftime`;
compare `time.time()` (see Time).

## os.version
`os.version([info])`

Returns information about the operating system. If `info` is not given
or is `null`, this will return the same value as `sys.platform`.
Otherwise, `info` may be one of the following strings, which will return
different types of information:

* `"host"`: returns the hostname or nodename, or `null` not available.
* `"major"`: returns the major version of the operating system,
  or `null` not available. on *nix operating systems, this will return
  the major kernel version.
* `"minor"`: returns the minor version of the operating system,
  or `null` not available. on *nix operating systems, this will return
  the minor kernel version.
* `"name"`: returns a human-readable string describing the operating
  system and its version, or `null` if none is available.
* `"posix"`: `true` if the platform is determined to be POSIX-compatible
  or `false` otherwise.
