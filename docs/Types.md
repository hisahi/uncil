# Types

This document explains the types of Uncil and their behavior.

The full list of types in Uncil (not including possible internal types that
are not visible to programs) is as follows:

* null
* bool
* int
* float
* string
* blob
* array
* table
* object
* opaque
* opaqueptr
* weakref
* function
* bound function

`null`, `bool`, `int`, `float` and `opaqueptr` are _value types_ - their values
are stored directly under the value. All other types are _reference types_
which only contain a reference to an object that is allocated separately. This
has an impact of assignment -- assigning an array to another variable does not
(automatically) create a copy of that array.

## Null

The null type is a special type with only one value, `null`. Its semantic
meaning is to represent an absence of any other value.

## Bool

Bool is the boolean type with two values; `false` and `true`.

The following values are considered false (when converted to boolean):

* `null`.
* `false`.
* An integer or float that equals zero (of either sign).
* Empty string (which contains no code points).
* Empty blob (which is zero bytes long).
* Empty array.
* Empty table (no keys).
* Objects and opaques with a `__bool` overload that returns a false value.

Any other value is considered to be true.

## Int

`int` is a signed integer type. Its range depends on the values available on
the target platform, but the type will be able to represent at least all
integers between -2147483647 and 2147483647 (inclusive).

Arithmetic with integers has automatic type coercion into floating-point numbers
when the other value is a floating-point value or if the result is too large
to fit into the integer type. This does not apply to bitwise operations.

## Float

`float` is a floating-point number type. It usually corresponds to the
double-precision floating-point type (binary64) in the IEEE 754 standard.

## String

A `string` contains zero or more Unicode code points (characters). Strings
are internally stored as UTF-8. All code points are allowed, even the null
terminator (`\0`). Strings are immutable. Accessing by index will extract
individual code points; assigning by or deleting an index is not allowed.

Accessing strings with the `->` attribute access syntax will automatically
resolve to the global table with the string standard library (see String),
which is (usually) also accessible as the global variable `string`.

## Blob

A `blob` is an array of zero or more bytes. It can be used to safely
store arbitrary binary data. Blobs are mutable -- the array can be modified
and the changes will be reflected in other references to the same blob.

Accessing blobs with the `->` attribute access syntax will automatically
resolve to the global table with the blob standard library (see Blob),
which is (usually) also accessible as the global variable `blob`.

## Array

Arrays in Uncil are heterogeneous arrays that can contain values of any,
even different, types. Much like blobs, arrays are mutable. Arrays can also be
declared by using the bracket syntax; `[ 1, 2, 3 ]` creates an array of three
elements, which are all integers (1, 2 and 3 respectively). Individual elements
can be replaced by index (using `delete` also works but sets the value to
`null`; other values are not shifted, compare `array.remove`).

Accessing arrays with the `->` attribute access syntax will automatically
resolve to the global table with the array standard library (see Array),
which is (usually) also accessible as the global variable `array`.

Strings, blobs and arrays in Uncil use zero-based indexing.

## Table

Uncil uses _table_ to refer to associative arrays, i.e. data structures that
store key-value pairs. Tables are implemented as hash tables, which means that
keys must be of a hashable type (`null`, `bool`, `int`, `float` or `string`), ´
while values may be of any type. Due to using a hash table, the order of keys
within a table is not usually well defined and should not be relied upon.
Much like with arrays, there exists a syntax for declaring tables that is
somewhat similar to JSON (see Syntax for more).

Accessing tables with the `->` attribute access syntax will automatically
resolve to the global table with the table standard library (see Table),
which is (usually) also accessible as the global variable `table`.

## Object

Objects are very similar to tables, but are not designed to be open data
structures but rather represent objects with specific keys and values. Unlike
tables, objects may also have a _prototype_. A prototype can be either `null`,
a table, another object or an opaque object. If objects do not have a matching
attribute, the prototype will be checked (and if still not, its prototype, etc.)
Thus, prototypes actually form a chain with which attributes are resolved.

Objects can be used to create object-oriented structures in Uncil.

Functions in objects will by default be called as if they were any other
function. By using `->` instead of `.` to access attributes, the object itself
will implicitly passed as the first argument; thus, with an object `obj`,
doing `obj->method(foo)` is roughly equivalent to doing `obj.method(obj, foo)`.

### Overloading

There are some reserved method or value names that can add special behaviors
to objects. These values or methods must be present in the prototype; any
fields or methods within the object itself are ignored when resolving overloads.

What follows is a full list of overloads. First, miscellaneous:
* `__name`: a field containing a string that contains the name of the
  "type" or "class" of the object.
* `__bool(value)`: called to convert a value into a boolean.
* `__int(value)`: called to convert a value into an integer when `int` is used.
  implicit conversions to int are not currently supported.
* `__float(value)`: called to convert a value into a floating-point value when
  `float` is used. implicit conversions to float are not currently supported.
* `__string(value)`: called to convert a value into a string.
* `__quote(value)`: called to convert a value into a string when `quote`
  is used.
* `__call(value, ...)`: this function is called if there is an attempt to call
  the object itself. any arguments after the value itself are those that the
  object was called with.
* `__iter(value)`: returns the iterator of an object; see `for` under Syntax.
* `__open(value)`: called for values when their context begins
  (see `with` under Syntax).
* `__close(value)`: called for values when their context ends
  (see `with` under Syntax).
