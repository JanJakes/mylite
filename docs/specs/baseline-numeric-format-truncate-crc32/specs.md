# Baseline Numeric FORMAT/TRUNCATE/CRC32 Functions

## Status

This phase completes the remaining baseline numeric/math function rows for
`CRC32()`, `FORMAT()`, and `TRUNCATE()` in the current scalar-expression
surface. The implementation is deliberately limited to no-source `SELECT`,
`SELECT ... FROM DUAL`, and `DO` expression execution over literal operands.
The later `baseline-row-numeric-extra-functions` slice extends these functions
into limited single-table row-scalar projection and `CONCAT()` nesting.

This slice does not add expression predicates, generated columns,
default-expression support, indexes, collation semantics, locale catalogs, or
SQLite fork changes.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing scalar numeric function implementation in
  `packages/libmylite/src/runtime/mylite_execution.c`
- Existing SQL parser/AST function scaffolding in `packages/libmylite/src/sql/`
- MySQL 8.4 Reference Manual, mathematical functions:
  https://dev.mysql.com/doc/refman/8.4/en/mathematical-functions.html
- MySQL 8.4 Reference Manual, string functions:
  https://dev.mysql.com/doc/refman/8.4/en/string-functions.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, and
existing MyLite source code. It does not copy MySQL, MariaDB, Percona, SQLite
implementation internals, or other restrictively licensed implementation
sources.

## MySQL 8.4.9 Observations

Runtime probes were run against the local `mysql:8.4.9` Docker runtime named
`mylite-mysql-849`.

Observed behavior shaping this slice:

- `CRC32(expr)` returns an unsigned 32-bit decimal result, `0` for an empty
  string, and `NULL` for `NULL`.
- `CRC32()` and `CRC32(a,b)` return MySQL error `1582 / 42000`.
- `CRC32(123)`, `CRC32(TRUE)`, and `CRC32(FALSE)` checksum the visible string
  forms `123`, `1`, and `0`.
- `CRC32(X'616263')` returns the checksum of the binary bytes `abc`.
- `CRC32(1+2)` evaluates the expression and checksums the visible value `3` in
  MySQL. This slice intentionally rejects expression arguments.
- `FORMAT(X,D)` returns an `en_US` formatted string with comma group separators,
  decimal point, and exactly `D` fractional digits after rounding. `D < 0`
  behaves like `0`; `D > 30` is capped at `30`.
- `FORMAT(1, TRUE)` and `FORMAT(1, FALSE)` use decimal-place values `1` and
  `0`.
- `FORMAT(X,D,NULL)` returns the default formatted value and records warning
  `1649 / HY000` for the unknown `NULL` locale. This slice does not admit the
  third argument yet.
- `FORMAT(1,2.6)` coerces the second argument and returns `1.000` in MySQL.
  This slice intentionally rejects non-integer decimal-place literals.
- `FORMAT(1,2,'de_DE')` applies locale-specific punctuation in MySQL. This
  slice intentionally rejects the locale form.
- `FORMAT()` / one argument / four arguments are syntax errors in MySQL, while
  MyLite may use its existing native-function parameter-count diagnostic for
  deterministic unsupported arity handling.
- `TRUNCATE(X,D)` truncates toward zero. Positive `D` keeps at most `D`
  fractional digits but never pads beyond the first argument's exact scale;
  `D = 0` removes the fractional part; negative `D` zeros digits to the left of
  the decimal point. `NULL` in either argument returns `NULL`.
- `TRUNCATE(1.9, TRUE)` and `TRUNCATE(1.9, FALSE)` use decimal-place values
  `1` and `0`.
- `TRUNCATE(1234,2)` returns integer text `1234`, while
  `TRUNCATE(1234.100,2)` returns `1234.10`.
- MySQL warns for string-to-number conversion in `FORMAT('abc',2)` and
  `TRUNCATE('abc',2)`. This slice rejects string numeric conversion rather than
  implementing broader expression coercion.

## Scope

Supported:

- `CRC32(value)` in no-source `SELECT`, `SELECT ... FROM DUAL`, and `DO`;
- `FORMAT(value, decimals)` in no-source `SELECT`, `SELECT ... FROM DUAL`, and
  `DO`;
- `TRUNCATE(value, decimals)` in no-source `SELECT`, `SELECT ... FROM DUAL`, and
  `DO`;
- direct literal operands only:
  - `NULL`, `TRUE`, and `FALSE`;
  - decimal integer literals with optional unary `+` or `-`;
  - fixed decimal literals with optional unary `+` or `-` for `FORMAT()` and
    `TRUNCATE()`;
  - ordinary string literals and hex literals for `CRC32()`;
- existing scalar projection aliases, mixed scalar projection lists, and scalar
  diagnostics conventions;
- `warning_count == 0` for all supported non-warning forms.

Out of scope:

- table-backed function projection and row-scalar evaluation, now covered
  separately by `baseline-row-numeric-extra-functions`;
- function operands other than the admitted direct literals;
- string-to-number conversion for `FORMAT()` / `TRUNCATE()`;
- approximate float operands;
- `FORMAT()` locale argument support, including `de_DE` and unknown-locale
  warning behavior;
- non-ASCII collation semantics;
- expression metadata beyond existing scalar-result conventions;
- SQLite SQL translation, SQLite extension APIs, and SQLite fork patches.

## Ownership Boundary

- Public API: unchanged. Applications continue to use `mylite_execute()` and
  the existing result accessors.
- Statement context: unchanged except for ordinary diagnostics produced by the
  scalar execution path.
- Parser/AST: adds MyLite-owned AST nodes for the three function calls and
  deterministic arity/unsupported-locale shapes where useful.
