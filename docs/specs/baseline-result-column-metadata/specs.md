# Baseline Result Column Metadata

## Summary

This phase exposes MySQL-shaped result-column metadata through `mylite_result`
for descriptor-backed single-table `SELECT` statements. The supported surface
covers selected base-table columns, aliases, `*` expansion, and the descriptor
families already admitted by current row storage and table `SELECT` slices:
integer, exact `DECIMAL`, approximate `FLOAT` / `DOUBLE`, `YEAR`, canonical
temporal types, `CHAR`, `VARCHAR`, baseline `TEXT`, binary string, and `BIT`.

The core behavior is:

- public result objects own metadata for their full lifetime;
- metadata is built from MyLite catalog descriptors, not SQLite column metadata;
- result labels follow select-item aliases, while origin names follow the
  descriptor source column;
- MySQL numeric field type IDs, display lengths, decimals, collation IDs, and
  protocol flag bit values are reported for the admitted descriptor families;
- existing non-row statements and scalar/result-set helpers keep default empty
  or unknown metadata until a later slice specifies them.

## Sources And Evidence

- MyLite architecture and engineering standards:
  - `README.md`
  - `AGENTS.md`
  - `docs/architecture/engineering-standards.md`
- Existing metadata and result ownership specs:
  - `docs/specs/runtime-handles-statement-context/specs.md`
  - `docs/specs/baseline-implementation-strategy/specs.md`
- Existing descriptor-backed table and type slices:
  - `docs/specs/baseline-select-where-lifecycle/specs.md`
  - `docs/specs/baseline-select-order-limit-lifecycle/specs.md`
  - `docs/specs/baseline-varchar-type/specs.md`
  - `docs/specs/baseline-text-type/specs.md`
  - `docs/specs/baseline-binary-string-types/specs.md`
  - `docs/specs/baseline-decimal-type/specs.md`
  - `docs/specs/baseline-approximate-numeric-types/specs.md`
  - `docs/specs/baseline-temporal-types/specs.md`
  - `docs/specs/baseline-bit-type/specs.md`
- Official MySQL 8.4 documentation:
  - C API `MYSQL_FIELD` metadata:
    <https://dev.mysql.com/doc/c-api/8.4/en/c-api-data-structures.html>
  - optional result-set metadata:
    <https://dev.mysql.com/doc/c-api/8.4/en/c-api-optional-metadata.html>
  - MySQL field type enum reference:
    <https://dev.mysql.com/doc/dev/mysql-server/8.4.7/field__types_8h.html>
  - MySQL column definition flag values:
    <https://dev.mysql.com/doc/dev/mysql-server/8.4.5/group__group__cs__column__definition__flags.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_result_column_metadata_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## Runtime Observations

MySQL 8.4.9 probes establish these expectations for the admitted slice:

- `MYSQL_FIELD.name` is the select-item label; an explicit `AS` alias replaces
  the column name in metadata.
- `MYSQL_FIELD.org_name` ignores aliases and reports the source column name.
- `MYSQL_FIELD.table` reports a table alias when present; otherwise it reports
  the table name. `MYSQL_FIELD.org_table` reports the base table name.
- `MYSQL_FIELD.db` reports the source database for base-table columns.
- With a `utf8mb4` result connection, nonbinary character columns report
  `charsetnr = 255` for `utf8mb4_0900_ai_ci`. Binary and numeric columns report
  `charsetnr = 63`.
- Display length is byte-oriented. `CHAR(n)`, `VARCHAR(n)`, and `TEXT` family
  columns under `utf8mb4` report `n * 4` or family maximum bytes multiplied by
  four. Binary string and `BIT` columns report their byte or bit width directly.
- Integer display lengths are MySQL's default widths: signed/unsigned
  `TINYINT` 4/3, `SMALLINT` 6/5, `MEDIUMINT` 9/8, `INT` 11/10, and `BIGINT`
  20/20.
- Exact `DECIMAL(p,s)` reports type `NEWDECIMAL`, `decimals = s`, and display
  length `p + 1` for unsigned scale-zero, `p + 2` for signed scaled, and the
  corresponding sign/decimal-point adjusted width for other supported shapes.
