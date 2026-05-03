# String search and leftmost-character code functions

## Scope

This feature implements the high-priority deterministic string functions:

- `ASCII(str)`
- `ORD(str)`
- `LOCATE(substr, str)` and `LOCATE(substr, str, pos)`
- `POSITION(substr IN str)`
- `INSTR(str, substr)`

The functions are available anywhere MyLite already evaluates the supported
Task 24 scalar-function subset: no-table `SELECT`, current table-backed
projection, `WHERE`, `ORDER BY`, and supported single-table `UPDATE` and
`DELETE` expression paths.

Out of scope:

- binary-string-specific display and matching behavior
- collation-sensitive substring matching, including case-insensitive and
  accent-insensitive equivalence
- embedded NUL byte behavior in string values
- exact native MySQL error-code exposure for every unsupported arity path
- `REGEXP_INSTR()` and the broader regular-expression function family

## Sources

- MySQL 8.4 Reference Manual, String Functions and Operators:
  https://dev.mysql.com/doc/refman/8.4/en/string-functions.html
- Existing MyLite scalar-function design:
  `docs/specs/scalar-built-in-functions/specs.md`
- Existing MyLite string-function slice:
  `docs/specs/string-functions-substring-trim/specs.md`
- Existing MyLite expression and metadata designs:
  - `docs/specs/expression-operator-foundation/specs.md`
  - `docs/specs/result-metadata-expression-labels/specs.md`
  - `docs/specs/update-single-table/specs.md`
  - `docs/specs/delete-single-table/specs.md`

Observed behavior was verified against MySQL 8.4.9 with `SET NAMES utf8mb4`.
The main probe set was supplied by the parent task, and local Docker probes
against container `mylite-mysql-849` verified empty-substring start-position
edges and default-collation case-insensitive examples.

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar,
documentation prose, or implementation sources.

## MySQL behavior summary

The official string-function documentation defines one-based string positions,
`ASCII()` leftmost-byte behavior for 8-bit characters, `ORD()` leftmost
character-code behavior, multibyte-safe `LOCATE()` / `INSTR()` search
positions, and `POSITION(substr IN str)` as the two-argument `LOCATE()`
synonym.

Runtime probes used `SET NAMES utf8mb4`.

### `ASCII()`

`ASCII(str)` evaluates its argument as a string. If the argument is `NULL`, the
result is `NULL`. If the string is empty, the result is `0`. Otherwise the
result is the unsigned numeric value of the first byte in the string.

Verified examples:

| Expression | Result |
| --- | --- |
| `ASCII('')` | `0` |
| `ASCII(NULL)` | `NULL` |
| `ASCII('A')` | `65` |
| `ASCII('海')` | `230` |

`ASCII()` and `ASCII('a','b')` are syntax errors in MySQL 8.4.9.

### `ORD()`

`ORD(str)` evaluates its argument as a string. If the argument is `NULL`, the
result is `NULL`. If the string is empty, the result is `0`. For a single-byte
leftmost character, the result matches `ASCII(str)`. For a multibyte leftmost
character, observed MySQL 8.4.9 packs the bytes of that first character in
input order by repeatedly multiplying the accumulator by `256` and adding the
next byte.

Verified examples:

| Expression | Result |
| --- | --- |
| `ORD('')` | `0` |
| `ORD(NULL)` | `NULL` |
| `ORD('A')` | `65` |
| `ORD('海')` | `15119799` |

`ORD()` and `ORD('a','b')` raise MySQL error 1582.

### `LOCATE()`

`LOCATE(substr, str)` returns the one-based character position of the first
occurrence of `substr` in `str`, or `0` when no match exists. If either
argument is `NULL`, the result is `NULL`.

`LOCATE(substr, str, pos)` starts searching at one-based character position
`pos`. Runtime behavior for this slice:

- `pos <= 0` returns `0`.
- `pos` beyond the end returns `0`, except an empty `substr` may match at
  `CHAR_LENGTH(str) + 1`.
- an empty `substr` returns the effective start position when that position is
  between `1` and `CHAR_LENGTH(str) + 1`.

Verified examples:

| Expression | Result |
| --- | --- |
| `LOCATE('pha', 'alpha')` | `3` |
| `LOCATE('z', 'alpha')` | `0` |
| `LOCATE('', 'alpha')` | `1` |
| `LOCATE('a', 'alpha', 2)` | `5` |
| `LOCATE('a', 'alpha', 0)` | `0` |
| `LOCATE('a', 'alpha', -1)` | `0` |
| `LOCATE('a', 'alpha', 99)` | `0` |
| `LOCATE(NULL, 'alpha')` | `NULL` |
| `LOCATE('a', NULL)` | `NULL` |
| `LOCATE('豚', '海豚猫')` | `2` |
| `LOCATE('猫', '海豚猫', 3)` | `3` |
| `LOCATE('猫', '海豚猫', 4)` | `0` |
| `LOCATE('', 'alpha', 2)` | `2` |
| `LOCATE('', 'alpha', 6)` | `6` |
| `LOCATE('', 'alpha', 7)` | `0` |

`LOCATE('a')` and `LOCATE('a','b','c','d')` raise MySQL error 1582.

### `POSITION()`

`POSITION(substr IN str)` is the special SQL form of two-argument `LOCATE()`.
The parser should normalize it into a function-call AST with the argument
order `(substr, str)` so runtime and metadata behavior share the same path as
`LOCATE()`.

