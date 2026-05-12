# Baseline FLOAT and DOUBLE Types

## Status

This feature specifies the first approximate numeric type slice for persistent
`.mylite` handles. It adds descriptor-owned `FLOAT` and `DOUBLE` columns, plus
the common MySQL aliases that normalize to those two physical classes.

The slice is intentionally not full floating-point expression support. It
stores approximate numeric row values using SQLite `REAL`, while MyLite owns
the MySQL-facing descriptors, DDL diagnostics, DML conversion, default
materialization, nullability, visible text formatting, and metadata. It does
not add table-backed floating-point predicates, ordering, indexes, aggregates,
casts, math, protocol-grade metadata, non-strict clipping, `ZEROFILL`, or
deprecated `FLOAT(M,D)` / `DOUBLE(M,D)` display-scale behavior.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- SQLite connection bootstrap policy:
  `docs/specs/sqlite-connection-bootstrap-policy/specs.md`
- File-backed MyLite opening VFS:
  `docs/specs/file-backed-mylite-opening-vfs/specs.md`
- MyLite file-format preamble:
  `docs/specs/mylite-file-format/specs.md`
- Baseline catalog foundation and existing DDL/DML/type specs under
  `docs/specs/`
- MySQL 8.4 Reference Manual, floating-point types:
  https://dev.mysql.com/doc/refman/8.4/en/floating-point-types.html
- MySQL 8.4 Reference Manual, numeric type syntax:
  https://dev.mysql.com/doc/refman/8.4/en/numeric-type-syntax.html
- MySQL 8.4 Reference Manual, numeric type attributes:
  https://dev.mysql.com/doc/refman/8.4/en/numeric-type-attributes.html
- MySQL 8.4 Reference Manual, out-of-range handling:
  https://dev.mysql.com/doc/refman/8.4/en/out-of-range-and-overflow.html
- MySQL 8.4 Reference Manual, numeric literal categories:
  https://dev.mysql.com/doc/refman/8.4/en/precision-math-numbers.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.COLUMNS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-columns-table.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_float_double_type_expectations.sh`
records the runtime probes for this feature. Observed behavior that shapes this
slice:

- Bare `FLOAT`, `FLOAT(0)`, `FLOAT(23)`, and `FLOAT(24)` render as `float`.
  `FLOAT(25)` through `FLOAT(53)` render as `double`. `FLOAT4` renders as
  `float`, and `FLOAT8` renders as `double`.
- Bare `DOUBLE`, `DOUBLE PRECISION`, and default-mode `REAL` render as
  `double`. `REAL_AS_FLOAT` changes `REAL`, but MyLite currently keeps
  `@@sql_mode` fixed and does not support mode-dependent parsing, so this
  baseline follows the default MySQL 8.4.9 mode.
- `UNSIGNED` on `FLOAT` and `DOUBLE` is accepted, disallows negative stored
  values, emits warning `1681`, and renders `float unsigned` or
  `double unsigned`. `SIGNED` has no visible effect in MySQL, but MyLite
  defers it to keep the parser attribute surface narrow.
- `ZEROFILL` is accepted by MySQL with warning `1681`, implies `UNSIGNED`, and
  affects visible zero padding. MyLite defers it for this slice.
- Deprecated `FLOAT(M,D)`, `DOUBLE(M,D)`, `REAL(M,D)`, and
  `DOUBLE PRECISION(M,D)` are accepted by MySQL with warning `1681`, retain
  `M,D` metadata, round visible values to `D` fractional digits, and constrain
  the stored range implied by `M,D`. MyLite defers those forms.
- `SHOW COLUMNS` renders default `FLOAT` as `float` and default `DOUBLE` as
  `double`. `SHOW CREATE TABLE` quotes non-`NULL` approximate defaults, for
  example `DEFAULT '1.5'`.
