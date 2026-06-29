# Baseline INFORMATION_SCHEMA ST_SPATIAL_REFERENCE_SYSTEMS

## Status

This phase adds `INFORMATION_SCHEMA.ST_SPATIAL_REFERENCE_SYSTEMS` as a
queryable synthetic information-schema system view. It exposes MySQL
8.4.9-shaped table and column metadata and returns no rows in MyLite because
MyLite does not yet implement SRID attributes, spatial value semantics, GIS
functions, or a spatial reference system dictionary.

The slice is metadata-only. It does not add EPSG data rows, SRID validation,
executable SRS catalog mutation, WKT definition parsing, spatial calculations,
or physical MySQL data dictionary tables. `CREATE SPATIAL REFERENCE SYSTEM` and
`DROP SPATIAL REFERENCE SYSTEM` are accepted separately as embedded no-op
placeholders.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing spatial information-schema specs, especially
  `docs/specs/baseline-information-schema-st-geometry-columns/specs.md` and
  `docs/specs/baseline-information-schema-st-units-of-measure/specs.md`
- MySQL 8.4 Reference Manual,
  `INFORMATION_SCHEMA.ST_SPATIAL_REFERENCE_SYSTEMS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-st-spatial-reference-systems-table.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, and
existing MyLite source code. It does not copy MySQL, MariaDB, Percona, SQLite
implementation internals, or other restrictively licensed implementation
sources.

## MySQL 8.4.9 Observations

Runtime probes were run against the local `mysql:8.4.9` Docker runtime named
`mylite-mysql-849`.

Observed behavior shaping this slice:

- `INFORMATION_SCHEMA.ST_SPATIAL_REFERENCE_SYSTEMS` exists as an
  `information_schema` `SYSTEM VIEW`.
- `INFORMATION_SCHEMA.TABLES` reports the system row with
  `TABLE_SCHEMA = 'information_schema'`,
  `TABLE_NAME = 'ST_SPATIAL_REFERENCE_SYSTEMS'`,
  `TABLE_TYPE = 'SYSTEM VIEW'`, `ENGINE = NULL`, `VERSION = 10`,
  `ROW_FORMAT = NULL`, `TABLE_ROWS = 0`, `DATA_LENGTH = 0`, and
  `AUTO_INCREMENT = NULL`.
- `INFORMATION_SCHEMA.COLUMNS` reports six columns in order: `SRS_NAME`,
  `SRS_ID`, `ORGANIZATION`, `ORGANIZATION_COORDSYS_ID`, `DEFINITION`, and
  `DESCRIPTION`.
- `SRS_NAME` is non-null `varchar(80)`, character set `utf8mb3`, collation
  `utf8mb3_general_ci`, character maximum length `80`, octet length `240`,
  and `COLUMN_DEFAULT = NULL`.
- `SRS_ID` is non-null `int unsigned` with numeric precision `10`, numeric
  scale `0`, and SQL `NULL` character metadata.
- `ORGANIZATION` is nullable `varchar(256)`, character set `utf8mb3`,
  collation `utf8mb3_general_ci`, character maximum length `256`, octet
  length `768`, and `COLUMN_DEFAULT = NULL`.
- `ORGANIZATION_COORDSYS_ID` is nullable `int unsigned` with numeric precision
  `10`, numeric scale `0`, and SQL `NULL` character metadata.
- `DEFINITION` is non-null `varchar(4096)`, character set `utf8mb3`,
  collation `utf8mb3_bin`, character maximum length `4096`, octet length
  `12288`, and `COLUMN_DEFAULT = NULL`.
- `DESCRIPTION` is nullable `varchar(2048)`, character set `utf8mb3`,
  collation `utf8mb3_bin`, character maximum length `2048`, octet length
  `6144`, and `COLUMN_DEFAULT = NULL`.
- The observed MySQL 8.4.9 runtime returned 5238 SRS rows: 4692 projected
  rows, 545 geographic rows, and one special non-EPSG row for SRID 0.
- Successful reads leave `@@warning_count = 0`, and `ROW_COUNT()` reports
  `-1`.

Representative probe:

```sh
docker exec -i mylite-mysql-849 mysql --protocol=TCP -h127.0.0.1 -uroot \
  --batch --raw --skip-column-names <<'SQL'
SELECT VERSION();
SHOW FULL TABLES FROM INFORMATION_SCHEMA
 WHERE Tables_in_INFORMATION_SCHEMA = 'ST_SPATIAL_REFERENCE_SYSTEMS';
SELECT TABLE_SCHEMA,TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,
       TABLE_ROWS,DATA_LENGTH,AUTO_INCREMENT
  FROM INFORMATION_SCHEMA.TABLES
 WHERE TABLE_SCHEMA='information_schema'
   AND TABLE_NAME='ST_SPATIAL_REFERENCE_SYSTEMS';
SELECT TABLE_NAME,COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,
       DATA_TYPE,CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,
       NUMERIC_PRECISION,NUMERIC_SCALE,DATETIME_PRECISION,
       CHARACTER_SET_NAME,COLLATION_NAME,COLUMN_TYPE,PRIVILEGES
  FROM INFORMATION_SCHEMA.COLUMNS
 WHERE TABLE_SCHEMA='information_schema'
   AND TABLE_NAME='ST_SPATIAL_REFERENCE_SYSTEMS'
 ORDER BY ORDINAL_POSITION;
SELECT COUNT(*) FROM INFORMATION_SCHEMA.ST_SPATIAL_REFERENCE_SYSTEMS;
SELECT COUNT(*),
       CASE LEFT(DEFINITION,6)
         WHEN 'PROJCS' THEN 'Projected'
         WHEN 'GEOGCS' THEN 'Geographic'
         ELSE 'Other'
       END AS SRS_TYPE
  FROM INFORMATION_SCHEMA.ST_SPATIAL_REFERENCE_SYSTEMS
 GROUP BY SRS_TYPE
 ORDER BY SRS_TYPE;
