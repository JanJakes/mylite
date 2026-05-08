# String `QUOTE()` function

## Scope

This feature implements the MySQL string function:

- `QUOTE(str)`

The function is available anywhere MyLite currently evaluates supported scalar
built-ins: no-table `SELECT`, current table-backed projection, `WHERE`,
`ORDER BY`, and supported single-table `UPDATE` and `DELETE` expression paths.

Out of scope:

- `max_allowed_packet` result-size enforcement
- full collation aggregation and coercibility
- SQL-mode-dependent stored or loadable function name resolution
- exact native error-code reporting for unsupported function arity

## Sources

- MySQL 8.4 Reference Manual, String Functions and Operators:
  https://dev.mysql.com/doc/refman/8.4/en/string-functions.html
- MySQL 8.4 Reference Manual, Function Name Parsing and Resolution:
  https://dev.mysql.com/doc/refman/8.4/en/function-resolution.html
- Existing MyLite scalar-function design:
  `docs/specs/scalar-built-in-functions/specs.md`
- Existing MyLite string-function slices:
  - `docs/specs/string-functions-substring-trim/specs.md`
  - `docs/specs/string-search-code-functions/specs.md`
  - `docs/specs/string-padding-repeat-functions/specs.md`
  - `docs/specs/string-insert-function/specs.md`

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

`QUOTE(str)` returns SQL-literal text for its argument. Non-`NULL` input is
converted to a string, wrapped in single quotes, and escaped byte-by-byte for
the characters MySQL treats specially in quoted SQL string literals.

If the argument is SQL `NULL`, the result is the four-byte string `NULL`. This
is not a SQL `NULL` result:

| Expression | Result |
| --- | --- |
| `QUOTE(NULL)` | `NULL` text |
| `QUOTE(NULL) IS NULL` | `0` |
| `HEX(QUOTE(NULL))` | `4E554C4C` |
| `LENGTH(QUOTE(NULL))` | `4` |

Representative runtime results with `SET NAMES utf8mb4`:

| Expression | Result or hex result |
| --- | --- |
| `QUOTE('Don''t')` | `'Don\'t'` |
| `HEX(QUOTE('Don''t'))` | `27446F6E5C277427` |
| `QUOTE('plain')` | `'plain'` |
| `QUOTE('')` | `''` |
| `QUOTE('海豚')` | `'海豚'` |
| `HEX(QUOTE('back\\slash'))` | `276261636B5C5C736C61736827` |
| `HEX(QUOTE(CONCAT('nul', CHAR(0), 'end')))` | `276E756C5C30656E6427` |
| `HEX(QUOTE(CHAR(26)))` | `275C5A27` |

Escaping rules observed in MySQL 8.4.9:

- the result begins and ends with a single quote for non-`NULL` input
- single quote becomes `\'`
- backslash becomes `\\`
- ASCII `NUL` becomes `\0`
- Control+Z becomes `\Z`
- newline, horizontal tab, and carriage return remain literal bytes inside the
  surrounding quotes
- UTF-8 bytes pass through unchanged except for the bytes above

MyLite implements these escaping rules over length-aware expression values,
including embedded `NUL` bytes produced by string literals and binary-returning
functions such as `FROM_BASE64()` and `UNHEX()`.

Wrong arity raises MySQL native error 1582:

| SQL | Error |
| --- | --- |
| `SELECT QUOTE()` | 1582 |
| `SELECT QUOTE('a', 'b')` | 1582 |

MyLite's first slice rejects unsupported arity through the existing scalar
function binding path. Exact native error-code exposure remains deferred with
the broader scalar-function diagnostic work.

## Result metadata

`QUOTE()` returns `VAR_STRING` with the current connection collation, MySQL's
not-fixed decimals marker (`31`), nullable metadata, and no numeric or binary
flags.

Constant result metadata was verified:

| Expression | Connection | Type | Collation id | Length | Decimals | Flags |
| --- | --- | --- | ---: | ---: | ---: | --- |
| `QUOTE('Don''t')` | `utf8mb4` | `VAR_STRING` | `255` | `48` | `31` | none |
| `QUOTE(NULL)` | `utf8mb4` | `VAR_STRING` | `255` | `16` | `31` | none |
| `QUOTE(42)` | `utf8mb4` | `VAR_STRING` | `255` | `32` | `31` | none |
| `QUOTE(_binary 'abc')` | `utf8mb4` | `VAR_STRING` | `255` | `32` | `31` | none |
| `QUOTE(CAST('abc' AS BINARY))` | `utf8mb4` | `VAR_STRING` | `255` | `104` | `31` | none |
| `QUOTE(varbinary_col_12)` | `utf8mb4` | `VAR_STRING` | `255` | `26` | `31` | none |
| `QUOTE('Don''t')` | `latin1` | `VAR_STRING` | `8` | `12` | `31` | none |

The observed display length for non-`NULL` text follows:

```text
(source character length * 2 + 2) * connection max bytes per character
```

The `NULL` input case reports:

```text
4 * connection max bytes per character
```

For non-text sources such as numeric literals or columns, MySQL uses the
source expression display length converted to the result character set as the
source term, then adds room for doubled content and the surrounding quotes.

Binary-string sources keep `QUOTE()` result collation in the current connection
character set, but width inference depends on whether the source is row-backed:

- cacheable binary expressions use the binary source descriptor width converted
  to the result character set, then add room for doubled content and quotes
- table-backed binary columns use byte-counted binary source width and
  byte-counted surrounding quotes

## Parser and AST design

Generic comma-separated calls continue to use
`MYLITE_SQL_AST_FUNCTION_CALL` and `MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST`.
No special AST node is required.

`QUOTE` is not a special lexer token in the current MyLite grammar. It is
accepted through the existing generic function-name rule:

```lemon
function_name ::= identifier.
```

No MyLite Lemon grammar production is required beyond the existing generic
function-call form:

```lemon
scalar_function_call ::= function_name LPAREN function_argument_list RPAREN.
```

## Runtime design

Implementation extends the existing scalar-function registry in
`mylite_expression.c`:

- add a function id for `QUOTE`
- validate exactly one argument
- evaluate the argument once
- return the text value `NULL` when the argument evaluates to SQL `NULL`
- convert non-`NULL` input with the existing string conversion helper
- append a leading single quote, escaped input bytes, and trailing single quote
- escape single quote, backslash, and Control+Z for the length-aware bytes
  MyLite can currently represent
- leave other control bytes and UTF-8 bytes unchanged

The result has no side effects, emits no warnings for ordinary supported
conversions, and does not touch storage or session state.

## Tests

Add C tests for:

- parser acceptance of `QUOTE(...)`
- no-table scalar results for ordinary text, empty text, UTF-8 text, numeric
  input, single quote, backslash, Control+Z, newline, tab, carriage return, and
  SQL `NULL`
- embedded `NUL` escaping through length-aware scalar values
- `QUOTE(NULL) IS NULL` and `LENGTH(QUOTE(NULL))`
- metadata for verified `utf8mb4`, `latin1`, scalar binary-expression, and
  table-backed binary-column cases
- wrong-arity rejection through the existing scalar binding path
- table-backed projection, `WHERE`, and `ORDER BY`
- single-table `UPDATE` assignment/predicate and `DELETE` predicate paths

## Compatibility status

After implementation and verification, the `QUOTE()` row in `COMPATIBILITY.md`
should move to partial support. The status remains partial because
`max_allowed_packet`, full collation/coercibility, and exact native arity
error-code exposure are deferred.
