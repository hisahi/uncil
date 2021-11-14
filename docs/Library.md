
# Main library

Documentation for the builtin main library. These are the default global
variables that are defined in an Uncil environment.

## array
_For the array library that is accessible through this object, see Array._
`array(iterable)`

Exhausts the iterator of an iterable, accumulates its return values into
a new array and returns that array. Roughly equivalent to

```
result = []
for value << iterable do
    result->push(value)
end
# result is returned
```

## blob
_For the blob library that is accessible through this object, see Blob._
`blob(size)`

Equivalent to `blob.new` (see Blob).

## bool
`bool(value)`

Converts a value into a boolean.

If `value` can have overloads, this function tries to call `__bool` which
is expected to take the object itself as a parameter and return a boolean.

## exit
`exit([exitcode])`

_Only available in the standard Uncil interpreter._

Exits the interpreter. By default, or if `exitcode` is `true` or `null`, uses
the successful exit code (0 on most platforms). If `exitcode` is `false`,
uses the generic failure exit code (usually 1). If `exitcode` is an integer,
uses it as the exit code. Any other value is printed to `stderr` and then the
generic failure exit code is used.

## float
`float(value)`

Converts `value` into a floating-point number and returns that number.

If `value` can have overloads, this function tries to call `__float` which
is expected to take the object itself as a parameter and return a float.

This callable also has some attributes:
* `float.min`: the smallest/least (most negative) _finite_ value `float`
  can represent. this value is always negative; compare `float.eps`.
* `float.max`: the greatest _finite_ value `float` can represent.
* `float.eps`: the smallest _positive_ value `float` can represent;
  i.e. the smallest value that is greater than zero.
* `float.inf`: positive infinity.
* `float.nan`: Not-a-Number (as defined in the IEEE 754 standard).
  Represents a "quiet NaN".

## getprototype
`getprototype(obj)`

If the given value is an object or opaque object, returns a reference to its
prototype which is either `null`, a table, another object or another
opaque object.

## input
`input([prompt])`

Reads a line of text from the standard input. Unless the standard input
has been piped in from elsewhere, this will wait until the user types in a
line into the console and presses return. If a prompt is given, it should be
a string; it will then be displayed to the user in some form, usually before
the line the user will type in.

In case of EOF, `input` returns `null`.

## int
`int(value)`

Converts `value` into an integer and returns that integer.

If `value` can have overloads, this function tries to call `__int` which
is expected to take the object itself as a parameter and return an integer.

This callable also has some attributes:
* `int.min`: the smallest/least (most negative) value `int` can represent,
  always less than -2147483647.
* `int.max`: the greatest value `int` can represent, always greater
  than 2147483647.

## object
`object([prototype, [initializer, [readonly]]])`

Creates a new Uncil object and returns it. If a prototype value is given,
the object shall have that prototype. The prototype must be either `null`,
a table, an object or an opaque object. If an initializer is given, it should
be a table that contains the list of keys and values to be assigned to the
object on creation.

If `readonly` is given and `true`, the object will be immutable. Attempts to
change or delete any of its attributes will silently fail.

## print
`print(values...)`

Converts each of the values given into a string (as `string` would) and then
prints them, one by one, into the standard output, separating each printed
value by a tab character, and finally ends the line with a newline character.
`print` is mostly useful for debugging and testing purposes, but can also be
used, alongside `input`, to implement a rudimentary user interface.

## quit
`quit()`

_Only available in the standard Uncil interpreter._

In interactive mode, exits the REPL. In non-interactive mode, equivalent to
`exit()`.

## require
`require(name)`

If a module called `name` (a string) is not cached, attempts to find a module
called `name`. If found, its code is run and all public variables declared
within will be gathered into an object, which is then returned and cached for
future calls of `require`. `require` can only import Uncil modules or
builtin modules, not C modules (for which see `sys.loaddl`).

If the module could not be found, a require error occurs.

See Modules for information about the module system and what `require`
actually does in an attempt to find the module.

## string
_For the string library that is accessible through this object, see String._
`string(value)`

Converts `value` into a string and returns that string.

If `value` can have overloads, this function tries to call `__string` which
is expected to take the object itself as a parameter and return a string.

## table
_For the table library that is accessible through this object, see Table._
`table(iterable)`

Exhausts the iterator of an iterable, accumulates its return values
(assumed to be two, key and value) into a new table and returns that
table. Roughly equivalent to

```
result = {}
for key, value << iterable do
    result[key] = value
end
return result
```

## throw
`throw(error)`
`throw(message)`
`throw(type, message)`

If a single argument is given, `throw` will throw it as an exception. Uncil
exceptions should conventionally be objects with at least the keys `type`
and `message`, both containing string values.

If a single argument is given but it is a string, `throw` will create a new
exception object with the `type` `"custom"` and the string as the `message`,
and then throw that as the error object.

If two arguments are given, they are taken as the `type` and the `message`
of the error, and such an error object will proceed to be thrown.

In any case, even if called incorrectly, `throw` will always result in an
error, and thus any code after it within the same scope (up to a function call
or `try` block) will not be executed (except for possible `__close` calls to
values in terminated `with` blocks).

## type
`type(value)`

Returns a string that represents the type of the `value` passed in.

The possible return values are:
* `"null"`
* `"bool"`
* `"int"`
* `"float"`
* `"string"`
* `"array"`
* `"table"`
* `"object"`
* `"blob"`
* `"function"`
* `"opaque"` (used for opaque objects)
* `"optr"` (used for opaque pointers)
* `"weakref"`
* `"boundfunction"` (used for functions bound with `->`)

## weakref
`weakref(value)`

Creates a weak reference to a value of a reference type and returns it. See
Types for more information on weak references.
