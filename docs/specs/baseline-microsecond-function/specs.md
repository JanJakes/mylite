# Baseline MICROSECOND Function

## Summary

This phase adds a narrow MySQL-compatible `MICROSECOND(expr)` slice. The
supported surface covers no-source scalar `SELECT`, `SELECT ... FROM DUAL`,
`DO`, and single-table row-scalar `SELECT` projection contexts over `NULL`,
canonical time/datetime strings with optional fractional seconds, date-only
strings, and descriptor `DATE`, `TIME`, `DATETIME`, `TIMESTAMP`, and
nonbinary string-family columns.

The implementation stays in MyLite's parser, analyzer/planner, diagnostics,
and runtime scalar function layer. SQLite remains the physical execution
engine for row scanning, filtering, ordering, limiting, and file-backed storage.
MyLite registers and calls its existing `_mylite_temporal_extract` SQLite
scalar function for row-backed projection; no SQLite fork patch is required.

## Sources And Evidence

- MyLite architecture and engineering standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
  - `third_party/sqlite/README.md`
- Existing temporal extractor design and tests:
  - `docs/specs/baseline-temporal-extract-functions/specs.md`
  - `docs/specs/baseline-time-function/specs.md`
  - `docs/specs/baseline-extract-function/specs.md`
  - `packages/libmylite/tests/mysql_baseline_temporal_extract_functions_expectations.sh`
- Official MySQL 8.4 Reference Manual:
  - date and time functions:
    <https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html>
  - fractional seconds in temporal values:
    <https://dev.mysql.com/doc/refman/8.4/en/fractional-seconds.html>
  - function-name parsing and resolution:
    <https://dev.mysql.com/doc/refman/8.4/en/function-resolution.html>
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_microsecond_function_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the behavior used by this phase:

- `MICROSECOND('12:00:00.123456')` returns `123456`.
- `MICROSECOND('12:00:00.1')` returns `100000`.
- `MICROSECOND('12:00:00.000001')` returns `1`.
- `MICROSECOND('12:00:00.999999')` returns `999999`.
- Fractional text with more than six digits is rounded to six fractional
  digits for this function. For example, `.1234567` returns `123457`; a carry
  from `.9999995` yields microsecond part `0`.
- `MICROSECOND('2019-12-31 23:59:59.000010')` returns `10`.
- Time or datetime strings without a fractional part return `0`.
- `MICROSECOND(NULL)` returns `NULL`.
- Date-only strings return `0` and warning `1292` with text beginning
  `Truncated incorrect time value:`.
- Datetime-shaped strings whose time part is outside the datetime `00:00:00`
  through `23:59:59` range return `NULL` with the same warning shape.
- Invalid non-`NULL` strings return `NULL` and the same warning shape.
- Stored descriptor `DATE`, `TIME`, `DATETIME`, and `TIMESTAMP` values without
  fractional seconds return `0` without adding warnings.
- String descriptor columns are parsed like string arguments; invalid strings
  return `NULL` with warning `1292`.
- MySQL accepts broader numeric temporal coercion such as
  `MICROSECOND(123456.789)`. This phase defers numeric, decimal, boolean,
  hex/bit, parameter, and arbitrary-expression arguments to a later general
  expression/coercion slice.
- `MICROSECOND()` with no argument and `MICROSECOND(value, extra)` are syntax
  errors in the current parser envelope.

## Supported SQL

No-source and `DUAL` forms:

```sql
SELECT MICROSECOND(microsecond_value)[, ...]
SELECT MICROSECOND(microsecond_value)[, ...] FROM DUAL
```

`DO` form:

```sql
DO MICROSECOND(microsecond_value)[, ...]
```

Single-table row-backed forms:

```sql
SELECT row_scalar_item[, row_scalar_item ...]
FROM table_name [AS alias]
[WHERE predicate]
[ORDER BY descriptor_column [ASC | DESC]]
[LIMIT row_count]
```

The admitted expression shape is:

```lemon
microsecond_expr(A) ::= MICROSECOND(T) LPAREN microsecond_value(V) RPAREN(R).
```

`microsecond_value` is one of:

- `NULL`;
- a single- or double-quoted string literal containing:
  - `HH:MM:SS`;
  - `-HH:MM:SS`;
  - `HHH:MM:SS`;
  - the same time forms with `.fraction`;
  - `YYYY-MM-DD HH:MM:SS`;
  - zero-date or partial-zero datetime forms with a time part;
  - the same datetime forms with `.fraction`;
  - `YYYY-MM-DD`, `0000-00-00`, or partial-zero date-only strings, returning
    `0` with a truncation warning;
- a descriptor column in a table-backed row-scalar `SELECT` whose descriptor
  family is `DATE`, `TIME`, `DATETIME`, `TIMESTAMP`, `CHAR`, `VARCHAR`, or the
  baseline `TEXT` family.

The fractional suffix parser accepts one or more decimal digits. It computes
the six-digit microsecond part by padding short fractions with trailing zeroes
and rounding based on the seventh digit when present. If rounding produces
`1000000`, only the microsecond part is visible, so the result is `0`.