SELECT SRS_NAME,SRS_ID
  FROM INFORMATION_SCHEMA.ST_SPATIAL_REFERENCE_SYSTEMS
 WHERE SRS_ID = 4326;
SELECT @@warning_count, ROW_COUNT();
SQL
```

## Scope

Supported:

- `SELECT` from `INFORMATION_SCHEMA.ST_SPATIAL_REFERENCE_SYSTEMS` using the
  existing information-schema query subset;
- wildcard projection with the MySQL-observed column order;
- case-insensitive information-schema table name lookup;
- table aliases, predicates, ordering, and `COUNT(*)` through the existing
  metadata query path;
- unqualified reads while `information_schema` is the selected schema;
- stable empty-row behavior, including after MyLite spatial columns and spatial
  index metadata are created;
- system metadata through `INFORMATION_SCHEMA.TABLES`,
  `INFORMATION_SCHEMA.COLUMNS`, `SHOW TABLES`, `SHOW FULL TABLES`, and
  `SHOW TABLE STATUS` via the existing built-in table directory.

Out of scope:

- the MySQL EPSG-backed SRS row catalog, including SRID 0 and common EPSG rows
  such as 4326;
- `mysql.st_spatial_reference_systems` data dictionary rows;
- SRID column attributes, spatial value parsing, or spatial function SRID
  validation;
- WKT SRS definition parsing, axis metadata, coordinate transformations, or
  distance calculations;
- executable `CREATE SPATIAL REFERENCE SYSTEM` and
  `DROP SPATIAL REFERENCE SYSTEM` catalog mutation;
- mutable SRS catalogs or user-defined SRS entries;
- privilege filtering or account-specific visibility;
- SQLite storage, VFS, extension, or fork changes.

## Ownership Boundary

- Public API: unchanged. Applications continue through `mylite_execute()` and
  current result accessors.
- Parser/AST: unchanged. The existing information-schema `SELECT` path already
  resolves table names, aliases, identifiers, predicates, and ordering.
- Analyzer/runtime: recognizes `ST_SPATIAL_REFERENCE_SYSTEMS` as a supported
  information-schema system view and returns an empty row set.
- Catalog metadata: unchanged. Current spatial column descriptors continue to
  expose `NULL` `SRS_NAME` and `SRS_ID` in `ST_GEOMETRY_COLUMNS`.
- Spatial runtime: unchanged. Spatial values and functions are not introduced
  by this table.
- SQLite storage/VFS: unchanged. No physical SQLite table, view, extension, or
  fork patch is required.

## Syntax

No new SQL grammar is added. The feature uses the existing admitted
information-schema `SELECT` grammar.

Examples in scope:

```sql
SELECT * FROM INFORMATION_SCHEMA.ST_SPATIAL_REFERENCE_SYSTEMS;
SELECT COUNT(*) FROM INFORMATION_SCHEMA.ST_SPATIAL_REFERENCE_SYSTEMS;
SELECT s.SRS_NAME, s.SRS_ID
  FROM INFORMATION_SCHEMA.ST_SPATIAL_REFERENCE_SYSTEMS AS s
 WHERE s.ORGANIZATION = 'EPSG'
 ORDER BY s.SRS_ID;
SELECT COUNT(*) FROM ST_SPATIAL_REFERENCE_SYSTEMS;
```

## Runtime Semantics

`ST_SPATIAL_REFERENCE_SYSTEMS` is registered in the static information-schema
table registry. Row production is intentionally empty:

- system rows for `TABLES` and `COLUMNS` are generated from static
  descriptors;
- no rows are generated from MyLite spatial column, index, table, or SQLite
  activity because MyLite does not expose an SRS dictionary in this slice;
- successful reads introduce no warnings;
- `ROW_COUNT()` after a successful `SELECT` remains the existing query value
  `-1`.

## Diagnostics

The feature relies on existing information-schema diagnostics:

- unknown selected columns fail with the current unknown-column diagnostic;
- unsupported expressions, joins, grouping, predicates, and limits retain the
  current information-schema query subset behavior;
- allocation failures use existing MyLite runtime diagnostics.

Successful reads introduce no warnings.

## Tests

Add a focused C runtime test and a MySQL expectation script. Coverage must
include:

- wildcard column labels and empty row set;
- row count and representative predicates over text and integer columns;
- case-insensitive table-name lookup;
- alias projection through the existing information-schema query path;
- `warning_count == 0` and `ROW_COUNT() == -1` after successful reads;
- `INFORMATION_SCHEMA.TABLES` system-view row;
- `INFORMATION_SCHEMA.COLUMNS` metadata for all six columns;
- `USE information_schema` unqualified-table reads;
- stable empty-row behavior after creating MyLite spatial columns and a
  metadata-only spatial index;
- MySQL 8.4.9 runtime observation of the full SRS catalog, stored in the
  expectation script as explicit out-of-scope behavior.

Verification before commit:

```sh
cmake --build --preset dev --target mylite_runtime_information_schema_st_spatial_reference_systems_test
ctest --preset dev -R '^libmylite\.runtime\.(information_schema_st_spatial_reference_systems|information_schema_st_geometry_columns|information_schema_st_units_of_measure|information_schema_static_catalogs|builtin_schema_table_directory)$' --output-on-failure
packages/libmylite/tests/mysql_baseline_information_schema_st_spatial_reference_systems_expectations.sh
git diff --check
cmake --workflow --preset check
```