- `INFORMATION_SCHEMA.COLUMNS` reports bare `FLOAT` with `DATA_TYPE = float`,
  `COLUMN_TYPE = float`, `NUMERIC_PRECISION = 12`, and `NUMERIC_SCALE = NULL`.
  Bare `DOUBLE` reports `DATA_TYPE = double`, `COLUMN_TYPE = double`,
  `NUMERIC_PRECISION = 22`, and `NUMERIC_SCALE = NULL`.
- Row readback uses a compact decimal or scientific text form. `FLOAT`
  readback reflects single-precision storage, for example
  `3.1415926535` stores visibly as `3.14159`. `DOUBLE` readback keeps the
  observed double-precision text, for example `3.1415926535`.
- Positive and negative zero read back as `0`.
- In default strict mode, a finite value outside the admitted type range fails
  with error `1264 / 22003`. `NULL` into `NOT NULL` fails with
  `1048 / 23000`. Tiny underflow to zero is accepted without warnings in the
  probed cases.
- MySQL accepts broader conversion sources, including numeric strings and
  deprecated scaled forms. MyLite starts with numeric literals, booleans,
  `NULL`, and `DEFAULT`; broader string-to-float conversion waits for a
  general conversion layer.

## Scope

The implementation must add:

- parser and AST support for approximate column types:
  - `FLOAT`;
  - `FLOAT(p)` where `p` is a nonnegative integer literal in `0..53`;
  - `FLOAT4`;
  - `FLOAT8`;
  - `DOUBLE`;
  - `DOUBLE PRECISION`;
  - `REAL` in the fixed default SQL mode;
  - optional single trailing `UNSIGNED`;
- descriptor-owned logical type text:
  - `FLOAT` / `FLOAT UNSIGNED`;
  - `DOUBLE` / `DOUBLE UNSIGNED`;
- physical SQLite type text `REAL`;
- durable catalog support using the existing text-default storage shape for
  canonical approximate defaults, without a catalog schema migration;
- `CREATE TABLE` support for persistent base tables containing approximate
  columns, including nullable and not-null columns plus explicit
  `DEFAULT NULL` and non-`NULL` approximate numeric defaults;
- `ALTER TABLE ... ADD [COLUMN]` support for approximate columns, with existing
  row backfill using descriptor defaults, nullable `NULL`, or implicit `0` for
  `NOT NULL` no-explicit-default additions;
- `ALTER TABLE ... ALTER [COLUMN] ... SET DEFAULT` and `DROP DEFAULT` support
  for approximate descriptors;
- `CREATE TABLE ... LIKE` descriptor cloning for approximate columns and their
  defaults;
- descriptor-backed `CREATE TABLE ... SELECT`, `INSERT ... SELECT`, and
  `REPLACE ... SELECT` copying when source values are already compatible with
  admitted approximate target descriptors;
- `SHOW COLUMNS`, `DESCRIBE`, `EXPLAIN table`, `SHOW CREATE TABLE`, and
  limited `INFORMATION_SCHEMA.COLUMNS` rendering for approximate descriptors;
- integer literal, fixed decimal literal, approximate literal, `TRUE`,
  `FALSE`, `NULL`, and `DEFAULT` values for `INSERT ... VALUES`,
  `INSERT ... SET`, `REPLACE ... VALUES`, `REPLACE ... SET`, and single-table
  `UPDATE` assignments into approximate columns;
- MyLite-owned conversion before SQLite binding: sign handling, decimal /
  exponent parsing, finite range checking, `FLOAT` single-precision rounding,
  `DOUBLE` double-precision storage, `UNSIGNED` lower-bound checking,
  zero-normalized visible text, and deterministic strict-mode diagnostics;
- limited `INSERT IGNORE ... VALUES` / `SET` handling for approximate
  descriptors where existing row-value infrastructure requires it: `NULL` into
  `NOT NULL` stores `0` with warning `1048`, and missing no-default
  not-null values store `0` with warning `1364`;
- descriptor-backed `SELECT` readback of approximate values as public result
  text;
- descriptor-backed `WHERE column IS NULL` and
  `WHERE column IS NOT NULL` on approximate columns;