- `FLOAT` reports length 12 and decimals 31; `DOUBLE` reports length 22 and
  decimals 31.
- `YEAR` reports type `YEAR`, length 4, flags `UNSIGNED ZEROFILL NUM`.
- `DATE`, `TIME`, `DATETIME`, and `TIMESTAMP` report binary collation and the
  `BINARY` flag with lengths 10, 10, 19, and 19 for the current zero-fractional
  subset.
- `BIT(n)` reports type `BIT`, binary collation, length `n`, and `UNSIGNED`.
- `TEXT` reports type `BLOB`, the `BLOB` flag, and a nonbinary result collation.
  `BLOB` reports type `BLOB`, binary collation, and both `BLOB` and `BINARY`.
- Successful metadata-only `SELECT ... LIMIT 0` probes produce no warnings.

## Ownership Boundaries

- Public API: gains field accessors on opaque `mylite_result`; no public struct
  layout is exposed. Invalid result pointers or out-of-range column indexes
  return `NULL`, `0`, or `MYLITE_RESULT_COLUMN_TYPE_UNKNOWN`.
- Statement context: remains responsible for diagnostics and statement
  lifecycle. It may own temporary metadata while analyzing, but public results
  must not borrow from statement context.
- Lexer/parser/AST: unchanged. This phase does not add SQL syntax.
- Analyzer/planner: descriptor-backed `SELECT` planning already resolves source
  schemas, source tables, selected descriptor columns, aliases, visibility, and
  source indexes. This phase records the single-table source alias in the plan
  so result metadata can match MySQL table-name behavior.
- Catalog module: remains authoritative for schema, table, column, primary-key,
  unique-index, nonunique-index, nullability, default, type, and auto-increment
  metadata. Metadata reads do not mutate catalog rows, descriptor versions,
  descriptor caches, catalog generation, or `sqlite_schema_generation`.
- SQLite physical row storage: SQLite executes the generated table `SELECT`.
  MyLite does not use SQLite result-column metadata because SQLite sees
  physical table names, physical column types, and translated SQL.
- Result builder: owns labels, row values, affected rows, warning counts, and
  result metadata. Metadata is appended before any rows.
- Storage/VFS/file format: unchanged. Metadata reads do not touch the `.mylite`
  preamble or shifted SQLite payload invariants.

## Public API

The new API exposes metadata through accessors:

```c
enum mylite_result_column_type {
    MYLITE_RESULT_COLUMN_TYPE_UNKNOWN = -1,
    MYLITE_RESULT_COLUMN_TYPE_DECIMAL = 0,
    MYLITE_RESULT_COLUMN_TYPE_TINY = 1,
    MYLITE_RESULT_COLUMN_TYPE_SHORT = 2,
    MYLITE_RESULT_COLUMN_TYPE_LONG = 3,
    MYLITE_RESULT_COLUMN_TYPE_FLOAT = 4,
    MYLITE_RESULT_COLUMN_TYPE_DOUBLE = 5,
    MYLITE_RESULT_COLUMN_TYPE_NULL = 6,
    MYLITE_RESULT_COLUMN_TYPE_TIMESTAMP = 7,
    MYLITE_RESULT_COLUMN_TYPE_LONGLONG = 8,
    MYLITE_RESULT_COLUMN_TYPE_INT24 = 9,
    MYLITE_RESULT_COLUMN_TYPE_DATE = 10,
    MYLITE_RESULT_COLUMN_TYPE_TIME = 11,
    MYLITE_RESULT_COLUMN_TYPE_DATETIME = 12,
    MYLITE_RESULT_COLUMN_TYPE_YEAR = 13,
    MYLITE_RESULT_COLUMN_TYPE_VARCHAR = 15,
    MYLITE_RESULT_COLUMN_TYPE_BIT = 16,
    MYLITE_RESULT_COLUMN_TYPE_NEWDECIMAL = 246,
    MYLITE_RESULT_COLUMN_TYPE_BLOB = 252,
    MYLITE_RESULT_COLUMN_TYPE_VAR_STRING = 253,
    MYLITE_RESULT_COLUMN_TYPE_STRING = 254
};

const char *mylite_result_column_schema_name(const mylite_result *, size_t);
const char *mylite_result_column_table_name(const mylite_result *, size_t);
const char *mylite_result_column_origin_schema_name(const mylite_result *, size_t);
const char *mylite_result_column_origin_table_name(const mylite_result *, size_t);
const char *mylite_result_column_origin_name(const mylite_result *, size_t);
enum mylite_result_column_type mylite_result_column_type(const mylite_result *, size_t);
uint32_t mylite_result_column_flags(const mylite_result *, size_t);
uint32_t mylite_result_column_charset_id(const mylite_result *, size_t);
uint32_t mylite_result_column_collation_id(const mylite_result *, size_t);
uint64_t mylite_result_column_display_length(const mylite_result *, size_t);
uint16_t mylite_result_column_decimals(const mylite_result *, size_t);
int mylite_result_column_nullable(const mylite_result *, size_t);
```

