# FORMAT() scalar function

## Scope

Implement MySQL-compatible scalar `FORMAT(X,D[,locale])` for the current MyLite
scalar expression call sites:

- no-table scalar `SELECT`
- one-table `SELECT` projection, `WHERE`, and `ORDER BY`
- single-table `UPDATE` assignments and predicates
- single-table `DELETE` predicates and order keys

`FORMAT()` returns a character string. It rounds its numeric input to the
requested decimal count, pads the fractional part to that count, inserts locale
grouping separators, and uses the connection character set/collation for result
metadata.

This slice intentionally does not add a full locale catalog or a heavyweight
locale dependency. It implements the MySQL 8.4.9 runtime-verified locales needed
by the first compatibility surface and documents fallback behavior for all other
locale names.

## Sources

- MySQL 8.4 Reference Manual, String Functions and Operators:
  https://dev.mysql.com/doc/refman/8.4/en/string-functions.html
- MySQL 8.4 Reference Manual, Locale Support:
  https://dev.mysql.com/doc/refman/8.4/en/locale-support.html
- Existing MyLite specs:
  - `docs/specs/scalar-built-in-functions/specs.md`
  - `docs/specs/round-function/specs.md`
  - `docs/specs/result-metadata-expression-labels/specs.md`
  - `docs/specs/character-set-collation-foundation/specs.md`

Runtime behavior was verified against MySQL 8.4.9 in Docker container
`mylite-mysql-849`, using the MySQL client with `--batch --raw --show-warnings`
for values and `--column-type-info -vvv` for result metadata.

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar,
documentation prose, or implementation sources.

## Syntax

MyLite's existing generic scalar function-call grammar accepts ordinary
identifier-style function calls. `FORMAT` is not a reserved keyword in this
surface.

Intended Lemon-shape snippets:

```lemon
expr(A) ::= ident_function_name(B) LPAREN expr(X) COMMA expr(D) RPAREN.
expr(A) ::= ident_function_name(B) LPAREN expr(X) COMMA expr(D) COMMA expr(L) RPAREN.
```

The binder validates `FORMAT` case-insensitively and accepts exactly two or
three arguments. MySQL 8.4.9 reports parser error 1064 for `FORMAT()`,
`FORMAT(1)`, and `FORMAT(1,2,3,4)`. MyLite may continue to report its existing
deterministic unsupported-arity diagnostic until native 1064 parity is added to
the generic function-call binder.

## Runtime Semantics

`FORMAT(X,D[,locale])` evaluates to `NULL` when `X` or `D` is `NULL`, with
MySQL's warning-sensitive evaluation rules:

- Literal locale arguments are validated before numeric conversion warnings are
  emitted. Non-literal locale expressions are evaluated after `D` conversion
  and before `X` conversion in the supported MyLite expression surface.
- After any literal-locale prevalidation, `D` is evaluated enough to detect
  `NULL`.
- If `D` is `NULL`, `X` numeric conversion is skipped; this suppresses warnings
  such as `FORMAT('abc', NULL)` and `FORMAT(1/0, NULL)`.
- If a third locale argument is present, it is evaluated and validated even when
  `X` or `D` is `NULL`.
- If `D` is not `NULL`, `D` is converted to an integer before `X` is evaluated
  and converted as a DOUBLE-style numeric input. This preserves the verified
  MySQL warning order for mixed text conversions such as
  `FORMAT('123abc','2abc')`.
- If `X` is `NULL`, `D` is still converted, so `FORMAT(NULL,'2abc')` returns
  `NULL` with one integer-conversion warning.

Verified NULL and evaluation behavior:

| Expression | Result | Warnings |
| --- | --- | --- |
| `FORMAT(NULL,2)` | `NULL` | none |
| `FORMAT(1.23,NULL)` | `NULL` | none |
| `FORMAT(NULL,1/0)` | `NULL` | 1365 division by zero |
| `FORMAT(1/0,NULL)` | `NULL` | none |
| `FORMAT(NULL,'2abc')` | `NULL` | 1292 integer truncation |
| `FORMAT(NULL,'2abc','bad_LOCALE')` | `NULL` | 1649 unknown locale, then 1292 integer truncation |
| `FORMAT(NULL,'2abc',1/0)` | `NULL` | 1292 integer truncation, 1365 division by zero, then 1649 unknown locale |
| `FORMAT('abc',NULL)` | `NULL` | none |
| `FORMAT(NULL,2,'bad_LOCALE')` | `NULL` | 1649 unknown locale |
| `FORMAT(NULL,NULL,NULL)` | `NULL` | 1649 unknown locale |
| `FORMAT(1/0,NULL,'bad_LOCALE')` | `NULL` | 1649 unknown locale only |
| `FORMAT(1234.56,NULL,'bad_LOCALE')` | `NULL` | 1649 unknown locale |

### Numeric input

`X` is converted using MySQL numeric-expression rules for this function. Text
inputs parse their numeric prefix, produce warning 1292 for truncated or
non-numeric text, and use zero when no numeric prefix is available.

Representative verified values:

| Expression | Result | Warnings |
| --- | --- | --- |
| `FORMAT('1234.567',2)` | `1,234.57` | none |
| `FORMAT('1234abc',2)` | `1,234.00` | 1292 double truncation |
| `FORMAT('abc',2)` | `0.00` | 1292 double truncation |
| `FORMAT('123abc','2abc')` | `123.00` | 1292 integer, then 1292 double |
| `FORMAT('123abc','2abc','bad_LOCALE')` | `123.00` | 1649 unknown locale, 1292 integer, then 1292 double |
| `FORMAT('abc','2abc',CONCAT('bad','_LOCALE'))` | `0.00` | 1292 integer, 1649 unknown locale, then 1292 double |

### Decimal count

`D` is converted as an integer count using MySQL integer conversion, then
clamped into the `0..30` range for `FORMAT()`.

Important verified behavior:

- non-integer numeric `D` rounds to the nearest integer before formatting:
  `2.1 -> 2`, `2.5 -> 3`, `2.9 -> 3`
- string `D` parses as an integer and warns when truncated:
  `'2abc' -> 2`, `'abc' -> 0`
- negative `D` formats with zero fractional digits after conversion and clamp
- values above `30` format with exactly 30 fractional digits

Representative verified values:

| Expression | Result |
| --- | --- |
| `FORMAT(123.456,2.9)` | `123.456` |
| `FORMAT(123.456,2.1)` | `123.46` |
| `FORMAT(1234.56,2.4)` | `1,234.56` |
| `FORMAT(1234.56,2.5)` | `1,234.560` |
| `FORMAT(123.456,-1)` | `123` |
| `FORMAT(1234.56,-2.4)` | `1,235` |
| `FORMAT(123.456,31)` | `123.456000000000000000000000000000` |

Exact numeric values round half away from zero for the verified MySQL 8.4.9
cases:

| Expression | Result |
| --- | --- |
| `FORMAT(2.5,0)` | `3` |
| `FORMAT(1.25,1)` | `1.3` |
| `FORMAT(-2.5,0)` | `-3` |

Approximate numeric values and text values converted through the DOUBLE path
follow MySQL's approximate rounding behavior. On the verified runtime,
`FORMAT(25E-1,0)` and `FORMAT('2.5',0)` both returned `2`. Approximate
negative values preserve a negative zero sign when rounding to zero, for example
`FORMAT(-0.001E0,2)` returns `-0.00`, while exact `FORMAT(-0.001,2)` returns
`0.00`.

### Locale

When omitted, the locale is `en_US`. MySQL 8.4.9 documentation describes locale
support generally, but runtime behavior is the source of truth for this slice.

Implemented locale table:

| Locale | Decimal separator | Grouping separator | Grouping pattern |
| --- | --- | --- | --- |
| omitted / `en_US` | `.` | `,` | western groups of 3 |
| `de_DE` | `,` | `.` | western groups of 3 |
| `en_IN` | `.` | `,` | final group of 3, preceding groups of 2 |
| `ru_RU` | `,` | space | western groups of 3 |
| `fr_FR` | `,` | none in verified runtime | no grouping |
| `nl_NL` | `,` | none in verified runtime | no grouping |

Locale names are matched case-insensitively, preserving MySQL's acceptance of
`DE_de`. `en-us` is not accepted by the verified runtime.

