# String list and index functions

## Scope

This feature implements the MySQL string/list scalar functions:

- `ELT(N, str1, str2, ...)`
- `FIELD(str, str1, str2, ...)`
- `FIND_IN_SET(str, strlist)`

The functions are available anywhere MyLite currently evaluates supported
scalar built-ins: no-table `SELECT`, current table-backed projection, `WHERE`,
`ORDER BY`, and supported single-table `UPDATE` and `DELETE` expression paths.

Out of scope:

- `MAKE_SET()`
- `SET` column bit-optimized `FIND_IN_SET()` execution
- binary-string-specific matching and result typing
- exact binary-collation metadata for all-`NULL` `ELT()` result lists
- full collation coercibility, accent-insensitive matching, and runtime
  switching between case-insensitive and binary collations
- exact decimal/real display formatting when non-integer numeric operands are
  coerced to strings
- `max_allowed_packet` result-size enforcement
- exact native error-code exposure for unsupported arity paths

## Sources

- MySQL 8.4 Reference Manual, String Functions and Operators:
  https://dev.mysql.com/doc/refman/8.4/en/string-functions.html
- MySQL 8.4 Reference Manual, Type Conversion in Expression Evaluation:
  https://dev.mysql.com/doc/refman/8.4/en/type-conversion.html
- Existing MyLite scalar-function design:
  `docs/specs/scalar-built-in-functions/specs.md`
- Existing MyLite string-function slices:
  - `docs/specs/string-functions-substring-trim/specs.md`
  - `docs/specs/string-search-code-functions/specs.md`
  - `docs/specs/string-padding-repeat-functions/specs.md`
  - `docs/specs/string-insert-function/specs.md`
  - `docs/specs/string-quote-function/specs.md`

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

`ELT()` and `FIELD()` are variadic and require at least two arguments. The
first argument has special meaning, so `ELT()` / `ELT(1)` and `FIELD()` /
`FIELD('a')` raise native MySQL error 1582.

`FIND_IN_SET()` requires exactly two arguments. One-argument and three-argument
calls raise native MySQL error 1582.

Generic comma-separated calls continue to use
`MYLITE_SQL_AST_FUNCTION_CALL` and `MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST`.
The names are ordinary function identifiers in MyLite; no dedicated token or
AST node is required.

The intended MyLite Lemon-style grammar remains the generic function-call
shape:

```lemon
scalar_function_call ::= function_name LPAREN function_argument_list RPAREN.
function_name ::= identifier.
```

## `ELT()`

`ELT(N, str1, str2, ...)` returns the list element whose one-based index is
`N`. `ELT(1, ...)` returns the first string argument, `ELT(2, ...)` returns the
second, and so on.

`N` is converted to a signed integer. Numeric real values round to the nearest
integer with halves away from zero. String values are parsed as integers; a
string with trailing noninteger text uses the parsed integer prefix and emits a
1292 truncation warning.

If `N` is `NULL`, less than `1`, or greater than the number of list arguments,
the result is SQL `NULL`. If the selected list element is `NULL`, the result is
SQL `NULL`. Non-`NULL` selected values are returned as strings.

Observed examples:

| Expression | Result | Warnings |
| --- | --- | --- |
| `ELT(1,'a','b')` | `a` | none |
| `ELT(2,'a','b')` | `b` | none |
| `ELT(0,'a','b')` | `NULL` | none |
| `ELT(-1,'a','b')` | `NULL` | none |
| `ELT(3,'a','b')` | `NULL` | none |
| `ELT(NULL,'a','b')` | `NULL` | none |
| `ELT(1,NULL,'b')` | `NULL` | none |
| `ELT(2.5,'a','b','c')` | `c` | none |
| `ELT('2.5','a','b','c')` | `b` | one 1292 warning |

Observed evaluation short-circuiting:

- `ELT(NULL, 1/0, 'x')` returns `NULL` without evaluating later arguments.
- `ELT(1, 'a', 1/0)` returns `a` without evaluating the second list element.
- `ELT(2, 'a', 1/0)` evaluates the selected element, returns `NULL`, and
  emits the division warning from that selected expression.

## `FIELD()`

