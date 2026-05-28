# Baseline INFORMATION_SCHEMA ST_GEOMETRY_COLUMNS

## Status

This phase adds `INFORMATION_SCHEMA.ST_GEOMETRY_COLUMNS` as a queryable
synthetic information-schema system view. It exposes MySQL 8.4.9-shaped table
and column metadata and returns one row for each supported persistent MyLite
base-table spatial column.

The slice is descriptor-backed metadata only. It does not add SRID attributes,
SRS catalogs, spatial value parsing, spatial functions, spatial predicates,
temporary-table spatial metadata, privilege filtering, or physical spatial
storage.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing spatial descriptor implementation and tests:
  `packages/libmylite/src/runtime/mylite_execution.c` and
  `packages/libmylite/tests/runtime_spatial_index_metadata_test.c`
- Existing information-schema implementation in
  `packages/libmylite/src/runtime/mylite_execution.c`
- MySQL 8.4 Reference Manual, `ST_GEOMETRY_COLUMNS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-st-geometry-columns-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA` table reference:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-table-reference.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, and
existing MyLite source code. It does not copy MySQL, MariaDB, Percona, SQLite
implementation internals, or other restrictively licensed implementation
sources.

## MySQL 8.4.9 Observations

Runtime probes were run against the local `mysql:8.4.9` Docker runtime named
`mylite-mysql-849`.

Observed behavior shaping this slice:

- `INFORMATION_SCHEMA.ST_GEOMETRY_COLUMNS` exists as an `information_schema`
  `SYSTEM VIEW`.
- With no user spatial columns in the target runtime, `SELECT COUNT(*) FROM
  INFORMATION_SCHEMA.ST_GEOMETRY_COLUMNS` returns `0`.
- Creating a persistent table with `GEOMETRY`, `POINT`, `LINESTRING`,
  `POLYGON`, `MULTIPOINT`, `MULTILINESTRING`, `MULTIPOLYGON`, and
  `GEOMETRYCOLLECTION` columns produces one metadata row for each spatial
  column.
- Runtime rows contain `TABLE_CATALOG = 'def'`, the user schema and table
  names, the column name, `SRS_NAME = NULL`, `SRS_ID = NULL` when no explicit
  SRID is declared, and lowercase `GEOMETRY_TYPE_NAME` values. MySQL reports
  `GEOMETRYCOLLECTION` as `geomcollection`.
- `INFORMATION_SCHEMA.TABLES` reports the system row with
  `TABLE_SCHEMA = 'information_schema'`, `TABLE_NAME = 'ST_GEOMETRY_COLUMNS'`,
  `TABLE_TYPE = 'SYSTEM VIEW'`, `ENGINE = NULL`, `VERSION = 10`,
  `ROW_FORMAT = NULL`, `TABLE_ROWS = 0`, `DATA_LENGTH = 0`, and
  `AUTO_INCREMENT = NULL`.
- `INFORMATION_SCHEMA.COLUMNS` reports seven columns in order:
  `TABLE_CATALOG`, `TABLE_SCHEMA`, `TABLE_NAME`, `COLUMN_NAME`, `SRS_NAME`,
  `SRS_ID`, and `GEOMETRY_TYPE_NAME`.
- `TABLE_CATALOG`, `TABLE_SCHEMA`, and `TABLE_NAME` are non-null
  `varchar(64)` columns with character set `utf8mb3`, collation
  `utf8mb3_bin`, character maximum length `64`, octet length `192`, and
  `COLUMN_DEFAULT = NULL`.
- `COLUMN_NAME` is nullable `varchar(64)` with character set `utf8mb3`,
  collation `utf8mb3_tolower_ci`, character maximum length `64`, octet length
  `192`, and `COLUMN_DEFAULT = NULL`.
- `SRS_NAME` is nullable `varchar(80)` with character set `utf8mb3`, collation
  `utf8mb3_general_ci`, character maximum length `80`, octet length `240`, and
  `COLUMN_DEFAULT = NULL`.
- `SRS_ID` is nullable `int unsigned` with `DATA_TYPE = 'int'`,
  `NUMERIC_PRECISION = 10`, `NUMERIC_SCALE = 0`, and SQL `NULL` character
  metadata.
- `GEOMETRY_TYPE_NAME` is nullable `longtext` with character set `utf8mb3`,
  collation `utf8mb3_bin`, character maximum and octet lengths
  `4294967295`, and SQL `NULL` numeric metadata.
- Successful supported reads leave `@@warning_count = 0`, and `ROW_COUNT()`
  reports `-1` after the `SELECT`.

## Scope

Supported:

- `SELECT` from `INFORMATION_SCHEMA.ST_GEOMETRY_COLUMNS` using the existing
  information-schema query subset;
- case-insensitive information-schema table name lookup;
- table aliases through the existing information-schema select path;
- table metadata through `INFORMATION_SCHEMA.TABLES`;
- column metadata through `INFORMATION_SCHEMA.COLUMNS`;
- descriptor rows for supported persistent base-table spatial columns:
  `GEOMETRY`, `POINT`, `LINESTRING`, `POLYGON`, `MULTIPOINT`,
  `MULTILINESTRING`, `MULTIPOLYGON`, and `GEOMETRYCOLLECTION`;
