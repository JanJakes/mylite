# String padding, repeat, space, and reverse functions

## Scope

This feature implements the deterministic Task 24 string functions:

- `REPEAT(str, count)`
- `SPACE(N)`
- `REVERSE(str)`
- `LPAD(str, len, padstr)`
- `RPAD(str, len, padstr)`

The functions are available anywhere MyLite currently evaluates supported
scalar built-ins: no-table `SELECT`, current table-backed projection, `WHERE`,
`ORDER BY`, and supported single-table `UPDATE` and `DELETE` expression paths.

Out of scope:

- binary-string-specific result typing and display behavior
- `max_allowed_packet` result-size enforcement
- full collation aggregation and coercibility
- exact warning promotion parity for every string-to-integer coercion path
- stored-program `REPEAT ... UNTIL` statement behavior, which is a separate
  grammar surface

## Sources

- MySQL 8.4 Reference Manual, String Functions and Operators:
  https://dev.mysql.com/doc/refman/8.4/en/string-functions.html
- Existing MyLite scalar-function design:
  `docs/specs/scalar-built-in-functions/specs.md`
- Existing MyLite string-function slices:
  - `docs/specs/string-functions-substring-trim/specs.md`
  - `docs/specs/string-search-code-functions/specs.md`
- Existing MyLite expression and metadata designs:
  - `docs/specs/expression-operator-foundation/specs.md`
  - `docs/specs/result-metadata-expression-labels/specs.md`
  - `docs/specs/update-single-table/specs.md`
  - `docs/specs/delete-single-table/specs.md`

Observed behavior was verified against MySQL 8.4.9 in Docker container
`mylite-mysql-849`, using:

- `docker exec -i mylite-mysql-849 mysql -uroot --batch --raw --show-warnings --force`
- `docker exec -i mylite-mysql-849 mysql -uroot --column-type-info -vvv --force`

Runtime probes used `SET NAMES utf8mb4` unless the metadata probe explicitly
switched to `latin1`.

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar,
documentation prose, or implementation sources.

## MySQL behavior summary

String positions and requested lengths are character counts, not bytes.
`REVERSE()`, `LPAD()`, and `RPAD()` are multibyte safe for UTF-8 text. Numeric
noninteger length/count arguments are rounded to the nearest integer, with
halves away from zero. String length/count arguments are converted as integers;
for example, `'2.5'` becomes `2` and produces warning 1292.

### `REPEAT()`

`REPEAT(str, count)` returns `str` repeated `count` times. If either argument
is `NULL`, the result is `NULL`. If `count < 1`, the result is the empty
string. Repeating an empty string returns the empty string.

Verified examples:

| Expression | Result |
| --- | --- |
| `REPEAT('ab', 3)` | `ababab` |
| `REPEAT('ab', 0)` | empty string |
| `REPEAT('ab', -1)` | empty string |
| `REPEAT('', 3)` | empty string |
| `REPEAT(NULL, 3)` | `NULL` |
| `REPEAT('ab', NULL)` | `NULL` |
| `REPEAT('x', 2.4)` | `xx` |
| `REPEAT('x', 2.5)` | `xxx` |

`REPEAT('a')` and `REPEAT('a',2,3)` are syntax errors in MySQL 8.4.9.

### `SPACE()`

`SPACE(N)` returns `N` ASCII space characters. If `N` is `NULL`, the result is
`NULL`. If `N < 1`, the result is the empty string.

Verified examples:

| Expression | Result |
| --- | --- |
| `SPACE(3)` | three spaces |
| `LENGTH(SPACE(3))` | `3` |
| `SPACE(0)` | empty string |
| `SPACE(-1)` | empty string |
| `SPACE(NULL)` | `NULL` |
| `LENGTH(SPACE(2.5))` | `3` |

`SPACE()` and `SPACE(1,2)` raise MySQL error 1582.

### `REVERSE()`

`REVERSE(str)` returns the characters of `str` in reverse order. If `str` is
`NULL`, the result is `NULL`. UTF-8 characters remain intact; only the
character order changes.

Verified examples:

| Expression | Result |
| --- | --- |
| `REVERSE('abc')` | `cba` |
| `REVERSE('海豚猫')` | `猫豚海` |
| `REVERSE(NULL)` | `NULL` |

