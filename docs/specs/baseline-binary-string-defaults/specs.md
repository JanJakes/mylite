# Baseline Binary String Defaults

## Status

This feature specifies literal defaults for MyLite's existing descriptor-owned
`BINARY` and `VARBINARY` columns. It builds on baseline binary string storage,
byte-safe result readback, DML `DEFAULT` keyword materialization, descriptor
copying, and metadata rendering.

This is not a general binary expression-default feature. It admits ordinary
string and hexadecimal literal defaults for `BINARY` and `VARBINARY` only.
BLOB-family defaults remain deferred except for existing `DEFAULT NULL`
behavior.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Baseline binary string types:
  `docs/specs/baseline-binary-string-types/specs.md`
- Baseline DML default keyword values:
  `docs/specs/baseline-dml-default-keyword-values/specs.md`
- Baseline string defaults:
  `docs/specs/baseline-string-defaults/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `BINARY` and `VARBINARY`:
  https://dev.mysql.com/doc/refman/8.4/en/binary-varbinary.html
- MySQL 8.4 Reference Manual, hexadecimal literals:
  https://dev.mysql.com/doc/refman/8.4/en/hexadecimal-literals.html
- MySQL 8.4 Reference Manual, data type default values:
  https://dev.mysql.com/doc/refman/8.4/en/data-type-defaults.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_binary_string_defaults_expectations.sh`
records runtime probes for this feature. Observed behavior that shapes this
slice:

- `BINARY(n)` and `VARBINARY(n)` accept ordinary string and hexadecimal literal
  defaults.
- `BINARY(n)` defaults are converted through normal binary-string storage
  rules. Short values are right-padded with `0x00` to `n` bytes.
- `VARBINARY(n)` defaults preserve the literal byte length and are not padded.
- A default value whose converted byte length exceeds the descriptor length
  fails with error `1067 / 42000` and an invalid-default message.
- Omitted-column `INSERT`, explicit `DEFAULT` in admitted `INSERT`, `REPLACE`,
  and `UPDATE`, and `ALTER TABLE ... ADD COLUMN ... DEFAULT` materialize the
  descriptor default bytes.
- `SHOW CREATE TABLE` renders binary defaults as either quoted binary strings
  with MySQL escapes or hexadecimal literals when non-display bytes make that
  the MySQL rendering. MyLite may render a hexadecimal literal for all
  supported binary defaults in this slice because it is semantically equivalent
  SQL and deterministic.
- `SHOW COLUMNS` and `INFORMATION_SCHEMA.COLUMNS.COLUMN_DEFAULT` report binary
  defaults as text. MySQL trims trailing `0x00` bytes in this metadata
  representation and reports `0x` for all-zero nonempty binary defaults. Empty
  `VARBINARY` defaults display as the empty string.
- BLOB-family literal defaults without parentheses are rejected by MySQL.
  BLOB-family expression defaults such as `DEFAULT (X'41')` are accepted by
  MySQL but remain outside this slice because MyLite's general expression
  default machinery is intentionally narrower today.

## Scope

The implementation must add:

- `CREATE TABLE` support for explicit ordinary string and hexadecimal literal
  defaults on `BINARY` and `VARBINARY` descriptor columns;
- `ALTER TABLE ... ADD [COLUMN]` support for the same explicit defaults,
  including existing-row backfill through generated SQLite DDL;
- `ALTER TABLE ... ALTER [COLUMN] column SET DEFAULT` support for the same
  explicit defaults when the target descriptor is `BINARY` or `VARBINARY`;
- descriptor materialization of binary defaults through omitted-column
  `INSERT`, explicit `DEFAULT` in admitted `INSERT`, `REPLACE`, and
  single-table `UPDATE` assignment forms, and compatible `DEFAULT(column_name)`
  forms already admitted by the baseline;
- descriptor cloning/copying through `CREATE TABLE ... LIKE` and compatible
  descriptor-inferred table-copy paths;
- `SHOW COLUMNS`, `DESCRIBE`, `EXPLAIN table`, `SHOW CREATE TABLE`, and
  limited `INFORMATION_SCHEMA.COLUMNS` rendering for explicit `BINARY` and
  `VARBINARY` defaults;
- durable catalog storage of the converted default bytes without embedding NUL
  bytes into catalog text fields;
- persistent reopen behavior, table rename/drop behavior, file-format preamble
  preservation, and independent file-backed handle behavior.

## Non-Goals

This feature must not implement:

- defaults for BLOB-family descriptors except existing nullable
  `DEFAULT NULL`;
- parenthesized binary expression defaults;
- `_binary` introducers, adjacent string literal concatenation, parameters, or
  nonliteral default expressions;
- warning-producing default truncation;
- binary comparison, ordering, grouping, full binary expression evaluation, or
  binary protocol metadata beyond existing byte-safe result cells;
- SQLite fork patches.

## Grammar

MyLite already admits `DEFAULT literal` in column definitions and
`ALTER COLUMN SET DEFAULT literal`. This slice narrows semantic acceptance by
target descriptor, not by adding new syntax. The independently authored
Lemon-shape grammar remains:

```lemon
column_default ::= DEFAULT literal_value.
alter_column_action ::= ALTER optional_column ident SET DEFAULT literal_value.
literal_value ::= string_literal.
literal_value ::= hex_literal.
literal_value ::= NULL.
```

For this feature, `string_literal` and `hex_literal` are accepted only for
`BINARY` and `VARBINARY` targets. `NULL` keeps the existing nullable
`DEFAULT NULL` behavior.

## Semantics

The parser records literal structure and spans only. During planning, MyLite
builds a temporary descriptor for the column, decodes the admitted literal with
the same binary string literal decoder used by row DML, applies the same
descriptor conversion rules, and stores the converted byte sequence as the
authoritative default.

`BINARY(n)` defaults are padded with `0x00` to exactly `n` bytes. `BINARY(0)`
defaults store zero bytes. `VARBINARY(n)` defaults store exactly the decoded
bytes and accept zero bytes. Overlength defaults return MySQL-style invalid
default diagnostics instead of truncating.

Durable catalog storage uses a new binary default kind whose `default_text`
payload is an uppercase hexadecimal encoding of the converted bytes. This keeps
the existing text catalog column byte-safe without making SQLite's schema text
authoritative. The hexadecimal payload is internal descriptor data, not the
user-visible default rendering.

`SHOW COLUMNS` and `INFORMATION_SCHEMA.COLUMNS.COLUMN_DEFAULT` decode the
catalog payload, trim trailing zero bytes for display, and return:

- an empty string for zero-length `VARBINARY` defaults;
- `0x` for nonempty defaults whose display bytes are all trimmed away;
- `0x` followed by uppercase hex digits for remaining bytes.

`SHOW CREATE TABLE` renders binary defaults as deterministic hexadecimal SQL
literals. This may differ cosmetically from MySQL's quoted-string rendering for
printable byte sequences, but it is equivalent SQL for the supported subset and
avoids relying on MySQL's display heuristic. Tests must verify stored bytes,
metadata defaults, and the accepted generated DDL shape.

## Ownership Boundary

- Public API owns unchanged result and diagnostic ownership. No public ABI is
  added in this slice.
- Statement context owns warnings and affected rows. Supported in-range binary
  defaults record `warning_count == 0`.
- Parser/AST owns syntax admission only and does not decode bytes.
- Analyzer/planner owns descriptor resolution, literal decoding, byte-length
  validation, `BINARY` padding, and default descriptor construction.
- Catalog owns durable logical type, physical type, nullability, default kind,
  and internal hex default payload. Catalog descriptors remain authoritative.
- Runtime execution owns generated SQLite DDL for physical add-column backfill
  and prepared-statement binding for DML default materialization.
- Storage/VFS preserve the `.mylite` preamble and shifted SQLite payload.
- SQLite stores physical row bytes as BLOB values; MyLite does not require a
  SQLite fork patch.

## Diagnostics

Supported in-range definitions and DML default materialization produce no
warnings. Unsupported or invalid behavior returns the existing deterministic
diagnostics:

- overlength converted binary defaults: `1067 / 42000`, invalid default value;
- `DEFAULT NULL` on `NOT NULL` columns: existing invalid-default diagnostic;
- BLOB-family literal defaults: existing BLOB/TEXT default rejection;
- BLOB-family expression defaults: unsupported expression-default diagnostic;
- unsupported literal kinds, parameters, functions, or expressions: existing
  unsupported/default diagnostics;
- allocation and physical SQLite failures: existing runtime diagnostics.

## Performance and SQLite Fit

The feature is descriptor-driven and does not materialize table data except
where MySQL-visible `ALTER TABLE ... ADD COLUMN ... DEFAULT` requires SQLite to
backfill existing rows through its normal table alteration path. Omitted-column
and explicit-DML default paths decode the catalog hex payload into a single
planned BLOB value per assigned cell and bind that value to SQLite. Queries and
updates otherwise remain on the existing SQLite execution path.

## Tests

Coverage must include:

- `CREATE TABLE` binary and varbinary defaults from string, `X'...'`, and
  `0x...` literals;
- `BINARY` padding, `VARBINARY` non-padding, empty defaults, all-zero defaults,
  and high-byte defaults;
- metadata through `SHOW COLUMNS`, `DESCRIBE`, `SHOW CREATE TABLE`, and
  `INFORMATION_SCHEMA.COLUMNS`;
- omitted-column `INSERT`, explicit `DEFAULT`, `REPLACE`, and `UPDATE`
  materialization, including no-op affected-row behavior for unchanged bytes;
- `ALTER TABLE ... ADD COLUMN ... DEFAULT` backfill and
  `ALTER TABLE ... ALTER COLUMN ... SET/DROP DEFAULT`;
- reopen persistence and independent handle behavior;
- overlength defaults and deferred BLOB-family expression defaults;
- unchanged `.mylite` preamble behavior and existing binary-string type tests.
