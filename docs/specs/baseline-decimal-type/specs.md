# Baseline DECIMAL Type

## Status

This feature specifies the first exact fixed-point numeric slice for persistent
`.mylite` handles. It adds descriptor-owned `DECIMAL`, `NUMERIC`, `DEC`, and
`FIXED` columns on top of the existing base-table, DML, default, result, and
introspection paths.

The feature is intentionally not full MySQL precision math. It stores and
returns exact fixed-point row values in MySQL's visible decimal text shape, but
it does not implement table-backed decimal arithmetic, decimal comparisons,
decimal ordering, decimal primary keys, `ZEROFILL`, casts, string-to-decimal
conversion, scientific-notation approximate inputs, or protocol-grade numeric
metadata.

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
- Baseline catalog foundation:
  `docs/specs/baseline-catalog-foundation/specs.md`
- Baseline row values, defaults, DML, primary-key, auto-increment, and
  `INFORMATION_SCHEMA` specs under `docs/specs/`
- MySQL lexer:
  `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, numeric type syntax:
  https://dev.mysql.com/doc/refman/8.4/en/numeric-type-syntax.html
- MySQL 8.4 Reference Manual, fixed-point `DECIMAL` / `NUMERIC` types:
  https://dev.mysql.com/doc/refman/8.4/en/fixed-point-types.html
- MySQL 8.4 Reference Manual, DECIMAL characteristics:
  https://dev.mysql.com/doc/refman/8.4/en/precision-math-decimal-characteristics.html
- MySQL 8.4 Reference Manual, out-of-range handling:
  https://dev.mysql.com/doc/refman/8.4/en/out-of-range-and-overflow.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.COLUMNS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-columns-table.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_decimal_type_expectations.sh` records
the runtime probes for this feature. Observed behavior that shapes this slice:

- MySQL treats `DECIMAL`, `NUMERIC`, `DEC`, and `FIXED` as fixed-point
  synonyms and renders all of them as `decimal(...)` in `SHOW` and
  `INFORMATION_SCHEMA` metadata.
- Bare `DECIMAL` is `decimal(10,0)`. `DECIMAL(M)` is `decimal(M,0)`.
  `DECIMAL(M,D)` uses precision `M` and scale `D`. MySQL accepts
  `DECIMAL(0)` and `DECIMAL(0,0)` as `decimal(10,0)`.
- Runtime-verified declaration bounds are: precision `M` through `65`, scale
  `D` through `30`, and `D <= M` after the `0,0` normalization. MySQL rejects
  too-large precision with error `1426`, too-large scale with error `1425`,
  and scale greater than precision with error `1427`. Negative precision is a
  syntax error `1064`.
- MySQL accepts `UNSIGNED` on decimal and floating-point types, emits warning
  `1681`, and renders `decimal(M,D) unsigned`. It accepts `ZEROFILL`, emits
  warning `1681`, implies `UNSIGNED`, and zero-pads visible values. MyLite
  admits only the `UNSIGNED` subset and defers `ZEROFILL`.
- `SHOW COLUMNS` reports decimal defaults as visible decimal text. `SHOW CREATE
  TABLE` quotes non-`NULL` decimal defaults, for example
  `DEFAULT '0.00'`. `INFORMATION_SCHEMA.COLUMNS` reports `DATA_TYPE` as
  `decimal`, `COLUMN_TYPE` as the rendered decimal type including `unsigned`
  when present, `NUMERIC_PRECISION` as `M`, and `NUMERIC_SCALE` as `D`.
- Row values read back with exactly `D` fractional digits when `D > 0`, with no
  decimal point when `D == 0`. Leading `+`, leading zeroes, and negative zero
  are not visible after storage.
- In strict default SQL mode, in-range values with more fractional digits than
  the target scale are rounded half away from zero. If any discarded fractional
  digit is nonzero, MySQL records note `1265`; discarded zeroes do not record a
  warning. If rounding pushes the value outside the target range, MySQL reports
  out-of-range error `1264`.
- In strict default SQL mode, out-of-range decimal inputs fail with error
  `1264`; `NULL` into `NOT NULL` fails with error `1048`.
- `INSERT IGNORE ... VALUES` / `SET` demotes out-of-range decimal inputs to
  warning `1264` and clips to the nearest endpoint. It demotes explicit `NULL`
  into decimal `NOT NULL` to warning `1048` and stores the implicit decimal
  zero for the target scale.
- Single-table `UPDATE` uses changed-row affected counts after decimal
  canonicalization. Reassigning `1.200` to `DECIMAL(5,2)` storing `1.20`
  reports zero changed rows.
- MySQL accepts wider conversion inputs, including strings, exponent notation,
  hex, bit, and expression values. MyLite defers those until the expression and
  conversion layers are broadened.

## Scope

The implementation must add:

- parser and AST support for `DECIMAL`, `DEC`, `NUMERIC`, and `FIXED` column
  types in bare, one-argument, and two-argument forms;
- optional single `UNSIGNED` on admitted decimal type names, with MySQL warning
  `1681`;
- descriptor-owned logical type text `DECIMAL(M,D)` or
  `DECIMAL(M,D) UNSIGNED`;
- physical SQLite type text `TEXT` for admitted decimal descriptors;
- durable catalog support for decimal default text values while preserving
  existing integer default behavior and descriptor generation rules;
- `CREATE TABLE` support for persistent base tables containing decimal
  columns, including nullable and not-null columns plus explicit `DEFAULT NULL`
  and non-`NULL` decimal defaults;
- `ALTER TABLE ... ADD [COLUMN]` support for decimal columns, including
  existing-row backfill with the descriptor default, nullable `NULL`, or the
  MySQL implicit decimal zero for `NOT NULL` no-explicit-default additions;
- `ALTER TABLE ... ALTER [COLUMN] ... SET DEFAULT` and `DROP DEFAULT` support
  for decimal descriptors;
- `CREATE TABLE ... LIKE` descriptor cloning for decimal columns and their
  defaults;
- descriptor-backed `CREATE TABLE ... SELECT`, `INSERT ... SELECT`, and
  `REPLACE ... SELECT` copying when source values are already compatible with
  admitted decimal target descriptors;
- `SHOW COLUMNS`, `DESCRIBE`, `EXPLAIN table`, `SHOW CREATE TABLE`, and
  limited `INFORMATION_SCHEMA.COLUMNS` rendering for decimal descriptors;
- decimal literal, integer literal, `TRUE`, `FALSE`, `NULL`, and `DEFAULT`
  values for `INSERT ... VALUES`, `INSERT ... SET`, `REPLACE ... VALUES`,
  `REPLACE ... SET`, and single-table `UPDATE` assignments into decimal
  columns;
- MyLite-owned exact decimal conversion before SQLite binding: sign handling,
  fixed decimal parsing, scale padding, half-away-from-zero rounding, note
  `1265` for nonzero discarded fractional digits, range checking, unsigned
  lower-bound checking, endpoint clipping for supported `INSERT IGNORE`
  adjustment, negative-zero normalization, and canonical visible text output;
- descriptor-backed `SELECT` readback of decimal values as public result text;
- descriptor-backed `WHERE column IS NULL` and
  `WHERE column IS NOT NULL` on decimal columns;
- deterministic rejection of decimal comparisons, `BETWEEN`, `IN`, truth
  predicates, ordering, `DISTINCT`, grouped columns, numeric aggregates,
  primary keys, auto-increment, and `ALTER TABLE ... MODIFY` / `CHANGE` type
  replacement involving decimal descriptors;
- persistent storage, reopen behavior, table rename/drop behavior, `.mylite`
  preamble preservation, and independent file-backed handle behavior for
  admitted decimal data;
- MySQL 8.4.9 expectation coverage for supported behavior and deliberately
  deferred wider MySQL behavior.

## Non-Goals

This feature must not implement:

- `FLOAT`, `DOUBLE`, `REAL`, `BIT`, `SERIAL`, temporal, binary, blob, enum,
  set, json, or spatial types;
- `ZEROFILL`, repeated attributes, `SIGNED` after decimal types, or decimal
  display-width concepts outside precision/scale;
- string-to-decimal conversion, exponent notation, approximate-value literals,
  hex/bit inputs, parameters, user variables, functions, arbitrary expression
  assignments, column-to-column assignments, or `DEFAULT(col_name)`;
- decimal comparison predicates, decimal `BETWEEN`, decimal `IN`, decimal
  truth tests, decimal `ORDER BY`, decimal `DISTINCT`, decimal grouping,
  decimal aggregates, decimal arithmetic, casts, or full precision math;
- decimal primary keys, unique indexes, secondary indexes, auto-increment, or
  optimizer use of decimal values;
- `ALTER TABLE ... MODIFY [COLUMN]` or `CHANGE [COLUMN]` to or from decimal;
- protocol-grade type metadata, field flags, binary protocol values, or origin
  metadata;
- SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns call validation,
  result ownership, public misuse behavior, and cleanup on failure.
- Statement context owns per-statement diagnostics, warnings, affected rows,
  and transaction completion. Decimal scale rounding may add MySQL note `1265`;
  supported `INSERT IGNORE` decimal adjustments record warnings through the
  existing diagnostics area.
- Lexer/parser/AST own syntax admission for decimal type names, precision and
  scale spans, optional `UNSIGNED`, decimal literal DML/default values, and
  structural source spans only. They do not resolve descriptors or convert
  values.