- stable `NULL` `SRS_NAME` and `SRS_ID` values while MyLite has no SRID
  attributes;
- file-backed reopen and independent-handle behavior through existing catalog
  descriptors.

Out of scope:

- explicit `SRID` column attributes and non-`NULL` SRS metadata;
- `INFORMATION_SCHEMA.ST_SPATIAL_REFERENCE_SYSTEMS`;
- `INFORMATION_SCHEMA.ST_UNITS_OF_MEASURE`;
- temporary-table rows;
- spatial value parsing, WKB/WKT storage, spatial functions, spatial
  predicates, validity checks, or coordinate operations;
- privilege filtering and complete MySQL data dictionary behavior;
- SQLite storage, VFS, extension, or fork changes.

## Ownership Boundary

- Public API: unchanged. Applications continue through `mylite_execute()` and
  current result accessors.
- Parser/AST: unchanged. The existing information-schema `SELECT` path already
  resolves table names and aliases.
- Analyzer/runtime: recognizes `ST_GEOMETRY_COLUMNS` as a supported
  information-schema system view and appends rows from catalog descriptors.
- Catalog metadata: persistent table descriptors own the spatial column logical
  type and nullability; this slice reads those descriptors only.
- Spatial runtime: unchanged. No spatial values, SRIDs, functions, or
  validation are introduced.
- SQLite storage/VFS: unchanged. No physical SQLite table, view, extension, or
  fork patch is required.

## Syntax

No new SQL grammar is added. The feature uses the existing admitted
information-schema `SELECT` grammar.

Examples in scope:

```sql
SELECT COUNT(*) FROM INFORMATION_SCHEMA.ST_GEOMETRY_COLUMNS;
SELECT TABLE_NAME, COLUMN_NAME, SRS_NAME, SRS_ID, GEOMETRY_TYPE_NAME
  FROM INFORMATION_SCHEMA.ST_GEOMETRY_COLUMNS
 WHERE TABLE_SCHEMA = DATABASE()
 ORDER BY TABLE_NAME, COLUMN_NAME;
SELECT s.COLUMN_NAME
  FROM INFORMATION_SCHEMA.ST_GEOMETRY_COLUMNS AS s
 WHERE s.GEOMETRY_TYPE_NAME = 'point';
```

## Runtime Semantics

`ST_GEOMETRY_COLUMNS` is registered in the static information-schema table
registry. Row production walks persistent MyLite catalog schemas and base-table
column descriptors:

- nonspatial descriptors are skipped;
- views are skipped because MyLite has no view spatial result-column
  dependency metadata for this table;
- temporary-table descriptors are skipped to match the current temporary
  spatial-column scope;
- each spatial column emits one row with `TABLE_CATALOG = 'def'`,
  descriptor-owned schema/table/column names, `SRS_NAME = NULL`, `SRS_ID =
  NULL`, and the descriptor display type;
- `GEOMETRYCOLLECTION` descriptors emit `geomcollection`, matching observed
  MySQL 8.4.9 behavior for the no-SRID subset.

## Diagnostics

The feature relies on existing information-schema diagnostics:

- unknown selected columns fail with the current unknown-column diagnostic;
- unsupported expressions, joins, ordering, predicates, and limits retain the
  current information-schema query subset behavior;
- allocation failures use existing MyLite allocation diagnostics;
- unsupported spatial DDL, non-`NULL` spatial values, and SRID attributes
  retain existing spatial diagnostics outside this table.

No new public diagnostics are introduced.

## Performance

The table is descriptor-backed and proportional to the number of persistent
table columns in the open MyLite handle. It does not read physical row storage,
create SQLite tables, or perform spatial parsing.

## Tests

Add a focused C runtime test and a MySQL expectation script. Coverage must
include:

- empty row count in the default MySQL and MyLite session state;
- rows for all currently supported MyLite spatial descriptor types;
- `GEOMETRYCOLLECTION` row display as `geomcollection`;
- `SRS_NAME` and `SRS_ID` as SQL `NULL` for no-SRID columns;
- case-insensitive table-name lookup;
- alias projection through the existing information-schema query path;
- column names for `SELECT *`;
- `warning_count == 0` and `ROW_COUNT() == -1` after successful reads;
- `INFORMATION_SCHEMA.TABLES` system-view row;
- `INFORMATION_SCHEMA.COLUMNS` metadata for all seven columns;
- file-backed reopen and independent file-backed handles.

Verification before commit:

```sh
cmake --build --preset dev
ctest --preset dev -R '^libmylite\.runtime\.(information_schema_st_geometry_columns|spatial_index_metadata|information_schema_static_catalogs|builtin_schema_table_directory)$' --output-on-failure
packages/libmylite/tests/mysql_baseline_information_schema_st_geometry_columns_expectations.sh
git diff --check
cmake --workflow --preset check
```