Successful supported calls return an integer/`NULL` value through existing
public result conventions. Successful supported calls produce no warnings
except date-only string coercions, which match MySQL's warning-producing time
coercion shape.

## Deferred Surface

This slice intentionally does not support:

- numeric temporal literals, decimal or float values, boolean values, bit/hex
  literals, parameters, variables as arguments, subqueries, or arbitrary
  expression arguments;
- `EXTRACT(MICROSECOND FROM value)`, because MySQL's `EXTRACT()` time-unit
  sign rules are distinct from `MICROSECOND()` and belong with the broader
  `EXTRACT()` compatibility surface;
- relaxed date/time strings outside the explicitly listed forms, compact
  numeric temporal text, two-digit year coercion, locale or time-zone coercion,
  and broader SQL-mode-sensitive temporal parsing;
- use in `WHERE`, `ORDER BY`, `GROUP BY`, `HAVING`, DML assignments, defaults,
  generated columns, indexes, constraints, joins, CTEs, or arbitrary SQLite
  pass-through.

## Grammar

MyLite adds this parser production:

```lemon
expression(A) ::= MICROSECOND(T) LPAREN expression(V) RPAREN(R).
```

The function name remains usable as an unquoted identifier where MyLite admits
ordinary MySQL nonreserved keywords:

```lemon
identifier(A) ::= MICROSECOND(T).
```

Analyzer/runtime acceptance is narrower than parser acceptance:

```lemon
microsecond_supported_value ::= descriptor_temporal_column.
microsecond_supported_value ::= descriptor_string_column.
microsecond_supported_value ::= string_literal.
microsecond_supported_value ::= NULL.
```

## Runtime And Ownership Boundaries

- Public API: no public ABI changes. `mylite_execute()` and result accessors use
  existing scalar/no-row statement conventions.
- Statement context: no new statement context fields. Diagnostics and warnings
  remain handle-owned.
- Lexer/parser/AST: parse `MICROSECOND(expr)` into a dedicated AST node and
  keep `MICROSECOND` available as an identifier token.
- Analyzer/planner: resolve row-backed descriptor columns through MyLite
  descriptors, not SQLite metadata. Unsupported argument shapes produce
  deterministic MyLite diagnostics.
- Catalog: no catalog mutation, descriptor version change, cache invalidation,
  or generated metadata change is needed.
- Runtime: evaluate no-source, `DUAL`, and `DO` scalar calls directly through
  MyLite's temporal extraction helper. Row-backed projection lowers to the
  existing `_mylite_temporal_extract(value, kind, input_kind, mode)` registered
  SQLite scalar function.
- Storage/VFS: no file-format, VFS, preamble, or SQLite payload changes.
- SQLite: public scalar-function registration is sufficient. There is no need
  for a targeted SQLite fork hook in this phase.

## Diagnostics And Warnings

- Syntax errors: unsupported arities such as `MICROSECOND()` and
  `MICROSECOND(value, extra)` remain syntax errors with MySQL-shaped parser
  diagnostics.
- Unsupported scalar arguments: numeric, boolean, hex/bit, decimal/float,
  identifiers without a source table, variables, parameters, subqueries, and
  expression arguments are rejected with the existing temporal extractor
  unsupported-argument diagnostic.
- Unknown row columns use the existing unknown-column diagnostic.
- Unsupported row column types use the existing temporal extractor unsupported
  descriptor diagnostic.
- String literals containing embedded `NUL` bytes are rejected through the
  existing temporal extractor literal diagnostic.
- Invalid non-`NULL` string inputs return `NULL` and warning `1292` /
  SQLSTATE `22007`, with text beginning `Truncated incorrect time value:`.
- Date-only string inputs return `0` and the same warning.
- Allocation failures propagate through the existing `HY001` out-of-memory
  diagnostic path.

## Tests

The feature is covered by:

- MySQL expectation script:
  `packages/libmylite/tests/mysql_baseline_microsecond_function_expectations.sh`
- Parser coverage in `packages/libmylite/tests/parser_test.c`
- Runtime coverage in
  `packages/libmylite/tests/runtime_temporal_extract_functions_test.c`

Required cases:

- no-source, `DUAL`, and `DO` supported calls;
- fractional time and datetime strings with padding and rounding;
- no-fraction time/datetime strings returning `0`;
- `NULL` propagation;
- date-only strings returning `0` with warning `1292`;
- invalid datetime time parts returning `NULL` with warning `1292`;
- invalid strings returning `NULL` with warning `1292`;
- descriptor `DATE`, `TIME`, `DATETIME`, `TIMESTAMP`, `VARCHAR`, `CHAR`, and
  `TEXT` row-backed projection;
- reopen persistence of data observed through `MICROSECOND()`;
- unsupported scalar argument shapes;
- unsupported descriptor column types;
- continued deferral of `EXTRACT(MICROSECOND FROM value)`.
