
# Module system documentation

The documentation for the module system Uncil uses and its conventions.

## Uncil modules

Uncil modules can be included by using the standard library function
`require()`, which takes a single string parameter. The function will return
an object that represents the new public variables defined by the module,
and it is cached so that the string given is used as the key.

A common idiom for loading a library is to assign it to a variable of the
same name. For example:
```
math = require("math")
```

The string should represent a file name without an extension. Any `/` 
characters in the string are automatically converted into the correct
directory separator symbol for the system, allowing "submodules".

Uncil tries to search modules in a few different ways. These are what it tries
in order:
* Going through all directories listed in an internal list of directories to
  search through (accessible as `sys.path`). The `uncil` interpreter, when
  executed, consults the environment variable `UNCILPATH`, the format for which
  is the same as the `PATH` environment variable on the system, and parses it
  to add all of the directories referenced into that list.
  * For every directory, the interpreter will join the given module name
    and `.unc` to try to find a source-code module to run. If it fails, it
    tries the same but with the file extension `.cnu` to find a precompiled
    byte code library. If both fail, Uncil checks if the search directory has
    a subdirectory of the specified name. If so, it tries to import a file
    called `_init.unc` (or `.cnu`) from within it. If this too fails, the
    directory is considered to not contain the module and the search continues.
  * By default, `sys.path` always contains an empty string as the first
    directory, which effectively searches files in the current
    working directory.
* Finding the standard library with the given name:
  * `"cbor"` => the CBOR library (see CBOR)
  * `"convert"` => the standard binary conversion library (see Convert)
  * `"coroutine"` => the coroutine library (see Coroutine)
  * `"fs"` => the file system library (see FS)
  * `"gc"` => the garbage collection control library (see GC)
  * `"io"` => the standard I/O library (see IO)
  * `"json"` => the JSON library (see JSON)
  * `"math"` => the standard math library (see Math)
  * `"os"` => the OS functionality access library (see OS)
  * `"process"` => the process handling library (see Process)
  * `"random"` => the standard randomization library (see Random)
  * `"regex"` => the regular expression library (see Regex)
  * `"sys"` => the library for accessing interpreter and system info (see Sys)
  * `"thread"` => the thread library (see Thread)
  * `"time"` => the date/time library (see Time)
  * `"unicode"` => the Unicode library (see Unicode)

The exception is when the string given to require begins with a `.` or `..`
directory. In such a case, the import is considered to be a _relative import_,
and only the directory in which the script being executed is located will be
searched (or if not known, the current working directory).

## C modules

The Uncil interpreter is written in C and is capable of importing other
libraries written in C as long as the operating system or platform provides
the means to do so. C modules cannot be imported directly through `require`,
but rather through `sys.loaddl`.

C modules are searched for much in the similar way as Uncil modules, but
the list of locations searched is that of `sys.dlpath`, not `sys.path`. Unlike
with `require`, no file extension is added; the script is expected to provide
the full file name, including the extension, and it can use the variables in
`sys` to determine the current operating system in order to determine the
correct file name of the module to load.

When a module file has been loaded, it is opened with the system call that the
OS provides for opening dynamic link libraries, such as `dlopen` on many
Unix-based systems. After the library has been opened, Uncil will try to
call an exported function. By default, the name is `uncilmain`, but `sys.loaddl`
also accepts a second parameter that can be used to use a different name.
The function used nevertheless should have the following signature:

```
Unc_RetVal uncilmain(struct Unc_View *w);
```

where the view is given as an `Unc_View`. Exporting functions or values is done
by trying to set public variables of the same name. `unc_exportcfunction` is
an Uncil API function that is provided for convenience for this very purpose.

If the function returns 0, the module import is considered successful; else
the module is considered to have been "found", but it caused an error, and thus
the search will _not_ continue. `uncilmain` can return an error with the code
`UNCIL_ERR_ARG_MODULENOTFOUND` to force the search to continue, but this
is not recommended.
