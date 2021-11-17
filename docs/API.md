# C API

This document explains the C programming interface (API) of Uncil.

The Uncil API can be included with `uncil.h` (note that the other header
files are also required, but should be not included directly).

## API/ABI stability

The Uncil major version is incremented every time there are breaking API changes
and the minor version is incremented every time there are non-breaking API
changes or (breaking) ABI changes.

ABI stability is only guaranteed for the public API.

## Types

Most types used by the API should be treated as opaque objects and you will
only be passing pointers to them rather than accessing them directly. There
are some exceptions:

* `Unc_Tuple`, which is well defined and public
```
typedef struct Unc_Tuple {
    Unc_Size count;
    Unc_Value *values;
} Unc_Tuple;
```
* `Unc_RetVal` is an integral type that is non-zero if and only if there was
  an error. Equivalent to `int`.
* `Unc_Size` is an unsigned integer type (usually equivalent to `size_t`).
* `Unc_Int` is a signed integer type, usually either `long` or `long long`.
* `Unc_Float` is a floating-point type, usually `double`.
* `Unc_Byte` is `unsigned char`.
* `Unc_MMask` is `unsigned long`.
* `Unc_Value`. Normally you should only pass pointers, and accessing the fields
  of an `Unc_Value` is not supported.
  However, if a function needs "local variables", this is done by having
  a variable of type `Unc_Value`.
  Such a variable **must** be initialized, either with `UNC_BLANK` (macro) or,
  equivalently, by zero-initialization (`{0}`).
  Passing a pointer to an uninitialized `Unc_Value` to the Uncil API
  leads to undefined behavior.
* `Unc_Pile`. Normally you should only pass pointers, and accessing the fields
  of an `Unc_Pile` is not supported. You can however create local
  `Unc_Pile` variables. These need not be initialized when passing a pointer
  to `unc_newpile` or `unc_call` (but must be with the former if calling
  `unc_callex`).

Function pointer types:
`typedef void *(*Unc_Alloc)(void *udata, Unc_Alloc_Purpose purpose, size_t oldsize, size_t newsize, void *ptr);`
* `udata` is the same pointer as given to `unc_create`.
* `purpose` is an enum that represents the purpose the memory is allocated for:
  * `Unc_AllocOther`: miscellaneous, internal use or freeing
  * `Unc_AllocEntity`: for entities (objects)
  * `Unc_AllocString`: for strings
  * `Unc_AllocArray`: for arrays
  * `Unc_AllocDict`: for tables
  * `Unc_AllocObject`: for objects
  * `Unc_AllocOpaque`: for opaque objects
  * `Unc_AllocBlob`: for blobs
  * `Unc_AllocFunc`: for functions
  * `Unc_AllocExternal`: for `unc_malloc`, `unc_mrealloc`
  Note that the value of `purpose` is only advisory.
* `oldsize` is the size of the current allocation or 0 for a new memory block.
* `newsize` is the requested new size (0 means a memory block is being freed)
* `ptr` is the old pointer, or `NULL` for new allocations.
The allocator should return a new pointer that contains a memory block with
at least as many `char`s as described by `newsize`, or free the memory block
in `ptr` if `newsize` is 0. If an existing pointer is given, the memory block
should be resized such that the first `min(oldsize, newsize)` characters (bytes)
are preserved. If allocation fails, `NULL` should be returned. Allocation may
not fail if oldsize > newsize.

The allocator is assumed to be thread-safe. If it is not, it should implement
its own locking system.

`typedef Unc_RetVal (*Unc_CFunc)(Unc_View *w, Unc_Tuple args, void *udata);`
Wraps C functions for calling from Uncil code.
* `w` is the Uncil view.
* `args` is a tuple containing the arguments.
* `udata` is the pointer given as `udata` to `unc_newcfunction`
  or `unc_exportcfunction`.
The value should return an error code (such as those returned by Uncil API
calls). If everything was successful, return `0`.

`typedef Unc_RetVal (*Unc_OpaqueDestructor)(Unc_View *w, size_t n, void *data);`
Called when an opaque object is about to be destroyed. An error may be returned,
but will not cause the program to be stopped.

Other types:
* `Unc_View` represents a local Uncil environment which is connected to
  a global state or _world_ (`Unc_World`, not directly accessible).
  Different worlds are completely separate and they shall not meet.
  There are three kinds of views: (proper/main) views, nondaemon subviews
  and daemon subviews.

## Restrictions

Breaking any of these restrictions results in undefined behavior.

* A `Unc_View` may be accessed through any API calls
  (unless otherwise specified) by only one thread at a time.
* _If_ Uncil was not compiled with multithreading support,
  a global state (world) may be accessed through any API calls
  (unless otherwise specified) by only one thread at a time.
  This applies even if accessing it through different views.
  You can check for multithreading support with `unc_getversion_flags`.
* None of the pointers to `Unc_Value` given to the API may point to an
  uninitialized `Unc_Value`.
* All locked objects (blobs, arrays or opaque objects) must be unlocked
  before the function ends.
* `Unc_Value` pointers or values may not be shared between worlds.
  All `Unc_Value` pointers given to the API must be tied to the view or world
  given as an argument to that API call.

## Functions

Unless otherwise specified, functions that return `Unc_RetVal` return
0 on success.

`int unc_getversion_major(void);`
* Returns the major version of the Uncil interpreter.

`int unc_getversion_minor(void);`
* Returns the minor version of the Uncil interpreter.

`int unc_getversion_patch(void);`
* Returns the patch version of the Uncil interpreter.

`int unc_getversion_flags(void);`
* Returns the compilation flags of the Uncil interpreter.
  * `UNC_VER_FLAG_MULTITHREADING`: the interpreter was compiled with
    multithreading support.

`Unc_View *unc_create(void);`
* Creates a new Uncil world (global state), a (main) view (local state) and
  attaches the view to that world. With `unc_create`, Uncil will use the
  standard `malloc`, `realloc`, `free` functions.
* Equivalent to `unc_createex(NULL, NULL, UNC_MMASK_DEFAULT)`.
* The returned value is a valid `Unc_View` pointer, or `NULL` if
  allocation fails.
* The builtin modules available are all except the following:
  `fs`, `gc`, `io`, `os`, `process`, `sys`, `thread`,
  as these might be used by Uncil scripts to affect the system they are
  running on. If sandboxing is not required, see `unc_createex`.

`Unc_View *unc_createex(Unc_Alloc alloc, void *udata, Unc_MMask mmask);`
* Creates a new Uncil world (global state), a (main) view (local state) and
  attaches the view to that world.
* The returned value is a valid `Unc_View` pointer, or `NULL` if
  allocation fails.
* A custom allocator can be specified here (`udata` is a pointer that is given
  to the allocator every time it is called). `alloc` may also be `NULL` in
  which case Uncil will use the standard `malloc`, `realloc`, `free` functions.
* `mmask` specifies a module mask (mmask) to control the available modules.
  All modules that were compiled into the Uncil interpreter are loaded in
  regardless; the mask only controls which ones are available for
  programs to load through `require`.