`FIELD(str, str1, str2, ...)` returns the one-based position of the first list
argument equal to `str`. The first list argument has position `1`; `NULL`
candidates still occupy a position but never match. A miss returns `0`.

If `str` is SQL `NULL`, the result is `0`.

Comparison mode follows MySQL's documented `FIELD()` rules:

- when all non-`NULL` arguments are strings, compare as strings
- when all non-`NULL` arguments are numbers, compare as numbers
- otherwise compare as double values, with string-to-number truncation warnings
  as MySQL emits them

Observed examples:

| Expression | Result | Warnings |
| --- | --- | --- |
| `FIELD('b','a','b','c')` | `2` | none |
| `FIELD('z','a','b','c')` | `0` | none |
| `FIELD(NULL,'a',NULL)` | `0` | none |
| `FIELD('a',NULL,'a')` | `2` | none |
| `FIELD(2,1,2,3)` | `2` | none |
| `FIELD('2',1,2,3)` | `2` | none |
| `FIELD('海','猫','海')` | `2` | none |
| `FIELD('2','02','2')` | `2` | none |
| `FIELD(2,'02','2')` | `1` | none |
| `FIELD(2.0,'02','2')` | `1` | none |
| `FIELD('2.0','2','2.0')` | `2` | none |
| `FIELD(0,'a','b')` | `1` | one 1292 warning |
| `FIELD('x',1,2)` | `0` | one 1292 warning |

The first MyLite slice performs ASCII case-insensitive text matching to follow
the default `utf8mb4_0900_ai_ci` connection behavior for common application
strings. Full collation coercibility, binary-collation switching, and Unicode
case/accent folding are deferred with the broader collation runtime.

## `FIND_IN_SET()`

`FIND_IN_SET(str, strlist)` searches for `str` in a comma-separated string
list. The return value is the one-based token position. A miss returns `0`.

If either argument is SQL `NULL`, the result is SQL `NULL`. The first argument
is converted to a string before matching, so integer inputs can match text
tokens. The second argument is converted to the comma-list string. No trimming
is applied around tokens. Exact decimal/real string formatting remains tied to
the broader MyLite expression-value model and is deferred.

An empty `strlist` returns `0`. Empty tokens inside a non-empty comma list are
real tokens and can match an empty search string. If `str` contains a comma,
observed MySQL 8.4.9 returns `0` for ordinary string-list inputs.

Observed examples:

| Expression | Result |
| --- | --- |
| `FIND_IN_SET('b','a,b,c')` | `2` |
| `FIND_IN_SET('a','b,a,a')` | `2` |
| `FIND_IN_SET('z','a,b,c')` | `0` |
| `FIND_IN_SET(NULL,'a,b')` | `NULL` |
| `FIND_IN_SET('a',NULL)` | `NULL` |
| `FIND_IN_SET('','a,,b')` | `2` |
| `FIND_IN_SET('', '')` | `0` |
| `FIND_IN_SET('', ',')` | `1` |
| `FIND_IN_SET('', ',,')` | `1` |
| `FIND_IN_SET('a,b','a,b')` | `0` |
| `FIND_IN_SET('b','a,b,')` | `2` |
| `FIND_IN_SET('','a,')` | `2` |
| `FIND_IN_SET('a',' a,a ')` | `0` |
| `FIND_IN_SET('海','猫,海')` | `2` |
| `FIND_IN_SET(2,'1,2,3')` | `2` |
| `FIND_IN_SET('2','1,02,2')` | `3` |

As with `FIELD()`, this slice performs ASCII case-insensitive text matching for
the default connection-collation behavior and defers full collation
coercibility and binary-collation switching.

## Result metadata

`ELT()` returns `VAR_STRING` with MySQL's not-fixed decimals marker (`31`),
nullable metadata, and no numeric or binary flags for ordinary text-coercible
result lists. Display length is based on the largest list-element display
length, not the runtime selected element. When every list element is `NULL`,
observed MySQL reports binary collation id `63`, length `0`, and the `BINARY`
flag; that narrow metadata shape is deferred in this MyLite slice.

Verified constant metadata:

| Expression | Connection | Type | Collation id | Length | Decimals | Flags |
| --- | --- | --- | ---: | ---: | ---: | --- |
| `ELT(2,'a','bc')` | `utf8mb4` | `VAR_STRING` | `255` | `8` | `31` | none |
| `ELT(3,'a','bc')` | `utf8mb4` | `VAR_STRING` | `255` | `8` | `31` | none |
| `ELT(1,1,20)` | `utf8mb4` | `VAR_STRING` | `255` | `12` | `31` | none |
| `ELT(2,'a','bc')` | `latin1` | `VAR_STRING` | `8` | `2` | `31` | none |

`FIELD()` returns a non-null `LONGLONG` with binary charset id `63`, display
length `3`, scale `0`, and `NOT_NULL BINARY NUM` flags, including when the
search argument is `NULL` and the runtime value is `0`.

`FIND_IN_SET()` returns `LONGLONG` with binary charset id `63`, display length
`3`, and scale `0`. Constant non-`NULL` calls expose `NOT_NULL BINARY NUM`;
calls whose arguments can be `NULL` are nullable and omit `NOT_NULL`.

Verified constant metadata:

| Expression | Type | Charset id | Length | Decimals | Flags |
| --- | --- | ---: | ---: | ---: | --- |
| `FIELD('b','a','b')` | `LONGLONG` | `63` | `3` | `0` | `NOT_NULL BINARY NUM` |
| `FIELD(NULL,'a')` | `LONGLONG` | `63` | `3` | `0` | `NOT_NULL BINARY NUM` |
| `FIND_IN_SET('b','a,b')` | `LONGLONG` | `63` | `3` | `0` | `NOT_NULL BINARY NUM` |
| `FIND_IN_SET(NULL,'a')` | `LONGLONG` | `63` | `3` | `0` | `BINARY NUM` |

## Runtime design

Implementation extends the scalar-function registry in `mylite_expression.c`:

- add function ids for `ELT`, `FIELD`, and `FIND_IN_SET`
- validate arity:
  - `ELT`: two or more arguments
  - `FIELD`: two or more arguments
  - `FIND_IN_SET`: exactly two arguments
- evaluate `ELT()` index first, convert it to a signed integer, and evaluate
  only the selected list element when the index is in range
- evaluate `FIELD()` search value first and return `0` immediately for SQL
  `NULL`; otherwise classify non-`NULL` argument values and apply the matching
  comparison mode
- evaluate `FIND_IN_SET()` left to right; return `NULL` without evaluating the
  second argument when the first argument is SQL `NULL`
- convert non-`NULL` string results and string-list operands with existing
  scalar value-to-string helpers
- use the existing numeric conversion helper for `ELT()` index casts and
  mixed-mode `FIELD()` comparisons so warning code/message behavior stays
  aligned with the current expression engine

## Tests

Add C tests for:

- parser acceptance of `ELT`, `FIELD`, and `FIND_IN_SET` generic calls
- no-table scalar results for all verified examples above
- `ELT()` index conversion, out-of-range handling, selected-argument `NULL`,
  string-numeric warnings, and selected-argument short-circuiting
- `FIELD()` hits, misses, `NULL` search, `NULL` candidates, all-string,
  all-numeric, mixed numeric/string comparison, conversion warnings, UTF-8, and
  ASCII case-insensitive default-collation matching
- `FIND_IN_SET()` hits, misses, `NULL` propagation, empty-list and empty-token
  behavior, comma-containing search string behavior, no trimming, integer
  source conversion, duplicate first-match behavior, UTF-8, and ASCII
  case-insensitive default-collation matching
- metadata for `ELT`, `FIELD`, and `FIND_IN_SET` under `utf8mb4` and `latin1`
- table-backed projection, `WHERE`, and `ORDER BY`
- single-table `UPDATE` assignment/predicate and `DELETE` predicate paths
- unsupported arity for all three functions through the existing scalar binding
  path

## Compatibility status

After implementation and verification, the `ELT()`, `FIELD()`, and
`FIND_IN_SET()` rows in `COMPATIBILITY.md` should move to partial support. The
status remains partial because `MAKE_SET()`, `SET`-column optimization,
binary-string behavior, all-`NULL` `ELT()` metadata, exact decimal/real
string-conversion formatting, full collation/coercibility, exact lazy warning
ordering for every expression-valued argument combination, and exact native
arity error-code exposure are deferred.