- persistent storage, reopen behavior, table rename/drop behavior, `.mylite`
  preamble preservation, and independent file-backed handle behavior for
  admitted approximate data;
- MySQL 8.4.9 expectation coverage for supported behavior and deliberately
  deferred wider MySQL behavior.

## Non-Goals

This feature must not implement:

- `FLOAT(M,D)`, `DOUBLE(M,D)`, `REAL(M,D)`, or
  `DOUBLE PRECISION(M,D)`;
- `ZEROFILL`, repeated attributes, `SIGNED`, or mode-dependent `REAL_AS_FLOAT`
  parsing;
- string-to-float conversion, hex/bit inputs, parameters, user variables,
  functions, arbitrary expression assignments, column-to-column assignments,
  or `DEFAULT(col_name)`;
- approximate comparison predicates except `IS NULL` / `IS NOT NULL`;
- approximate `BETWEEN`, approximate `IN`, truth tests, `ORDER BY`,
  `DISTINCT`, grouping, aggregates, arithmetic, casts, or expression metadata;
- approximate primary keys, unique indexes, secondary indexes,
  `AUTO_INCREMENT`, foreign keys, generated columns, check constraints, or
  optimizer use of approximate values;
- non-strict clipping of out-of-range approximate inputs beyond the existing
  narrow `INSERT IGNORE` null/default behavior;
- protocol-grade type metadata, field flags, binary protocol values, or origin
  metadata;
- SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns call validation,
  result ownership, public misuse behavior, and cleanup on failure.
- Statement context owns per-statement diagnostics, warnings, affected rows,
  and transaction completion. Approximate DDL may add warning `1681` for
  admitted `UNSIGNED`.
- Lexer/parser/AST own syntax admission for approximate type names, optional
  precision spans, optional `UNSIGNED`, approximate numeric literals in
  DML/default contexts, and structural source spans only. They do not resolve
  descriptors or convert values.
- Analyzer/planner code maps approximate AST nodes to durable descriptors,
  resolves schemas/tables/columns through MyLite catalog descriptors, converts
  admitted approximate values, rejects unsupported approximate operations, and
  produces descriptor-driven SQLite plans.
- The catalog remains authoritative for logical type, physical type,
  nullability, visibility, default kind, default text, column order,
  primary-key membership, and auto-increment attributes. SQLite schema text is
  not metadata authority.
- Result and introspection builders render logical descriptors and descriptor
  defaults to MySQL-shaped text.
- SQLite owns physical `REAL` row storage and mutation for generated prepared
  statements. MyLite binds converted finite values and formats readback rather
  than exposing SQLite's default numeric text as the compatibility contract.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  This feature writes only inside the shifted SQLite payload and must not touch
  byte range `[0, 4096)`.

## Supported SQL Grammar

This feature extends the existing limited column definition grammar:

```sql
column_type:
    existing_type
  | approximate_type

approximate_type:
    FLOAT [ ( precision ) ] [ UNSIGNED ]
  | FLOAT4 [ UNSIGNED ]
  | FLOAT8 [ UNSIGNED ]
  | DOUBLE [ PRECISION ] [ UNSIGNED ]
  | REAL [ UNSIGNED ]
```

MyLite Lemon-syntax sketch:

```lemon
column_type(A) ::= approximate_type(T).

approximate_type(A) ::= approximate_type_name(N) float_precision_opt(P)
    approximate_unsigned_opt(U).
approximate_type(A) ::= DOUBLE(D) PRECISION(P) approximate_unsigned_opt(U).

approximate_type_name(A) ::= FLOAT_TYPE(T).
approximate_type_name(A) ::= FLOAT4(T).
approximate_type_name(A) ::= FLOAT8(T).
approximate_type_name(A) ::= DOUBLE(T).
approximate_type_name(A) ::= REAL(T).

float_precision_opt(A) ::= .
float_precision_opt(A) ::= LPAREN INTEGER(P) RPAREN(R).

approximate_unsigned_opt(A) ::= .
approximate_unsigned_opt(A) ::= UNSIGNED(U).
```

