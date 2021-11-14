# Operators

This document contains details on the behavior of operators in Uncil.

For overload semantics, see Types.

The behavior for operators on values with given types, if not specified,
results in a runtime error.

## Unary operators

### Unary positive

`+` is the unary positive operator.

* With numbers (either integer or floating-point), the result is that number.
* With overloadable values, the `__posit` overload is called.

### Unary negation

`-` is the unary numerical negation operator.

* With numbers (either integer or floating-point), the result is the negative
  value with the same magnitude. If the number is an integer and the negative
  does not fit in the integer type, it is automatically converted to a
  floating-point number.
* With overloadable values, the `__negate` overload is called.

### Unary bitwise inversion

`~` is the unary bitwise inversion operator.

* With integers, the result is the integer value with all bits inverted.
* With overloadable values, the `__invert` overload is called.

### Unary logical negation

`not` is the unary logical negation operator. The value is converted into
a boolean (see Types for semantics) and the result is the logical inverse
of that boolean value (false if that value is true, or else true).

## Binary operators

### Addition

`+` is the binary addition operator.

* With two integers, the result is the sum of the two integers. If the value
  does not fit in an integer, it is converted into a floating-point number.
* With two numbers (of which either or both are floating-point),
  the result is the sum of the two numbers.
* With overloadable values, the `__add` overload is called for the
  left-hand value, and if it fails, the `__add2` overload is called for the
  right-hand value.

### Subtraction

`-` is the binary subtraction operator.

* With two integers, the result is the difference of the two integers. If the
  value does not fit in an integer, it is converted into a floating-point number.
* With two numbers (of which either or both are floating-point),
  the result is the difference of the two numbers.
* With overloadable values, the `__sub` overload is called for the
  left-hand value, and if it fails, the `__sub2` overload is called for the
  right-hand value.

### Multiplication

`*` is the binary multiplication operator.

* With two integers, the result is the product of the two integers. If the value
  does not fit in an integer, it is converted into a floating-point number.
* With two numbers (of which either or both are floating-point),
  the result is the product of the two numbers.
* With overloadable values, the `__mul` overload is called for the
  left-hand value, and if it fails, the `__mul2` overload is called for the
  right-hand value.

### Division

`/` is the binary division operator.

* With two numbers, the result is the quotient of the two numbers.
  It is of floating-point type. If the right-hand value is zero,
  an error occurs.
* With overloadable values, the `__div` overload is called for the
  left-hand value, and if it fails, the `__div2` overload is called for the
  right-hand value.

### Integer division

`//` is the binary division operator.

* With two integers, the result is the quotient of the two integers.
  The result is rounded down (floored). If the right-hand value is zero,
  an error occurs.
* With two numbers (of which either or both are floating-point),
  the result is the quotient of the two numbers rounded down to the nearest
  integer. If the right-hand value is zero, an error occurs.
* With overloadable values, the `__idiv` overload is called for the
  left-hand value, and if it fails, the `__idiv2` overload is called for the
  right-hand value.

### Remainder

`%` is the binary remainder operator.

* With two integers, the result is the remainder of the two integers.
  If the right-hand value is zero, an error occurs.
  The sign of the result is that of the divisor. Thus,
  `(-1) % 3` equals `2`.
* With two numbers (of which either or both are floating-point),
  the result is the remainder of the two numbers. If the
  right-hand value is zero, an error occurs.
  The sign of the result is that of the divisor.
* With overloadable values, the `__mod` overload is called for the
  left-hand value, and if it fails, the `__mod2` overload is called for the
  right-hand value.

### Relational operators

`==`, `!=` are equality comparison operators that return a boolean value
by default.

The below behavior describes `==`:

* With overloadable values, the `__eq` overload is called for the
  left-hand value, and if it fails, the `__eq2` overload is called for the
  right-hand value.
* With two booleans, the values are checked for equality with usual rules.
* With two numbers, the numbers are tested for exact equality.
* With two strings, the strings are compared with a code point by code point
  comparison and the value is true only if both strings have the same
  code points in the same order.
* With two blobs, the two blobs are compared and the value is `true` only if
  they are the same size and contain the same data.
* With two opaque pointers, the pointers are compared for equality.
* With two reference types (array, table, object, opaque, function), reference
  equality is evaluated.
* Otherwise, the value is `false` (no error thrown).

`!=` is exactly the negation of the above. `a != b` -> `not (a == b)`.

`<`, `<=`, `>`, `>=` are value comparison operators.