* The following values are defined:
  * `UNC_MMASK_NONE`: no builtin modules available from `require`.
    (the standard library with strings, blobs, etc. is always available).
  * `UNC_MMASK_DEFAULT`: the default mask (see `unc_create`)
  * `UNC_MMASK_ALL`: all modules
  * `UNC_MMASK_M_CBOR`: `"cbor"`
  * `UNC_MMASK_M_CONVERT`: `"convert"`
  * `UNC_MMASK_M_COROUTINE`: `"coroutine"`
  * `UNC_MMASK_M_FS`: `"fs"`
  * `UNC_MMASK_M_GC`: `"gc"`
  * `UNC_MMASK_M_IO`: `"io"`
  * `UNC_MMASK_M_JSON`: `"json"`
  * `UNC_MMASK_M_MATH`: `"math"`
  * `UNC_MMASK_M_OS`: `"os"`
  * `UNC_MMASK_M_PROCESS`: `"process"` (see caveats below)
  * `UNC_MMASK_M_RANDOM`: `"random"`
  * `UNC_MMASK_M_REGEX`: `"regex"`
  * `UNC_MMASK_M_SYS`: `"sys"`
  * `UNC_MMASK_M_THREAD`: `"thread"` (see caveats below)
  * `UNC_MMASK_M_TIME`: `"time"`
  * `UNC_MMASK_M_UNICODE`: `"unicode"`
  These values can be combined with standard bitwise operations.
  * Both `process` and `thread` modules have caveats when used in programs
    that embed the Uncil interpreter.
    * `process`: the program may not handle SIGCHLD signals by waiting for
      child processes that have been started through Uncil. doing so results
      in undefined behavior.
    * `thread`: `unc_destroy` may halt indefinitely if non-daemon Uncil
      thread launched through the thread library are still running, as the
      interpreter will wait for those threads to finish before continuing.
      daemon threads will be abruptly stopped.

`Unc_View *unc_dup(Unc_View *w);`
* Creates a new (main) view tied to the same world or global state as the given
  view. The returned value is a valid `Unc_View` pointer, or `NULL` if
  allocation fails (most likely due to insufficient memory or system resources).

`Unc_View *unc_fork(Unc_View *w, int daemon);`
* Creates a new subview tied to the same world or global state as the given
  view. `daemon` specifies whether the subview is a daemon subview or not.
  The returned value is a valid `Unc_View` pointer, or `NULL` if allocation
  fails (most likely due to insufficient memory or system resources).
  Subviews are designed to be used in separate threads. Careless use of
  subviews may cause resource leaks or deadlocks; see `unc_destroy`.

`int unc_coinhabited(Unc_View *w1, Unc_View *w2);`
* Returns a non-zero value if and only if the two views share the same world
  (global state).

`void unc_halt(Unc_View *w);`
* Halts a `Unc_View`. When a view is halted, many Uncil API calls will fail
  and return the error code `UNCIL_ERR_HALT` for that view. It is
  expected that functions using the C API will return this error up the chain.

`void unc_destroy(Unc_View *w);`
* Frees a `Unc_View`. If the associated world has no more main views remaining,
  the following shall take place:
  * The `unc_destroy` waits until any nondaemon subviews have finished
    running (i.e. have been called with `unc_destroy`).
  * After this, any remaining daemon subviews will keep running and the
    world will stay in memory, but they will all be halted (as if with
    `unc_halt`) and will no longer be able to create new views, main views
    or otherwise. Programs are free to exit even if daemon subviews are still
    running; the assumption is that daemon subviews can be safely terminated
    even at a moment's notice.

`void unc_copyprogram(Unc_View *w1, Unc_View *w2);`
* Sets the program of `w1` to be that of `w2`.

`Unc_RetVal unc_compilestring(Unc_View *w, Unc_Size n, const char* text);`
* Compiles a string (with explicit length) and loads the compiled program
  as the program for the given view.
* Error codes:
  * `UNCIL_ERR_MEM`: could not allocate enough memory.
  * `UNCIL_ERR_SYNTAX`: syntax error.
    * `UNCIL_ERR_SYNTAX_UNTERMSTR`
    * `UNCIL_ERR_SYNTAX_BADESCAPE`
    * `UNCIL_ERR_SYNTAX_BADUESCAPE`
    * `UNCIL_ERR_SYNTAX_TRAILING`
    * `UNCIL_ERR_SYNTAX_STRAYEND`
    * `UNCIL_ERR_SYNTAX_TOODEEP`
    * `UNCIL_ERR_SYNTAX_BADBREAK`
    * `UNCIL_ERR_SYNTAX_BADCONTINUE`
    * `UNCIL_ERR_SYNTAX_INLINEIFMUSTELSE`
    * `UNCIL_ERR_SYNTAX_NOFOROP`
    * `UNCIL_ERR_SYNTAX_CANNOTPUBLICLOCAL`
    * `UNCIL_ERR_SYNTAX_OPTAFTERREQ`
    * `UNCIL_ERR_SYNTAX_UNPACKLAST`
    * `UNCIL_ERR_SYNTAX_NODEFAULTUNPACK`
    * `UNCIL_ERR_SYNTAX_ONLYONEELLIPSIS`
    * `UNCIL_ERR_SYNTAX_FUNCTABLEUNNAMED`
  * `UNCIL_ERR_TOODEEP`: the program is too complicated.
  * `UNCIL_ERR_IO_INVALIDENCODING`: the source code has invalid UTF-8
    (all source code is assumed to be encoded in UTF-8).

`Unc_RetVal unc_compilestringc(Unc_View *w, const char* text);`
* Compiles a string (with implicit length, as in a C string) and loads the
  compiled program as the program for the given view.
* Error codes: see `unc_compilestring`.

`Unc_RetVal unc_compilefile(Unc_View *w, FILE *file);`
* Compiles code from a file and loads the compiled program as the program
  for the given view.
* Error codes: see `unc_compilestring`.

`Unc_RetVal unc_compilestream(Unc_View *w, int (*getch)(void *), void *udata);`
* Compiles code from a stream (with a get-next-character function)
  and loads the compiled program as the program for the given view.
* Error codes: see `unc_compilestring`.

`Unc_Size unc_getcompileerrorlinenumber(Unc_View *w);`
* If the last `unc_compile`* function returned an error, this will return
  the line number at which the error occurred.
* Otherwise the behavior is undefined.

`Unc_RetVal unc_loadfile(Unc_View *w, FILE *file);`
* Loads a precompiled byte code program from a file and sets it as the
  program of the given view.
* Error codes:
  * `UNCIL_ERR_MEM`: could not allocate enough memory.
  * `UNCIL_ERR_PROGRAM_INCOMPATIBLE`: the byte code is incompatible
    with the current version of the interpreter.

`Unc_RetVal unc_loadstream(Unc_View *w, int (*getch)(void *), void *udata);`
* Loads a precompiled byte code program from a stream (with a
  get-next-byte function) and sets it as the program of the given view.
* Error codes: see `unc_loadfile`.

`Unc_RetVal unc_loadfileauto(Unc_View *w, const char *fn);`
* Opens the file with the given name, automatically determines whether it
  contains source code or byte code, compiles/loads it and sets
  it as the program of the given view.
* Error codes:
  * Any from `unc_compilestring`
  * Any from `unc_loadfile`
  * `UNCIL_ERR_IO_GENERIC`: for I/O errors from the file.

`int unc_dumpfile(Unc_View *w, FILE *file);`
* Dumps the currently loaded program into a file as byte code.
* Error codes:
  * `UNCIL_ERR_MEM`: could not allocate enough memory.
  * `UNCIL_ERR_IO_GENERIC`: for I/O errors from the file.

