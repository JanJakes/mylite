# `SUBSTRING_INDEX()` function

## Scope

This feature implements `SUBSTRING_INDEX(str, delim, count)` as a deterministic
string scalar built-in.

The function is available anywhere MyLite already evaluates the supported
scalar built-in subset:

- no-table `SELECT`
- one-table `SELECT` projection, `WHERE`, and `ORDER BY`
- supported single-table `UPDATE` assignments and predicates
- supported single-table `DELETE` predicates

Out of scope:

- binary-string-specific metadata beyond MyLite's current scalar text model
- complete collation aggregation and coercibility
- `INSERT ... VALUES` and `INSERT ... SET` scalar-function evaluation, matching
  the current Task 24 checkpoint
- exact native MySQL numeric error-code plumbing for unsupported arity, beyond
  MyLite's current deterministic unsupported-function prepare result

## Sources

- MySQL 8.4 Reference Manual, String Functions and Operators:
  https://dev.mysql.com/doc/refman/8.4/en/string-functions.html
- Existing MyLite scalar-function design:
  `docs/specs/scalar-built-in-functions/specs.md`
- Existing related MyLite string specs:
  - `docs/specs/string-functions-substring-trim/specs.md`
  - `docs/specs/string-search-code-functions/specs.md`
  - `docs/specs/string-list-index-functions/specs.md`

Observed behavior was verified against MySQL 8.4.9 in Docker container
`mylite-mysql-849`, using:

- `docker exec -i mylite-mysql-849 mysql -h127.0.0.1 -uroot --batch --raw --show-warnings --force`
- `docker exec -i mylite-mysql-849 mysql -h127.0.0.1 -uroot --column-type-info -vvv --force`

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar,
documentation prose, or implementation sources.

## MySQL behavior summary

Runtime probes used `SET NAMES utf8mb4` unless noted.

`SUBSTRING_INDEX(str, delim, count)` converts non-`NULL` arguments to the needed
string or integer form and returns a string slice based on delimiter matches.
The delimiter match is bytewise and case-sensitive for the currently supported
MyLite text model.

Rules:

- If any argument is `NULL`, the result is `NULL`.
- `count = 0` returns the empty string.
- Empty `delim` returns the empty string.
- Positive `count` searches from the left and returns the text before the
  delimiter that completes the requested occurrence count.
- Negative `count` searches from the right and returns the text after the
  delimiter that completes the absolute occurrence count.
- If the requested delimiter occurrence does not exist, the original string is
  returned.
- Multi-character delimiters are matched as whole byte sequences.
- UTF-8 input and delimiter strings are preserved; matching still occurs on the
  exact delimiter bytes.
- String counts use MySQL integer conversion rules for the current scalar
  evaluator: leading integer text is accepted and trailing garbage produces
  warning 1292; text without leading digits converts to `0` and warns.
- Unsigned integer literal counts larger than `INT64_MAX` are treated as large
  positive counts. String counts in the unsigned 64-bit range follow MySQL's
  signed-complement conversion without a warning; string counts outside the
  accepted integer range warn and clamp before that conversion.
- Approximate numeric counts are rounded to the nearest integer by the existing
  MyLite signed-integer conversion helper, matching the observed MySQL behavior
  for this function.

Verified examples:

| Expression | Result | Warnings |
| --- | --- | --- |
| `SUBSTRING_INDEX('www.mysql.com', '.', 2)` | `www.mysql` | none |
| `SUBSTRING_INDEX('www.mysql.com', '.', -2)` | `mysql.com` | none |
| `SUBSTRING_INDEX('www.mysql.com', '.', 0)` | empty string | none |
| `SUBSTRING_INDEX('www.mysql.com', '.', 9)` | `www.mysql.com` | none |
| `SUBSTRING_INDEX('www.mysql.com', '.', -9)` | `www.mysql.com` | none |
| `SUBSTRING_INDEX('abcdef', '.', 1)` | `abcdef` | none |
| `SUBSTRING_INDEX('abcdef', '', 1)` | empty string | none |
| `SUBSTRING_INDEX('', '.', 1)` | empty string | none |
| `SUBSTRING_INDEX('ab--cd--ef', '--', 2)` | `ab--cd` | none |
| `SUBSTRING_INDEX('ab--cd--ef', '--', -1)` | `ef` | none |
| `SUBSTRING_INDEX('AaA', 'a', 1)` | `A` | none |
| `SUBSTRING_INDEX('海.豚.猫', '.', 2)` | `海.豚` | none |
| `SUBSTRING_INDEX('海豚猫豚犬', '豚', 1)` | `海` | none |
| `SUBSTRING_INDEX('海豚猫豚犬', '豚', -1)` | `犬` | none |
| `SUBSTRING_INDEX(NULL, '.', 1)` | `NULL` | none |
| `SUBSTRING_INDEX('abc', NULL, 1)` | `NULL` | none |
| `SUBSTRING_INDEX('abc', '.', NULL)` | `NULL` | none |
| `SUBSTRING_INDEX(12345, 3, 1)` | `12` | none |
| `SUBSTRING_INDEX('a,b,c', ',', '2x')` | `a,b` | 1292 |
| `SUBSTRING_INDEX('a,b,c', ',', 'x')` | empty string | 1292 |
| `SUBSTRING_INDEX('a,b,c', ',', 1.9)` | `a,b` | none |
| `SUBSTRING_INDEX('a,b,c', ',', 18446744073709551615)` | `a,b,c` | none |
| `SUBSTRING_INDEX('a,b,c', ',', '18446744073709551615')` | `c` | none |
| `SUBSTRING_INDEX('a,b,c', ',', '18446744073709551616')` | `c` | 1292 |
| `SUBSTRING_INDEX('a,b,c', ',', '-9223372036854775809')` | `a,b,c` | 1292 |