`REVERSE()` and `REVERSE('a','b')` are syntax errors in MySQL 8.4.9.

### `LPAD()` and `RPAD()`

`LPAD(str, len, padstr)` and `RPAD(str, len, padstr)` return a string of
`len` characters. If `str` is longer than `len`, both functions return the
leftmost `len` characters of `str`. If `str` is shorter than `len`, `LPAD()`
prepends enough `padstr` characters and `RPAD()` appends enough `padstr`
characters; the final pad repetition is truncated when needed. If `len = 0`,
the result is the empty string. If `len < 0`, the result is `NULL`. If any
argument is `NULL`, the result is `NULL`.

An empty non-`NULL` `padstr` is special: when padding would be required, the
result is the empty string; when no padding is required, normal leftmost
truncation still applies.

Verified examples:

| Expression | Result |
| --- | --- |
| `LPAD('hi', 5, '.')` | `...hi` |
| `RPAD('hi', 5, '.')` | `hi...` |
| `LPAD('abcdef', 3, '.')` | `abc` |
| `RPAD('abcdef', 3, '.')` | `abc` |
| `LPAD('hi', 1, '.')` | `h` |
| `RPAD('hi', 1, '.')` | `h` |
| `LPAD('hi', 0, '.')` | empty string |
| `RPAD('hi', 0, '.')` | empty string |
| `LPAD('hi', -1, '.')` | `NULL` |
| `RPAD('hi', -1, '.')` | `NULL` |
| `LPAD('hi', 5, '')` | empty string |
| `RPAD('hi', 5, '')` | empty string |
| `LPAD('abcdef', 3, '')` | `abc` |
| `RPAD('abcdef', 3, '')` | `abc` |
| `LPAD(NULL, 5, '.')` | `NULL` |
| `LPAD('hi', NULL, '.')` | `NULL` |
| `LPAD('hi', 5, NULL)` | `NULL` |
| `RPAD(NULL, 5, '.')` | `NULL` |
| `RPAD('hi', NULL, '.')` | `NULL` |
| `RPAD('hi', 5, NULL)` | `NULL` |
| `LPAD('海', 3, '豚猫')` | `豚猫海` |
| `RPAD('海', 3, '豚猫')` | `海豚猫` |

Wrong-arity `LPAD()` and `RPAD()` calls raise MySQL error 1582.

## Result metadata

The implemented functions return `VAR_STRING` with the current connection
collation, MySQL's not-fixed decimals marker (`31`), nullable metadata, and no
numeric/binary flags for the supported paths.

Constant result metadata was verified:

| Expression | Connection | Type | Collation id | Length | Decimals | Flags |
| --- | --- | --- | ---: | ---: | ---: | --- |
| `REPEAT('ab',3)` | `utf8mb4` | `VAR_STRING` | `255` | `24` | `31` | none |
| `SPACE(3)` | `utf8mb4` | `VAR_STRING` | `255` | `12` | `31` | none |
| `REVERSE('abc')` | `utf8mb4` | `VAR_STRING` | `255` | `12` | `31` | none |
| `LPAD('hi',5,'.')` | `utf8mb4` | `VAR_STRING` | `255` | `20` | `31` | none |
| `RPAD('hi',5,'.')` | `utf8mb4` | `VAR_STRING` | `255` | `20` | `31` | none |
| `REPEAT('ab',3)` | `latin1` | `VAR_STRING` | `8` | `6` | `31` | none |
| `SPACE(3)` | `latin1` | `VAR_STRING` | `8` | `3` | `31` | none |
| `REVERSE('abc')` | `latin1` | `VAR_STRING` | `8` | `3` | `31` | none |
| `LPAD('hi',5,'.')` | `latin1` | `VAR_STRING` | `8` | `5` | `31` | none |
| `RPAD('hi',5,'.')` | `latin1` | `VAR_STRING` | `8` | `5` | `31` | none |

Table-backed constant-width metadata was also verified for `utf8mb4`:

| Expression over `s VARCHAR(12)` | Type | Collation id | Length | Decimals | Flags |
| --- | --- | ---: | ---: | ---: | --- |
| `LPAD(s,7,'.')` | `VAR_STRING` | `255` | `28` | `31` | none |
| `RPAD(s,7,'.')` | `VAR_STRING` | `255` | `28` | `31` | none |
| `REPEAT(s,3)` | `VAR_STRING` | `255` | `144` | `31` | none |
| `SPACE(n)` | `LONG_BLOB` | `255` | `268435456` | `31` | none |
| `LPAD(s,n,'.')` | `LONG_BLOB` | `255` | `268435456` | `31` | none |
| `RPAD(s,n,'.')` | `LONG_BLOB` | `255` | `268435456` | `31` | none |
| `REPEAT(s,n)` | `LONG_BLOB` | `255` | `268435456` | `31` | none |
| `LPAD(s,NULL,'.')` | `LONG_BLOB` | `255` | `268435456` | `31` | none |
| `RPAD(s,NULL,'.')` | `LONG_BLOB` | `255` | `268435456` | `31` | none |
| `REPEAT(s,NULL)` | `LONG_BLOB` | `255` | `268435456` | `31` | none |
| `LPAD(s,-1,'.')` | `LONG_BLOB` | `255` | `67108864` | `31` | none |
| `RPAD(s,-1,'.')` | `LONG_BLOB` | `255` | `67108864` | `31` | none |
| `REPEAT(s,-1)` | `LONG_BLOB` | `255` | `67108864` | `31` | none |

Variable or `NULL` target/count metadata uses MySQL's dynamic `LONG_BLOB`
width. Negative literal target/count metadata uses the same connection
character-set multiplier as the maximum constant-width padding path.

## Parser and AST design

Generic comma-separated calls continue to use
`MYLITE_SQL_AST_FUNCTION_CALL` and `MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST`.
No special AST node is required for this slice.

`REPEAT` is a reserved keyword because stored programs also use a `REPEAT`
statement. The expression grammar must still allow it as a built-in function
name. `REPEAT()` and `REVERSE()` wrong-arity calls are parser-level syntax
errors to match MySQL 8.4.9. `SPACE()`, `LPAD()`, and `RPAD()` wrong-arity
calls can be accepted by the parser and rejected by binding with MyLite's
current unsupported-arity diagnostic path.

The intended MyLite Lemon-style grammar addition is:

```lemon
function_name ::= REPEAT.
```

All other names in this slice are ordinary function identifiers in the current
lexer.

## Runtime design

Implementation extends the existing scalar-function registry in
`mylite_expression.c`:

- add function ids for `REPEAT`, `SPACE`, `REVERSE`, `LPAD`, and `RPAD`
- validate arity:
  - `REPEAT`: two arguments
  - `SPACE`: one argument
  - `REVERSE`: one argument
  - `LPAD` / `RPAD`: three arguments
- evaluate arguments left to right
- return `NULL` when MySQL specifies `NULL` propagation
- convert string-producing operands with the existing string conversion helper
- convert length/count arguments through MyLite's integer-cast helper so
  numeric decimals round like MySQL and text decimals truncate with warnings

`REVERSE()`, truncating `LPAD()` / `RPAD()`, and padding-length calculation use
the existing UTF-8 character-offset helpers. Invalid UTF-8 follows the current
MyLite text-helper policy used by `CHAR_LENGTH`, `LEFT`, `RIGHT`, and
`SUBSTRING`.

## Tests

Add C tests for:

- parser acceptance of ordinary `REPEAT`, `SPACE`, `REVERSE`, `LPAD`, and
  `RPAD` function calls
- parser rejection for `REPEAT` and `REVERSE` wrong-arity calls
- binding rejection for `SPACE`, `LPAD`, and `RPAD` wrong-arity calls
- no-table scalar results for the verified examples above
- decimal and string length/count coercion examples
- metadata for constant results under `utf8mb4` and `latin1`
- table-backed projection, `WHERE`, and `ORDER BY`
- single-table `UPDATE` assignment/predicate and `DELETE` predicate paths
- `NULL` propagation and empty-pad behavior

## Compatibility status

After implementation and verification, the `REPEAT()`, `SPACE()`,
`REVERSE()`, `LPAD()`, and `RPAD()` rows in `COMPATIBILITY.md` should move to
partial support. The status remains partial because binary-string-specific
typing, `max_allowed_packet`, complete collation/coercibility behavior, and
full MySQL diagnostic fidelity are deferred.