`int unc_dumpstream(Unc_View *w, int (*putch)(int, void *), void *udata);`
* Dumps the currently loaded program into a stream as byte code.
* Error codes: see `unc_dumpfile`.

`void unc_unload(Unc_View *w);`
* Unloads the currently loaded program for a view.

`void unc_copy(Unc_View *w, Unc_Value *dst, Unc_Value *src);`
* Copies a value from one `Unc_Value` pointer to another.
  `Unc_Value`s should _not_ be copied by value (or
  undefined behavior will occur).

`void unc_move(Unc_View *w, Unc_Value *dst, Unc_Value *src);`
* Combination of `unc_copy(w, dst, src)` and `unc_clear(src)`.

`void unc_swap(Unc_View *w, Unc_Value *va, Unc_Value *vb);`
* Swaps a `Unc_Value` between `va` and `vb`.

`int unc_issame(Unc_View *w, Unc_Value *a, Unc_Value *b);`
* Performs a value comparison (for value types) or reference comparison
  (for reference types) and returns a non-zero value only if the two given
  values are equal. Note that the behavior is not identical to that of the
  `==` operator.

`void unc_clear(Unc_View *w, Unc_Value *v);`
* Decrements the reference count of the given value. If it drops to zero,
  the value is freed. Sets `v` to an Uncil `null` afterwards.

`void unc_clearmany(Unc_View *w, Unc_Size n, Unc_Value *v);`
* Decrements the reference count of multiple values. Behaves as if `unc_clear`
  were to be called in a loop.

`Unc_ValueType unc_gettype(Unc_View *w, Unc_Value *v);`
* Returns the type of the given value. Valid return values:
```
    Unc_TNull
    Unc_TBool
    Unc_TInt
    Unc_TFloat
    Unc_TString
    Unc_TArray
    Unc_TTable
    Unc_TObject
    Unc_TBlob
    Unc_TFunction
    Unc_TOpaque
    Unc_TOpaquePtr
    Unc_TWeakRef
    Unc_TBoundFunction
```
* It is guaranteed that `Unc_TNull` == 0; thus this can also be used to
  check whether a value is `null`.

`int unc_getpublic(Unc_View *w, Unc_Size nl, const char *name, Unc_Value *value);`
* Gets the public variable with the given name (string passed with explicit
  length) and assigns it into the given value slot.
* Error codes:
  * `UNCIL_ERR_ARG_NOSUCHNAME`: no public variable with the given name exists

`Unc_RetVal unc_setpublic(Unc_View *w, Unc_Size nl, const char *name, Unc_Value *value);`
* Assigns the given value into a public variable with the given name
  (string passed with explicit length).
* If successful, the reference count of the assigned value is incremented.
* Error codes:
  * `UNCIL_ERR_MEM`: could not allocate enough memory.

`int unc_getpublicc(Unc_View *w, const char *name, Unc_Value *value);`
* Gets the public variable with the given name (string passed with implicit
  length, "C string") and assigns it into the given value slot.
* Error codes: see `unc_getpublic`.

`Unc_RetVal unc_setpublicc(Unc_View *w, const char *name, Unc_Value *value);`
* Assigns the given value into a public variable with the given name
  (string passed with implicit length, "C string").
* If successful, the reference count of the assigned value is incremented.
* Error codes:
  * `UNCIL_ERR_MEM`: could not allocate enough memory.

`Unc_RetVal unc_getbool(Unc_View *w, Unc_Value *v, Unc_RetVal nul);`
* Gets the boolean value of a boolean.
* Return values: 0 (`false`), 1 (`true`), `nul` if v is `null`, or the
  error `UNCIL_ERR_TYPE_NOTBOOL` if not a boolean or null.

`Unc_RetVal unc_converttobool(Unc_View *w, Unc_Value *v);`
* Converts a value into a boolean.
* Return values: 0 (`false`), 1 (`true`), or something else for an error code.
  (You can use the `UNCIL_IS_ERR` macro to check the return code.)
* Error codes:
  * As this function may call an overload,
    any error code that may be returned by `unc_run`.

`Unc_RetVal unc_getint(Unc_View *w, Unc_Value *v, Unc_Int *ret);`
* Gets the value as an integer.
* Error codes:
  * `UNCIL_ERR_CONVERT_TOINT`: the value is not an integer.

`Unc_RetVal unc_getfloat(Unc_View *w, Unc_Value *v, Unc_Float *ret);`
* Gets the value as a floating-point number.
* Error codes:
  * `UNCIL_ERR_CONVERT_TOFLOAT`: the value is not a float.

`Unc_RetVal unc_getstring(Unc_View *w, Unc_Value *v, Unc_Size *n, const char **p);`
* Gets the value as a string. The size (in bytes) and pointer to character
  data are assigned via output pointers.
* `p` will be null-terminated, but may even contain null characters somewhere
  within the first `n` bytes. Code passing strings to functions that expect C
  strings should handle this the best way they see fit -- consider using
  `unc_getstringc` instead.
* `p` will become invalid if the string value is destroyed (either through
  reference counting or garbage collection).
* Error codes:
  * `UNCIL_ERR_TYPE_NOTSTR`: the value is not a string.

`Unc_RetVal unc_getstringc(Unc_View *w, Unc_Value *v, const char **p);`
* Gets the value as a string. The pointer to null-terminated character
  data are assigned via output pointers.
* This does not return the size like `unc_getstring`, but in turn it checks
  whether `p` contains null characters before the terminator and causes an
  error if so.
* `p` will become invalid if the string value is destroyed (either through
  reference counting or garbage collection).
* Error codes:
  * `UNCIL_ERR_TYPE_NOTSTR`: the value is not a string.
  * `UNCIL_ERR_ARG_NULLCHAR`: string contained null characters.

`Unc_RetVal unc_getopaqueptr(Unc_View *w, Unc_Value *v, void **p);`
* Gets the value as an opaque pointer (`void *`).
* Error codes:
  * `UNCIL_ERR_TYPE_NOTOPAQUEPTR`: the value is not a opaqueptr.

`Unc_RetVal unc_getblobsize(Unc_View *w, Unc_Value *v, Unc_Size *ret);`
* Gets the size of the given blob passed as a value.
* Error codes:
  * `UNCIL_ERR_TYPE_NOTBLOB`: the value is not a blob.

`Unc_RetVal unc_getarraysize(Unc_View *w, Unc_Value *v, Unc_Size *ret);`
* Gets the length of the given array passed as a value.
* Error codes:
  * `UNCIL_ERR_TYPE_NOTARRAY`: the value is not a blob.

`int unc_iscallable(Unc_View *w, Unc_Value *v);`
* Returns a non-zero value if and only if `v` is callable. This value may
  change over time (such as if the prototype of `v` gains or loses a
  `__call` overload) and should not taken as constant.

`Unc_RetVal unc_lockblob(Unc_View *w, Unc_Value *v, Unc_Size *n, Unc_Byte **p);`
* If the value is a blob, locks it (prevents any other code, including
  _subsequent_ `unc_lockblob` calls) from accessing it, and then returns its
  size and a pointer to its data.
* Blob locks are _not_ re-entrant.
* Locking a blob does not prevent the value from getting destroyed by
  reference counting or garbage collection. Thus, at least one reference
  shall persist, or `*p` may become invalid.