The existing `mylite_result_column_name()` remains the label accessor. The
`charset_id` and `collation_id` accessors currently both expose MySQL's
`charsetnr` / collation-id value because MySQL's C API identifies the
character-set/collation pair by one numeric ID. MyLite may later add name
accessors if protocol or client adapters need them.

Public flag constants use MySQL protocol bit values for the admitted flags:

```c
MYLITE_RESULT_COLUMN_FLAG_NOT_NULL       1
MYLITE_RESULT_COLUMN_FLAG_PRI_KEY        2
MYLITE_RESULT_COLUMN_FLAG_UNIQUE_KEY     4
MYLITE_RESULT_COLUMN_FLAG_MULTIPLE_KEY   8
MYLITE_RESULT_COLUMN_FLAG_BLOB           16
MYLITE_RESULT_COLUMN_FLAG_UNSIGNED       32
MYLITE_RESULT_COLUMN_FLAG_ZEROFILL       64
MYLITE_RESULT_COLUMN_FLAG_BINARY         128
MYLITE_RESULT_COLUMN_FLAG_AUTO_INCREMENT 512
MYLITE_RESULT_COLUMN_FLAG_NO_DEFAULT     4096
MYLITE_RESULT_COLUMN_FLAG_PART_KEY       16384
MYLITE_RESULT_COLUMN_FLAG_NUM            32768
```

## Metadata Semantics

For a descriptor-backed single-table `SELECT` result column:

- label: select-item alias if present, otherwise the descriptor column name;
- schema name: selected source schema;
- table name: table alias if present, otherwise the descriptor table name;
- origin schema name: selected source schema;
- origin table name: descriptor table name;
- origin column name: descriptor column name;
- nullable: descriptor `is_nullable`;
- type, flags, collation, display length, and decimals: mapped from the
  descriptor and loaded index metadata.

`SELECT *` and visible wildcard expansion report metadata for visible
descriptor columns. Explicit references to invisible descriptor columns report
metadata when current SELECT rules admit them.

Joined SELECTs, scalar no-source/`DUAL` SELECTs, `SHOW`, and
`INFORMATION_SCHEMA` synthetic results keep default metadata for this phase:
labels are present, but origin names are empty, type is unknown unless already
set by a future caller, lengths and flags are zero, and nullable is true.

## Type Mapping

- `TINYINT` and `TINYINT(1)`: `TINY`, binary collation, numeric flag.
- `SMALLINT`: `SHORT`, binary collation, numeric flag.
- `MEDIUMINT`: `INT24`, binary collation, numeric flag.
- `INT` and `INTEGER`: `LONG`, binary collation, numeric flag.
- `BIGINT`: `LONGLONG`, binary collation, numeric flag.
- Unsigned integer variants add `UNSIGNED`.
- `DECIMAL(p,s)`: `NEWDECIMAL`, binary collation, numeric flag, decimals `s`.
- Unsigned decimal variants add `UNSIGNED`.
- `FLOAT`: `FLOAT`, binary collation, numeric flag, decimals 31.
- `DOUBLE`: `DOUBLE`, binary collation, numeric flag, decimals 31.
- `YEAR`: `YEAR`, binary collation, `UNSIGNED`, `ZEROFILL`, and `NUM`.
- `DATE`, `TIME`, `DATETIME`, `TIMESTAMP`: their matching MySQL type, binary
  collation, and `BINARY`.
