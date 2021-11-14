
# String library

Documentation for the builtin string library, accessible as the global
variable `string` (by default) as well as by using `->` on string objects.

## string.asciilower
`str->asciilower()` = `string.asciilower(str)`

Returns a new copy of `str` in which the ASCII alphabetic characters (letters
A-Z) have been converted into their lowercase variants. As the name suggests,
only those characters are affected; this does not affect other, notably
Unicode, characters. See the `unicode` library for Unicode-compatible
string handling.

## string.asciiupper
`str->asciiupper()` = `string.asciiupper(str)`

Returns a new copy of `str` in which the ASCII alphabetic characters (letters
a-z) have been converted into their uppercase variants. As the name suggests,
only those characters are affected; this does not affect other, notably
Unicode, characters. See the `unicode` library for Unicode-compatible
string handling.

## string.char
`string.char(codepoint)`

Encodes `codepoint` as UTF-8 and returns it as a string.

## string.charcode
`str->charcode([index])` = `string.charcode(str, [index])`

Returns the code point in the string at position `index` (0 if not specified).

## string.find
`str->find(str2, [index])` = `string.find(str, str2, [index])`

Finds the first instance of the substring `str2` from the string and returns its
index, or -1 if none were found. If `index` is specified, begins the search
in `str` at that code point index.

## string.findlast
`str->findlast(str2, [index])` = `string.findlast(str, str2, [index])`

Finds the last instance of the substring `str2` from the string and returns its
index, or -1 if none were found. If `index` is specified, begins the search
in `str` at that code point index (proceeding backwards).

## string.join
`str->join()` = `string.join(str, iterable)`

Returns a string in which all of the elements in `iterable` have been
converted into strings and concatenated into a single string where
each pair of items is separated by `str`.

## string.length
`str->length()` = `string.length(tbl)`

Returns the number of code points in the string `str`.

## string.repeat
`str->repeat(count)` = `string.repeat(str, count)`

Creates a new string that contains `count` repeats of the string `str` and
returns it.

## string.replace
`str->replace(str2, replacement, [replacements])`
= `string.replace(str, str2, replacement, [replacements])`

Replaces matches of `str2` (either a string or a `regex.pattern`) within `str`
with `replacement` (a string). Returns the string with the replacements.

If `replacements` is given, at most that many replacements will be performed.
If it is positive, replacements are done from the beginning to the end of
`str`; if it is negative, replacements are done from the end to the beginning
(e.g. -1 means replace only the last match).

## string.reverse
`str->reverse()` = `string.reverse(str)`

Returns a new string that contains the same code points as `str`,
but in reverse order. Note that this function does not reverse the order of
_graphemes_ ("characters"), but _code points_; thus combining characters,
for example, may end up on the "wrong character".

## string.size
`str->size()` = `string.size(tbl)`

Returns the number of bytes in the string `str`. Since Uncil strings are
encoded in UTF-8, this value is at least as large as the length, and the
two values are equal if and only if the string does not contain any
non-ASCII characters.

## string.split
`str->split(separator, [splits])` = `string.split(str, separator, [splits])`

Splits the string `str` using `separator` (a string) as a separator and
returns the split tokens as an array.

If `splits` is given, at most that many splits will be done, and the array
will then have at most `splits + 1` items.
If it is positive, splits are done from the beginning to the end of
`str`; if it is negative, splits are done from the end to the beginning.

## string.sub
`str->sub(begin, [end])` = `string.sub(str, begin, [end])`

Returns a copy of a contiguous section of `str`. `begin` is the index of the
first code point to include in the copy. It may be negative in which case the
length of the string is added to it; thus `-1` as `start` will only start
the copy from the _last_ code point in `str`.

If `end` is given, it is the index of the first code point after `begin` that is
_not_ included in the copy. If `end` is omitted, the copy will extend
from `begin` until the end of `str`.