`DOUBLE PRECISION(p)` and all `M,D` forms are intentionally omitted until a
feature owns their deprecated metadata and scaled value behavior.

## Descriptor Mapping

`FLOAT`, `FLOAT4`, `FLOAT(0..24)`, and `FLOAT ... UNSIGNED` map to logical
types `FLOAT` or `FLOAT UNSIGNED`, physical type `REAL`, and MySQL metadata
`DATA_TYPE = float`, `NUMERIC_PRECISION = 12`, `NUMERIC_SCALE = NULL`.

`DOUBLE`, `DOUBLE PRECISION`, `REAL`, `FLOAT8`, `FLOAT(25..53)`, and the
matching `UNSIGNED` forms map to logical types `DOUBLE` or `DOUBLE UNSIGNED`,
physical type `REAL`, and MySQL metadata `DATA_TYPE = double`,
`NUMERIC_PRECISION = 22`, `NUMERIC_SCALE = NULL`.

`FLOAT(p)` accepts `p` from `0` through `53`. Values through `24` map to
`FLOAT`; values from `25` through `53` map to `DOUBLE`. `p > 53` fails with
MySQL error `1063 / 42000` for this baseline. Negative precision is a syntax
error because `-` is not an admitted precision literal.

`UNSIGNED` emits warning `1681 / HY000` with the same deprecation text already
used for decimal/floating-point unsigned attributes.

## DML Conversion

The admitted value inputs are:

- decimal integer literals with optional unary sign;
- fixed decimal literals with optional unary sign;
- approximate numeric literals with optional unary sign;
- `TRUE` and `FALSE`, converted to `1` and `0`;
- `NULL`;
- `DEFAULT`.

`FLOAT` conversion parses the numeric literal to a finite host `double`,
checks the MySQL single-precision finite range, casts to `float`, stores the
result as a finite SQLite `REAL`, and formats readback from the rounded
single-precision value. Values smaller than the minimum positive finite
single-precision range may underflow to zero without warning for this slice.

`DOUBLE` conversion parses the numeric literal to a finite host `double`,
checks the MySQL double finite range, stores it as SQLite `REAL`, and formats
readback from the stored double. Values smaller than the minimum positive
double range may underflow to zero without warning for this slice.

Negative zero is stored and read back as `0`. `UNSIGNED` rejects negative
finite values, including negative values that would otherwise round to a
representable nonzero value, with error `1264 / 22003`.

Out-of-range finite values fail with `1264 / 22003` in strict mode. Non-finite
inputs such as parsed infinities fail deterministically with a MyLite-owned
diagnostic unless the parser rejects them earlier.

`NULL` into nullable columns stores `NULL`. `NULL` into not-null columns fails
with `1048 / 23000`, except the existing limited `INSERT IGNORE` paths demote
that to warning `1048` and store `0`.

## Readback

`SELECT` returns approximate values as text through the existing public result
API. MyLite formats finite values using a compact form that round-trips through
the stored binary value. The baseline accepts MySQL-compatible observed text
for the covered values and does not claim byte-for-byte parity for every
platform-dependent floating-point formatting edge case.

## Metadata

`SHOW COLUMNS` / `DESCRIBE` / `EXPLAIN table`:

- `Type`: `float`, `float unsigned`, `double`, or `double unsigned`;
- `Null`, `Key`, `Default`, and `Extra` use existing descriptor rules.

`SHOW CREATE TABLE` renders lowercase type text and quotes non-`NULL`
approximate defaults, for example:

```sql
`f` float DEFAULT '1.5'
`d` double NOT NULL DEFAULT '-2.25'
```

`INFORMATION_SCHEMA.COLUMNS`:

- `DATA_TYPE`: `float` or `double`;
- `COLUMN_TYPE`: lowercase type text including `unsigned` if present;
- `NUMERIC_PRECISION`: `12` for `FLOAT`, `22` for `DOUBLE`;
- `NUMERIC_SCALE`: `NULL`;
- character and datetime precision fields remain `NULL`;
- `COLUMN_DEFAULT` is the visible canonical default text or `NULL`.

