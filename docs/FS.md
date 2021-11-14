
# File system library

Documentation for the builtin file system library. The features it provides
may not be available on all platforms.

This module can usually be accessed with `require("fs")`.

This module is disabled by default when Uncil is used as a library, but
available on the standalone interpreter; see `mmask` in `API.md`.

## fs.abspath
`fs.abspath(path)`

Returns the given path as a normalized absolute path.

## fs.basename
`fs.basename(path)`

Returns the basename (file name) of a path; it returns the component of the
path after the final directory separator and thus returns an empty string
if the path ends in a separator.

## fs.chmod
`fs.chmod(file, mode)`

Changes the file mode (permissions). The mode flags should be a bitwise OR
(combination) of zero or more of the `fs.stat` mode flags.

## fs.copy
`fs.copy(source, destination, [metadata], [overwrite])`

Copies the file at `source` to `destination`, which may be a file or
directory (in the latter case, the file is copied with the same name to under
that directory). If `source` is a directory, an empty directory will be created
in `destination` as if it were a file. If an existing file or directory would
be overwritten, an error will be thrown, unless `overwrite` is `true` (and
if both the source and destination are the same type of file, i.e. regular
file, directory, etc.; if not, an error occurs).

By default, the new copy is treated as if it were a new file that just happens
to have the same contents as the original file. If `metadata` is `true`,
`fs.copy` tries to copy file metadata. These include:
* file permission/mode bits (POSIX)
* file owner (POSIX; if permissions allow)
* access/modification times
* attributes (Windows)

Not all metadata is copied and the behavior is largely platform-dependent.
The following information may not be copied:
* ACLs (POSIX / Windows)
* file owner (Windows)
* alternate data streams (Windows)
* resource forks (macOS)

If the copy fails halfway during a read from the source or a write to the
destination, Uncil will try to remove the partially copied destination file
(even if an existing file was overwritten).

## fs.dirname
`fs.dirname(path)`

Returns the dirname (directory portion) of a path; it returns the component
of the path before the final directory separator (unless the path is a direct
reference to a file under the root directory, in which case it only contains
the directory separator). If the path has no directory separators,
the function returns `null`.

## fs.exists
`fs.exists(path)`

Returns `true` if a file exists in the given path (it may be a file,
directory, etc.) or `false` otherwise.

## fs.getcwd
`fs.getcwd()`

Returns the path of the current working directory or `null` if there isn't
any (such as if the platform does not have any equivalent concept).

## fs.getext
`fs.getext(path)`

Returns the file extension in the given path (including at most one dot) or
an empty string if there isn't any.

## fs.isdir
`fs.isdir(path)`

Returns `true` if `path` refers to an existing directory and `false` otherwise.

## fs.isfile
`fs.isfile(path)`

Returns `true` if `path` refers to an existing regular file
and `false` otherwise.

## fs.lcopy
`fs.lcopy(source, destination, [metadata], [overwrite])`

Same as `fs.copy`, but will copy a symbolic link `source` instead of
following it, that is to say, copying the file it refers to.

## fs.link
`fs.link(target, link, [symbolic])`

Creates a link called `link` that points to `target`. By default, the
function creates a hard link, but if `symbolic` is `true`, a symbolic link
is created instead. If an existing file would be overwritten, an error will
be thrown.

## fs.lstat
`fs.lstat(path)`

Same as `fs.stat`, but does not follow symbolic links.

## fs.mkdir
`fs.mkdir(path, [mode], [parents])`

Creates an empty directory in `path`. An error occurs if the directory already
exists. If `parents` is `true`, all necessary parents are also created, and
no error occurs if the directory already exists.

If `mode` is specified and not null, it should be a bitwise OR (combination)
of zero or more of the `fs.stat` mode flags. If not specified or null,
the default mode is used, which depends on the currently used umask (0777 with
bits possibly masked off; on many Linux systems, the default umask 0022 will
result in directories created with 0755).

## fs.move
`fs.move(source, destination, [overwrite])`

Moves the file or directory `source` to `destination`, which should be a new
name for the file at `source` (either a file or directory depending on which
one it is). If an existing file or directory would be overwritten,
an error will be thrown, unless `overwrite` is `true` (and if both the source
and destination are the same type of file, i.e. regular file, directory, etc.,
and if the destination is a directory, it is empty; if not, an error occurs).
To replace a non-empty directory at destination, see `fs.rmove`.

## fs.normpath
`fs.normpath(path)`

Normalizes a path according to the current platform's conventions.

## fs.pathjoin
`fs.pathjoin(paths...)`

Combines one or more path components into a single valid path. Absolute paths
will throw away any previous components (on Windows, the drive letter is kept,
but thrown away if a component includes one).

This is roughly equivalent to merging multiple `cd` commands (albeit possibly
without normalization, so components like `..` may remain).

The actual valid format for paths depends on the platform.

## fs.pathprefix
`fs.pathprefix(paths...)`

Returns the longest common prefix of all of the given paths, which must all
either be absolute or relative (or the return value will be `null`). The
returned path is guaranteed to describe an existing file if all of the given
paths do.

## fs.pathsep
`fs.pathsep`