- `CHAR(n)` and `BINARY(n)`: `STRING`; `BINARY(n)` uses binary collation and
  `BINARY`.
- `VARCHAR(n)` and `VARBINARY(n)`: `VAR_STRING`; `VARBINARY(n)` uses binary
  collation and `BINARY`.
- `TEXT` family: `BLOB`, current `utf8mb4` result collation, `BLOB`.
- `BLOB` family: `BLOB`, binary collation, `BLOB` and `BINARY`.
- `BIT(n)`: `BIT`, binary collation, `UNSIGNED`.

The current descriptor catalog has table-level default charset/collation but no
per-column charset/collation descriptor. Nonbinary string metadata therefore
uses the current session `collation_connection` ID for admitted `SET NAMES`
`utf8mb4` collations, defaulting to `255` for `utf8mb4_0900_ai_ci`. If a table
default collation is `utf8mb4_bin`, string columns additionally carry
`BINARY`, matching the verified MySQL behavior for the current table default
slice.

## Flags

Flags are derived from descriptors and loaded key metadata:

- `NOT_NULL`: descriptor is not nullable.
- `PRI_KEY`: descriptor column participates in the primary key.
- `UNIQUE_KEY`: descriptor column participates in a supported unique secondary
  index and is not already flagged primary.
- `MULTIPLE_KEY`: descriptor column participates in a supported nonunique
  secondary index.
- `PART_KEY`: descriptor column participates in any supported primary,
  unique, or nonunique key.
- `NO_DEFAULT`: descriptor is `NOT NULL`, not auto-increment, and has no
  explicit default.
- `AUTO_INCREMENT`: descriptor is auto-increment.
- type-family flags are set as described in type mapping.

For supported prefix indexes, the indexed descriptor column still receives the
same key flag class as MySQL for the current index kind; prefix length itself is
not part of this result metadata API.

## Diagnostics

This phase does not add new SQL diagnostics. Metadata allocation failures map
to the existing `MYLITE_NOMEM` path and set the existing out-of-memory
diagnostic. Public accessor misuse is non-failing and follows existing result
accessor conventions by returning `NULL`, `0`, or
`MYLITE_RESULT_COLUMN_TYPE_UNKNOWN`.

## Performance And SQLite

Metadata construction is descriptor-driven and runs once per result column
before row execution. It loads key descriptors once for the single-table source
and then maps each selected column in memory. Row scans, predicates, ordering,
limits, and value materialization remain on the existing SQLite-backed
execution path. No SQLite fork patch is required.

## Test Plan

- MySQL expectation script verifies `--column-type-info` output for aliases,
  table aliases, representative types, display lengths, decimals, collations,
  and flags.
- C runtime tests verify public accessors over descriptor-backed single-table
  `SELECT` metadata for aliases, wildcard expansion, nullable/not-null,
  integer families, decimal, approximate, temporal, string, binary string,
  `BIT`, primary/unique/nonunique key flags, auto-increment, and reopen
  persistence.
- Misuse tests verify `NULL` result and out-of-range column behavior.
- Existing result metadata, parser, runtime lifecycle, type, index, and
  workflow checks must continue to pass.

## Known Gaps

- No MySQL wire protocol packets are added in this phase.
- No metadata optionality negotiation is implemented.
- No full expression metadata for scalar expressions, functions, aggregates,
  subqueries, `SHOW`, or `INFORMATION_SCHEMA` synthetic result columns.
- No per-column charset/collation descriptors, collation coercibility,
  expression collation derivation, `max_length`, default-value metadata,
  privilege-sensitive metadata, prepared-statement metadata, or `COM_FIELD_LIST`.