* Error codes:
  * `UNCIL_ERR_TYPE_NOTBLOB`: the value is not a blob.
  * If an error occurs, the object will not be locked.

`Unc_RetVal unc_lockarray(Unc_View *w, Unc_Value *v, Unc_Size *n, Unc_Value **p);`
* If the value is an array, locks it (prevents any other code, including
  _subsequent_ `unc_lockarray` calls) from accessing it, and then returns its
  size and a pointer to its elements.
* Array locks are _not_ re-entrant.
* Locking an array does not prevent the value from getting destroyed by
  reference counting or garbage collection. Thus, at least one reference
  shall persist, or `*p` may become invalid.
* Error codes:
  * `UNCIL_ERR_TYPE_NOTARRAY`: the value is not an array.
  * If an error occurs, the object will not be locked.

`Unc_RetVal unc_lockopaque(Unc_View *w, Unc_Value *v, Unc_Size *n, void **p);`
* If the value is an opaque object, locks it (prevents any other code, including
  _subsequent_ `unc_lockopaque` calls) from accessing it, and then returns its
  size and a pointer to its data.
* Opaque locks are _not_ re-entrant.
* Unlike with `unc_lockblob` or `unc_lockarray`, `n` may also be `NULL`.
* Locking an opaque object does not prevent the value from getting destroyed by
  reference counting or garbage collection. Thus, at least one reference
  shall persist, or `*p` may become invalid.
* Error codes:
  * `UNCIL_ERR_TYPE_NOTOPAQUE`: the value is not an opaque object.
  * If an error occurs, the object will not be locked.

`Unc_RetVal unc_trylockopaque(Unc_View *w, Unc_Value *v, Unc_Size *n, void **p);`
* Same as `unc_lockopaque`, but returns an error if it is already locked.
* Error codes:
  * `UNCIL_ERR_LOGIC_CANNOTLOCK`: object is already locked. Note that
    opaque locks are _not_ re-entrant.
  * See `unc_lockopaque`.

`void unc_unlock(Unc_View *w, Unc_Value *v);`
* Unlocks a value (such as a blob, an array or opaque object locked by the
  previous two API calls). All locked values must be unlocked before the
  function that locked them finishes executing, or else deadlocks may occur.
* Once a value has been unlocked, any pointers obtained to within (such as the
  `Unc_Byte *` to blob data, `void *` to opaque data or `Unc_Value *` to array
  data) is considered invalid and dereferencing it will result in
  undefined behavior.

`void unc_lockthisfunc(Unc_View *w);`
* Locks the lock associated with the current function.
* This lock is re-entrant, but must be unlocked the same number of times it
  was locked. The lock should be unlocked before the function finishes.
* This lock is only really useful for `UNC_CFUNC_CONCURRENT` functions when you
  do want to enforce critical sections, such as when accessing bound values.
* Results in undefined behavior if called outside a C function that has been
  called either directly or indirectly through `unc_call`.

`void unc_unlockthisfunc(Unc_View *w);`
* Unlocks the lock associated with the current function.
* Results in undefined behavior if called outside a C function that has been
  called either directly or indirectly through `unc_call`.

`Unc_RetVal unc_resizeblob(Unc_View *w, Unc_Value *v, Unc_Size n, Unc_Byte **p);`
* Resizes an **already locked** blob to a new size. Any existing bytes will
  be preserved (except those that would be beyond the new end), while the new
  area will have indeterminate contents. Calling this for a blob that is not
  locked beforehand results in undefined behavior.
* Error codes:
  * `UNCIL_ERR_MEM`: could not allocate enough memory.
  * `UNCIL_ERR_TYPE_NOTBLOB`: the value is not a blob.

`Unc_RetVal unc_resizearray(Unc_View *w, Unc_Value *v, Unc_Size n, Unc_Value **p);`
* Resizes an **already locked** array to a new size. Any existing elements will
  be preserved (except those that would be beyond the new end), while the new
  area will be filled with `null`s. Calling this for an array that is not
  locked beforehand results in undefined behavior.
* Error codes:
  * `UNCIL_ERR_MEM`: could not allocate enough memory.
  * `UNCIL_ERR_TYPE_NOTARRAY`: the value is not an array.

`Unc_RetVal unc_getindex(Unc_View *w, Unc_Value *v, Unc_Value *i, Unc_Value *out);`
* Indexes the value `v` with the index `i` and returns the value in `out`.
* If `out` is assigned, its reference count is incremented.
* Error codes:
  * `UNCIL_ERR_MEM`: could not allocate enough memory.
  * `UNCIL_ERR_ARG_INDEXNOTINTEGER`: `i` should have been an integer,
    but it was not.
  * `UNCIL_ERR_ARG_INDEXOUTOFBOUNDS`: `i`, integer, out of bounds for `v`.
  * `UNCIL_ERR_ARG_NOSUCHATTR`: If `v` is a table or object,
    the given index `i` does not have a corresponding value.
  * `UNCIL_ERR_ARG_NOTINDEXABLE`: `v` does not support indexing.
  * As this function may call an overload,
    any error code that may be returned by `unc_run`.

`Unc_RetVal unc_setindex(Unc_View *w, Unc_Value *v, Unc_Value *i, Unc_Value *in);`
* Indexes the value `v` with the index `i` and assigns the value in `in` to it.
* Error codes:
  * `UNCIL_ERR_MEM`: could not allocate enough memory.
  * `UNCIL_ERR_ARG_INDEXNOTINTEGER`: `i` should have been an integer,
    but it was not.
  * `UNCIL_ERR_ARG_INDEXOUTOFBOUNDS`: `i`, integer, out of bounds for `v`.
  * `UNCIL_ERR_ARG_CANNOTSETINDEX`: `v` does not support assignment by index.
  * `UNCIL_ERR_ARG_NOTINDEXABLE`: `v` does not support indexing.
  * As this function may call an overload,
    any error code that may be returned by `unc_run`.

`Unc_RetVal unc_getattrs(Unc_View *w, Unc_Value *v, Unc_Size an, const char *as, Unc_Value *out);`
* Gets the attribute of `v` identified by a string `as` with
  explicit length and assigns the result in `out`.
* If `out` is assigned, its reference count is incremented.
* Error codes:
  * `UNCIL_ERR_MEM`: could not allocate enough memory.
  * `UNCIL_ERR_ARG_NOSUCHATTR`: attribute not found.
  * `UNCIL_ERR_ARG_NOTATTRABLE`: `v` does not have any attributes.

`Unc_RetVal unc_setattrs(Unc_View *w, Unc_Value *v, Unc_Size an, const char *as, Unc_Value *in);`
* Assigns the value in `in` to the attribute of `v` identified by a string
  `as` with explicit length.
* If successful, the reference count of the assigned value is incremented.
* Error codes:
  * `UNCIL_ERR_MEM`: could not allocate enough memory.
  * `UNCIL_ERR_ARG_NOTATTRSETTABLE`: `v` does not support attribute assignment.
  * `UNCIL_ERR_ARG_NOTATTRABLE`: `v` does not have any attributes.

`Unc_RetVal unc_getattrv(Unc_View *w, Unc_Value *v, Unc_Value *a, Unc_Value *out);`
* Gets the attribute of `v` identified by a value `a`
  and assigns the result in `out`.
* If `out` is assigned, its reference count is incremented.
* Error codes: see `unc_getattrs`.