- Analyzer/runtime: validates the admitted literal-only scalar operands,
  computes values in MyLite code, stages warnings where the supported subset can
  produce them, and rejects broader expression forms deterministically.
- Catalog/storage/VFS: unchanged. The functions do not read or write
  descriptors, SQLite user rows, catalog rows, `.mylite` preamble bytes, or
  shifted SQLite payload state.
- SQLite physical storage: unchanged. No SQLite SQL is generated for this
  scalar slice.

## Syntax

Independently authored MyLite Lemon-syntax snippets:

```lemon
expression(A) ::= CRC32(T) LPAREN expression(B) RPAREN(R).
expression(A) ::= FORMAT(T) LPAREN expression(B) COMMA expression(C) RPAREN(R).
expression(A) ::= FORMAT(T) LPAREN expression(B) COMMA expression(C) COMMA expression(D) RPAREN(R).
expression(A) ::= TRUNCATE(T) LPAREN expression(B) COMMA expression(C) RPAREN(R).

expression(A) ::= CRC32(T) LPAREN RPAREN(R).
expression(A) ::= CRC32(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R).
expression(A) ::= FORMAT(T) LPAREN RPAREN(R).
expression(A) ::= FORMAT(T) LPAREN expression(B) RPAREN(R).
expression(A) ::= FORMAT(T) LPAREN expression(B) COMMA expression(C) COMMA expression(D) COMMA function_argument_list(E) RPAREN(R).
expression(A) ::= TRUNCATE(T) LPAREN RPAREN(R).
expression(A) ::= TRUNCATE(T) LPAREN expression(B) RPAREN(R).
expression(A) ::= TRUNCATE(T) LPAREN expression(B) COMMA expression(C) COMMA function_argument_list(D) RPAREN(R).
```

## Runtime Semantics

`CRC32()`:

- `NULL` input returns `NULL`.
- `TRUE` and `FALSE` are converted to `1` and `0`.
- decimal integer literals are normalized to their visible decimal text before
  checksumming.
- ordinary string literals are decoded using the current SQL mode string policy.
- hex literals are decoded as binary bytes.
- the checksum implementation is MyLite-owned IEEE CRC-32 with the MySQL-visible
  unsigned decimal result.

`FORMAT()`:

- `NULL` in either admitted argument returns `NULL`.
- `TRUE` and `FALSE` numeric arguments are `1` and `0`.
- fixed decimal and integer first arguments are formatted from exact decimal
  text, not through SQLite.
- the decimal-place argument is an admitted integer, boolean, or `NULL` literal;
  `TRUE` and `FALSE` are `1` and `0`; integer values below zero are treated as
  zero and values above thirty are capped at thirty.
- rounding is half away from zero for the admitted exact-value subset.
- the result uses comma group separators and a dot decimal separator.
- a parsed third locale argument is rejected with an explicit unsupported
  diagnostic in this slice.

`TRUNCATE()`:

- `NULL` in either admitted argument returns `NULL`.
- `TRUE` and `FALSE` numeric arguments are `1` and `0`.
- fixed decimal and integer first arguments are evaluated from exact decimal
  text.
- the decimal-place argument is an admitted integer, boolean, or `NULL` literal;
  `TRUE` and `FALSE` are `1` and `0`.
- positive `D` keeps at most `D` fractional digits and never pads beyond the
  first argument's original scale.
- `D = 0` emits no fractional part.
- negative `D` zeros digits to the left of the decimal point; when the magnitude
  covers all integer digits, the result is `0`.
- all truncation is toward zero.

## Diagnostics

Supported in-range calls produce no warnings.

Deterministic errors:

- wrong argument count: existing native-function parameter-count diagnostic;
- `FORMAT()` locale argument: MyLite-specific unsupported diagnostic;
- unsupported operands: MyLite-specific unsupported diagnostics naming the
  admitted scalar subset;
- malformed decoded string or hex literals: existing literal diagnostics;
- allocation failure: existing `MYLITE_NOMEM` / out-of-memory diagnostics.

The broader MySQL warning-producing coercion surface for string, float, and
locale arguments remains out of scope. MySQL-supported expression operands,
coerced decimal-place expressions such as `FORMAT(1,2.6)`, and locale-specific
`FORMAT()` output are verified as future work and rejected deterministically in
this slice.

## Performance

All computation is scalar and in-process. `CRC32()` scans the decoded input once.
`FORMAT()` and `TRUNCATE()` operate on bounded literal text and produce bounded
result strings. No row materialization, catalog scan, SQLite prepare/step, or
file I/O is introduced.

## Tests

Add a focused C runtime test and a MySQL expectation script. Coverage must
include:

- no-source `SELECT`, `SELECT ... FROM DUAL`, and `DO` execution;
- aliases and mixed scalar projection lists;
- `CRC32()` string, empty string, `NULL`, integer, boolean, signed integer, and
  hex literal inputs;
- `FORMAT()` integer, fixed decimal, negative value, `NULL`, negative `D`, and
  `D > 30`;
- `TRUNCATE()` positive, zero, and negative `D`; integer versus fixed decimal
  display; negative values; `NULL`;
- unsupported arity, unsupported locale, unsupported string numeric conversion,
  and unsupported nested expression forms;
- diagnostics snapshots and zero-warning successful supported cases;
- file preamble and independent-handle preservation.

The later `baseline-row-numeric-extra-functions` slice owns row-backed
projection tests for these functions.

Verification before commit:

```sh
cmake --build --preset dev
ctest --preset dev -R '^libmylite\.runtime\.(numeric_format_truncate_crc32|round_function|ceil_floor_functions|bin_oct_functions|bit_count_function)$' --output-on-failure
packages/libmylite/tests/mysql_baseline_numeric_format_truncate_crc32_expectations.sh
cmake --workflow --preset check
```
