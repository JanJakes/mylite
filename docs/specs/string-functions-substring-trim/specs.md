# String function slice: CONCAT_WS, SUBSTRING, and TRIM

## Scope

This feature implements the next top-priority deterministic string-function
slice in MyLite's scalar expression evaluator:

- `CONCAT_WS(separator, arg [, arg] ...)`
- `SUBSTRING(str, pos)`, `SUBSTRING(str, pos, len)`, and the `SUBSTR()` /
  `MID()` synonyms
- SQL-standard `FROM` / `FOR` forms for `SUBSTRING`, `SUBSTR`, and `MID`
- `TRIM(str)` and the MySQL substring-removal forms
- `LTRIM(str)` and `RTRIM(str)`

The functions are available anywhere MyLite already evaluates the supported
Task 24 scalar-function subset: no-table `SELECT`, current table-backed
projection, `WHERE`, `ORDER BY`, and supported single-table `UPDATE` and
`DELETE` expression paths.

Out of scope:

- `SUBSTRING_INDEX()`; see the dedicated
  `docs/specs/substring-index-function/specs.md` slice
- `LOCATE()`, `POSITION()`, and `INSTR()`
- `LPAD()`, `RPAD()`, `REPEAT()`, `REVERSE()`, `HEX()`, and `UNHEX()`
- binary-string runtime behavior beyond the current MyLite value model
- full collation coercibility and character-set aggregation
- exact numeric error-code exposure for MyLite's current unsupported-function
  prepare errors, except where the parser naturally rejects syntax

## Sources

- MySQL 8.4 Reference Manual, String Functions and Operators:
  https://dev.mysql.com/doc/refman/8.4/en/string-functions.html
- Existing MyLite scalar-function design:
  `docs/specs/scalar-built-in-functions/specs.md`
- Existing MyLite expression and metadata designs:
  - `docs/specs/expression-operator-foundation/specs.md`
  - `docs/specs/result-metadata-expression-labels/specs.md`
  - `docs/specs/update-single-table/specs.md`
  - `docs/specs/delete-single-table/specs.md`

Observed behavior was verified against MySQL 8.4.9 in Docker container
`mylite-mysql-849`, using:

- `docker exec -i mylite-mysql-849 mysql -uroot --batch --raw --show-warnings --force`
- `docker exec -i mylite-mysql-849 mysql -uroot --column-type-info -vvv --force`

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar,
documentation prose, or implementation sources.

## MySQL behavior summary

Runtime probes used `SET NAMES utf8mb4`.

### `CONCAT_WS()`

`CONCAT_WS()` requires at least two arguments: the separator plus at least one
value argument. The separator is converted to string and inserted between
non-`NULL` value arguments.

Rules:

- `NULL` separator returns `NULL`.
- `NULL` value arguments after the separator are skipped.
- Empty-string value arguments are kept and can produce adjacent separators.
- If every value argument is `NULL`, the result is the empty string.
- Numeric and other scalar values use the same string conversion as existing
  MyLite string functions.

Verified examples:

| Expression | Result |
| --- | --- |
| `CONCAT_WS(',', 'a', NULL, '', 'b')` | `a,,b` |
| `CONCAT_WS(NULL, 'a')` | `NULL` |
| `CONCAT_WS(',', NULL, NULL)` | empty string |
| `CONCAT_WS(',', 'a')` | `a` |

`CONCAT_WS()` and `CONCAT_WS(',')` raise MySQL error 1582.

### `SUBSTRING()`, `SUBSTR()`, and `MID()`

The three names share behavior. The result starts at one-based character
position `pos`. Positive positions count from the beginning. Negative positions
count backward from the end, so `-1` names the final character. Position `0`
returns the empty string. When `len` is present, fewer than one character
requested returns the empty string.

The function is `NULL` if any argument is `NULL`.

Supported spellings:

- `SUBSTRING(str, pos)`
- `SUBSTRING(str, pos, len)`
- `SUBSTRING(str FROM pos)`
- `SUBSTRING(str FROM pos FOR len)`
- the same forms for `SUBSTR` and `MID`

Verified examples:

| Expression | Result |
| --- | --- |
| `SUBSTRING('abcdef', 2, 3)` | `bcd` |
| `SUBSTRING('abcdef', -2)` | `ef` |
| `SUBSTRING('abcdef', 0, 3)` | empty string |
| `SUBSTRING('abcdef', 2, 0)` | empty string |
| `SUBSTRING('abcdef', 2, -1)` | empty string |
| `SUBSTRING('海豚猫', 2, 1)` | `豚` |
| `SUBSTRING('abcdef' FROM 2 FOR 3)` | `bcd` |
| `SUBSTRING('abcdef' FROM -2)` | `ef` |
| `SUBSTR('abcdef', 2, 3)` | `bcd` |
| `MID('abcdef', 2, 3)` | `bcd` |

`SUBSTRING('abc')`, `SUBSTRING('abc',1,2,3)`, and malformed repeated `FOR`
syntax are syntax errors in MySQL 8.4.9.