`Unc_RetVal unc_setattrv(Unc_View *w, Unc_Value *v, Unc_Value *a, Unc_Value *in);`
* Assigns the value in `in` to the attribute of `v` identified by a value `a`.
  If successful, the reference count of the assigned value is incremented.
* Error codes: see `unc_setattrs`.

`Unc_RetVal unc_getattrc(Unc_View *w, Unc_Value *v, const char *as, Unc_Value *out);`
* Gets the attribute of `v` identified by a string `as` with
  implicit length (C string) and assigns the result in `out`.
* If `out` is assigned, its reference count is incremented.
* Error codes: see `unc_getattrv`.

`Unc_RetVal unc_setattrc(Unc_View *w, Unc_Value *v, const char *as, Unc_Value *in);`
* Assigns the value in `in` to the attribute of `v` identified by a string
  `as` with implicit length (C string).
* If successful, the reference count of the assigned value is incremented.
* Error codes: see `unc_setattrv`.

`Unc_Size unc_getopaquesize(Unc_View *w, Unc_Value *v);`
* Gets the size of an opaque object, or 0 if the value is not one.

`void unc_getprototype(Unc_View *w, Unc_Value *v, Unc_Value *p);`
* Gets the prototype of the object `v`, or `null` if the value could not
  have a prototype. The result is assigned to `p`.

`Unc_Size unc_getopaqueboundcount(Unc_View *w, Unc_Value *v);`
* Gets the number of bound values for an opaque object `v`,
  or 0 if the value is not one.

`Unc_Value *unc_opaqueboundvalue(Unc_View *w, Unc_Value *v, Unc_Size i);`
* Gets the pointer to a bound value for an opaque object `v`,
  or `NULL` if the value `v` is not one.
* You should lock the opaque object before accessing its bound values
  (unless you are only ever reading them).
* `i` must be less than the number of bound values for that object.

`void unc_setnull(Unc_View *w, Unc_Value *v);`
* Assigns `null` into `v`. Effectively equivalent to `unc_clear`.

`void unc_setbool(Unc_View *w, Unc_Value *v, int b);`
* Assigns a boolean value (`true` if b is non-zero, else `false`) into `v`.

`void unc_setint(Unc_View *w, Unc_Value *v, Unc_Int i);`
* Assigns an integer value (of type `int`) into `v`.

`void unc_setfloat(Unc_View *w, Unc_Value *v, Unc_Float f);`
* Assigns a floating-point value (of type `float`) into `v`.

`Unc_RetVal unc_newstring(Unc_View *w, Unc_Value *v, Unc_Size n, const char *c);`
* Assigns a string (of type `string`) into `v`. A new string is allocated
  and its contents are copied from the given character pointer, with the size
  (number of bytes) specified by `n`.
* The returned value will have a reference count of 1, and thus it should be
  passed to `unc_decref` or `unc_clear` before the function ends.
* Error codes:
  * `UNCIL_ERR_MEM`: could not allocate enough memory.
  * `UNCIL_ERR_IO_INVALIDENCODING`: the string is not in valid UTF-8.
  * If an error code is returned, `v` is left as it was before.

`Unc_RetVal unc_newstringc(Unc_View *w, Unc_Value *v, const char *c);`
* Assigns a string (of type `string`) into `v`. A new string is allocated
  and its contents are copied from the given character pointer, which is treated
  as a C string.
* The returned value will have a reference count of 1, and thus it should be
  passed to `unc_decref` or `unc_clear` before the function ends.
* Error codes:
  * `UNCIL_ERR_MEM`: could not allocate enough memory.
  * `UNCIL_ERR_IO_INVALIDENCODING`: the string is not in valid UTF-8.
  * If an error code is returned, `v` is left as it was before.

`Unc_RetVal unc_newstringmove(Unc_View *w, Unc_Value *v, Unc_Size n, char *c);`
* Assigns a string (of type `string`) into `v`. A new string is created
  and its contents are taken from the given pointer which must have been
  allocated with `unc_malloc()`. The pointer is _moved_ and should be considered
  freed afterwards. A null terminator is added automatically.
* `n` represents the size of the string in `c` (which may be smaller than the
  number of bytes in `c`, but may not be larger).
* The returned value will have a reference count of 1, and thus it should be
  passed to `unc_decref` or `unc_clear` before the function ends.
* Error codes:
  * `UNCIL_ERR_MEM`: could not allocate enough memory.
  * `UNCIL_ERR_IO_INVALIDENCODING`: the string is not in valid UTF-8.
  * `UNCIL_ERR_ARG_INDEXOUTOFBOUNDS`: `n` was greater than the number of bytes
    allocated in `c`. you should not rely on this error.
  * If an error code is returned, `v` is left as it was before and
    `c` stays allocated.

`Unc_RetVal unc_newstringcmove(Unc_View *w, Unc_Value *v, char *c);`
* Same as `unc_newstringmove`, but takes a C string in `c`, which must be
  allocated with `unc_malloc` and contain a null terminator.
* The returned value will have a reference count of 1, and thus it should be
  passed to `unc_decref` or `unc_clear` before the function ends.
* Error codes:
  * `UNCIL_ERR_MEM`: could not allocate enough memory.
  * `UNCIL_ERR_IO_INVALIDENCODING`: the string is not in valid UTF-8.
  * `UNCIL_ERR_ARG_INDEXOUTOFBOUNDS`: no null terminator was found in `c`.
    you should not rely on this error.
  * If an error code is returned, `v` is left as it was before and
    `c` stays allocated.

`Unc_RetVal unc_newarrayempty(Unc_View *w, Unc_Value *v);`
* Assigns an array (of type `array`) into `v`.
* The array will initially be empty and will not be locked.
* The returned value will have a reference count of 1, and thus it should be
  passed to `unc_decref` or `unc_clear` before the function ends.
* Error codes:
  * `UNCIL_ERR_MEM`: could not allocate enough memory.
  * If an error code is returned, `v` is left as it was before.

`Unc_RetVal unc_newarray(Unc_View *w, Unc_Value *v, Unc_Size n, Unc_Value **p);`
* Assigns an array (of type `array`) into `v`.
* The array will initially contain `n` copies of null values and the pointer
  `p` will point to those items.
* The array is automatically **locked** and should be unlocked
  (with `unc_unlock`) when you are done.
* The returned value will have a reference count of 1, and thus it should be
  passed to `unc_decref` or `unc_clear` before the function ends.
* Error codes:
  * `UNCIL_ERR_MEM`: could not allocate enough memory.
  * If an error code is returned, `v` is left as it was before.

`Unc_RetVal unc_newarrayfrom(Unc_View *w, Unc_Value *v, Unc_Size n, Unc_Value *a);`
* Assigns an array (of type `array`) into `v`.
* The initial values for the array are taken from the passed value pointer.
* Any values copied will have their reference counts incremented.
* The array will not be initially locked.
* The returned value will have a reference count of 1, and thus it should be
  passed to `unc_decref` or `unc_clear` before the function ends.
* Error codes:
  * `UNCIL_ERR_MEM`: could not allocate enough memory.
  * If an error code is returned, `v` is left as it was before.

`Unc_RetVal unc_newtable(Unc_View *w, Unc_Value *v);`
* Assigns a table (of type `table`) into `v`.
* The table will be initially empty.
* The returned value will have a reference count of 1, and thus it should be
  passed to `unc_decref` or `unc_clear` before the function ends.
