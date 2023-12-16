
# Conversion library

Documentation for the builtin conversion library that is used to convert
between binary representations and/or character encodings.

This module can usually be accessed with `require("convert")`.

## convert.decode
`convert.decode(bl, format, [offset])`

Decodes data from `bl` according to the specified `format`. See `convert.encode`
for how to specify `format`. Returns the number of bytes read in total followed
by all of the decoded values, or an error if there was an end-of-file. If
`offset` is given, it should be an integer that tells the offset into the
blob from which to start reading.

## convert.decodeb64
`convert.decodeb64(s, [altchars], [allowwhitespace])`

Decodes a Base64 string `s` into a blob and returns it. The character set used
begins with the uppercase alphabet A-Z, then the lowercase alphabet a-z,
digits 0-9 and two special characters. By default, the two special characters
are set to `+/`, but this can be changed by specifying  `altchars`. If it is
`false` or `null`, `+/` are used; if it is `true`, `-_` (for URIs) are used.
Any invalid character causes an error; passing `true` for `allowwhitespace`
will allow (some) whitespace characters (which will be ignored).

## convert.decodetext
`convert.decodetext(bl, [encoding])`

Converts a blob `bl` into a string according to the specified character
encoding (or UTF-8 if `null` or not specified). See `encodetext` for more
info on supported encodings. Throws an error if the blob has invalid data for
the given encoding.

## convert.encode
`convert.encode(format, ...)`

Encodes the parameters according to the `format` and returns the encoded
data in the form of a blob.

`format` accepts the following specifications. Each character either encodes
or decodes a single value, or controls how encoding/decoding proceeds. The
default mode is that specified by `@`. Only a single mode is allowed at the
beginning of the format string:

* `@`: uses native mode in which all type sizes are as the compiler suggests,
  and types are likewise padded according to the same rules. native
  endianness is used.
* `=`: types are standard sizes (`char` = 1 byte, `short` = 2 bytes,
  `int` = 4 bytes, `long` = `long long` = 8 bytes, `float` = 4 bytes,
  `double` = 8 bytes, `bool` = 1 byte) without any padding, but native
  endianness is used.
* `<`: same as `=`, but forces little-endian mode.
* `>`: same as `=`, but forces big-endian mode.

The mode, if any, is then followed by one or more of any of the following
value specifiers:

* `b`: byte (`unsigned char`), as a blob. when encoding, the blob is either
  truncated or zero-padded to the specified length.
* `c`: `signed char` from/to an Uncil integer
* `C`: `unsigned char` from/to an Uncil integer
* `h`: `signed short` from/to an Uncil integer
* `H`: `unsigned short` from/to an Uncil integer
* `i`: `signed int` from/to an Uncil integer
* `I`: `unsigned int` from/to an Uncil integer
* `l`: `signed long` from/to an Uncil integer
* `L`: `unsigned long` from/to an Uncil integer
* `q`: `signed long long` from/to an Uncil integer, if available
* `Q`: `unsigned long long` from/to an Uncil integer, if available
* `?`: `bool` from/to an Uncil boolean
* `f`: `float` from/to an Uncil float
* `d`: `double` from/to an Uncil float
* `D`: `long double` from/to an Uncil float (note possible loss of precision;
  only available with `@`)
* `Z`: `size_t` from/to an Uncil integer (only available with `@`)
* `p`: `void *` from/to an Uncil opaqueptr (only available with `@`)
* `*`: byte (`unsigned char`) which are skipped on decoding and filled with
  zeroes on encoding.

If a decoded value is too large to by represented by a target type (such as if
an unsigned value is out of range of the integer type), an error occurs.

All specifiers may also be preceded by an integer to express a repeat count
(or a blob size for `b` and `*`).

As an example, the following structure
```
struct example {
    int i;
    float f;
    char name[8];
}
```

could be represented as `if8b`.

## convert.encodeb64
`convert.encodeb64(bl, [altchars])`

Encodes a blob `bl` into a Base64 string and returns it. The character set used
begins with the uppercase alphabet A-Z, then the lowercase alphabet a-z,
digits 0-9 and two special characters. By default, the two special characters
are set to `+/`, but this can be changed by specifying  `altchars`. If it is
`false` or `null`, `+/` are used; if it is `true`, `-_` (for URIs) are used.

## convert.encodetext
`convert.encodetext(str, [encoding], [terminate])`

Converts a string `str` into a blob according to the specified character
encoding (or `UTF-8` if `null` or not specified). If `terminate` is true,
the blob will also have a null terminator (`\0`) at the end; however, the
string may also contain null characters when it is encoded (and this should
be taken into account).

The following encodings are supported natively by Uncil. There is currently
no way to add support for character encodings at run time, but a method may
eventually be added:
* `"utf8"`: UTF-8
* `"utf16le"`: UTF-16, little-endian
* `"utf16be"`: UTF-16, big-endian
* `"utf32le"`: UTF-32, little-endian
* `"utf32be"`: UTF-32, big-endian
* `"latin1"`: ISO-8859-1 aka Latin 1
* `"ascii"`: ASCII (low 7 bits only)

## convert.format
`convert.format(format, ...)`

Formats a list of values using the given format string `format`. It uses a
C printf-like syntax where format specifiers are prefixed with `%`. All
the standard specifiers from C99 are supported except:
* the wide character `l` specifier, which is ignored.
* `%n` is not supported
* `%p` is not supported
* `*` in widths or precisions is not allowed
* POSIX positional arguments are supported (e.g. `%1$s`). Unlike in POSIX,
  it is possible to only have the second parameter be read, not just the
  first. If unnumbered specifiers follow numbered ones, they continue
  from where the previous parameter left off.

Note that string width and precision applies to Unicode codepoints.

## convert.fromhex
`convert.fromhex(str)`

Converts a string `str` containing bytes represented in hexadecimal form back
into a blob. `fromhex` accepts at least the `tohex` format, even if newlines
are replaced with space characters and/or uppercase letters replaced with
lowercase ones.

## convert.fromintbase
`convert.fromintbase(num, base)`

Converts a number `num`, either an integer or a floating-point number
(fractional part ignored), into a string in base `base`, which must be a
number between 2-36 (inclusive). The digits after 10 shall be uppercase
letters, in the order A-Z.

## convert.tohex
`convert.tohex(bl)`

Converts a blob `bl` into a string of hexadecimal bytes. The default format
groups the bytes such that there are 16 bytes on one line and that each byte,
represented as two hexadecimal digits (using uppercase letters), is separated
by a single space.

## convert.tointbase
`convert.tointbase(str, base)`

Converts a string `str` containing digits into a integer or floating-point
number (the latter only if necessary due to lack of range), taking in digits
in base `base`, which must be a number between 2-36 (inclusive). The digits
after 10 shall be uppercase or lowercase letters, in the order A-Z.