* `__getindex(value, index)`: called when an object is indexed with the
  bracket syntax (not with the dot syntax). if the overload does not return
  any values, it is treated as "index does not exist"; otherwise the value
  returned is the value with that index.
* `__setindex(value, index, newvalue)`: called when an object is indexed with
  the bracket syntax (not with the dot syntax) and a value is assigned.
  any return values are ignored.
* `__delindex(value, index)`: called when an object is indexed with the
  bracket syntax (not with the dot syntax) and deleted with `delete`.
  any return values are ignored.

Unary operator overloads:
* `__posit(value)`: overloads the unary `+` operator.
* `__negate(value)`: overloads the unary `-` operator.
* `__invert(value)`: overloads the unary `~` operator.

Binary operator overloads. Uncil will first attempt to resolve the primary
overload (without the 2 in the name) from the left-hand side of the computation,
and pass the other object as the second argument `rhs`. If this fails, it will
attempt to use the secondary overload (with the 2), pass the right-hand side
object as `rhs` and the other one as the first argument `lhs`.
* `__add(lhs, rhs), __add2(lhs, rhs)`: overloads the binary `+` operator.
* `__sub(lhs, rhs), __sub2(lhs, rhs)`: overloads the binary `-` operator.
* `__mul(lhs, rhs), __mul2(lhs, rhs)`: overloads the binary `*` operator.
* `__div(lhs, rhs), __div2(lhs, rhs)`: overloads the binary `/` operator.
* `__idiv(lhs, rhs), __idiv2(lhs, rhs)`: overloads the binary `//` operator.
* `__mod(lhs, rhs), __mod2(lhs, rhs)`: overloads the binary `%` operator.
* `__cat(lhs, rhs), __cat2(lhs, rhs)`: overloads the binary `~` operator.
* `__shl(lhs, rhs), __shl2(lhs, rhs)`: overloads the binary `<<` operator.
* `__shr(lhs, rhs), __shr2(lhs, rhs)`: overloads the binary `>>` operator.
* `__band(lhs, rhs), __band2(lhs, rhs)`: overloads the binary `&` operator.
* `__bor(lhs, rhs), __bor2(lhs, rhs)`: overloads the binary `|` operator.
* `__bxor(lhs, rhs), __bxor2(lhs, rhs)`: overloads the binary `^` operator.
* `__eq(lhs, rhs), __eq2(lhs, rhs)`: overloads the binary `==` operator, and
  its inverse, `!=`.
* `__cmp(lhs, rhs), __cmp2(lhs, rhs)`: provides binary comparisons. return a
  positive number for "lhs > rhs", negative for "lhs < rhs" and
  0 for "lhs = rhs".

Operator overloads should ideally follow intuitive rules and behave similarly
as the operators would with their supported types (see Operators).

## Opaque

An opaque object is, as the name suggests, an opaque blob of data that can only
be directly acted upon through the C API. Opaque objects may have prototypes
(and thus they too can have overloads), but do not have a key-value store like
normal objects do. In addition, opaque objects can also have a special
destructor that will be called before the opaque object is itself destroyed.

## Opaqueptr

Unlike opaque objects, an opaqueptr (opaque pointer) is simply a C `void *`
generic pointer encoded as an Uncil value. It has no special behaviors
on its own.

## Weakref

Weakref objects are weak references. They can be created out of any reference
values. Unlike all other references (such as of elements in an array), weakrefs
are _weak_ references in the sense that they do not prevent an object from
getting destroyed by garbage collection. A weakref is resolved by calling it
like a function, and it returns the object as a reference, or `null` if it has
since been destroyed.

## Functions

Functions are special values that can contain Uncil (or C) code, which can
be called with zero or more parameters and return zero or more values.
(See Syntax for more information on how to define functions.) In Uncil,
functions are first-class values.

Values (primarily functions) can be called by following a value with parentheses
and any possible arguments placed between these parentheses. At least as many
arguments must be given as the number of required parameters in the function
declaration. Function arguments are assigned to parameters much in the same way
assignment statements would.

Bound functions are automatically created by using the `->` attribute syntax.
When a bound function (i.e. method) is called, the value to which it has been
bound will automatically be passed as the first argument, followed by any
of the arguments given as part of the function call.

No tail-call optimization is currently performed with function calls, but it
may eventually be implemented.

## Errors

Errors are simply values like any other. The convention for errors is a
table or object with two strings under the keys `type` and `message`.

List of error types that are used by builtin errors in Uncil:
* `call`: Used for errors that relate to calling functions or other callables,
  such as when the number of arguments is wrong.
* `custom`: Used for custom errors from `throw` if no type was specified.
* `encoding`: Used for encoding-related errors, such as when invalid UTF-8
  is encountered.
* `interface`: Used for errors that pertain to invalid usage from the C API.
* `internal`: Used for internal errors.
* `io`: Used for I/O errors, such as those when trying to access a file.
* `math`: Used for arithmetic or other mathematical errors, such as
  domain errors with mathematical functions.
* `recursion`: Used when the maximum recursion depth is exceeded.
* `require`: Used when `require` fails to find a module.
* `syntax`: Used for syntax errors.
* `system`: Used for errors that pertain to the currently running system
  or platform.
* `type`: Used when an operation or function does not support values of
  the given type.
* `usage`: Used when an operation is used incorrectly.
* `value`: Used when a value is of the correct type, but otherwise invalid,
  for some operation or function.