* Error codes:
  * `UNCIL_ERR_MEM`: could not allocate enough memory.
  * If an error code is returned, `v` is left as it was before.

`Unc_RetVal unc_newobject(Unc_View *w, Unc_Value *v, Unc_Value *prototype);`
* Assigns an object (of type `object`) into `v`.
* The object will be initially empty and will have the given prototype
  (if `prototype == NULL`, a `null` value will be assigned as the prototype).
* The returned value will have a reference count of 1, and thus it should be
  passed to `unc_decref` or `unc_clear` before the function ends.
* The reference count of the prototype will also be incremented.
* Error codes:
  * `UNCIL_ERR_MEM`: could not allocate enough memory.
  * `UNCIL_ERR_ARG_INVALIDPROTOTYPE`: the prototype is not of a valid type
    (it must be `null`, a `table`, an `object` or `opaque`)
  * If an error code is returned, `v` is left as it was before.

`Unc_RetVal unc_newblob(Unc_View *w, Unc_Value *v, Unc_Size n, Unc_Byte **data);`
* Assigns a blob (of type `blob`) into `v`. A new blob is allocated with
  the given size and a pointer to the data is returned.
* The blob is automatically **locked** and should be unlocked
  (with `unc_unlock`) when you are done.
* The returned value will have a reference count of 1, and thus it should be
  passed to `unc_decref` or `unc_clear` before the function ends.
* Error codes:
  * `UNCIL_ERR_MEM`: could not allocate enough memory.
  * If an error code is returned, `v` is left as it was before.

`Unc_RetVal unc_newblobfrom(Unc_View *w, Unc_Value *v, Unc_Size n, Unc_Byte *data);`
* Assigns a blob (of type `blob`) into `v`. A new blob is allocated
  and its contents are copied from the given byte pointer, with the size
  (number of bytes) specified by `n`.
* The returned value will have a reference count of 1, and thus it should be
  passed to `unc_decref` or `unc_clear` before the function ends.
* The blob will not be initially locked, unlike with `unc_newblob`.
* Error codes:
  * `UNCIL_ERR_MEM`: could not allocate enough memory.
  * If an error code is returned, `v` is left as it was before.

`Unc_RetVal unc_newblobmove(Unc_View *w, Unc_Value *v, Unc_Byte *data);`
* Assigns a blob (of type `blob`) into `v`. A new blob is created
  and its contents are taken from the given pointer which must have been
  allocated with `unc_malloc()`. The pointer is _moved_ and should be considered
  freed afterwards.
* The returned value will have a reference count of 1, and thus it should be
  passed to `unc_decref` or `unc_clear` before the function ends.
* The blob will not be initially locked, unlike with `unc_newblob`.
* Error codes:
  * `UNCIL_ERR_MEM`: could not allocate enough memory.
  * If an error code is returned, `v` is left as it was before.

`Unc_RetVal unc_newopaque(Unc_View *w, Unc_Value *v, Unc_Value *prototype, Unc_Size n, void **data, Unc_OpaqueDestructor destructor, Unc_Size refcount, Unc_Value *initvalues, Unc_Size refcopycount, Unc_Size *refcopies);`
* Assigns an opaque object (of type `opaque`) into `v`. It will have the
  given prototype (if `prototype == NULL`, a `null` value will be assigned
  as the prototype). The pointer will be returned in `data`.
* A destructor may be specified which will be called when the object is
  destroyed (or pass `NULL` for no destructor).
* An opaque object may also have values bound to it. These values are considered
  strong references and thus they will survive for at least as long as the
  opaque object does. Use `refcount` for "new bound values", in which case
  `initvalues` must be a pointer to at least that many `Unc_Value`s
  which will be copied (and their reference counts will be incremented).
  These values will be used as the initial values of those bound values.
  `initvalues` may also be `NULL` in which case the initial values will all
  be `null`.
* Use `refcopycount` and `refcopies` to copy bound values from the currently
  executing C function. `refcopies` must have at least `refcopycount` valid
  `Unc_Size` values corresponding to the bound value indexes (as if accessed
  by `unc_boundvalue`) and may be `NULL` only if `refcopycount` is zero. The
  bound values from `refcopies` will be located after the new bound values
  from `initvalues`.
* The returned value will have a reference count of 1, and thus it should be
  passed to `unc_decref` or `unc_clear` before the function ends.
* The opaque object is automatically **locked** and should be unlocked
  (with `unc_unlock`) when you are done.
* The reference count of the prototype will also be incremented.
* Error codes:
  * `UNCIL_ERR_MEM`: could not allocate enough memory.
  * `UNCIL_ERR_ARG_INVALIDPROTOTYPE`: the prototype is not of a valid type
    (it must be `null`, a `table`, an `object` or `opaque`)
  * `UNCIL_ERR_ARG_REFCOPYOUTOFBOUNDS`: a refcopy index was out of bounds.
  * If an error code is returned, `v` is left as it was before.

`Unc_RetVal unc_newcfunction(Unc_View *w, Unc_Value *v, Unc_CFunc func, int cflags, Unc_Size argcount, int ellipsis, Unc_Size optcount, Unc_Value *defaults, Unc_Size refcount, Unc_Value *initvalues, Unc_Size refcopycount, Unc_Size *refcopies, const char *fname, void *udata);`
* Assigns a function (of type `function`) into `v`. The function will wrap
  an existing C function `func` of type `Unc_CFunc`.
* `cflags` can be any of the following:
  * `UNC_CFUNC_DEFAULT`: default behavior. only one thread may run the
    C function at a time, but other threads will keep running.
  * `UNC_CFUNC_CONCURRENT`: the function can be executed by multiple
    different threads at once. it should be thread-safe. note that bound
    values are not thread-safe; if they are ever assigned to while in a
    function, manual locking (such as with `unc_lockthisfunc`) is required.
  * `UNC_CFUNC_EXCLUSIVE`: all other threads will be paused while this
    function runs. use with care, as deadlocks may occur.
* `argcount` is the number of required parameters.
* `ellipsis` is non-zero if there is an ellipsis parameter at the end.
  In that case, there may be more than `argcount + optcount` parameters
  in `Unc_Tuple args`.
* `optcount` is the number of optional parameters, and `defaults` is
  a pointer to at least `optcount` values that will be used as defaults for
  these optional parameters. `defaults` may be `NULL` in which case the defaults
  will all be `null`. Any values copied over will have their reference counts
  incremented.
* `refcount`, `initvalues`, `refcopycount`, `refcopies` behave as with
  `unc_newopaque`.
* `fname` is a C string to the name of the function. It will be copied.
* `udata` is a pointer that will be given to `func` every time it is called.
* The returned value will have a reference count of 1, and thus it should be
  passed to `unc_decref` or `unc_clear` before the function (that called
  `unc_newcfunction`) ends.
* Error codes:
  * `UNCIL_ERR_MEM`: could not allocate enough memory.
  * `UNCIL_ERR_ARG_REFCOPYOUTOFBOUNDS`: a refcopy index was out of bounds.
  * If an error code is returned, `v` is left as it was before.

