# Syntax

This document explains the syntax of Uncil.

## Expressions

Expression syntax is similar to many other languages, including C, Python, Lua
and JavaScript. Unary and binary operations exist:

* `variable`
* `variable + 2`
* `3 * 5 + 1`
* `3 * (5 + 1)`

In most cases, expressions may also be lists of expressions. Using parentheses
will coerce a list of values into a single value by picking the first value.

## Identifiers

For now, only ASCII identifiers are supported, but Unicode identifiers may be
supported in the future. ASCII identifiers consist of any number of alphanumeric
characters (A-Z, either in uppercase or lowercase) and/or underscores, but may
not begin with a digit. Identifiers are case-sensitive.

The following words are however reserved and may not be used as identifiers:

* `break`
* `catch`
* `continue`
* `delete`
* `do`
* `else`
* `end`
* `false`
* `for`
* `function`
* `if`
* `not`
* `null`
* `public`
* `return`
* `then`
* `true`
* `try`
* `while`
* `with`

## Strings

String literals are delimited by double quotes and support a variety of
escapes that can be expressed with a backslash:

- `\"`: a double quote `"` but which does not delimit a string literal.
- `\0`: a null character
- `\\`: a backslash `\`
- `\` followed by newline: a newline.
- `\b`: a backspace
- `\f`: a form feed
- `\n`: a line feed
- `\r`: a carriage return
- `\t`: a tab character
- `\xXX`: an Unicode code point U+00XX.
- `\uXXXX`: an Unicode code point U+XXXX.
- `\UXXXXXXXX`: an Unicode code point U+XXXXXXXX
                (values greater than 0x110000 are invalid).

## Numbers

Numerical constants can be integers or floating-point constants. Integers
are sequences of decimal digits, but other bases can also be used with prefixes
(`0b` for binary, `0o` for octal, `0x` for hexadecimal).

Floating-point numbers use the standard notation with decimal digits, an
optional decimal part and an optional exponent specifier (`e` or `E` followed
by an integer with a possible sign).

## Lists

Lists can be specified by using brackets and separating values with commas.
For example: `[1, 2, 3]`

## Tables

Tables can be specified by using braces and specifying key-value pairs with
a colon between the key and the value.
For example: `{a: 1, b: 2}` defines a table for which `a` corresponds to 1
and `b` corresponds to 2. By default, identifiers are considered strings,
but values can also be used as keys by using parentheses. Thus, `{a: 1}`
specifies a table with `a` = 1, but `{(a): 1` specified a table for which
whatever `a` evaluates to is a key with the value `1`.

Named functions may also be declared in tables without using a colon.

## Variables

Variables do not need to be declared separately, and they can simply be
created by assigning to them. They are local by default, but you can use
a `public` declaration to announce that you want some names to resolve to
public variables, even if you assign to them: `public a` means that `a` refers
to the public variable of that name from this point on (multiple identifiers
may be specified by separating with commas). `public`  lines may also assign
to a variable, but in that case only one variable may be specified at a time.

Local variables have lexical scope and once used can be used freely in the
function, including in any possible subfunctions (in which case a closure
is formed). If a local variable is used before it is assigned to, it will
attempt to resolve the public variable of the same name. If the local variable
is assigned conditionally, its value is taken as `null` if it is accessed before
it has been assigned to. Trying to access a nonexistent public variable will
result in a runtime error.

Objects can be called with the standard function call syntax: parentheses
within which there is a (possibly empty) list of expressions that will be
treated as arguments to a function.

Values can be indexed with the brackets within which an expression is present.
For example, `list[0]` with a variable `list` that is an array type would (try
to) access the first element of that array. Indexes can be assigned to as well.

The second way to index is by using the attribute indexing syntax, which uses
a dot followed by an identifier. A `value.attribute` would get the attribute
`attribute` from `value`. With tables, indexing with the attribute syntax is
equivalent to doing so with the bracket syntax, but this is not the case with
objects or opaque values (more on them under Types).

The third way to index is by using the bound attribute indexing syntax, which
is the same as above except using `->` instead of `.`. This syntax is designed
to only be used with functions. With strings, blobs, arrays and tables, this
will resolve to a builtin set of methods, while for objects and opaques the
behavior is closer to the normal dot attribute syntax (but see Types for a
more elaborate description).

## Functions

Functions are declared by using the `function` syntax. It can be followed (or
in some cases must be followed) by a name. After this, there must be a (possibly
empty) list of parameters enclosed by parentheses (which must be present even
if the list is empty). The function body then follows until it ends with an
`end`.

In addition, there is a shorthand syntax for functions that only return a
single expression. In this case, the right parenthesis for the parameter list
must immediately be followed by `=`. For example,

```
function double(x)
    return x * 2
end
```
can also be declared as
```
function double(x) = x * 2
```

Function declarations behave slightly differently depending on whether they
are inside an expression or not. If not (or they are directly inside
a table declaration), they must have a name, and the corresponding local
variable, public variable (if `function` is preceded by a `public`) or table
key is defined and assigned with the function.

Function parameters may also be optional and have default values. This is done
by assigning to a parameter name as if it were a variable. No required parameter
may follow an optional parameter. The default value is evaluated every time
the function is created (i.e. the function declaration is evaluated), but will
not be re-evaluated every time the function is called.

In addition, the final function parameter may also have an ellipsis preceding
the identifier. In this case, the "remaining" parameters (those that are
left after all required and any optional parameters have been filled) will be
stored in a list (which may be empty) and assigned under that identifier when
the function is called.

As an example of parameters, the following function declaration
```
function test(a, b = 2, ...c)
    print(a, b, c)
end
```

will print the following when called as such:

* `test(1)` -> `1, 2, []`
* `test(1, 4)` -> `1, 4, []`
* `test(1, 4, 9)` -> `1, 4, [ 9 ]`
* `test(1, 4, 9, 16)` -> `1, 4, [ 9, 16 ]`

Functions that do not use the shorthand syntax can return values by using
the `return` statement. Functions can even return multiple values, which
should be separated by commas.

Function declarations can also be used as expressions.

## Assignment

Assigning values is done with the `=` operator. Both sides of the assignment
may have a list of values separated by commas. Thus `a, b = 1, 2` will assign
`1` to `a` and `2` to `b`. If the right-hand side contains functions that return
multiple values (or other expressions that evaluate to multiple values),
those will be assigned one-to-one, in order, to assignables on the left-side.
There must be at least as many values to assign on the right-hand side as there
are assignables on the left-hand side, or an error occurs (the exception is if
only assigning to a single variable in which case a `null` is stored); any
remaining values on the right-hand side will be evaluated but not assigned
anywhere. Note that both sides of an assignment must have at least one token
(an assignment statement beginning with or ending in `=` is not valid).

## Ellipsis

The ellipsis operator acts differently depending on whether it is on the
left or right hand side of an assignment (the latter if in another expression).
If the latter, the ellipsis operator will take a list and automatically unpack
it, effectively dumping all of the values in that list into the current context.
The ellipsis operator may not be used in places that expect a single-value
expression.

If on the left-hand side, the ellipsis will store any remaining values as
a list. It may also be placed between other identifiers, but there may be
only one ellipsis on the left hand side of an assignment. Example:

```
a, ...b, c = 1, 2, 3, 4, 5
```

will assign `1` to `a`, `[ 2, 3, 4 ]` to `b` and `5` to `c`. The ellipsis
assignment may result in an empty list (`a, ...b, c = 1, 2` is valid,
assigns `1` to `a`, `[]` to `b` and `2` to c).

## Deletion

Values can be deleted by using the `delete` statement. It should be followed
by one or more variables or attribute/index expressions. A public variable
that is deleted is removed; a local variable that is deleted is set to `null`,
and the behavior of deleting an index or attribute depends on the type.

## Operators

List of unary operators:

* `+` (unary positive)
* `-` (unary negative)
* `~` (unary bitwise invert)
* `not` (unary logical negation)

List of binary operators:
* `+` (addition)
* `-` (subtraction)
* `*` (multiplication)
* `/` (floating-point division)
* `//` (integer division)
* `%` (remainder)
* `==` (comparison for equality)
* `!=` (comparison for nonequality)
* `<` (less-than comparison)
* `<=` (less-than-or-equal comparison)
* `>` (greater-than comparison)
* `>=` (greater-than-or-equal comparison)
* `<<` (bitwise left shift)
* `>>` (bitwise right shift)
* `&` (bitwise or logical AND)
* `^` (bitwise XOR)
* `|` (bitwise or logical OR)
* `and` (short-circuiting logical AND) 
* `or` (short-circuiting logical OR)
* `~` (concatenation)

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
Their behavior is defined in more detail under Operators.

## Comments

A `#` character (except in a string literal) begins a comment, and the rest
of the line is ignored by the parser.

`#<` and `>#` delimit block comments.

## Whitespace

New lines are valid whitespace only directly within parentheses, brackets or
braces; otherwise they represent the end of an expression or statement.

## Visibility

Local variables have lexical scope (but only functions are considered;
other types of blocks are not).

## Control structures

### if

`if` is used to introduce conditionally executed code blocks. `if` should
be followed by an expression and `then`. The code block after `then` (delimited
by an `end`) will be executed only if the expression evaluates to `true` when
converted into a boolean (see Types for the details).

The `if` statement may also have `else` conditions which will be skipped
if any block preceding one ends up being executed due to a truthy condition.
An `else` may be unqualified (it always runs if nothing else has) or qualified,
in which case the `else` is followed by another `if` condition `then`
(`else if` may also be spelled `elseif`).

Thus,
```
if condition_a then
    block_a
elseif condition_b then
    block_b
else
    block_c
end
```

will be executed as follows:
* If `condition_a` is true, run `block_a`.
* Else:
  * If `condition_b` is true, run `block_b`.
  * Else:
    * Run `block_c`.

Besides being a control block, `if` may also be used inline within expressions
in much the same way. The main difference is that if used in an expression, an
`else` block must be present.

### while

`while` is a loop block. It is similar to `if` except it does not support
`else`, but will loop as long as the condition is true; if the block inside
a `while` finishes (including via `continue` but not via `break`), the
beginning of the `while` block, including the condition,
will be evaluated again.

### for

`for` is a more advanced loop statement. There are two forms; numeric `for`
and iterator `for`.

Numeric `for` takes in a single identifier and range in the form
`variable = start, OP end[, step]`, where `OP` is a relational operator,
`start`, `end` and `step` are expressions that evaluate into numerical values.
The variable will be initialized with `start` and the loop will execute as long
as it `OP end`. For example, `i = 0, <10` will initialize `i` with `0` and run
the loop as long as `i` is less than `10`. After every iteration, `step` (`1`
if not specified) is added to the variable before the condition is evaluated.

Iterator `for` takes the form `variable[, variable...] << iterable`. Unlike
with numeric for, there might be multiple variables to assign to (even
ellipsis is supported). The `iterable` must be an array, iterable object
or a function. If it is an array, a new iterator (function) is created that
returns every element in that array (in fact, it returns two values; the value
and its index). If it is an iterable object (object with a `__iter` overload),
that overload is called and the iterator is the singular return value.
If `iterable` is a function, it is taken as the iterator.

The iterator is simply a function that returns one or more values. At the
beginning of every loop, the iterator is called. If it returns zero
values, the loop ends; else and its values are assigned as if by standard
assignment to the variables and the loop is executed.

The variable(s) declared in the `for` block's condition is/are always local.

### try

A `try` block can be used to specify a block to execute in case a code block
results in an error. It takes the form

```
try
    block
catch variable do
    block
end
```

Only exceptions in the block under `try` will be caught.
Only one `catch` expression is supported and it will catch errors of any type.
The error object is stored under `variable` if the `catch` block is executed.
If no exceptions occur, it is skipped.

### do

A `do` block is a simple block with no conditions that is executed only once
(unless `continue` is used).

### with

`with` blocks provide contexts or scopes for closable values. A `with`
is followed by an assignment statement in which one or more local variables is
assigned to (they must be (local) variables; assigning to attributes, indexes,
or as a list by ellipsis is not supported), after which there is a `do`
code block. When the `with` block begins, any values assigned right after
the `with` will be "opened"; if they have a `__open` overload, it will be
called. Respectively, when the `with` block ends (either normally or abruptly),
any values assigned right after the `with` will be "closed"; if they have a
`__close` overload, it will be called. If an error occurs during an `__open`,
`__close` will be called only for any values initialized up to that point.

An example of this can be found with files in the builtin io module:

```
with file = io.open("test.txt", "r") do
    file->seek(0, "end")
    print("file size", file->tell())
end
# the file is closed automatically
```

is roughly equivalent to

```
do
    file = io.open("test.txt", "r")
    getprototype(file).__open(file)
    file->seek(0, "end")
    print("file size", file->tell())
    getprototype(file).__close(file)
end
```

(except in case either of the file calls returns an error in which case the
`__close` function is executed regardless)

### break/continue

`break` and `continue` are supported inside `do`, `for`, `while` loops.
`break` will immediately exit that block without rerunning it, ending the loop,
while `continue` will immediately exit the block and re-evaluate the condition,
and the loop may continue if the conditions for it are met. Both statements
will only break or continue the innermost loop.
