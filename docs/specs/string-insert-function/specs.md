# String `INSERT()` function

## Scope

This feature implements the MySQL string function:

- `INSERT(str, pos, len, newstr)`

The function is available anywhere MyLite currently evaluates supported scalar
built-ins: no-table `SELECT`, current table-backed projection, `WHERE`,
`ORDER BY`, and supported single-table `UPDATE` and `DELETE` expression paths.

Out of scope:

- binary-string runtime behavior beyond the current value model
- `max_allowed_packet` result-size enforcement
- full collation aggregation and coercibility
- SQL-mode-dependent stored or loadable function name resolution
- exact metadata parity for every `NULL` argument combination

## Sources

- MySQL 8.4 Reference Manual, String Functions and Operators:
  https://dev.mysql.com/doc/refman/8.4/en/string-functions.html
- MySQL 8.4 Reference Manual, Function Name Parsing and Resolution:
  https://dev.mysql.com/doc/refman/8.4/en/function-resolution.html
- MySQL 8.4 Reference Manual, Keywords and Reserved Words:
  https://dev.mysql.com/doc/refman/8.4/en/keywords.html
- Existing MyLite scalar-function design:
  `docs/specs/scalar-built-in-functions/specs.md`
- Existing MyLite string-function slices:
  - `docs/specs/string-functions-substring-trim/specs.md`
  - `docs/specs/string-search-code-functions/specs.md`
  - `docs/specs/string-padding-repeat-functions/specs.md`

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

`INSERT(str, pos, len, newstr)` returns `str` with `newstr` spliced at
one-based character position `pos`, replacing up to `len` characters from the
source. String positions and lengths are character counts, not bytes.

If any argument is `NULL`, the result is `NULL`. MySQL does not always convert
the numeric arguments before returning `NULL`: when `str`, `newstr`, or `pos`
is `NULL`, observed MySQL 8.4.9 skips later `pos`/`len` integer-conversion
warnings. When `len` is `NULL` and earlier non-`NULL` arguments are present,
`pos` conversion still happens before the `NULL` result.

Runtime behavior verified with `SET NAMES utf8mb4`:

| Expression | Result |
| --- | --- |
| `INSERT('Quadratic', 3, 4, 'What')` | `QuWhattic` |
| `INSERT('Quadratic', -1, 4, 'What')` | `Quadratic` |
| `INSERT('Quadratic', 0, 4, 'What')` | `Quadratic` |
| `INSERT('Quadratic', 99, 4, 'What')` | `Quadratic` |
| `INSERT('Quadratic', 3, 0, 'What')` | `QuWhatadratic` |
| `INSERT('Quadratic', 3, -1, 'What')` | `QuWhat` |
| `INSERT('Quadratic', 3, 99, 'What')` | `QuWhat` |
| `INSERT('Quadratic', 3, 4, '')` | `Qutic` |
| `INSERT('', 1, 1, 'x')` | empty string |
| `INSERT('', 2, 1, 'x')` | empty string |
| `INSERT('abc', 1, 0, 'X')` | `Xabc` |
| `INSERT('abc', 1, -1, 'X')` | `X` |
| `INSERT('abc', 4, 1, 'X')` | `abc` |
| `INSERT('abc', 3, 0, 'X')` | `abXc` |

UTF-8 character behavior is multibyte safe:

| Expression | Result |
| --- | --- |
| `INSERT('海豚猫', 2, 1, '鳥')` | `海鳥猫` |
| `INSERT('海豚猫', 2, 2, '鳥')` | `海鳥` |

Numeric argument behavior:

| Expression | Result | Warnings |
| --- | --- | --- |
| `INSERT('abc', 2.5, 1, 'X')` | `abX` | none |
| `INSERT('abc', 2, 1.5, 'X')` | `aX` | none |
| `INSERT('abc', '2.5', '1.5', 'X')` | `aXc` | two 1292 warnings |

Numeric real arguments round to the nearest integer with halves away from zero,
matching the existing MyLite integer-cast helper. String numeric arguments are
parsed as integers and warn on trailing noninteger text.

Wrong arity is a syntax error in MySQL 8.4.9:

| SQL | Error |
| --- | --- |
| `SELECT INSERT()` | 1064 |
| `SELECT INSERT('a', 1, 1)` | 1064 |
| `SELECT INSERT('a', 1, 1, 'x', 'y')` | 1064 |