* With overloadable values, the `__cmp` overload is called for the
  left-hand value, and if it fails, the `__cmp2` overload is called for the
  right-hand value. The return value should be a number. `<` will be true if
  that value is negative, `<=` if it is negative or zero, `>` if it is positive
  and `>=` if it is positive or zero.
* With two numbers, the numbers are compared as one would expect.
* With two strings, the strings are compared with a code point by code point
  comparison. The result is evaluated by lexicographical comparison; a string
  is "less than" another if one of its code points is lower than that of the
  other string at the same index or, if they would otherwise be equal,
  if it is shorter.
* With two blobs, the blobs are compared with a byte by byte comparison.
  The result is evaluated by lexicographical comparison; a blob is
  "less than" another if one of its bytes is lower than that of the other
  blob at the same index or, if they would otherwise be equal,
  if it is shorter.

Technical details: in Uncil, only `a < b` actually exists as an instruction;
the others are syntactical sugar for the `<` operator.
* `a <= b` -> `not (b < a)`
* `a > b` -> `b < a`
* `a >= b` -> `not (a < b)`

#### Relational operator chaining

Relational binary operators (`==`, `!=`, `<`, `<=`, `>`, `>=`) have special
chaining behavior. For example, instead of `a == b == c` being interpreted as
`(a == b) == c`, it will instead be interpreted as equivalent to
`(a == b) and (b == c)`, except `b` is evaluated only once and no
short circuiting is applied.

### Concatenation

`~` is the binary concatenation operator.

* With two strings, the result is the two strings concatenated (the right-hand
  string after the left-hand string).
* With two blobs, the result is the two blobs concatenated (the right-hand
  blob after the left-hand blob).
* With two arrays, the result is the two arrays concatenated (the right-hand
  array after the left-hand array).
* With overloadable values, the `__cat` overload is called for the
  left-hand value, and if it fails, the `__cat2` overload is called for the
  right-hand value.

### Bit shift

`<<`, `>>` are the bit shift operators.

* With two integers, the result is the left-hand integer shifted by a number
  of bits as specified by the right-hand value (which may not be negative).
  Right shifts are arithmetic (`-2 >> 1 == -1`).
  If the shift count exceeds the size, the result is `0` for left shift and
  `0` or `-1` (depending on the sign) for right shift.
* With overloadable values, the `__shl` overload is called for the
  left-hand value, and if it fails, the `__shl2` overload is called for the
  right-hand value.

`<<` has special behavior when in the condition for `for` loops. See Syntax
for more details.

### Bitwise AND

`&` is the logical/bitwise AND operator.

* With two booleans, the result is `true` only if both values are `true`.
* With two integers, the result is the bitwise AND of the two integers.
* With overloadable values, the `__band` overload is called for the
  left-hand value, and if it fails, the `__band2` overload is called for the
  right-hand value.

### Bitwise OR

`|` is the logical/bitwise OR operator.

* With two booleans, the result is `true` if either of the values is `true`
  or if both values are `true`.
* With two integers, the result is the bitwise OR of the two integers.
* With overloadable values, the `__bor` overload is called for the
  left-hand value, and if it fails, the `__bor2` overload is called for the
  right-hand value.

### Bitwise XOR

`^` is the bitwise XOR operator.

* With two integers, the result is the bitwise XOR of the two integers.
* With overloadable values, the `__bxor` overload is called for the
  left-hand value, and if it fails, the `__bxor2` overload is called for the
  right-hand value.

### Logical AND

`and` is a logical AND operator. Its result is the left-hand value if it
evaluates to `false` when converted into a boolean, otherwise it is the
right-hand value.

This operator exhibits short-circuiting behavior. If the left-hand value
evaluates to `false`, the right-hand side will not even be evaluated
(unlike with other operators).

### Logical OR

`or` is a logical OR operator. Its result is the left-hand value if it
evaluates to `true` when converted into a boolean, otherwise it is the
right-hand value.

This operator exhibits short-circuiting behavior. If the left-hand value
evaluates to `true`, the right-hand side will not even be evaluated
(unlike with other operators).

## Precedences

Binary operator precedences (from highest to lowest):
1. `or` (short-circuit)
2. `and` (short-circuit)
3. `|`
4. `^`
5. `&`
6. `==` | `!=` | `<` | `<=` | `>` | `>=`
7. `<<` | `>>`
8. `+` | `-` | `~`
9. `*` | `/` | `//` | `%`

All binary operators are left-associative.