- Analyzer/planner code maps decimal AST nodes to durable descriptors,
  resolves schemas/tables/columns through MyLite catalog descriptors, converts
  admitted decimal values, rejects unsupported decimal operations, and produces
  descriptor-driven SQLite plans.
- The catalog remains authoritative for logical type, physical type,
  nullability, visibility, default kind, default text, column order, primary-key
  membership, and auto-increment attributes. SQLite schema text is not metadata
  authority.
- Result and introspection builders render logical descriptors and descriptor
  defaults to MySQL-shaped text.
- SQLite owns physical row storage and row mutation for generated prepared
  statements. Decimal values bind as canonical `TEXT`; MyLite never stores them
  through SQLite `REAL`.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  This feature writes only inside the shifted SQLite payload and must not touch
  byte range `[0, 4096)`.

## Supported SQL Grammar

This feature extends the existing limited column definition grammar:

```sql
column_type:
    existing_integer_type
  | existing_string_type
  | decimal_type

decimal_type:
    decimal_type_name [ ( precision ) | ( precision , scale ) ] [ UNSIGNED ]

decimal_type_name:
    DECIMAL
  | DEC
  | NUMERIC
  | FIXED
```

`precision` and `scale` are unsigned decimal integer literals. MyLite admits
the MySQL-runtime-verified ranges described above and normalizes every admitted
synonym to `DECIMAL(M,D)` logical descriptor text.

### MyLite Lemon-Syntax Snippet

This snippet describes MyLite's intended grammar extension, not MySQL's full
grammar:

```lemon
column_type ::= decimal_type.

decimal_type ::= decimal_type_name.
decimal_type ::= decimal_type_name LPAREN INTEGER RPAREN.
decimal_type ::= decimal_type_name LPAREN INTEGER COMMA INTEGER RPAREN.
decimal_type ::= decimal_type_name UNSIGNED.
decimal_type ::= decimal_type_name LPAREN INTEGER RPAREN UNSIGNED.
decimal_type ::= decimal_type_name LPAREN INTEGER COMMA INTEGER RPAREN UNSIGNED.

decimal_type_name ::= DECIMAL_TYPE.
decimal_type_name ::= DEC.
decimal_type_name ::= NUMERIC.
decimal_type_name ::= FIXED.

insert_value ::= DECIMAL.
insert_value ::= PLUS DECIMAL.
insert_value ::= MINUS DECIMAL.

update_value ::= DECIMAL.
update_value ::= PLUS DECIMAL.
update_value ::= MINUS DECIMAL.

column_default_value ::= DECIMAL.
column_default_value ::= PLUS DECIMAL.
column_default_value ::= MINUS DECIMAL.
```

`DECIMAL_TYPE` is the parser token for the `DECIMAL` keyword. `DECIMAL`
without `_TYPE` remains the existing numeric-literal token.

## Descriptor and Storage Mapping

Admitted decimal descriptors use:

- logical type: `DECIMAL(M,D)` or `DECIMAL(M,D) UNSIGNED`;
- physical type: `TEXT`;
- default kind:
  - `NONE` for explicit or effective nullable `DEFAULT NULL`;
  - `NO_EXPLICIT` when `ALTER ... DROP DEFAULT` removes an implicit nullable
    default;
  - `INTEGER` for existing integer-family descriptors only;
  - `DECIMAL` for decimal default text values;
- default text: canonical decimal text with target scale, present only when
  default kind is `DECIMAL`.

The catalog schema is bumped for default text storage. Existing catalog rows
from earlier versions migrate with `default_text = NULL` and preserve integer
defaults exactly.

Physical SQLite row values are canonical decimal text. This keeps exactness and
visible readback independent of SQLite numeric affinity, and it avoids a SQLite
fork. Generated SQL continues to quote identifiers and bind values with
prepared statements.

## Decimal Conversion

The conversion layer accepts only these value forms for decimal targets:

- integer literals with optional unary `+` or `-`;
- fixed decimal literals with optional unary `+` or `-`;
- `TRUE` and `FALSE`, stored as `1` or `0` with target scale;
- `NULL`;
- descriptor-resolved `DEFAULT`.

For a decimal descriptor `DECIMAL(M,D)`:

- `integer_digits = M - D`;
- signed positive endpoint is `integer_digits` nines plus `D` fractional nines;
- signed negative endpoint is the negative of the positive endpoint;
- unsigned lower endpoint is zero.

Conversion steps:

1. Decode sign and decimal digits without using binary floating point.
2. Strip leading zeroes from the integer part for range comparison.
3. Pad missing fractional digits with zeroes.
4. If fractional digits beyond `D` exist, round half away from zero. Record note
   `1265` only when at least one discarded digit is nonzero.
5. Normalize negative zero to positive zero.
6. Check range after rounding. Strict DML reports error `1264`; supported
   `INSERT IGNORE` clips to the nearest endpoint and records warning `1264`.