A character that acts as the path separator for values with multiple paths
on the current platform; examples include `":"` for *nix systems and
`";"` on Windows.

## fs.pathsplit
`fs.pathsplit(path)`

Splits a path into its components. The returned array will, if combined all
into one string, equal the original path. The components will not include the
directory separator, unless one refers to the root (in which case the component
will only be the separator). For example, on *nix systems,
the path `/home/test/file.txt` will be split into
`[ "/", "home", "test", 'file.txt" ]`, and on Windows, the path
`C:\documents\file.txt` will be split into
`[ "C:", "\\", "documents", "file.txt" ]`.

## fs.rcopy
`fs.rcopy(source, destination, [metadata], [overwrite], [follow])`

Copies a directory `source` recursively to `destination`, a directory.
The directory `source` is copied to under `destination`; thus, for example,
`fs.rcopy("/foo","/bar")` copies `foo` such that there is another copy of it
under `/bar` (`/bar/foo`...). If the destination directory already exists,
the existing files in it are kept.
If a (sub)directory already exists, nothing happens; if a file already exists,
it is not copied, unless `overwrite` is `true`, in which case it is overwritten.
If `overwrite` is true and a file would be overwritten by a file of another
type (such as a regular file with a directory), an error occurs.
`metadata` is applied to every copy as if it were used for `fs.copy`.

A failure during copy will leave already copied files extant.

By defaults, symbolic links are copied as-is. Use `follow` to follow symlinks
instead.

## fs.rdestroy
`fs.rdestroy(path)`

Removes the link at `path`; if it is a directory, `rdestroy` is
applied recursively to its contents.

Symbolic links are deleted and are not followed.

## fs.realpath
`fs.realpath(path)`

Returns the given path as a normalized canonical path. May be different
from `abspath` if `path` points to a symbolic link; in case of cycles, the
function may fail.

## fs.relpath
`fs.relpath(path, [cwd])`

Returns the given path as a path relative to `cwd`, or the current working
directory if not specified. Both paths given should be absolute (or will
be converted into such) and are assumed to point to directories.

## fs.remove
`fs.remove(path)`

Removes the link at `path`. If it is the only remaining hard link to a file
(such as on systems without the concept of multiple hard links), the file
stored in `path` is deleted. If `path` is a directory, it must be empty.

## fs.scan
`fs.scan(path)`

An iterator to all of the names of the files under the directory `path`. The
returned string is the path name including `path`, which may be absolute or
relative and will be absolute or relative respectively in the returned paths.
Subdirectories are not iterated over.
The order of files is that provided by the underlying file system API. Any
changes done to the file system during iteration results in undefined behavior
(files added or removed may or may not be included).

## fs.sep
`fs.sep`

A character that acts as the directory separator for paths on the current
platform; examples include `"/"` for *nix systems and `"\\"` on Windows.

## fs.setcwd
`fs.setcwd(path)`

Changes the current working directory based on a path that is by default
interpreted as relative to the current working directory. Returns the path of
the current working directory as if by a call to `fs.getcwd()`.

## fs.stat
`fs.stat(path)`

Returns information about the file or directory identified by `path`, or throws
an error if not accessible. The returned value is a table or object with
one or more of the following keys, whichever of them are accessible for the
given path and/or platform:

`stat` follows symbolic links. To stat a symbolic link, use `lstat`.

* `type`: a string. always present. one of the following:
  * `file` for regular files
  * `dir` for directories
  * `block` for block devices
  * `char` for character devices
  * `fifo` for FIFOs / named pipes
  * `link` for symbolic links
  * `socket` for sockets
  * `mqueue` for message queues
  * `sem` for semaphores
  * `sharedmem` for shared memory objects
  * `typedmem` for typed memory objects
  * `other` for other files (corner case)
* `size`: size in bytes.
* `atime`: time of last access as a Unix timestamp.
* `mtime`: time of last modification as a Unix timestamp.
* `ctime`: time of creation or last status change as a Unix timestamp.
* `inode`: a unique "serial number" that identifies the file or directory
  within the file system.
* `device`: the ID of the device that the file is on.
* `links`: number of hard links pointing to this file.
* `uid`: the ID of the user that owns the file.
* `gid`: the ID of the group that owns the file.
* `mode`: file mode (permission bits) as a bit mask. if available, `fs`
  defines the following bit masks that can be used with bitwise AND (`&`) to
  check bits from `mask`:
    * `fs.S_IRUSR`: user read permission bit
    * `fs.S_IWUSR`: user write permission bit
    * `fs.S_IXUSR`: user execute permission bit
    * `fs.S_IRGRP`: group read permission bit
    * `fs.S_IWGRP`: group write permission bit
    * `fs.S_IXGRP`: group execute permission bit
    * `fs.S_IROTH`: other read permission bit
    * `fs.S_IWOTH`: other write permission bit
    * `fs.S_IXOTH`: other execute permission bit
    * `fs.S_ISUID`: setuid bit (assume owner user ID during execution)
    * `fs.S_ISGID`: setgid bit (assume owner group ID during execution)
    * `fs.S_ISVTX`: sticky bit