## Result metadata

For the supported text-string path, `INSERT()` returns `VAR_STRING` with the
current connection collation, MySQL's not-fixed decimals marker (`31`),
nullable metadata, and no numeric/binary flags. Binary-source table-backed
`INSERT()` results use binary collation id `63`, byte-counted source and
replacement widths, and the `BINARY` flag.

Constant result metadata was verified:

| Expression | Connection | Type | Collation id | Length | Decimals | Flags |
| --- | --- | --- | ---: | ---: | ---: | --- |
| `INSERT('Quadratic',3,4,'What')` | `utf8mb4` | `VAR_STRING` | `255` | `52` | `31` | none |
| `INSERT('abc',NULL,1,'x')` | `utf8mb4` | `VAR_STRING` | `255` | `16` | `31` | none |
| `INSERT('Quadratic',3,4,'What')` | `latin1` | `VAR_STRING` | `8` | `13` | `31` | none |
| `INSERT(varbinary_col,2,2,'-')` | `utf8mb4` | `VAR_STRING` | `63` | source octet length + 4 | `31` | `BINARY` |

Observed display length for ordinary constant and table-column text arguments
is based on the maximum source text length plus maximum replacement text
length, multiplied by the current result character set's maximum bytes per
character. It is not based on the actual post-splice result length. For
example, under `utf8mb4`, `'Quadratic'` plus `'What'` yields `(9 + 4) * 4 =
52`, and `VARCHAR(20)` plus `'X'` yields `(20 + 1) * 4 = 84`.

MySQL reports binary metadata for some `NULL` source combinations, such as
`INSERT(NULL,1,1,'x')`. MyLite's first slice keeps the existing text scalar
metadata architecture and targets the connection-collation text behavior above.

## Parser and AST design

Generic comma-separated calls continue to use
`MYLITE_SQL_AST_FUNCTION_CALL` and `MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST`.
No special AST node is required.

`INSERT` is a reserved statement keyword in MySQL and in MyLite's lexer. The
expression grammar must explicitly allow the `INSERT` token where a function
name is expected. MySQL accepts whitespace before the parenthesis for this
function, so MyLite does not need whitespace-sensitive handling for this slice.

The intended MyLite Lemon-style grammar addition is:

```lemon
function_name ::= INSERT.
```

`INSERT()` wrong-arity calls are parser-level syntax errors to match MySQL
8.4.9.

## Runtime design

Implementation extends the existing scalar-function registry in
`mylite_expression.c`:

- add a function id for `INSERT`
- validate exactly four arguments
- evaluate the source first, then replacement, then position, then length, so
  `NULL` short-circuit and warning behavior follows observed MySQL 8.4.9
  behavior
- return `NULL` when MySQL specifies `NULL` propagation
- convert the source and replacement operands with the existing string
  conversion helper
- convert `pos` and `len` through MyLite's signed integer-cast helper, after
  the observed MySQL `NULL` short-circuit checks
- use existing UTF-8 character-count and character-offset helpers for position
  and replacement length semantics

Splice rules:

- `pos <= 0` returns the original source string.
- `pos > CHAR_LENGTH(str)` returns the original source string.
- `len = 0` inserts `newstr` before the character at `pos`.
- `len > 0` replaces at most `len` characters, stopping at the end.
- `len < 0` replaces from `pos` through the end of the source.

## Tests

Add C tests for:

- parser acceptance of `INSERT(...)`, including `INSERT` as a reserved-token
  function name
- parser rejection for `INSERT()`, three-argument `INSERT()`, and five-argument
  `INSERT()`
- no-table scalar results for the verified examples above
- `NULL` propagation and observed conversion-warning short-circuit behavior
- numeric rounding and string-numeric truncation warnings
- UTF-8 character position and length behavior
- metadata for the verified `utf8mb4` and `latin1` constant cases
- table-backed projection, `WHERE`, and `ORDER BY`
- single-table `UPDATE` assignment/predicate and `DELETE` predicate paths

## Compatibility status

After implementation and verification, the `INSERT()` row in
`COMPATIBILITY.md` should move to partial support. The status remains partial
because binary-string runtime behavior, `max_allowed_packet`, full
collation/coercibility, exact all-`NULL` metadata, and full MySQL diagnostic
fidelity are deferred.