Verified examples:

| Expression | Result |
| --- | --- |
| `POSITION('ph' IN 'alpha')` | `3` |
| `POSITION('z' IN 'alpha')` | `0` |
| `POSITION('' IN 'alpha')` | `1` |
| `POSITION('a' IN ('abc'))` | `1` |

`POSITION('a')`, `POSITION('a','b')`, and malformed repeated `IN` forms are
syntax errors in MySQL 8.4.9.

### `INSTR()`

`INSTR(str, substr)` shares the two-argument `LOCATE()` search behavior, but
the argument order is reversed. If either argument is `NULL`, the result is
`NULL`.

Verified examples:

| Expression | Result |
| --- | --- |
| `INSTR('alpha', 'ph')` | `3` |
| `INSTR('alpha', 'z')` | `0` |
| `INSTR('海豚猫', '豚')` | `2` |

`INSTR('a')` and `INSTR('a','b','c')` raise MySQL error 1582.

## Result metadata

The implemented functions return `LONGLONG` with binary charset id `63`,
decimals `0`, and numeric/binary flags. Constant non-`NULL` calls are
`NOT_NULL`; calls whose arguments can be `NULL` are nullable.

Verified constant metadata with `SET NAMES utf8mb4`:

| Expression | Type | Charset id | Length | Decimals | Flags |
| --- | --- | ---: | ---: | ---: | --- |
| `ASCII('A')` | `LONGLONG` | `63` | `3` | `0` | `NOT_NULL BINARY NUM` |
| `ORD('海')` | `LONGLONG` | `63` | `21` | `0` | `NOT_NULL BINARY NUM` |
| `LOCATE('ph','alpha')` | `LONGLONG` | `63` | `11` | `0` | `NOT_NULL BINARY NUM` |
| `POSITION('ph' IN 'alpha')` | `LONGLONG` | `63` | `11` | `0` | `NOT_NULL BINARY NUM` |
| `INSTR('alpha','ph')` | `LONGLONG` | `63` | `11` | `0` | `NOT_NULL BINARY NUM` |

Changing the connection character set does not change these numeric-result
descriptors.

## Parser and AST design

Generic comma-separated calls continue to use
`MYLITE_SQL_AST_FUNCTION_CALL` and `MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST`.
`POSITION(substr IN str)` is represented with the same function-call shape and
two normalized arguments. Runtime maps the `POSITION` function name to the same
implementation id as `LOCATE`.

The intended MyLite Lemon-style grammar addition is:

```lemon
position_search_operand ::= bit_or_expression.
scalar_function_call ::= POSITION LPAREN position_search_operand IN expression RPAREN.
```

The parser maps unquoted `POSITION` to a dedicated parser token for this
special form while keeping it usable as an ordinary identifier elsewhere.
Ordinary `POSITION(...)` comma calls remain syntax errors because MySQL does
not accept that spelling. The left operand intentionally stops below
comparison operators so the separator `IN` is unambiguous in MyLite's
expression grammar; comparison expressions can still be used there when
parenthesized. Other function calls must continue to accept ordinary `IN`
predicate arguments such as `ABS(1 IN (1))`. `ASCII()` and `ASCII(a,b)` are
also parser-level syntax errors to match MySQL 8.4.9.

## Runtime design

Implementation extends the existing scalar-function registry in
`mylite_expression.c`:

- add function ids for `ASCII`, `ORD`, `LOCATE`, and `INSTR`
- map `POSITION` to the `LOCATE` function id
- validate arity:
  - `ASCII`: one argument
  - `ORD`: one argument
  - `LOCATE` / `POSITION`: two or three arguments for the shared id, with the
    parser only producing two arguments for `POSITION`
  - `INSTR`: two arguments
- evaluate arguments left to right
- return `NULL` if any accepted argument is `NULL`
- convert non-string scalar arguments using the existing string conversion
  helper

`ASCII()` uses the first byte of the converted string. `ORD()` uses the bytes
belonging to the first UTF-8 character according to the existing MyLite UTF-8
helper policy.

`LOCATE()` and `INSTR()` use MyLite's current UTF-8 helper behavior to return
character positions while matching exact byte sequences. Full collation and
binary-string-specific matching are deferred until MyLite has a broader
collation coercibility and binary string model.

## Tests

Add C tests for:

- parser acceptance of `POSITION(substr IN str)`
- parser rejection for ordinary `POSITION(...)`, malformed repeated `IN`
  forms, `ASCII()`, `ASCII(a,b)`, and non-`POSITION` `... IN ...` function
  forms
- ordinary function-call parsing for `ASCII`, `ORD`, `LOCATE`, and `INSTR`
- no-table scalar results for all verified examples above
- metadata for the new numeric functions under `SET NAMES utf8mb4`
- connection charset metadata stability under another supported connection
  character set
- table-backed projection, `WHERE`, and `ORDER BY`
- single-table `UPDATE` assignment/predicate and `DELETE` predicate paths
- unsupported arity for `ORD`, `LOCATE`, and `INSTR`

## Compatibility status

After implementation and verification, the `ASCII()`, `ORD()`, `LOCATE()`,
`POSITION()`, and `INSTR()` rows in `COMPATIBILITY.md` should move to
MyLite's reduced-fidelity implemented status, with notes about current scalar
expression call sites and deferred binary/collation-specific matching.
