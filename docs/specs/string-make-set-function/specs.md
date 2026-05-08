# String `MAKE_SET()` Function

## Scope

This feature implements MySQL-compatible `MAKE_SET(bits, str1, str2, ...)`
as a scalar string function.

`MAKE_SET()` is available anywhere MyLite currently evaluates supported scalar
built-ins:

- no-table `SELECT`
- table-backed `SELECT` projection, `WHERE`, and `ORDER BY`
- supported single-table `UPDATE` assignments, predicates, and order keys
- supported single-table `DELETE` predicates and order keys

Out of scope:

- binary-string-specific collation aggregation beyond the current MyLite string
  metadata model
- `max_allowed_packet` result-size enforcement
- exact decimal/real string-conversion formatting for selected numeric members
- exact native error-code exposure for unsupported arity paths
- evaluation of argument positions beyond the low 64 bits; MyLite ignores bits
  above bit 63 until broader unsigned bit-vector behavior exists

## Sources

- MySQL 8.4 Reference Manual, String Functions and Operators:
  https://dev.mysql.com/doc/refman/8.4/en/string-functions.html
- MySQL 8.4 Reference Manual, Type Conversion in Expression Evaluation:
  https://dev.mysql.com/doc/refman/8.4/en/type-conversion.html
- MySQL 8.4 Reference Manual, Character Set and Collation of Function Results:
  https://dev.mysql.com/doc/refman/8.4/en/string-functions-charset.html
- Existing MyLite scalar-function design:
  `docs/specs/scalar-built-in-functions/specs.md`
- Existing MyLite string list/index function slice:
  `docs/specs/string-list-index-functions/specs.md`

Observed behavior was verified against MySQL 8.4.9 in Docker container
`mylite-mysql-849`, using:

- `docker exec -i mylite-mysql-849 mysql -uroot --batch --raw --show-warnings --force`
- `docker exec -i mylite-mysql-849 mysql -uroot --column-type-info -vvv --force`

Runtime probes used `SET NAMES utf8mb4` unless the metadata probe explicitly
switched to `latin1`.

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar,
documentation prose, or implementation sources.

## Syntax and arity

`MAKE_SET()` is a variadic scalar function with one mask argument and at least
one member argument. Zero-argument and one-argument calls raise native MySQL
error 1582. MyLite may continue to reject those paths through its existing
unsupported-function/arity diagnostic until exact native function diagnostics
are implemented globally.

Generic function-call grammar is sufficient. No dedicated token or AST node is
required.

The intended MyLite Lemon-style grammar remains:

```lemon
scalar_function_call ::= function_name LPAREN function_argument_list RPAREN.
function_name ::= identifier.
```

## Semantics

`MAKE_SET(bits, str1, str2, ...)` returns a comma-separated string containing
the non-`NULL` members whose corresponding zero-based bit is set:

- `str1` uses bit 0
- `str2` uses bit 1
- `str3` uses bit 2
- and so on

The output order is argument order, not numeric bit order after sorting. A
selected empty string is included as a real member, so later selected members
can produce leading or repeated commas. A selected member containing a comma is
not escaped or split; its bytes are appended as part of that member.

`NULL` member arguments still occupy their bit positions but are omitted from
the result when selected. If no selected non-`NULL` member remains, the result
is the empty string. If `bits` is SQL `NULL`, the result is SQL `NULL`.

`bits` is converted to an unsigned 64-bit mask using MySQL's integer conversion
behavior for this function:

- integer masks are used directly as unsigned bit patterns
- real masks round to the nearest integer, with halves away from zero, then use
  the resulting two's-complement unsigned bit pattern
- string masks parse an integer prefix, emit warning 1292 for invalid,
  fractional, trailing, or overflowing text, and then use the parsed unsigned
  bit pattern
- negative masks therefore select high and low bits according to unsigned
  two's-complement representation

Only selected member expressions are evaluated. Unselected members do not emit
warnings or side effects in the currently supported expression model. When
`bits` is `NULL`, no member expression is evaluated.

Observed examples:

| Expression | Result | Warnings |
| --- | --- | --- |
| `MAKE_SET(1,'a','b','c')` | `a` | none |
| `MAKE_SET(2,'a','b','c')` | `b` | none |
| `MAKE_SET(3,'a','b','c')` | `a,b` | none |
| `MAKE_SET(4,'a','b','c')` | `c` | none |
| `MAKE_SET(0,'a','b','c')` | empty string | none |
| `MAKE_SET(16,'a','b','c')` | empty string | none |
| `MAKE_SET(NULL,'a','b')` | `NULL` | none |
| `MAKE_SET(7,'a',NULL,'c')` | `a,c` | none |
| `MAKE_SET(7,'','dup','dup')` | `,dup,dup` | none |
| `MAKE_SET(3,'a,b','c')` | `a,b,c` | none |
| `MAKE_SET(3,'猫','海')` | `猫,海` | none |
| `MAKE_SET(-1,'a','b','c')` | `a,b,c` | none |
| `MAKE_SET(-1.5,'a','b','c')` | `b,c` | none |
| `MAKE_SET('2.5','a','b','c')` | `b` | one 1292 warning |
| `MAKE_SET(' 3x ','a','b','c')` | `a,b` | one 1292 warning |
| `MAKE_SET('x3','a','b','c')` | empty string | one 1292 warning |
| `MAKE_SET('-1','a','b','c')` | `a,b,c` | none |

