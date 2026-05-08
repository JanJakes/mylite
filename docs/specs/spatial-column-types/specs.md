# Spatial column types

## Scope

This feature adds the first metadata/storage slice for MySQL spatial column
types in supported `CREATE TABLE` paths:

- `GEOMETRY`
- `POINT`
- `LINESTRING`
- `POLYGON`
- `MULTIPOINT`
- `MULTILINESTRING`
- `MULTIPOLYGON`
- `GEOMCOLLECTION`
- `GEOMETRYCOLLECTION`, normalized to `geomcollection`
- optional column `SRID n` metadata for the initial supported SRS ids `0` and
  `4326`

The slice covers parser/AST support, type descriptors, catalog persistence,
`INFORMATION_SCHEMA.COLUMNS`, `SHOW COLUMNS`, `SHOW CREATE TABLE`, and
table-backed result metadata. It deliberately does not implement geometry
constructors, geometry validation, spatial functions, `INFORMATION_SCHEMA`
spatial-reference tables, or executable `SPATIAL` indexes.

## Sources

- MySQL 8.4 Reference Manual, Spatial Data Types:
  https://dev.mysql.com/doc/refman/8.4/en/spatial-type-overview.html
- MySQL 8.4 Reference Manual, `CREATE TABLE` statement:
  https://dev.mysql.com/doc/refman/8.4/en/create-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.COLUMNS` table:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-columns-table.html
- MySQL 8.4 Reference Manual, Spatial Reference System Support:
  https://dev.mysql.com/doc/refman/8.4/en/spatial-reference-systems.html
- Observed MySQL 8.4.9 runtime behavior from Docker container
  `mylite-mysql-849`.

This specification is independently authored from official documentation and
observed runtime behavior. It does not copy MySQL grammar or implementation
sources.

## MySQL 8.4.9 behavior summary

MySQL accepts the spatial type names as column types. `GEOMETRYCOLLECTION` and
`GEOMCOLLECTION` both display as `geomcollection` in `SHOW COLUMNS`,
`SHOW CREATE TABLE`, and `INFORMATION_SCHEMA.COLUMNS`.

For nullable columns without an explicit default, `SHOW CREATE TABLE` renders
`DEFAULT NULL`. For `POINT SRID 4326`, MySQL renders the SRID comment form
before the default clause:

```sql
`p` point /*!80003 SRID 4326 */ DEFAULT NULL
```

For `POINT NOT NULL SRID 4326`, MySQL renders:

```sql
`p` point NOT NULL /*!80003 SRID 4326 */
```

`INFORMATION_SCHEMA.COLUMNS` reports `DATA_TYPE` and `COLUMN_TYPE` as the
lowercase display type. Character length, octet length, numeric precision,
numeric scale, datetime precision, character set, and collation are `NULL`.
`SRS_ID` is `NULL` unless an `SRID` attribute is present.

Table-backed result metadata for every spatial subtype reports MySQL protocol
field type `GEOMETRY`, binary collation id `63`, length `4294967295`, decimals
`0`, and flags `BLOB` and `BINARY`. `NOT NULL` adds `NOT_NULL`, and a required
no-default spatial column adds `NO_DEFAULT_VALUE`.

MySQL accepts `SRID 0` and `SRID 4326` on spatial columns. Invalid SRID syntax,
such as a missing or negative value, is a parse error. Applying `SRID` to a
non-spatial column raises error 1221, "Incorrect usage of SRID and
non-geometry column". A syntactically valid but unknown SRID such as `999999`
raises error 3548.

## MyLite behavior

### Parser and AST

MyLite accepts all listed spatial type names as column types and records a
dedicated AST column type for each normalized subtype. `GEOMETRYCOLLECTION`
normalizes to the `GEOMCOLLECTION` AST and descriptor type because MySQL
displays the shorter spelling.

Independently authored Lemon-shape grammar:

```lemon
column_type ::= spatial_column_type.
spatial_column_type ::= GEOMETRY.
spatial_column_type ::= POINT.
spatial_column_type ::= LINESTRING.
spatial_column_type ::= POLYGON.
spatial_column_type ::= MULTIPOINT.
spatial_column_type ::= MULTILINESTRING.
spatial_column_type ::= MULTIPOLYGON.
spatial_column_type ::= GEOMCOLLECTION.
spatial_column_type ::= GEOMETRYCOLLECTION.

column_attribute ::= SRID INTEGER.
```

Spatial type names and `SRID` remain available as identifiers through the
existing keyword fallback behavior.

### Type descriptor

The spatial descriptor records:

- MySQL-visible lowercase `DATA_TYPE`
- canonical `COLUMN_TYPE`, with `GEOMETRYCOLLECTION` as `geomcollection`
- a dedicated `is_spatial` marker and normalized spatial subtype
- no character, numeric, or datetime metadata
- no string-family marker
- no fixed MyLite logical storage width

The descriptor rejects length, precision, scale, display width, signedness,
`ZEROFILL`, national, binary/byte, and character-set/collation attributes.

### Runtime storage and metadata

Supported `CREATE TABLE` paths persist spatial catalog metadata and use SQLite
`BLOB` physical storage. The first slice treats stored values as opaque binary
payloads; geometry validation and constructor encoding are deferred.

`INFORMATION_SCHEMA.COLUMNS`, `SHOW COLUMNS`, and `SHOW CREATE TABLE` derive
from the catalog descriptor. SRID values are stored in the column catalog
`srs_id` field and are rendered in `SHOW CREATE TABLE` as MySQL versioned SRID
comments.

Table-backed result metadata reports:

- `MYLITE_FIELD_TYPE_GEOMETRY`
- binary charset/collation id `63`
- declared length `4294967295`
- decimals `0`
- `BLOB | BINARY` flags, plus normal nullability/default flags

### SRID validation

MyLite accepts `SRID 0` and `SRID 4326` in this slice. Other syntactically
valid SRS ids fail with MySQL error 3548 until a broader spatial reference
system catalog exists. Applying `SRID` to a non-spatial column fails with MySQL
error 1221 before catalog mutation.

## Deferred behavior

- spatial constructors and WKB/WKT parsing
- geometry validity checks
- coordinate accessors and spatial predicates
- `ST_SRID()` and broader spatial scalar functions
- `INFORMATION_SCHEMA.ST_GEOMETRY_COLUMNS`
- `INFORMATION_SCHEMA.ST_SPATIAL_REFERENCE_SYSTEMS`
- `CREATE SPATIAL REFERENCE SYSTEM` / `DROP SPATIAL REFERENCE SYSTEM`
- executable `SPATIAL` indexes and optimizer integration
- full SRS registry and geographic/cartesian behavior
- storage-engine-specific SRID/index requirements
- `ALTER TABLE` value conversion for existing spatial columns

## Tests

Runtime expectations were verified against MySQL 8.4.9 for:

- `CREATE TABLE` with every spatial subtype
- `GEOMETRYCOLLECTION` display normalization
- nullable and `NOT NULL` `SRID` columns
- `INFORMATION_SCHEMA.COLUMNS`
- `SHOW COLUMNS`
- `SHOW CREATE TABLE`
- table-backed result metadata
- non-spatial `SRID` diagnostics
- unknown-SRID diagnostics
- invalid SRID syntax

MyLite coverage includes column descriptor tests, parser AST and rejection
tests, runtime metadata assertions for the same catalog surfaces, and runtime
diagnostics for the covered SRID error cases.
