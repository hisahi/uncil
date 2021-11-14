
# Regular expression library

Documentation for the builtin regular expression library that implements
PCRE-like regexes. Whether all PCRE features are supported depends on
the platform.

This module can usually be accessed with `require("regex")`.

## regex.compile
`regex.compile(pattern, [flags])`

Compiles a regular expression `pattern` into a compiled regex object
(of an opaque type `regex.pattern`).

`flags` may be either `null` or a string containing one or more of
the following flags in any order:
* `i` (case insensitive)
* `m` (multiline)
* `s` (dotall)

## regex.engine
`regex.engine()`

Returns the currently used regular expression engine that was compiled into
the Uncil interpreter. Possible options are:
* `"pcre2"`: pcre2
* `null`: no engine available

## regex.escape
`regex.escape(text)`

Escapes `text` so that it can be used as a literal text in regular expression
patterns.

## regex.escaperepl
`regex.escaperepl(text)`

Escapes `text` so that it can be used as a literal text as the replacement
for `regex.replace`.

## regex.find
`regex.find(text, pattern, [flags], [index])`

Finds the first match of `pattern` (either a string or a `regex.pattern`)
from `text`, Returns two values; the index and an array containing
each of the captured groups, or -1 and `null` if there were no matches.
If `index` is specified, begins the search in `text` at that code point index.
`flags` are ignored if `pattern` is a compiled pattern.

## regex.findall
`regex.findall(text, pattern, [flags], [index])`

Finds all non-overlapping matches of `pattern` (either a string or a
`regex.pattern`) from `text`, Returns a list of matches, where each item is a
list containing the same two values as `regex.find` would return (the first item
is the index and the second item is an array of captured groups).
`flags` are ignored if `pattern` is a compiled pattern.

## regex.findlast
`regex.findlast(text, pattern, [flags], [index])`

Finds the last match of `pattern` (either a string or a `regex.pattern`)
from `text`. Returns two values; the index and an array containing
each of the captured groups, or -1 and `null` if there were no matches.
If `index` is specified, begins the search in `text` at that code point
index (proceeding backwards).
`flags` are ignored if `pattern` is a compiled pattern.

## regex.match
`regex.match(text, pattern, [flags])`

Returns whether `text` as a whole matches the given `pattern` (either a string
or a `regex.pattern`). `flags` are ignored if `pattern` is a compiled pattern.

If the text matches, returns `true` and the list of matches as an array.
If it doesn't, returns `false` and `null`.

## regex.replace
`regex.replace(text, pattern, replacement, [flags], [replacements])`

Replaces matches of `pattern` (either a string or a `regex.pattern`) within
`text` with `replacement`, which may be a string or a function. If it is a
string, captured groups (including group 0, the entire match) may be represented
with the `$` sign, as such as `$1` for the first captured group. `$$` can be
used for a literal `$` and `$<`...`>` to encode multidigit capture groups
(group 10 is `$<10>`). If `replacement` is a function, it is given a list of
captures as anarray  argument, and the function should return the replacement
text. Returns the string with the replacements.
`flags` are ignored if `pattern` is a compiled pattern.

If `replacements` is given, at most that many replacements will be performed.
If it is positive, replacements are done from the beginning to the end of
`text`; if it is negative, replacements are done from the end to the beginning
(e.g. -1 means replace only the last match).

## regex.split
`regex.split(text, pattern, [flags], [splits])`

Splits the string `text` using `pattern` (either a string or a `regex.pattern`)
as a separator and returns the split tokens as an array.
`flags` are ignored if `pattern` is a compiled pattern.

If `splits` is given, at most that many splits will be done, and the array
will then have at most `splits + 1` items.
If it is positive, splits are done from the beginning to the end of
`text`; if it is negative, splits are done from the end to the beginning.

## regex.pattern
`regex.pattern`

A table used to identify compiled regular expressions (as they have this
table as their prototype).