7. Format the canonical text with exactly `D` fractional digits when `D > 0`.

`NULL` into decimal `NOT NULL` reports error `1048`; supported
`INSERT IGNORE` stores the target-scale zero and records warning `1048`.

Omitted or explicit `DEFAULT` uses descriptor defaults. If no explicit default
exists:

- nullable decimal columns materialize `NULL`;
- strict DML into `NOT NULL` decimal columns reports error `1364`;
- supported `INSERT IGNORE` stores target-scale zero and records warning
  `1364`.

## Query and DML Semantics

Successful decimal `INSERT`, `REPLACE`, and `UPDATE` mutate only physical row
values. They do not mutate catalog rows, descriptor versions, catalog
generation, or SQLite schema generation unless the statement is DDL.

Successful decimal `UPDATE` uses MySQL changed-row affected counts after
canonicalization. For example, updating a stored `1.20` to `1.200` affects zero
rows; updating it to `1.235` stores `1.24`, records note `1265`, and affects
one row.

`SELECT` returns decimal values as text result cells. `WHERE column IS NULL`
and `IS NOT NULL` are admitted for decimal columns. All other decimal
predicates and ordering are rejected until MyLite owns decimal comparison and
ordering semantics.

## Introspection

For decimal columns:

- `SHOW COLUMNS`, `DESCRIBE`, and `EXPLAIN table` render `decimal(M,D)` or
  `decimal(M,D) unsigned`;
- `SHOW CREATE TABLE` renders the same type text and quotes non-`NULL` decimal
  defaults;
- `INFORMATION_SCHEMA.COLUMNS.DATA_TYPE` is `decimal`;
- `COLUMN_TYPE` is `decimal(M,D)` or `decimal(M,D) unsigned`;
- `NUMERIC_PRECISION` is `M`;
- `NUMERIC_SCALE` is `D`;
- string length, octet length, character set, collation, and datetime precision
  are `NULL`;
- `COLUMN_DEFAULT` is `NULL` or the canonical default text.

## Diagnostics

The implementation must provide deterministic diagnostics for:

- syntax errors and unsupported grammar;
- missing default schema, unknown schema, unknown table, reserved
  `_mylite_*` names, and unsupported object kinds through existing resolver
  behavior;
- duplicate columns and duplicate table names through existing DDL behavior;
- unsupported decimal type attributes, including `ZEROFILL`, repeated
  `UNSIGNED`, `SIGNED`, and expression precision/scale;
- precision above `65`, scale above `30`, and scale greater than precision;
- unsupported decimal values: strings, floats/exponents, hex, bit, parameters,
  functions, and arbitrary expressions;
- decimal out-of-range values;
- nonzero discarded fractional digits, as note `1265`;
- `NULL` into `NOT NULL`;
- missing defaults for `NOT NULL` columns;
- unsupported decimal predicates, ordering, distinct, grouping, aggregates,
  primary keys, auto-increment, and alter type replacement;
- physical SQLite failures and allocation failures;
- public API misuse, if any existing public-surface misuse path is reached.

## Test Plan

Add fast plain C tests under `packages/libmylite/tests/`, registered with a
dotted CTest name such as `libmylite.runtime.decimal_type`. Add parser tests to
the existing parser binary.

MySQL-runtime expectation coverage must include:

- accepted declaration forms: bare `DECIMAL`, `DECIMAL(M)`,
  `DECIMAL(M,D)`, `DECIMAL(0)`, `DECIMAL(0,0)`, `NUMERIC`, `DEC`, `FIXED`,
  and `UNSIGNED`;
- precision/scale diagnostics and unsupported/deferred syntax;
- `SHOW COLUMNS`, `SHOW CREATE TABLE`, and `INFORMATION_SCHEMA.COLUMNS`
  metadata;
- successful insert, replace, update, defaults, `ALTER ADD COLUMN`, and
  `ALTER SET/DROP DEFAULT` paths;
- exact canonical readback, scale padding, fractional rounding, negative zero,
  signed and unsigned range boundaries, clipping under `INSERT IGNORE`, and
  strict out-of-range/null/no-default failures;
- `WHERE IS NULL` / `IS NOT NULL` over decimal columns and rejection of decimal
  comparisons/order/distinct/grouping/aggregates;
- reopen persistence, independent file-backed handles, rename/drop behavior,
  `.mylite` preamble preservation, and cleanup on failure;
- regression coverage for existing parser, runtime lifecycle, catalog,
  information-schema, string/integer type, DML, storage, and VFS tests.

Verification before marking the implementation done:

1. `cmake --build --preset dev`
2. Focused parser/runtime CTest entries including the new decimal test and
   existing integer/string/default/DML/introspection lifecycle tests
3. `./packages/libmylite/tests/mysql_baseline_decimal_type_expectations.sh`
4. `cmake --workflow --preset check`