Observed lazy evaluation:

- `MAKE_SET(0, 1/0, 'b')` returns the empty string without a division warning.
- `MAKE_SET(1, 1/0, 'b')` evaluates the first member, omits the resulting
  `NULL`, returns the empty string, and emits one division warning.
- `MAKE_SET(2, 1/0, 'b')` skips the first member and returns `b` without a
  division warning.
- `MAKE_SET(NULL, 1/0, 'b')` returns `NULL` without a division warning.
- `MAKE_SET(1, 'a', 1/0)` skips the second member and returns `a`.
- `MAKE_SET(3, 'a', 1/0)` evaluates the second member, omits the resulting
  `NULL`, returns `a`, and emits one division warning.

## Result metadata

`MAKE_SET()` reports `VAR_STRING` with MySQL's not-fixed decimals marker `31`.
The result charset/collation follows the string-member aggregation shape
currently used by MyLite string functions: ordinary text results use the
connection result collation, any binary member makes the result binary with
byte-counted member and comma widths, and all-`NULL` member lists use binary
metadata.

The display length is the sum of member display lengths plus one separator
character between adjacent member positions. This is a maximum declaration
length, independent of the runtime bit mask. Numeric member display lengths use
their string-converted numeric descriptor width under the current connection
charset.

Nullability follows argument metadata. The field is nullable if `bits` can be
`NULL` or any member argument can be `NULL`; otherwise it is `NOT_NULL`, even
when the runtime bit mask selects no member.

Verified metadata:

| Expression | Connection | Type | Collation id | Length | Decimals | Flags |
| --- | --- | --- | ---: | ---: | ---: | --- |
| `MAKE_SET(3,'a','bc')` | `utf8mb4` | `VAR_STRING` | `255` | `16` | `31` | `NOT_NULL` |
| `MAKE_SET(4,'a','bc')` | `utf8mb4` | `VAR_STRING` | `255` | `16` | `31` | `NOT_NULL` |
| `MAKE_SET(1,1,20)` | `utf8mb4` | `VAR_STRING` | `255` | `24` | `31` | `NOT_NULL` |
| `MAKE_SET(NULL,'a','bc')` | `utf8mb4` | `VAR_STRING` | `255` | `16` | `31` | none |
| `MAKE_SET(3,NULL,NULL)` | `utf8mb4` | `VAR_STRING` | `63` | `1` | `31` | `BINARY` |
| `MAKE_SET(3,'a',NULL,'bc')` | `utf8mb4` | `VAR_STRING` | `255` | `20` | `31` | none |
| `MAKE_SET(3,'a','bc')` | `latin1` | `VAR_STRING` | `8` | `4` | `31` | `NOT_NULL` |
| `MAKE_SET(NULL,'a')` | `latin1` | `VAR_STRING` | `8` | `1` | `31` | none |
| `MAKE_SET(nullable_bits, s, n, u)` over `VARCHAR(20), INT, BIGINT UNSIGNED` | `latin1` | `VAR_STRING` | `8` | `53` | `31` | none |
| `MAKE_SET(1, _binary 'ab')` | `utf8mb4` | `VAR_STRING` | `63` | `2` | `31` | `NOT_NULL BINARY` |
| `MAKE_SET(3, _binary 'ab', 'cd')` | `utf8mb4` | `VAR_STRING` | `63` | `5` | `31` | `NOT_NULL BINARY` |
| `MAKE_SET(3, b, s)` over `VARBINARY(8), VARCHAR(8)` | `utf8mb4` | `VAR_STRING` | `63` | `17` | `31` | `BINARY` |

## Runtime design

Implementation extends the scalar-function registry in
`mylite_expression.c`:

- add a function id for `MAKE_SET`
- validate arity as two or more arguments
- evaluate and convert the mask argument first
- return `NULL` immediately for a `NULL` mask
- evaluate only selected member arguments whose bit index is below 64
- convert selected non-`NULL` members to strings with existing expression
  value-to-text helpers
- append selected non-`NULL` strings in argument order with literal comma
  separators
- count selected empty strings as appended members so comma placement matches
  MySQL

Metadata inference in `mylite.c` adds a `MAKE_SET()` string descriptor using
the display-length, charset, flag, and nullability rules above.

No storage or file-format changes are required.

## Tests

Add C tests for:

- parser acceptance of generic `MAKE_SET()` calls
- unsupported zero-argument and one-argument arity
- no-table scalar results for bit positions, no selected members, `NULL` mask,
  negative masks, unsigned boundaries, real and string mask conversion,
  truncation warnings, `NULL` members, empty strings, duplicate strings,
  comma-containing strings, UTF-8 strings, and lazy selected-member evaluation
- metadata under `utf8mb4` and `latin1`, including constant text members,
  numeric members, nullable masks, nullable members, all-`NULL` members, and
  nullable table-backed examples
- table-backed `SELECT` projection, `WHERE`, and `ORDER BY`
- supported single-table `UPDATE` assignment/predicate/order key and `DELETE`
  predicate/order key

## Compatibility status

After this feature, `MAKE_SET()` is partially supported for the existing scalar
expression call sites. The status remains partial because binary-string
collation aggregation, exact decimal/real string formatting for selected
numeric members, `max_allowed_packet`, and exact native arity diagnostics remain
deferred.
