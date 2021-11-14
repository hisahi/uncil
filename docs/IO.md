
# Standard I/O library

Documentation for the builtin input/output library that is used to read input
from and write output to files.

This module can usually be accessed with `require("io")`.

This module is disabled by default when Uncil is used as a library, but
available on the standalone interpreter; see `mmask` in `API.md`.

## io.newline
`io.newline`

A string representing the newline for the current operating system or platform.
Note that for text files, you don't have to replace `\n` by `io.newline`
manually - the conversion is done for you.

## io.open
`io.open(filename, mode, [encoding], [buffer])`

Opens a file. This is a wrapper around `fopen` (or a possible platform-specific
equivalent or such). `filename` is a string that contains a file name, or if
the platform supports it, an absolute or relative path ultimately resolving to
a file name.

`mode` is likewise also a string. It should begin with one of the following
three characters: `r` (read), `w` (write) or `a` (append), and it may be
followed by one or more of the following modifiers in any order: `+`, `b`, `x`.

The file modes represent the following (where `+` represents that mode with
the `+` modifier in the same string):
* `r`: read. the file can only be read, but not written. the file pointer
  can be freely moved around. the file must exist, or an error will occur.
* `w`: write. the file can only be written to. if the file already exists,
  it is truncated to zero bytes (all of its existing contents are wiped).
  the file pointer can be freely moved around.
* `a`: append. the file can only be written to. the file pointer is locked
  to the end of the file, and all writes end up expanding the file.
* `r+`: read/write. same as `r`, but write operations are also supported.
  the file must exist, or an error will occur.
* `w+`: read/write. same as `w`, but read operations are also supported.
  like `w` and unlike `r+`, `w+` can create new files, but it will also
  truncate any existing files.
* `a+`: read/append. same as `a`, but read operations are also supported.
  unlike `a`, the file pointer can be freely moved around, but like `a`, all
  writes take place at the end of the file (and cause the pointer to be moved
  there).

By default, files are opened in text mode; all reads return strings and writes
expect to be done as strings. The `b` modifier opens the files in binary mode
instead; reads return blobs and writes too expect blobs to be given.
Note that Uncil always tells the underlying API to open files in binary mode.

The `x` modifier can be used with `w`. If specified, `io.open` will fail if
the file already exists. Otherwise the (newly created) file is opened with
"exclusive access" to the extent the platform supports it.

The `encoding` parameter can be any string representing any of the supported
character encodings. If not specified or given as `null`, text files
are assumed to be in UTF-8. The value of the encoding parameter is ignored
with binary files. See `encodetext` under Convert for info on
supported character encodings.

The `buffer` parameter controls the buffering for the opened file. If not
specified or `null`, the default buffer is used. An integer will set the
buffer size to be that many bytes (with 0 disabling the buffer entirely).
Giving `"line"` will attempt to use line buffering.

The opened file is returned as an object with `io.file` as its prototype,
or if opening the file fails, an error occurs. The file has a `__close`
method which makes it possible to use with `with` blocks, and the file will
automatically be closed when the `with` block ends.

## io.pipe
`io.pipe()`

Creates a new pipe and returns two files; the first one for reading from
the pipe and the second one for writing to it. Both files are opened in
binary mode (by default) and have no buffering.

## io.stderr
`io.stderr`

The standard error stream, by default as an UTF-8 text stream.

To write binary data, use `io.stderr->setbinary(true)` to turn the stream
into a binary mode one.

## io.stdin
`io.stdin`

The standard input stream, by default as an UTF-8 text stream.

To read binary data, use `io.stdin->setbinary(true)` to turn the stream
into a binary mode one.

## io.stdout
`io.stdout`

The standard output stream, by default as an UTF-8 text stream.

To write binary data, use `io.stdout->setbinary(true)` to turn the stream
into a binary mode one.

## io.tempfile
`io.tempfile([named], [mode], [encoding], [buffer])`

Creates a new temporary file. The default `mode` is `w+b` (the file will
be in binary mode by default). For `encoding` and `buffer`, see `io.open`.
The existence of the temporary file is not guaranteed after it has been
closed; in most cases, it will be deleted at least when Uncil exits.

If named is specified as `true`, the temporary file wlil be guaranteed to have
a visible name on the file system; this is not guaranteed if it is `false`
(which is the default).

The function returns two values; the file object (as an `io.file` object)
and the (complete) file name, or `null` if no visible name was given
(as is possible when `named` is false).

## io.file

This sublibrary is used for files. All file objects returned by `io.open` have
this table as their prototypes.

### io.file.canread
`fd->canread()` = `io.file.canread(fd)`

Returns `true` if file `fd` is known to support read operations, and
`false` otherwise. It is not guaranteed that a read succeeds if `canread`
returns `true` or that it fails if `canread` returns `false`, but that
assumption can be made in most cases.

### io.file.canseek
`fd->canseek()` = `io.file.canseek(fd)`

Returns `true` if file `fd` is known to support seek operations, and
`false` otherwise. It is not guaranteed that a seek succeeds if `canseek`
returns `true` or that it fails if `canseek` returns `false`, but that
assumption can be made in most cases.

### io.file.canwrite
`fd->canwrite()` = `io.file.canwrite(fd)`

Returns `true` if file `fd` is known to support write operations, and
`false` otherwise. It is not guaranteed that a write succeeds if `canwrite`
returns `true` or that it fails if `canwrite` returns `false`, but that
assumption can be made in most cases.

### io.file.close
`fd->close()` = `io.file.close(fd)`

Closes file `fd`. If it is already closed, has no effect.

File objects also have a `__close` method that automatically calls `close`.

### io.file.flush
`fd->flush()` = `io.file.flush(fd)`