## Diagnostics

The supported subset must produce deterministic diagnostics for:

- syntax errors and unsupported grammar;
- missing default schema, unknown schema, unknown table, and reserved names
  through existing descriptor resolution;
- duplicate columns and unsupported object kinds through existing DDL paths;
- `FLOAT(p)` precision out of range: `1063 / 42000`;
- `UNSIGNED` deprecation warning: `1681 / HY000`;
- unsupported `SIGNED`, `ZEROFILL`, `M,D` forms, approximate indexes,
  approximate primary keys, approximate auto-increment, and approximate
  table-backed comparison/order/group/aggregate use;
- unsupported assignment/default literals or expression values;
- out-of-range assignment/default literals: `1264 / 22003`;
- `NULL` into `NOT NULL`: `1048 / 23000`;
- physical SQLite failures, allocation failures, and public API misuse through
  existing MyLite diagnostics.

## Physical SQLite Handling

Generated user tables use SQLite `REAL` columns for approximate descriptors.
All generated SQL continues to use stable physical table names such as
`_mylite_user_table_<table_id>`, quotes every generated identifier, and binds
values through prepared statements. User SQL is never passed through to SQLite
for these DDL/DML paths.

No SQLite fork patch is required. The implementation uses MyLite-owned
conversion/formatting plus public SQLite prepared-statement binding and column
read APIs.

## Performance Notes

Approximate row storage follows the same descriptor-driven DML path as existing
integer, decimal, temporal, and string row values. Inserts and updates bind
converted values directly into SQLite statements; selects stream SQLite rows
into MyLite result objects without pre-materializing full tables for
conversion. Validation is per-value and does not add table scans except for
pre-existing DDL operations that already scan rows.

## Tests

Add a fast C runtime test, preferably
`libmylite.runtime.float_double_type`, covering:

- parser acceptance of supported approximate type grammar and rejection of
  deferred forms;
- `CREATE TABLE` with `FLOAT`, `FLOAT(0)`, `FLOAT(24)`, `FLOAT(25)`,
  `FLOAT(53)`, `FLOAT4`, `FLOAT8`, `DOUBLE`, `DOUBLE PRECISION`, `REAL`, and
  `UNSIGNED` forms;
- `SHOW COLUMNS`, `SHOW CREATE TABLE`, and `INFORMATION_SCHEMA.COLUMNS`
  metadata;
- nullable and not-null descriptors, `DEFAULT NULL`, and non-`NULL` defaults;
- `INSERT`, `REPLACE`, and one-assignment `UPDATE` values using integer,
  fixed decimal, approximate, boolean, `NULL`, and `DEFAULT` inputs;
- `FLOAT` single-precision readback and `DOUBLE` readback for representative
  values;
- strict out-of-range diagnostics and unsigned negative diagnostics;
- `IS NULL` / `IS NOT NULL` predicates;
- reopen persistence, table rename/drop behavior, `.mylite` preamble
  preservation, independent file-backed handles, and zero-initialized cleanup
  for new objects;
- deterministic unsupported diagnostics for `FLOAT(M,D)`, `DOUBLE(M,D)`,
  `SIGNED`, `ZEROFILL`, approximate indexes/primary keys, auto-increment,
  table-backed comparisons/order/grouping/aggregates if not admitted, string
  assignment/default conversion, hex/bit literals, parameters, functions, and
  arbitrary expressions.

The MySQL expectation script must run against MySQL 8.4.9 and cover every
user-visible behavior this slice introduces.

## Compatibility Documentation

Update `COMPATIBILITY.md` and
`docs/compatibility/type-system-literals-conversion.md` to mark `FLOAT`,
`DOUBLE` / `REAL`, and `FLOAT4` / `FLOAT8` as limited. Update DDL/DML docs only
for the exact supported row-value/default surface. Do not overclaim full
floating-point expressions, casts, functions, indexes, scaled display forms,
non-strict mode, binary protocol metadata, or SQLite pass-through.