### `TRIM()`, `LTRIM()`, and `RTRIM()`

`TRIM(str)` removes leading and trailing space characters. `LTRIM(str)` removes
leading space characters. `RTRIM(str)` removes trailing space characters.

`TRIM()` also accepts MySQL's substring-removal syntax:

- `TRIM(remstr FROM str)` removes `remstr` from both ends.
- `TRIM(BOTH remstr FROM str)` removes `remstr` from both ends.
- `TRIM(LEADING remstr FROM str)` removes `remstr` from the start.
- `TRIM(TRAILING remstr FROM str)` removes `remstr` from the end.
- `TRIM(BOTH FROM str)`, `TRIM(LEADING FROM str)`, and
  `TRIM(TRAILING FROM str)` use a single space as `remstr`.

Removal is repeated while the selected side starts or ends with the full
`remstr` string. Empty `remstr` leaves `str` unchanged. If any argument in the
accepted form is `NULL`, the result is `NULL`.

Verified examples:

| Expression | Result |
| --- | --- |
| `TRIM('  hi  ')` | `hi` |
| `LTRIM('  hi  ')` | `hi  ` |
| `RTRIM('  hi  ')` | `  hi` |
| `TRIM(BOTH 'x' FROM 'xxhix')` | `hi` |
| `TRIM(LEADING 'x' FROM 'xxhix')` | `hix` |
| `TRIM(TRAILING 'x' FROM 'xxhix')` | `xxhi` |
| `TRIM('x' FROM 'xxhix')` | `hi` |
| `TRIM(BOTH FROM '  hi  ')` | `hi` |
| `TRIM(LEADING FROM '  hi  ')` | `hi  ` |
| `TRIM(TRAILING FROM '  hi  ')` | `  hi` |

`TRIM(NULL)`, `TRIM(NULL FROM 'abc')`, and `TRIM('x' FROM NULL)` return
`NULL`. `TRIM()` and `TRIM('x','abc')` are syntax errors. `LTRIM()` /
`RTRIM()` with zero or two arguments raise MySQL error 1582.

## Result metadata

The first MyLite slice follows the existing scalar-function metadata policy:
supported text functions expose MySQL's not-fixed decimals marker and nullable
metadata without `NOT_NULL` or numeric flags for the supported paths.
`CONCAT_WS` and `SUBSTRING` use the known constant result length when one is
available. The trim family uses the source string's declared length, which
matches observed constant metadata. `LOWER` / `LCASE` and `UPPER` / `UCASE`
derive table-backed string metadata from the argument descriptor, preserving
source declared length and binary charset/flags for binary strings.
Binary-source `CONCAT_WS()`, `SUBSTRING()`, `SUBSTR()`, `MID()`, `TRIM()`,
`LTRIM()`, and `RTRIM()` expressions use binary collation id `63`,
byte-counted lengths, and the `BINARY` flag.

Verified constant metadata with `SET NAMES utf8mb4`:

| Expression | Type | Collation id | Length | Decimals | Flags |
| --- | --- | ---: | ---: | ---: | --- |
| `CONCAT_WS(',', 'a', 'b')` | `VAR_STRING` | `255` | `12` | `31` | none |
| `SUBSTRING('abcdef', 2, 3)` | `VAR_STRING` | `255` | `12` | `31` | none |
| `SUBSTR('abcdef', 2, 3)` | `VAR_STRING` | `255` | `12` | `31` | none |
| `MID('abcdef', 2, 3)` | `VAR_STRING` | `255` | `12` | `31` | none |
| `TRIM('  hi  ')` | `VAR_STRING` | `255` | `24` | `31` | none |
| `LTRIM('  hi  ')` | `VAR_STRING` | `255` | `24` | `31` | none |
| `RTRIM('  hi  ')` | `VAR_STRING` | `255` | `24` | `31` | none |
| `LOWER(varchar_col)` | `VAR_STRING` | source collation id | source octet length | `31` | none |
| `LOWER(varbinary_col)` | `VAR_STRING` | `63` | source octet length | `31` | `BINARY` |
| `LOWER(NULL)` | `VAR_STRING` | `63` | `0` | `31` | `BINARY` |
| `LOWER(int_col)` | `VAR_STRING` | `255` | source display length times connection max bytes per character | `31` | none |

Verified table-backed `SUBSTRING()` metadata over `s VARCHAR(12)` and
`n INT NULL` with `SET NAMES utf8mb4`:

| Expression | Type | Collation id | Length | Decimals | Flags |
| --- | --- | ---: | ---: | ---: | --- |
| `SUBSTRING(s,2,3)` | `VAR_STRING` | `255` | `12` | `31` | none |
| `SUBSTR(s,n,3)` | `VAR_STRING` | `255` | `12` | `31` | none |
| `MID(s,2,n)` | `VAR_STRING` | `255` | `44` | `31` | none |

Verified table-backed binary-source metadata over `vb VARBINARY(12)` with
`SET NAMES utf8mb4`:

| Expression | Type | Collation id | Length | Decimals | Flags |
| --- | --- | ---: | ---: | ---: | --- |
| `CONCAT_WS('.', vb, s)` | `VAR_STRING` | `63` | `64` | `31` | `BINARY` |
| `SUBSTRING(vb,2,3)` | `VAR_STRING` | `63` | `3` | `31` | `BINARY` |
| `TRIM(vb)` | `VAR_STRING` | `63` | `12` | `31` | `BINARY` |
| `SUBSTRING(s,n,n)` | `VAR_STRING` | `255` | `48` | `31` | none |
| `SUBSTRING(s,-2,n)` | `VAR_STRING` | `255` | `8` | `31` | none |
| `SUBSTRING(s,2)` | `VAR_STRING` | `255` | `44` | `31` | none |

## Parser and AST design

Generic comma-separated calls continue to use the existing
`MYLITE_SQL_AST_FUNCTION_CALL` and `MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST`
shape. `SUBSTRING` / `SUBSTR` / `MID` `FROM` forms are normalized into the same
argument-list shape as the comma forms so evaluator and metadata logic do not
need a separate substring AST.

`TRIM` special syntax needs to preserve a non-expression direction without
adding a separate expression node. MyLite annotates
`MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST` with a trim-spec flag and a direction
enum. Ordinary `TRIM(str)` has one unannotated argument. Special trim syntax is
normalized so the source string is the first argument and an explicit `remstr`,
when present, is the second argument.

The intended MyLite Lemon-style grammar additions are:

```lemon
scalar_function_call ::= function_name LPAREN function_argument_list RPAREN.
scalar_function_call ::= function_name LPAREN expression FROM expression RPAREN.
scalar_function_call ::= function_name LPAREN expression FROM expression FOR expression RPAREN.
scalar_function_call ::= function_name LPAREN trim_direction expression FROM expression RPAREN.
scalar_function_call ::= function_name LPAREN trim_direction FROM expression RPAREN.

trim_direction ::= BOTH.
trim_direction ::= LEADING.
trim_direction ::= TRAILING.
```

The parser helpers must reject `FROM` / `FOR` forms for names other than
`SUBSTRING`, `SUBSTR`, `MID`, and `TRIM`, so unrelated function calls with
`FROM` remain unsupported syntax rather than being accepted as generic function
calls.

## Runtime design

Implementation extends the existing scalar-function registry in
`mylite_expression.c`:

- add function ids for `CONCAT_WS`, `SUBSTRING`, `TRIM`, `LTRIM`, and `RTRIM`
- map `SUBSTR` and `MID` to the `SUBSTRING` id
- validate arity:
  - `CONCAT_WS`: at least two arguments
  - `SUBSTRING` / `SUBSTR` / `MID`: two or three arguments
  - `TRIM`: one expression argument or a trim-annotated list with one or two
    normalized arguments
  - `LTRIM` / `RTRIM`: one argument
- evaluate arguments left to right
- keep UTF-8 character offsets consistent with existing `LEFT` / `RIGHT`
  behavior
- preserve current deterministic unsupported-function prepare behavior for
  unrelated functions and arity failures

Substring indexes use character counts. The byte offsets for returned text come
from MyLite's UTF-8 offset helpers. Invalid UTF-8 is not expanded in this slice;
it follows the current text helper behavior used by `CHAR_LENGTH`, `LEFT`, and
`RIGHT`.

`TRIM`, `LTRIM`, and `RTRIM` remove ASCII space (`0x20`) for default-space
forms. Broader character-set space-class semantics are deferred until MyLite has
full character set and collation evaluation.

## Tests

Add C tests for:

- parser acceptance of `SUBSTRING` / `SUBSTR` / `MID` `FROM` and `FOR` forms
- parser acceptance of all supported `TRIM` forms
- syntax rejection for malformed `SUBSTRING` and `TRIM` forms
- no-table scalar results for all verified examples above
- metadata for the new text functions under default and non-default connection
  character sets
- `CONCAT_WS` separator `NULL`, skipped `NULL` values, empty strings, and
  all-`NULL` value arguments
- substring positive, negative, zero, zero-length, negative-length, omitted
  length, and multibyte offsets
- trim direction, omitted `remstr`, explicit single- and multichar `remstr`,
  empty `remstr`, and `NULL` propagation
- table-backed projection, `WHERE`, and `ORDER BY`
- table-backed special `TRIM` forms to verify copied trim metadata in cloned
  expression trees
- single-table `UPDATE` assignment/predicate and `DELETE` predicate paths
- unsupported arity for `CONCAT_WS`, `LTRIM`, and `RTRIM`, plus parser
  rejection for invalid `SUBSTRING` and `TRIM` arity forms

## Compatibility status

After implementation and verification, the `CONCAT_WS()`, `SUBSTRING()`,
`SUBSTR()`, `MID()`, `TRIM()`, `LTRIM()`, and `RTRIM()` rows in
`COMPATIBILITY.md` should move to MyLite's reduced-fidelity implemented status,
with notes about current text-function call sites and deferred binary runtime
behavior, collation/coercibility, and exact native error-code surfaces.