Flushes file `fd`; forces user-mode buffers to be written directly to the
underlying file. Should not be called for files opened in read-only modes,
but on most systems doing so has no effect.

### io.file.getbinary
`fd->getbinary()` = `io.file.getbinary(fd)`

Returns whether the file `fd` is in binary mode (`true`)
or in text mode (`false`).

### io.file.getencoding
`fd->getencoding()` = `io.file.getencoding(fd)`

Returns the name of the current character encoding for the file `fd`.

### io.file.iseof
`fd->iseof()` = `io.file.iseof(fd)`

Returns `true` if the file `fd` is at its end, or else `false`.

### io.file.isopen
`fd->isopen()` = `io.file.isopen(fd)`

Returns `true` if the file `fd` is open, or else `false`.

### io.file.lines
`fd->lines()` = `io.file.lines(fd)`

Returns an iterator for that iterates over the lines in file `fd` for use
with a `for` block. The iterator will simply call `fd->readline()` until
it returns `null` (for EOF).

### io.file.read
`fd->read(size)` = `io.file.read(fd, size)`

For binary files, reads `size` bytes from `fd` and returns them in a blob.
Fewer than `size` bytes may be returned if there are not enough bytes
available or remaining. If the file is at its end (end-of-file), this
function returns `null`.

For text files, reads `size` characters from `fd` and returns them as a string.
Fewer than `size` characters may be returned if there are not enough characters
available or remaining. If the file is at its end (end-of-file), this
function returns `null`. Enough bytes are read in order to accumulate `size`
characters. Invalid encodings will throw an error.

After the read, the file pointer will be at the first byte or character that
was not read.

### io.file.readall
`fd->readall()` = `io.file.readall(fd)`

For binary files, reads the rest of the file `fd` and returns the contents
in a blob. If the file is at its end (end-of-file), this function
returns `null`.

For text files, reads the rest of the file `fd` and returns the contents
as a string. If the file is at its end (end-of-file), this function
returns `null`. Enough bytes are read in order to accumulate `size` characters.
Invalid encodings will throw an error.

After the read, the file pointer will be at the end of the file.

### io.file.readbyte
`fd->readbyte()` = `io.file.readbyte(fd)`

Reads one byte from `fd` and returns it as a non-negative integer. If the file
is at its end (end-of-file), this function returns -1. This function is not
supported with files in text mode. The file pointer is incremented by one.

### io.file.readline
`fd->readline()` = `io.file.readline(fd)`

For binary files, reads bytes from `fd` until a newline character (`\n`) is
found, and returns them in a blob. If the file is at its end (end-of-file), this
function returns `null`.

For text files, reads characters from `fd` up until a newline (`\n`) and
returns the read characters as a string.  If the file is at its end
(end-of-file), this function returns `null`. Invalid encodings will
throw an error.

After the read, the file pointer will be at the first byte or character that
was not read.

### io.file.seek
`fd->seek(offset, [whence])` = `io.file.seek(fd, offset, [whence])`

Changes the position of the file pointer for file `fd`. `offset` must be
an integer, while `whence` must be one of the following:
* integer 0 or string `"set"` (default): the offset represents the number
  of bytes relative to the beginning of the file; an offset is 0 means
  "the beginning of the file".
* integer 1 or string `"cur"`: the offset represents the number
  of bytes relative to the current position; offset 0 means that the pointer
  is not moved, 1 moves it one byte forwards and -1 moves it one byte back.
* integer 2 or string `"end"`: the offset represents the number
  of bytes relative to the end of the file; an offset is 0 means "the end
  of the file", while -1 means "one byte before the end of the file".

### io.file.setbinary
`fd->setbinary(flag)` = `io.file.setbinary(fd, flag)`

Sets whether the file is in binary mode (`true`) or in text mode (`false`).

### io.file.setencoding
`fd->setencoding(encoding)` = `io.file.setencoding(fd, encoding)`

Changes the character encoding used by the file, if it is a text file.
Calling `setencoding` on a binary file changes the text encoding, which will
take effect if and when `setbinary(false)` is called.

`encoding` should be a string that corresponds to the name of a supported
character encoding. See `Convert` for information on the encodings that
Uncil supports by default.

### io.file.tell
`fd->tell()` = `io.file.tell(fd)`

Returns the position of the file pointer as an offset from the beginning of
the file. If the file is not seekable, throws an error.

If the file is seekable, It is guaranteed that `fd->seek(fd->tell())` does not
change the position of the file pointer.

### io.file.write
`fd->write(data)` = `io.file.write(fd, data)`

Writes `data` into the file at the current position.

If `fd` is a binary file, `data` must be a blob.

If `fd` is a text file, `data` must be a string. If the current file encoding
does not support some of the characters in the string, an error occurs.

The file position is incremented by however many bytes were written.

Writes may be buffered. Failing to flush or close the file before exiting
may cause writes to be lost. If there is a text encoding failure or I/O error
during write, "partial writes" may occur.

### io.file.writebyte
`fd->writebyte(byte)` = `io.file.writebyte(fd, byte)`

Writes `byte`, a non-negative value, into file `fd`, which must be a
binary file, at the current position.

The file position is incremented by one.

Writes may be buffered. Failing to flush or close the file before exiting
may cause writes to be lost.

### io.file.writeline
`fd->writeline(data)` = `io.file.writeline(fd, data)`

Writes `data` into the file at the current position.

If `fd` is a binary file, `data` must be a blob.

If `fd` is a text file, `data` must be a string. If the current file encoding
does not support some of the characters in the string, an error occurs.

Unlike `write`, in addition to the data, a newline character is written.

The file position is incremented by however many bytes were written.

Writes may be buffered. Failing to flush or close the file before exiting
may cause writes to be lost. If there is a text encoding failure or I/O error
during write, "partial writes" may occur.