`Unc_RetVal unc_exportcfunction(Unc_View *w, const char *name, Unc_CFunc func, int cflags, Unc_Size argcount, int ellipsis, Unc_Size optcount, Unc_Value *defaults, Unc_Size refcount, Unc_Value *initvalues, Unc_Size refcopycount, Unc_Size *refcopies, void *udata);`
* This function is a combination of `unc_newcfunction` and `unc_setpublicc`.
  The created function is directly assigned as a public value.
  Error codes: see `unc_newcfunction`, `unc_setpublic`.

`void unc_setopaqueptr(Unc_View *w, Unc_Value *v, void *data);`
* Assigns a `void *` (of type `opaqueptr`) into `v`.

`Unc_RetVal unc_freezeobject(Unc_View *w, Unc_Value *v);`
* Freezes an object `v`, making it (irreversibly) immutable. You should only
  ever really call this for objects you yourself created.
* Error codes:
  * `UNCIL_ERR_TYPE_NOTOBJECT`: value was not an object.

`int unc_yield(Unc_View *w);`
* If there is a request to currently pause all Uncil threads, pauses this
  thread.
* C functions should call this function in longer-duration loops in order to
  prevent deadlocks in situations where all other threads need to be stopped
  (such as for garbage collection).
* Returns zero, unless there was a request to halt the current thread, in
  which case returns `UNCIL_ERR_HALT`, and the C code should try to
  clean up and then likewise return `UNCIL_ERR_HALT` back to
  the Uncil API.
* If called in a C function with `UNC_CFUNC_EXCLUSIVE`, always immediately
  returns zero.

`int unc_yieldfull(Unc_View *w);`
* Same as `unc_yield`, but may also let other threads run the C function
  (if it has not been declared as `UNC_CFUNC_CONCURRENT`).

`Unc_RetVal unc_reserve(Unc_View *w, Unc_Size n);`
* Makes sure there are spots for at least `n` further values on the stack so
  that it does not have to be reallocated until at least `n` values have been
  pushed.
* Error codes:
  * `UNCIL_ERR_MEM`: could not allocate enough memory.

`Unc_RetVal unc_push(Unc_View *w, Unc_Size n, Unc_Value *v, Unc_Size *counter);`
* Pushes one or more values from `v` onto the stack. The stack is used to
  pass arguments to other functions or to return values.
* If `counter` is not `NULL`, `n` will be automatically added to the value
  it is pointing at.
* Any pushed values will have their reference counts incremented.
* Error codes:
  * `UNCIL_ERR_MEM`: could not allocate enough memory.
  * If an error code is returned, the stack will have the same number of
    values as before (but references, such as `unc_returnvalues` tuples,
    might still be invalidated).

`Unc_RetVal unc_pushmove(Unc_View *w, Unc_Value *v, Unc_Size *counter);`
* Pushes `v` onto the stack. Unlike `unc_push`, this does not increment the
  reference count. It is effectively a combined `unc_push` + `unc_clear`.
* If `counter` is not `NULL`, 1 will be automatically added to the value
  it is pointing at.
* Error codes:
  * `UNCIL_ERR_MEM`: could not allocate enough memory.
  * If an error code is returned, the stack will have the same number of
    values as before (but references, such as `unc_returnvalues` tuples,
    might still be invalidated).

`void unc_pop(Unc_View *w, Unc_Size n, Unc_Size *counter);`
* Removes `n` top values from the stack.
* If `counter` is not `NULL`, `n` will be automatically subtracted
  from the value it is pointing at.
* Any popped values are discarded and will have their reference
  counts decremented.

`Unc_RetVal unc_shove(Unc_View *w, Unc_Size d, Unc_Size n, Unc_Value *v, Unc_Size *counter);`
* Pushes one or more values from `v` onto the stack under `d` elements.
* If `counter` is not `NULL`, `n` will be automatically added to the value
  it is pointing at.
* Any pushed values will have their reference counts incremented.
* Error codes:
  * `UNCIL_ERR_MEM`: could not allocate enough memory.
  * If an error code is returned, the stack will have the same number of
    values as before (but references, such as `unc_returnvalues` tuples,
    might still be invalidated).

`void unc_yank(Unc_View *w, Unc_Size d, Unc_Size n, Unc_Size *counter);`
* Removes one or more values from the stack under `d` elements. `d`
  must be greater than or equal to `n`.
* If `counter` is not `NULL`, `n` will be automatically subtracted from the
  value it is pointing at.
* Any popped values are discarded and will have their reference
  counts decremented.

`Unc_RetVal unc_throw(Unc_View *w, Unc_Value *v);`
* Used to throw an Uncil error from a C function. `v` will be the value
  that will be thrown as an error.
* Returns `UNCIL_ERR_UNCIL` on success. This function should be used directly
  in a `return` statement.
* Error codes:
  * `UNCIL_ERR_MEM`: could not allocate enough memory.

`Unc_RetVal unc_throwex(Unc_View *w, Unc_Value *type, Unc_Value *message);`
* Used to throw an Uncil error from a C function. `type` and `message` are
  Uncil values. They will be assigned to an object under the keys
  `"type"` and `"message"`, respectively.
* Returns `UNCIL_ERR_UNCIL` on success. This function should be used directly
  in a `return` statement.
* Error codes: see `unc_throw`

`Unc_RetVal unc_throwext(Unc_View *w, const char *type, Unc_Value *message);`
* Used to throw an Uncil error from a C function. `type` is a C string, while
  `message` is an Uncil value. They will be assigned to an object under the keys
  `"type"` and `"message"`, respectively.
* Returns `UNCIL_ERR_UNCIL` on success. This function should be used directly
  in a `return` statement.
* Error codes: see `unc_throw`

`Unc_RetVal unc_throwexc(Unc_View *w, const char *type, const char *message);`
* Used to throw an Uncil error from a C function. `type` and `message` are
  C strings. They will be assigned to an object under the keys
  `"type"` and `"message"`, respectively.
* Returns `UNCIL_ERR_UNCIL` on success. This function should be used directly
  in a `return` statement.
* Error codes: see `unc_throw`

`void *unc_malloc(Unc_View *w, size_t n);`
* Allocates `n` bytes through the configured Uncil allocator.
* The initial contents of the allocated region are all zeros.
* Returns `NULL` in case of failure.

`void *unc_mrealloc(Unc_View *w, void *p, size_t n);`
* Reallocates a block of memory at `p` and makes it `n` bytes large through
  the configured Uncil allocator. Memory contents are preserved much in the
  same way they would be for `realloc`. If the region is expanded, the initial
  contents of the area in which the region expanded are all zeros.
* `p` may be `NULL` and `unc_mrealloc(w, p, n)` shall then behave identically
  to `unc_malloc(w, n)`.
* Returns `NULL` in case of failure. The old pointer remains valid if
  and only if `unc_mrealloc` returns `NULL`. This function will never return
  `NULL` if `n` is less than or equal the size of the existing allocation
  at `p`.

`void unc_mfree(Unc_View *w, void *p);`
* Frees the given memory block with the configured Uncil allocator. The pointer
  becomes invalid once the memory block associated to it has been freed.
* Calling with `p == NULL` is safe (and does nothing).

`Unc_Size unc_boundcount(Unc_View *w);`
* Gets the number of values bound to the currently executing function.
* Results in undefined behavior if called outside a C function that has been
  called either directly or indirectly through `unc_call`.

`Unc_Value *unc_boundvalue(Unc_View *w, Unc_Size index);`
* Gets the pointer to the bound value with index `index`.
* Results in undefined behavior if called outside a C function that has been
  called either directly or indirectly through `unc_call`, or if `index`
  is greater than or equal to the value that `unc_boundcount` would return.

