
# Uncil randomization library

Documentation for the builtin randomization library that provides means to
generate random data.

This module can usually be accessed with `require("random")`.

## random.random
`random.random([randomsource])`

Generates a random floating-point number between 0 (inclusive)
and 1 (exclusive). The number will be uniformly distributed within that range.
The generator uses the specified random source, which should be a function
that takes in a single nonnegative number and generates that many random
bytes, returning them as a blob. If not specified, `random.randombytes`
will be used.

## random.randombytes
`random.randombytes(size)`

Takes a nonnegative number and returns that many random bytes. This is the
default random data source.

This random number generator is not cryptographically secure and should not
be used in contexts where true, secure randomness is desired.

## random.randomint
`random.randomint(a, b, [randomsource])`

Given two integers `a`, `b`, such that `a < b`, returns a random integer
uniformly distributed within the range `[a, b[`. The `randomsource`
is as for `random.random`.

## random.securerandom
`random.securerandom(size)`

Takes a nonnegative number and returns that many random bytes by using
a platform-specific secure random number source. This source should be used
in contexts where cryptographically secure data is required.

If the generator is not supported, an error will be returned.

## random.shuffle
`random.shuffle(array, [randomsource])`

Takes in an `array` and shuffles it in place; the items in the array are
randomly permuted. The given array is modified in-place. If the array is
modified during the shuffle operation, undefined behavior occurs.
The `randomsource` is as for `random.random`.