MySQL rejects non-three-argument calls with error 1582. MyLite maps the same
arity mismatch to its current unsupported-function prepare path.

## Result metadata

The result is a nullable `VAR_STRING` using the connection character set and
collation. For the current MyLite descriptor model, the declared length follows
the first argument descriptor, matching the maximum possible returned slice.
Exact MySQL display-width inference for non-ASCII constant strings remains part
of the broader charset/collation metadata work.

Verified MySQL constant metadata:

| Connection | Expression | Type | Collation id | Length | Decimals | Flags |
| --- | --- | --- | ---: | ---: | ---: | --- |
| `utf8mb4` | `SUBSTRING_INDEX('www.mysql.com', '.', 2)` | `VAR_STRING` | 255 | 52 | 31 | none |
| `utf8mb4` | `SUBSTRING_INDEX('海.豚.猫', '.', 2)` | `VAR_STRING` | 255 | 20 | 31 | none |
| `utf8mb4` | `SUBSTRING_INDEX(12345, 3, 1)` | `VAR_STRING` | 255 | 24 | 31 | none |
| `latin1` | `SUBSTRING_INDEX('www.mysql.com', '.', 2)` | `VAR_STRING` | 8 | 13 | 31 | none |
| `latin1` | `SUBSTRING_INDEX(12345, 3, 1)` | `VAR_STRING` | 8 | 6 | 31 | none |

## Parser and AST design

No special grammar is required. `SUBSTRING_INDEX` is an ordinary scalar
function call with three comma-separated arguments, using the existing
`MYLITE_SQL_AST_FUNCTION_CALL` and `MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST`
shape.

The intended MyLite Lemon-style accepted shape is:

```lemon
scalar_function_call ::= function_name LPAREN function_argument_list RPAREN.
```

The function registry resolves `SUBSTRING_INDEX` case-insensitively and
requires exactly three arguments.

## Runtime design

Implementation extends the existing scalar-function registry in
`mylite_expression.c`:

- add a `SUBSTRING_INDEX` function id and case-insensitive registry entry
- validate exactly three arguments
- evaluate arguments left to right
- return `NULL` if any argument is `NULL`
- convert `str` and `delim` through the existing scalar string-conversion path
- convert `count` through `cast_value_to_signed_integer()` so string truncation
  warnings and approximate numeric rounding match other MyLite functions that
  require integer operands
- use length-aware byte scanning to preserve embedded bytes and avoid accidental
  dependence on NUL termination once the surrounding scalar value model allows
  embedded NULs in more paths
- allocate only the final result and temporary converted argument strings

Positive counts scan forward for whole-delimiter matches. Negative counts use
the same non-overlapping delimiter sequence, then select the requested match
from the right. Overlapping delimiter matches are not double-counted because the
next search continues after the matched delimiter.

## Tests

Add C tests for:

- parser acceptance of case-insensitive ordinary calls
- unsupported arity for one-, two-, and four-argument calls
- positive, negative, and zero count
- count larger than the number of occurrences
- empty delimiter and empty source string
- delimiter absent
- multi-character delimiter
- UTF-8 source strings and UTF-8 delimiters
- delimiter case sensitivity
- `NULL` propagation for each argument
- numeric string conversion, string-count warnings, and approximate numeric
  count conversion
- result metadata under `utf8mb4` and `latin1`
- no-table `SELECT`
- table projection, `WHERE`, and `ORDER BY`
- single-table `UPDATE` assignment/predicate
- single-table `DELETE` predicate

## Compatibility status

The `SUBSTRING_INDEX()` row in `COMPATIBILITY.md` is marked implemented with
documented gaps for broad collation coercion, binary-string-specific behavior,
`INSERT` expression paths, and exact native arity diagnostics.
