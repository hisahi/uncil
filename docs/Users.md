
# User documentation

Users' documentation for the Uncil interpreter.

## uncil

`uncil` is the main Uncil interpreter. It can either be executed with a script
as the first numbered command-line parameter, or if none are given, it enters
a REPL (read-eval-print-loop) which can be used as an interactive shell that
runs Uncil code and automatically prints results into the console.

If a file is executed with the `-i` command-line flag specified, the script
is executed and an interactive shell will be provided afterwards. It is thus
somewhat equivalent to pasting an entire script into the console (except that
errors cause it to stop executing).

`uncil` can also run precompiled `.cnu` files (see `uncilc`).

If the script defines a public function called `main`, it will be called with
a single parameter; the list of command-line arguments given to the script,
including the first element containing the script name. If `main` returns
a value, it will be passed to `exit()` (see Library), except with `-i` where
the return value is displayed to the user.

## uncilc

`uncilc` is used to convert Uncil scripts from source code `.unc` into a
precompiled byte code file (`.cnu`). These byte codes can then be executed
later with `uncil` without having to parse the source code file again.

`.cnu` files are only guaranteed to be executable on the same architecture
as they were compiled on. They may also become incompatible as Uncil versions
progress and the format of these files changes. There will be an attempt to
offer some form of backwards compatibility so that older `.cnu` files can
be run on newer versions of the interpreter, but this is _not_ guaranteed.
