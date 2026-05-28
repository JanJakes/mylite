# Baseline INFORMATION_SCHEMA ST_UNITS_OF_MEASURE

## Status

This phase adds `INFORMATION_SCHEMA.ST_UNITS_OF_MEASURE` as a queryable
synthetic information-schema system view. It exposes MySQL 8.4.9-shaped table
and column metadata and returns the static unit catalog that MySQL uses for
spatial distance unit names.

The slice is metadata-only. It does not add `ST_Distance()` unit validation,
spatial calculations, coordinate reference system support, mutable unit
catalogs, or physical MySQL data dictionary tables.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing information-schema implementation in
  `packages/libmylite/src/runtime/mylite_execution.c`
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.ST_UNITS_OF_MEASURE`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-st-units-of-measure-table.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, and
existing MyLite source code. It does not copy MySQL, MariaDB, Percona, SQLite
implementation internals, or other restrictively licensed implementation
sources.

## MySQL 8.4.9 Observations

Runtime probes were run against the local `mysql:8.4.9` Docker runtime named
`mylite-mysql-849`.

Observed behavior shaping this slice:

- `INFORMATION_SCHEMA.ST_UNITS_OF_MEASURE` exists as an `information_schema`
  `SYSTEM VIEW`.
- `INFORMATION_SCHEMA.TABLES` reports the system row with
  `TABLE_SCHEMA = 'information_schema'`, `TABLE_NAME = 'ST_UNITS_OF_MEASURE'`,
  `TABLE_TYPE = 'SYSTEM VIEW'`, `ENGINE = NULL`, `VERSION = 10`,
  `ROW_FORMAT = NULL`, `TABLE_ROWS = 0`, `DATA_LENGTH = 0`, and
  `AUTO_INCREMENT = NULL`.
- `INFORMATION_SCHEMA.COLUMNS` reports four columns in order: `UNIT_NAME`,
  `UNIT_TYPE`, `CONVERSION_FACTOR`, and `DESCRIPTION`.
- `UNIT_NAME` is nullable `varchar(255)`, character set `utf8mb4`, collation
  `utf8mb4_0900_ai_ci`, character maximum length `255`, octet length `1020`,
  and `COLUMN_DEFAULT = NULL`.
- `UNIT_TYPE` is nullable `varchar(7)`, character set `utf8mb4`, collation
  `utf8mb4_0900_ai_ci`, character maximum length `7`, octet length `28`, and
  `COLUMN_DEFAULT = NULL`.
- `CONVERSION_FACTOR` is nullable `double` with numeric precision `22`, SQL
  `NULL` numeric scale, and SQL `NULL` character metadata.
- `DESCRIPTION` is nullable `varchar(255)`, character set `utf8mb4`, collation
  `utf8mb4_0900_ai_ci`, character maximum length `255`, octet length `1020`,
  and `COLUMN_DEFAULT = NULL`.
- A fresh runtime returns 47 rows. Every observed row has `UNIT_TYPE =
  'LINEAR'` and `DESCRIPTION = ''`.
- Supported reads leave `@@warning_count = 0`, and `ROW_COUNT()` reports `-1`
  after the `SELECT`.

## Scope

Supported:

- `SELECT` from `INFORMATION_SCHEMA.ST_UNITS_OF_MEASURE` using the existing
  information-schema query subset;
- case-insensitive information-schema table name lookup;
- table aliases through the existing information-schema select path;
- table metadata through `INFORMATION_SCHEMA.TABLES`;
- column metadata through `INFORMATION_SCHEMA.COLUMNS`;
- the observed MySQL 8.4.9 static 47-row linear unit catalog;
- file-backed and in-memory handles with no storage mutation beyond opening
  the database.

Out of scope:

- `ST_Distance()` unit validation or spatial distance calculation;
- complete spatial function, predicate, WKB/WKT, SRID, or SRS support;
- `INFORMATION_SCHEMA.ST_SPATIAL_REFERENCE_SYSTEMS`;
- mutable unit catalogs or user-defined units;
- privilege filtering or account-specific visibility;
- physical MySQL data dictionary tables;
- SQLite storage, VFS, extension, or fork changes.

## Ownership Boundary

- Public API: unchanged. Applications continue through `mylite_execute()` and
  current result accessors.
- Parser/AST: unchanged. The existing information-schema `SELECT` path already
  resolves table names and aliases.
- Analyzer/runtime: recognizes `ST_UNITS_OF_MEASURE` as a supported synthetic
  information-schema system view and appends static rows.
- Catalog metadata: unchanged. No descriptors are introduced.
- Spatial runtime: unchanged. Spatial functions and value semantics are not
  introduced by this table.
- SQLite storage/VFS: unchanged. No physical SQLite table, view, extension, or
  fork patch is required.

## Syntax

No new SQL grammar is added. The feature uses the existing admitted
information-schema `SELECT` grammar.

Examples in scope:

```sql
SELECT COUNT(*) FROM INFORMATION_SCHEMA.ST_UNITS_OF_MEASURE;
SELECT UNIT_NAME, UNIT_TYPE, CONVERSION_FACTOR, DESCRIPTION
  FROM INFORMATION_SCHEMA.ST_UNITS_OF_MEASURE
 ORDER BY UNIT_NAME;
SELECT u.CONVERSION_FACTOR
  FROM INFORMATION_SCHEMA.ST_UNITS_OF_MEASURE AS u
 WHERE u.UNIT_NAME = 'metre';
```

## Runtime Semantics

`ST_UNITS_OF_MEASURE` is registered in the static information-schema table
registry. Row production emits the MySQL 8.4.9 observed unit rows as static
metadata:

- `UNIT_NAME`: the MySQL unit name;
- `UNIT_TYPE`: `LINEAR`;
- `CONVERSION_FACTOR`: MySQL's displayed conversion factor text;
- `DESCRIPTION`: the empty string.

The row set is independent of database contents and does not interact with
MyLite spatial column descriptors, SRS metadata, or physical storage.

## Diagnostics

The feature relies on existing information-schema diagnostics:

- unknown selected columns fail with the current unknown-column diagnostic;
- unsupported expressions, joins, grouping, predicates, ordering, and limits
  retain the current information-schema query subset behavior;
- allocation failures use existing MyLite runtime diagnostics.

Spatial function calls and unsupported unit arguments remain outside this
table's surface and continue to follow existing parser or unsupported-feature
behavior.

## Performance

The table emits 47 static rows. It does not read or write MyLite catalog
descriptors, physical row storage, SQLite tables, or spatial data.

## Tests

Add a focused C runtime test and a MySQL expectation script. Coverage must
include:

- wildcard column labels for `SELECT *`;
- exact ordered 47-row catalog contents;
- row count and `UNIT_TYPE` / empty-description invariants;
- case-insensitive table-name lookup;
- alias projection for a representative unit row;
- `warning_count == 0` and `ROW_COUNT() == -1` after successful reads;
- `INFORMATION_SCHEMA.TABLES` system-view row;
- `INFORMATION_SCHEMA.COLUMNS` metadata for all four columns;
- file-backed read behavior and unchanged MyLite file preamble.

Verification before commit:

```sh
cmake --build --preset dev --target mylite_runtime_information_schema_st_units_of_measure_test
ctest --preset dev -R '^libmylite\.runtime\.(information_schema_st_units_of_measure|information_schema_static_catalogs|builtin_schema_table_directory)$' --output-on-failure
packages/libmylite/tests/mysql_baseline_information_schema_st_units_of_measure_expectations.sh
git diff --check
cmake --workflow --preset check
```
