# Grammar
This document documents the grammar and syntax of Uncil in technical terms.

## EBNF
Terminals Identifier, LiteralInt, LiteralFloat and LiteralString are defined
in other sections.

```
Block ::= [ Statement { EndOfStatement Statement } ]

ListedExpression ::= '...' Identifier | Expression
ListedIdentifier ::= '...' Identifier | Identifier

IdentifierList ::= [ ListedIdentifier ] { ',' ListedIdentifier }
ExpressionList ::= [ ListedExpression ] { ',' ListedExpression }
AssignmentExpression ::= ExpressionList '=' ExpressionList

FunctionDefinition ::= ( '=' ExpressionList ) | ( Block 'end' )
Parameter ::= Identifier | (Identifier '=' Expression) | ('...' Identifier)
ParameterList ::= [ Parameter ] { ',' Parameter }
FunctionExpression ::= function [ Identifier ] ( [ ParameterList ] ) FunctionDefinition
FunctionStatement ::= [ 'public' ] function Identifier ( [ ParameterList ] ) FunctionDefinition
Deletable ::= Identifier | ValueExpression ( Indexing | Attribute )
DeletableList ::= [ Deletable ] { ',' Deletable }

IfStatement ::= 'if' Expression 'then' Block { ( 'elseif' | 'else' 'if' ) Expression 'then' Block } [ 'else' Block ] 'end'
DoStatement ::= 'do' Block 'end'
DeleteStatement ::= 'delete' DeletableList
ForStatement ::= 'for' ( Identifier '=' Expression ',' RelOp Expression [ ',' Expression ] | IdentifierList '<<' Expression ) 'do' Block 'end'
PublicStatement ::= 'public' Identifier { ( ',' Identifier ) | ( '=' Expression ) }
WhileStatement ::= `while` Expression `do` Block `end`
TryStatement ::= `try` Block `catch` Identifier `do` Block `end`
WithStatement ::= `with` AssignmentExpression `do` Block `end`

EndOfStatement ::= ';' | '\n'
Statement ::= Expression |
              AssignmentExpression |
              FunctionStatement |
              IfStatement |
              DoStatement |
              ForStatement |
              WhileStatement |
              TryStatement |
              WithStatement |
              DeleteStatement |
              PublicStatement |
              'break' |
              'continue' |
              'return' ExpressionList

FunctionCall ::= '(' ExpressionList ')'
Indexing ::= '[' Expression ']'
Attribute ::= '.' Identifier
AttributeBind ::= '->' Identifier

Atom ::= '(' Expression ')' | Identifier | LiteralInt | LiteralFloat | LiteralString | LiteralList | LiteralTable | 'null' | 'false' | 'true'
ExpressionAtom ::= Expression { FunctionCall | Indexing | Attribute | AttributeBind }
ExpressionUnary ::= { UnaryOp } ExpressionAtom
ExpressionBinary ::= ExpressionUnary BinaryOp ExpressionUnary
Expression ::= IfExpression | FunctionExpression | ExpressionBinary

RelOp ::= '==' | '!=' | '<' | '>' | '<=' | '>='
BinaryOp ::= '+' | '-' | '*' | '/' | '//' | '%' | '~' | '&' | '|' | '^' | '<<' | '>>' | 'and' | 'or' | RelOp
UnaryOp ::= '+' | '-' | '~' | 'not'
LiteralList ::= '[' ExpressionList ']'
TableValue ::= FunctionStatement | ( ( Identifier | LiteralInt | LiteralFloat | LiteralString | '(' Expression ')' ) ':' Expression )
LiteralTable ::= '{' [ TableValue ] { ',' TableValue } '}'
```

## Identifier
In standard ASCII based format, identifier must match the regex

`[A-Za-z_][0-9A-Za-z_]*`

Unicode support for identifiers may be added later.

## LiteralInt
LiteralInt consists of either:
- one or more decimal digits (0-9)
- 0x followed by one or more hexadecimal digits (0-9, A-F or a-f)
- 0o followed by one or more octal digits (0-7)
- 0b followed by one or more binary digits (0-1)

## LiteralFloat
LiteralFloat is equivalent to the format of floating-point numbers in the
"subject sequence" accepted by the C standard library function `strtod`
as defined in ISO/IEC 9899:1990.

## LiteralString
A string literal is delimited by double quotes (except those that are escaped).
Escaping is done by preceding a character with a backslash. The following
escapes are accepted:

- `\"`: a double quote `"` but which does not delimit a string literal.
- `\0`: a null character (that which has ASCII code 0 or Unicode codepoint U+0000).
- `\\`: a backslash `\` but which does not begin an escape. Thus, for example,
        `"\\\""` is a string literal that contains the two characters `\"`.
- `\` followed by newline: a newline.
- `\b`: the backspace character (U+0008, ASCII code 8)
- `\f`: the form feed character (U+000C, ASCII code 12)
- `\n`: the line feed character (U+000A, ASCII code 10)
- `\r`: the carriage return character (U+000D, ASCII code 13)
- `\t`: the tab character (U+0009, ASCII code 9)
- `\x`: followed by two hexadecimal digits. results in an Unicode character
        with the code point determined by the integer value of those two
        characters; the entire escape sequence is converted into a single
        code point.
- `\u`: followed by four hexadecimal digits. results in an Unicode character
        with the code point determined by the integer value of those four
        characters. the digits are not included in the final string;
        the entire escape sequence is converted into a single code point.
- `\U`: followed by eight hexadecimal digits. results in an Unicode character
        with the code point determined by the integer value of those eight
        characters, except that values that exceed the maximum Unicode
        code point are invalid. the digits are not included in the final string;
        the entire escape sequence is converted into a single code point.

# Binary operator precedences

Listed in order from lower precedence to higher precedence:

1. `or` (short-circuit)
2. `and` (short-circuit)
3. `|`
4. `^`
5. `&`
6. `==` | `!=` | `<` | `<=` | `>` | `>=`
7. `<<` | `>>`
8. `+` | `-` | `~`
9. `*` | `/` | `//` | `%`

All operators are left-associative.

## Relational operator chaining

Relational binary operators (`==`, `!=`, `<`, `<=`, `>`, `>=`) have special
chaining behavior. For example, instead of `a == b == c` being interpreted as
`(a == b) == c`, it will instead be interpreted as equivalent to
`(a == b) and (b == c)`, except `b` is evaluated only once and no
short circuiting is applied.
