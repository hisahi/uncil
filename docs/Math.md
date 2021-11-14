
# Mathematics library

Documentation for the builtin mathematics library.

This module can usually be accessed with `require("math")`.

## math.PI
`math.PI`

pi, the mathematical constant.

## math.E
`math.E`

e, the mathematical constant that represents the base of the
natural logarithm.

## math.sign
`math.sign(x)`

Returns the sign of `x` as an integer; `0` if `x` is zero, `1` if
`x` is positive and `-1` if `x` is negative. Returns NaN for a NaN input.

## math.abs
`math.abs(x)`

Given an integer or floating-point number `x`, returns its absolute value with
the same type, unless the value will not fit within that type in which case it
will be returned as a floating-point number.

## math.deg
`math.deg(x)`

Returns the angle `x`, expressed in radians, as degrees.

## math.rad
`math.rad(x)`

Returns the angle `x`, expressed in degrees, as radians.

## math.sin
`math.sin(x)`

Returns the sine of the angle `x` specified in radians. The sine function
is periodic and repeats with a period of `2 * math.PI`.

## math.cos
`math.cos(x)`

Returns the cosine of the angle `x` specified in radians. The cosine function
is periodic and repeats with a period of `2 * math.PI`.

## math.tan
`math.tan(x)`

Returns the tangent of the angle `x` specified in radians. The tangent function
is periodic and repeats with a period of `2 * math.PI`.

## math.asin
`math.asin(x)`

Returns the arcsine of `x` (which must be a floating point number between
-1.0 and 1.0) in radians. The return value will be between `-math.PI / 2`
and `math.PI / 2` (inclusive).

## math.acos
`math.acos(x)`

Returns the arccosine of `x` (which must be a floating point number between
-1.0 and 1.0) in radians. The return value will be between 0
and `math.PI` (inclusive).

## math.atan
`math.atan(x)`

Returns the arctangent of `x`. The return value will be between `-math.PI / 2`
and `math.PI / 2` (inclusive).

## math.sinh
`math.sinh(x)`

Returns the hyperbolic sine of `x`.

## math.cosh
`math.cosh(x)`

Returns the hyperbolic cosine of `x`.

## math.tanh
`math.tanh(x)`

Returns the hyperbolic tangent of `x`.

## math.asinh
`math.asinh(x)`

Returns the inverse hyperbolic sine of `x`.

## math.acosh
`math.acosh(x)`

Returns the inverse hyperbolic cosine of `x`.

## math.atanh
`math.atanh(x)`

Returns the inverse hyperbolic tangent of `x`.

## math.atan2
`math.atan2(y, x)`

Returns the arctangent of `y / x` as an angle between `-math.PI` and `math.PI`,
adjusted to the correct quadrant according to the signs of `y` and `x` and
also correctly handling the case where `x` equals zero.

## math.hypot
`math.hypot(x, y)`

Returns the length of the hypotenuse on a right triangle with side lengths `x`
and `y`. Equivalent to `math.sqrt(x * x + y * y)`, but with better handling
for edge and corner cases.

## math.sqrt
`math.sqrt(x)`

Returns the square root of `x`. Domain error occurs if `x` is negative.

## math.exp
`math.exp(x)`

Returns `math.E`, the base of the natural logarithm, raised to the power `x`.

## math.log
`math.log(x)`

Returns the natural logarithm of `x`, i.e. the number `y` such that `math.E`
raised to the power `y` equals `x`. A domain error occurs with negative `x`.

## math.log10
`math.log10(x)`

Returns the base-10 logarithm of `x`. i.e. the number `y` such that 10
raised to the power `y` equals `x`. A domain error occurs with negative `x`.

## math.log2
`math.log2(x)`

Returns the base-2 logarithm of `x`. i.e. the number `y` such that 2
raised to the power `y` equals `x`. A domain error occurs with negative `x`.

## math.pow
`math.pow(x, y)`

Returns `x` raised to the power of `y`. `pow(0, 0)` is defined as `1`. A
negative `x` with a fractional `y` results in a domain error.

## math.floor
`math.floor(x)`

Rounds `x` down (towards the negative infinity) to the nearest integer.

## math.ceil
`math.ceil(x)`

Rounds `x` up (towards the positive infinity) to the nearest integer.

## math.round
`math.round(x)`

Rounds `x` to the nearest integer according to standard mathematical rules
in which values are rounded to the nearest integer, and numbers with
a fractional part of 0.5 are rounded away from zero.

## math.trunc
`math.trunc(x)`

Truncates `x` to its integer part; i.e. rounds it to the nearest integer
towards zero.

## math.min
`math.min(x...)`

Returns the smallest of all of the numbers given as arguments.

## math.max
`math.max(x...)`

Returns the largest of all of the numbers given as arguments.

## math.clamp
`math.clamp(x, a, b)`

Returns `x` clamped within `[a, b]`, i.e. returns `a` if `x` is less than `a`,
`b` is `x` is greater than `b`, and `x` if it is between the two. If `a > b`,
the two numbers `a`, `b` are swapped before the rest of the function.