Unknown locale names, the empty string, and `NULL` locale values fall back to
`en_US` formatting and append warning 1649. MySQL 8.4.9 returns
`FORMAT(1234.56,2,NULL) = '1,234.56'` and warns `Unknown locale: 'NULL'`;
MyLite follows the runtime behavior rather than the documentation implication
that a `NULL` locale silently defaults.

Representative verified values:

| Expression | Result |
| --- | --- |
| `FORMAT(123456789.12,2)` | `123,456,789.12` |
| `FORMAT(123456789.12,2,'de_DE')` | `123.456.789,12` |
| `FORMAT(123456789.12,2,'en_IN')` | `12,34,56,789.12` |
| `FORMAT(123456789.12,2,'ru_RU')` | `123 456 789,12` |
| `FORMAT(123456789.12,2,'fr_FR')` | `123456789,12` |
| `FORMAT(123456789.12,2,'nl_NL')` | `123456789,12` |
| `FORMAT(123456789.12,2,'bad_LOCALE')` | `123,456,789.12` plus warning 1649 |

## Metadata

`FORMAT()` result metadata follows MySQL's string-result behavior for the
current connection:

- field type: `VAR_STRING`
- decimals: `31`
- flags: no `NOT_NULL`, no numeric flag, no binary flag
- charset/collation: current connection collation, for example
  `latin1_swedish_ci` id `8` or `utf8mb4_0900_ai_ci` id `255`
- nullable: yes

The first slice matches the verified constant and column descriptor lengths used
by the tests:

| Expression/source | latin1 length |
| --- | ---: |
| `FORMAT(1234,2)` | `38` |
| `FORMAT(-1234,2)` | `38` |
| `FORMAT(1234.5,2)` | `41` |
| `FORMAT(1234.567,2)` | `44` |
| `FORMAT(1234.567E0,2)` | `61` |
| `FORMAT('1234.567',2)` | `42` |
| `FORMAT(NULL,2)` | `32` |
| `FORMAT(1234.567,2,'de_DE')` | `44` |
| `INT` column argument | `46` |
| `DECIMAL(8,3)` column argument | `45` |
| `DOUBLE` column argument | `61` |
| `VARCHAR(32)` column argument | `74` |

Signs on numeric literals do not widen the `FORMAT()` descriptor. Decimal
literals use MySQL's fixed-point descriptor width plus grouping slack rather
than the final formatted display text. With `SET NAMES utf8mb4`, the verified
lengths are `164` bytes for `FORMAT(1234.5,2)` and `168` bytes for
`FORMAT(1234.56,2)`.

## DML Behavior

`FORMAT()` returns a string, so sort and comparison behavior follows the current
string comparison subset for supported expression contexts:

- `ORDER BY FORMAT(d,2)` sorts `NULL`, `'-1,234.57'`, then `'1,234.57'`
  lexically in the verified fixture.
- `WHERE FORMAT(d,0)='-1,235'` matches the negative rounded row.
- `UPDATE t SET s = FORMAT(d,1)` stores formatted strings such as `1,234.6`
  and `-1,234.6`.
- `DELETE FROM t WHERE FORMAT(nn,0)='0'` can remove rows whose numeric value
  rounds to the formatted string `0`.

## Implementation Notes

The implementation should stay inside the existing scalar function registry and
expression evaluator:

- add `FORMAT` as a supported scalar function with arity 2 or 3
- format using internal decimal/grouping helpers instead of SQLite or platform
  locale APIs
- use existing expression warning machinery for 1292 conversion warnings and
  add warning 1649 for unknown locales
- derive string result metadata in `mylite.c`, using the current connection
  charset/collation

## Deferred Gaps

- Full MySQL locale catalog beyond `en_US`, `de_DE`, `en_IN`, `ru_RU`,
  `fr_FR`, and `nl_NL`.
- Native MySQL parser error 1064 for invalid `FORMAT()` arity, if the generic
  function binder still reports MyLite's unsupported-arity diagnostic.
- Exact fixed-point preservation for every DECIMAL column and expression path
  beyond the currently supported value representation.
- Exhaustive metadata length parity for every possible source expression class.