`Unc_Size unc_recurselimit(Unc_View *w);`
* Gets the number of recursions remaining in the current Uncil context. This
  is useful for C API functions that perform recursion in order to avoid going
  over a recursion limit. For example, if the value returns 10, it means that
  functions should ideally only recurse at most 10 times, and then for example
  return the error code `UNCIL_ERR_TOODEEP` (or an exception with type
  `"recursion"`).

`Unc_RetVal unc_newpile(Unc_View *w, Unc_Pile *pile);`
* Initializes a new _pile_. A pile represents a frame or region of the stack
  that is used to handle return values from functions.
* Error codes:
  * `UNCIL_ERR_MEM`: could not allocate enough memory.

`Unc_RetVal unc_callex(Unc_View *w, Unc_Value *func, Unc_Size argn, Unc_Pile *ret);`
* Calls a function.
* `func` should be a callable value or `NULL`. If `NULL`, the main function
  (note: _not_ the `main` function, but rather the script's top-level code)
  of the currently loaded program will be called.
* `argn` is the number of arguments. At least that many values should have
  been pushed to the stack at this point. When calling the top-level main
  function (`func == NULL`), any arguments are simply dropped.
* `ret` should be a `Unc_Pile` variable for storing return values. The pile
  must have been initialized at this point, either through `unc_newpile` or
  using `unc_call` instead of `unc_callex`. `unc_callex` specifically (as
  opposed to `unc_call`) is useful mostly for pushing the return values
  of multiple function calls onto the same pile. The pile need not be discarded
  if an error is returned by `unc_call`(`ex`).
* If the function to be called is an Uncil function, this will enter a
  Uncil VM execution context and then exit once finished.
* Error codes:
  * `UNCIL_ERR_MEM`: could not allocate enough memory.
  * `UNCIL_ERR_UNCIL`: an Uncil error occurred
    (accessible through `unc_getexception` that should be the following
     call right after `unc_call`/`unc_callex`).
  * `UNCIL_ERR_HALT`: VM was halted

`Unc_RetVal unc_call(Unc_View *w, Unc_Value *func, Unc_Size argn, Unc_Pile *ret);`
* Combination of `unc_newpile` and `unc_callex`.
* Error codes: see `unc_newpile`, `unc_callex`.

`void unc_getexception(Unc_View *w, Unc_Value *out);`
* Gets the last error in `w` as an Uncil error object and assigns it to `out`.
  The result will have a non-zero reference count (it is incremented).

`void unc_getexceptionfromcode(Unc_View *w, Unc_Value *out, int e);`
* Converts an error code `e` into an Uncil error object and assigns it to `out`.
  The result will have a non-zero reference count (it is incremented).
  The error object may have less details than with `unc_getexception`.

`Unc_RetVal unc_valuetostring(Unc_View *w, Unc_Value *v, Unc_Size *n, char *c);`
* Converts a value into a string and returns the string in a buffer.
  `*n` should be set to the size of the buffer (including the null
  terminator) and will be set to the number of characters written.
* Error codes:
  * `UNCIL_ERR_MEM`: could not allocate enough memory.

`Unc_RetVal unc_valuetostringn(Unc_View *w, Unc_Value *v, Unc_Size *n, char **c);`
* Converts a value into a string and returns the string in a buffer that is
  allocated as if by `unc_malloc`.
* The string is null-terminated. `*c` will be a memory allocation with size
  `*n + 1`.
* Error codes:
  * `UNCIL_ERR_MEM`: could not allocate enough memory.

`Unc_RetVal unc_exceptiontostring(Unc_View *w, Unc_Value *exc, Unc_Size *n, char *c);`
* Converts an error/exception value into a string and returns the string in a
  buffer. `*n` should be set to the size of the buffer (including the null
  terminator) and will be set to the number of characters written.
* Error codes:
  * `UNCIL_ERR_MEM`: could not allocate enough memory.

`Unc_RetVal unc_exceptiontostringn(Unc_View *w, Unc_Value *exc, Unc_Size *n, char **c);`
* Converts an error/exception value into a string and returns the string in a
  buffer that is allocated as if by `unc_malloc`. See `unc_valuetostringn`
  for info on `n` and `c`.
* Error codes:
  * `UNCIL_ERR_MEM`: could not allocate enough memory.

`void unc_returnvalues(Unc_View *w, Unc_Pile *pile, Unc_Tuple *tuple);`
* Used to get the return values form a pile. The resulting `Unc_Tuple` should
  be a local variable and will be initialized with the number of values
  and a pointer to them. The tuple will remain valid until the stack is acted
  upon again (by `unc_push`, `unc_pushmove`, `unc_pop`, `unc_call`,
  `unc_callex`...).

`Unc_RetVal unc_discard(Unc_View *w, Unc_Pile *pile);`
* Discards a given pile. The pile must be the most recently created one
  (either by `unc_newpile` or `unc_call`).
* Not all piles need to be discarded. If you want to return the values returned
  by another function, you can actually keep the pile as the function exits.
* Error codes:
  * `UNCIL_ERR_ARG_NOTMOSTRECENT`: the pile given was not the one that was
    most recently created.

`Unc_RetVal unc_getiterator(Unc_View *w, Unc_Value *v, Unc_Value *res);`
* Gets the iterator of `v` and returns it in `res`. This returns a function
  that should be called without any parameters, and it wil return the next
  values - or no values if the iteration is over.
* Error codes:
  * `UNCIL_ERR_MEM`: could not allocate enough memory.
  * `UNCIL_ERR_ARG_NOTITERABLE`: the value `v` is not iterable.

## Dangerous functions

Use these only if you know what you are doing.

`void unc_incref(Unc_View *w, Unc_Value *v);`
* Increments the reference count of the given value.
  (Note that this nor `decref`/`clear` has any effect on value types)
* Prefer using `unc_copy`, `unc_move` etc. instead of using this directly.

`void unc_decref(Unc_View *w, Unc_Value *v);`
* Decrements the reference count of the given value. If it drops to zero,
  the value is freed.
* Prefer using `unc_clear`, `unc_move` etc. instead of using this directly.

`void unc_vmpause(Unc_View *w);`
* Tells the Uncil VM that it is "paused". This should be used if a C function
  needs to do a long, possibly indefinitely long, operation which does not
  involve using the Uncil API. Otherwise functions or other tasks which require
  exclusive access may have to wait for the operation to finish.
* If you can control the length of the task, prefer `unc_yield`.
* **UNDEFINED BEHAVIOR OCCURS** if:
  * the function returns back to the Uncil API after calling `unc_vmpause`
    but before calling `unc_vmresume`.
  * the function performs any API call with a paused `Unc_View` except for
    the following:
    * `unc_vmresume`
    * `unc_coinhabited`
    * `unc_gettype`
  * `unc_vmpause` calls do not stack and thus they are covered by the above.

`int unc_vmresume(Unc_View *w);`
* Tells the Uncil VM that it is no longer "paused". Required to be called
  after a `unc_vmpause`.
* May block until VMs should no longer be paused.
* **UNDEFINED BEHAVIOR OCCURS** if the call was not preceded
  by a `unc_vmpause` call.
* Returns 0, or `UNCIL_ERR_HALT` if the view has been halted.
