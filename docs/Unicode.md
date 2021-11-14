
# Unicode library

Documentation for the builtin Unicode handling library. The Uncil interpreter
must have been compiled with Unicode support.

This module can usually be accessed with `require("unicode")` unless Unicode
support has not been compiled in.

## unicode.assigned
`unicode.assigned(c)`

Returns whether the code point of the first character in the string `c`
is assigned (including if it is a private-use character or a surrogate pair,
but not noncharacters).

## unicode.bidi
`unicode.bidi(c)`

Returns the Unicode bidirectional class of the first character in the
string `c`. The return value is the standard abbreviation (such as `L` or `EN`
or `null` if no category could be found.

## unicode.block
`unicode.block(c)`

Returns the name of the Unicode block containing the first character in the
string `c`, or `null` if not available.

## unicode.category
`unicode.category(c)`

Returns the Unicode character category of the first character in the string `c`.
The return value is a two-character string (such as `Lu` or `Nd`) or `null`
if no category could be found.

## unicode.combining
`unicode.combining(c)`

Returns the Unicode canonical combining class of the first character in the
string `c`. The return value is an integer value or `null`
if no category could be found.

## unicode.decimal
`unicode.decimal(c)`

Returns the decimal value associated with the code point of the
first character in the string `c`, or `null` if not found.
This is used for normal decimal digits.

## unicode.decompose
`unicode.decompose(c)`

Returns the assigned decomposition mapping for the first character
in the string `c`. The return value is `null` if not defined, but if defined,
is an array of integers each corresponding to a Unicode code point.

## unicode.digit
`unicode.digit(c)`

Returns the digit value associated with the code point of the
first character in the string `c`, or `null` if not found.
This is used for special decimal digits (such as superscripts).

## unicode.eawidth
`unicode.eawidth(c)`

Returns the Unicode East Asian width class of the first character in the
string `c`. The return value is an abbreviation (such as `W`) or `null`
if no category could be found.

## unicode.glength
`unicode.glength(s)`

Returns the number of _graphemes_ in the string `s`.

## unicode.graphemes
`unicode.graphemes(s)`

Returns the list of graphemes in the string `s` as an array.

## unicode.icmp
`unicode.icmp(s, s2, [locale])`

Compares the two strings `s` and `s2` according to the given `locale` (or
the current locale if not given) in a case-insensitive manner, and returns
`0` if they are equal, `-1` if s "comes before" s2 or `1` if s "comes after"
s2.

If a `locale` is given, it should be a string that contains a locale ID in
the standard locale format: an ISO 639-1 or ISO 639-3 language code in
lowercase, possibly followed by an underscore and an ISO 3166 country code
in uppercase.

## unicode.lookup
`unicode.lookup(name)`

Returns the code point with the name `name`, or `null` if not found.

## unicode.lower
`unicode.lower(s, [locale])`

Converts the code points in the string `s` to lower case and returns the
converted string. The conversion occurs according to rules in the current
locale unless another locale is given.

See `unicode.icmp` for locale information.

## unicode.mirrored
`unicode.mirrored(c)`

Returns whether the bidirectional mirrored flag is set for the first character
in the string `c`. The return value is `false` if not defined.

## unicode.name
`unicode.name(c)`

Returns the canonical Unicode code point name of the first character in the
string `c`, or `null` if not found.

## unicode.noncharacter
`unicode.noncharacter(c)`

Returns whether the code point of the first character in the string `c`
represents a noncharacter. Most noncharacters are ones where the low 16 bits
are `0xFFFE` or `0xFFFF`, but there are also 32 noncharacters in the range
`U+FDD0` - `U+FDEF`.

## unicode.normalize
`unicode.normalize(s, form)`

Normalizes the string `s` according to Unicode rules and returns the normalized
string. `form` must be any of the following:
* `"C"`, `"NFC"` (case-insensitive): NFC
* `"D"`, `"NFC"` (case-insensitive): NFD
* `"KC"`, `"NFKC"` (case-insensitive): NFKC
* `"KD"`, `"NFKD"` (case-insensitive): NFKD

## unicode.numeric
`unicode.numeric(c)`

Returns the numeric value associated with the code point of the
first character in the string `c`, or `null` if not found.
This is used for other numerals, such as non-decimal digits, CJKV characters
representing numerals or fractions.

## unicode.private
`unicode.private(c)`

Returns whether the code point of the first character in the string `c`
represents a private-use character.

## unicode.surrogate
`unicode.surrogate(c)`

Returns whether the code point of the first character in the string `c`
represents one half of a surrogate pair, a special character pair used to
represent non-BMP characters in UTF-16 and which are invalid in other
Unicode encodings as well as individually. (Note that Uncil lets you store
surrogate pair characters in strings just fine)

## unicode.title
`unicode.title(s, [locale])`

Converts the code points in the string `s` to title case and returns the
converted string. The conversion occurs according to rules in the current
locale unless another locale is given.

See `unicode.icmp` for locale information.

## unicode.trim
`unicode.trim(s)`

Returns the string `s` with its initial and final white space characters
stripped out. In other words, it decomposes a string `s` into three parts
`AXB` where `A` and `B` only consist of white space characters. The string
is considered to consist of these three parts concatenated into one, and then
only `X` is returned. If `s` only contains white space characters, an empty
string is returned.

## unicode.upper
`unicode.upper(s, [locale])`

Converts the code points in the string `s` to upper case and returns the
converted string. The conversion occurs according to rules in the current
locale unless another locale is given.

See `unicode.icmp` for locale information.

## unicode.version
`unicode.version`

The version of the Unicode data as a string, such as `"14.0"` for
Unicode 14.0(.0), or `null` if Unicode features are not available.
