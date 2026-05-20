# Baseline Spatial Index Metadata Specification

## Summary

This phase adds a deliberately narrow spatial metadata slice for persistent
MyLite base tables:

```sql
CREATE TABLE t (
  g GEOMETRY [NULL | NOT NULL],
  p POINT NOT NULL,
  SPATIAL KEY spatial_g (g)
)

ALTER TABLE t ADD SPATIAL INDEX spatial_p (p)
CREATE SPATIAL INDEX spatial_p ON t (p)
```

The goal is to accept schema-import DDL that declares MySQL spatial columns and
spatial indexes, preserve descriptor metadata, and expose MySQL-shaped metadata
through existing `SHOW` and limited `INFORMATION_SCHEMA` surfaces. This is not
spatial search. It does not add geometry constructors, coordinate validation,
SRID enforcement, R-tree storage, optimizer behavior, or spatial functions.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing index metadata specifications:
  `docs/specs/baseline-fulltext-index-metadata/specs.md`,
  `docs/specs/baseline-add-fulltext-indexes/specs.md`,
  `docs/specs/baseline-index-options-metadata/specs.md`
- MySQL 8.4 Reference Manual, spatial data types:
  <https://dev.mysql.com/doc/refman/8.4/en/spatial-types.html>
- MySQL 8.4 Reference Manual, spatial type overview:
  <https://dev.mysql.com/doc/refman/8.4/en/spatial-type-overview.html>
- MySQL 8.4 Reference Manual, creating spatial columns:
  <https://dev.mysql.com/doc/refman/8.4/en/creating-spatial-columns.html>
- MySQL 8.4 Reference Manual, creating spatial indexes:
  <https://dev.mysql.com/doc/refman/8.4/en/creating-spatial-indexes.html>
- MySQL 8.4 Reference Manual, `CREATE INDEX` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/create-index.html>
- MySQL 8.4 Reference Manual, `SHOW INDEX` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/show-index.html>
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.STATISTICS` table:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-statistics-table.html>
- Observed MySQL 8.4.9 runtime behavior recorded by
  `packages/libmylite/tests/mysql_baseline_spatial_index_metadata_expectations.sh`.

This specification is independently authored from official MySQL 8.4
documentation, observed MySQL 8.4.9 behavior, public SQLite APIs, and existing
MyLite source. It does not copy MySQL, MariaDB, Percona, SQLite implementation
internals, or restrictively licensed implementation sources.

## MySQL 8.4.9 Observations

Runtime probes establish:

- `GEOMETRY`, `POINT`, `LINESTRING`, `POLYGON`, `MULTIPOINT`,
  `MULTILINESTRING`, `MULTIPOLYGON`, and `GEOMETRYCOLLECTION` are accepted as
  column types. `SHOW CREATE TABLE` renders `GEOMETRYCOLLECTION` as
  `geomcollection`.
- Nullable spatial columns render `DEFAULT NULL`; non-null spatial columns
  render no default clause.
- Explicit `DEFAULT NULL` is accepted for nullable spatial columns. A non-NULL
  default fails with `1101 / 42000`, and `NOT NULL DEFAULT NULL` fails with
  `1067 / 42000`.
- `SPATIAL KEY name (column)`, `SPATIAL INDEX name (column)`,
  `ALTER TABLE ... ADD SPATIAL INDEX name (column)`, and
  `CREATE SPATIAL INDEX name ON table (column)` succeed for one `NOT NULL`
  spatial column.
- Ordinary nonunique `KEY name (spatial_column)`,
  `ALTER TABLE ... ADD KEY name (spatial_column)`, and
  `CREATE INDEX name ON table (spatial_column)` are accepted and render as
  `SPATIAL KEY`.
- Spatial indexes are nonunique. `UNIQUE` or `PRIMARY KEY` over a spatial
  column fails with `3728 / HY000` and
  `Spatial indexes can't be primary or unique indexes.`
- Spatial index columns must be `NOT NULL`; otherwise MySQL returns
  `1252 / 42000`, `All parts of a SPATIAL index must be NOT NULL`.
- Spatial indexes contain exactly one key part. Multiple key parts fail with
  `1070 / 42000`, `Too many key parts specified; max 1 parts allowed`.
- Prefix lengths are rejected with `1089 / HY000`.
- Explicit key-part `ASC` or `DESC` fails with `1221 / HY000` and
  `Incorrect usage of spatial/fulltext/hash index and explicit index order`.
- `USING BTREE`, `USING HASH`, and observed `USING RTREE` forms around
  `SPATIAL INDEX` are syntax errors in the admitted MySQL 8.4.9 forms.
- `COMMENT`, `VISIBLE`, and `INVISIBLE` are accepted as trailing spatial index
  options. `SHOW CREATE TABLE`, `SHOW INDEX`, and
  `INFORMATION_SCHEMA.STATISTICS` expose them like ordinary supported index
  metadata.
- Because this slice does not admit SRID attributes, each successful spatial
  index creation reports warning `3674 / HY000` with MySQL's optimizer/SRID
  message.
- `SHOW INDEX` and `INFORMATION_SCHEMA.STATISTICS` report
  `Index_type` / `INDEX_TYPE = SPATIAL`, `Collation` / `COLLATION = A`, and
  `Sub_part` / `SUB_PART = 32` for the observed spatial column types.
- Nullable spatial DML stores omitted or `DEFAULT` values as `NULL`.
  `NOT NULL` spatial columns without an explicit value fail with
  `1364 / HY000`, `Field 'name' doesn't have a default value`.

## Scope

Supported:

- persistent MyLite base tables;
- spatial column descriptors for `GEOMETRY`, `POINT`, `LINESTRING`, `POLYGON`,
  `MULTIPOINT`, `MULTILINESTRING`, `MULTIPOLYGON`, and
  `GEOMETRYCOLLECTION`;
- optional `NULL`, `NOT NULL`, and explicit `DEFAULT NULL` on spatial columns;
- omitted/default values for nullable spatial columns storing SQL `NULL`;
- strict diagnostics for omitted, explicit `DEFAULT`, or explicit `NULL` into
  non-null spatial columns without a usable default;
- table-level `SPATIAL [KEY | INDEX] [index_name] (column_name)` in
  persistent `CREATE TABLE`;
- single-action `ALTER TABLE table_name ADD SPATIAL [KEY | INDEX] [index_name]
  (column_name)`;
- standalone `CREATE SPATIAL INDEX index_name ON table_name (column_name)`;
- implicit spatial descriptors for nonunique ordinary index forms over one
  spatial column where MySQL renders the result as `SPATIAL KEY`;
- trailing `COMMENT 'string'`, `VISIBLE`, and `INVISIBLE` spatial index
  options through the existing index-option metadata path;
- descriptor-backed `SHOW COLUMNS`, `SHOW CREATE TABLE`, `SHOW INDEX`,
  `CREATE TABLE ... LIKE`, `ALTER TABLE ... DROP INDEX|KEY`,
  `ALTER TABLE ... RENAME INDEX|KEY`, standalone `DROP INDEX`, and limited
  `INFORMATION_SCHEMA.COLUMNS` / `STATISTICS` metadata;
- no-result DDL result shape with zero affected rows and warning `3674` for
  each supported spatial index created without SRID metadata;
- file-backed persistence, independent handles, and `.mylite` preamble
  preservation.

Deferred:

- actual geometry value parsing or validation beyond SQL `NULL`;
- WKB/WKT input, `ST_GeomFromText()`, `ST_AsText()`, spatial predicates,
  spatial casts, and spatial functions;
- SRID attributes and `INFORMATION_SCHEMA.ST_GEOMETRY_COLUMNS`;
- R-tree or other spatial physical indexes;
- spatial optimizer behavior;
- temporary spatial columns or indexes;
- `ALTER TABLE ... ADD|MODIFY|CHANGE COLUMN` spatial column definitions;
- spatial primary keys or unique indexes;
- multi-column spatial indexes, prefix parts, ordered spatial key parts,
  functional parts, expression parts, table-qualified parts, parser plugins,
  `USING` clauses, `KEY_BLOCK_SIZE`, engine attributes, standalone
  `ALGORITHM` / `LOCK`, and broader online-DDL semantics;
- non-`NULL` spatial defaults, generated columns, constraints over spatial
  values, and full protocol-grade geometry metadata.

## Ownership Boundaries

- Public API: no new ABI. Applications continue to use `mylite_execute()` and
  existing result/diagnostic accessors.
- Statement context: owns diagnostics reset, warning count, affected rows,
  `ROW_COUNT()`, and cleanup. This phase does not add session state.
- Lexer/parser/AST: admits spatial type and spatial index syntax and stores
  nodes without descriptor or SQLite access.
- Analyzer/planner/runtime: resolves schemas, tables, index names, spatial
  columns, nullability, and options from MyLite descriptors before any physical
  SQL is considered.
- Catalog: MyLite descriptors are authoritative. Spatial columns are logical
  column descriptors, and spatial indexes are logical index descriptors.
- Result builder/introspection: renders `SHOW CREATE TABLE`, `SHOW COLUMNS`,
  `SHOW INDEX`, and information-schema rows from descriptors only.
- Storage/VFS: `.mylite` preamble and shifted SQLite payload invariants are
  unchanged.
- SQLite physical storage: spatial column values use the existing row-table
  storage path with SQL `NULL` for this slice. Spatial index descriptors do not
  create SQLite R-tree, FTS, trigger, or ordinary b-tree indexes.

## Supported SQL Grammar

MyLite admits:

```sql
spatial_type:
  GEOMETRY
  POINT
  LINESTRING
  POLYGON
  MULTIPOINT
  MULTILINESTRING
  MULTIPOLYGON
  GEOMETRYCOLLECTION

CREATE TABLE table_name (
  column_name spatial_type [NULL | NOT NULL] [DEFAULT NULL],
  SPATIAL [KEY | INDEX] [index_name] (column_name) [spatial_index_option]...
)

ALTER TABLE table_name ADD SPATIAL [KEY | INDEX] [index_name]
  (column_name) [spatial_index_option]...

CREATE SPATIAL INDEX index_name ON table_name
  (column_name) [spatial_index_option]...

spatial_index_option:
  COMMENT 'string'
  VISIBLE
  INVISIBLE
```

MyLite Lemon-syntax sketch:

```lemon
column_type ::= spatial_type.
spatial_type ::= GEOMETRY.
spatial_type ::= POINT.
spatial_type ::= LINESTRING.
spatial_type ::= POLYGON.
spatial_type ::= MULTIPOINT.
spatial_type ::= MULTILINESTRING.
spatial_type ::= MULTIPOLYGON.
spatial_type ::= GEOMETRYCOLLECTION.

table_constraint ::= spatial_index_definition.

spatial_index_definition ::=
    SPATIAL spatial_index_keyword_opt index_name_opt
    LPAREN secondary_index_part_list RPAREN spatial_index_option_list_opt.

spatial_index_keyword_opt ::= .
spatial_index_keyword_opt ::= KEY.
spatial_index_keyword_opt ::= INDEX.

spatial_index_option_list_opt ::= .
spatial_index_option_list_opt ::= spatial_index_option_list.
spatial_index_option_list ::= spatial_index_option.
spatial_index_option_list ::= spatial_index_option_list spatial_index_option.
spatial_index_option ::= COMMENT STRING.
spatial_index_option ::= VISIBLE.
spatial_index_option ::= INVISIBLE.

create_index_statement ::=
    CREATE SPATIAL INDEX identifier ON table_name
    LPAREN secondary_index_part_list RPAREN spatial_index_option_list_opt.
```

The implementation may reuse the existing secondary key-part list AST but must
reject all spatial prefixes, explicit directions, multi-part lists, and
non-spatial columns before catalog mutation.

## Resolution Semantics

Target table resolution follows existing policy:

- unqualified table names require the selected/default schema;
- schema-qualified table names use the explicit schema and do not require a
  selected schema;
- unknown schemas fail with `1049 / 42000`;
- unknown persistent tables fail with `1146 / 42S02`;
- reserved `_mylite_*` schema/table names are rejected before generated SQL;
- information-schema write targets use the existing access-denied policy.

Index names are table-local:

- explicit names must not collide case-insensitively with existing primary,
  unique, secondary, fulltext, or spatial descriptors;
- duplicate explicit names fail with `1061 / 42000`;
- `PRIMARY` fails with `1280 / 42000`;
- omitted table-level or alter-added names derive from the first key-part
  column and append `_2`, `_3`, ... until no descriptor name collides.

Column names are descriptor-owned:

- spatial key parts must be existing unqualified descriptor columns;
- unknown columns fail with `1072 / 42000`;
- non-spatial descriptor columns in explicit `SPATIAL` forms fail with
  `1687 / 42000`, `A SPATIAL index may only contain a geometrical type column`;
- nonunique ordinary indexes over one spatial descriptor column are normalized
  to `SPATIAL` descriptors;
- unique or primary indexes over spatial columns fail with `3728 / HY000`;
- spatial indexed columns must be `NOT NULL`;
- descriptor identifier comparison follows the current catalog
  case-insensitive policy.

## Spatial Column Descriptors

The catalog stores a spatial type family and concrete spatial subtype. The
subtype controls display text:

| MySQL input | Display type |
| --- | --- |
| `GEOMETRY` | `geometry` |
| `POINT` | `point` |
| `LINESTRING` | `linestring` |
| `POLYGON` | `polygon` |
| `MULTIPOINT` | `multipoint` |
| `MULTILINESTRING` | `multilinestring` |
| `MULTIPOLYGON` | `multipolygon` |
| `GEOMETRYCOLLECTION` | `geomcollection` |

Spatial columns are binary logical values for this slice:

- nullable columns have effective default `NULL` and render `DEFAULT NULL`;
- non-null columns have no explicit default unless a future non-NULL spatial
  value subset is added;
- explicit `DEFAULT NULL` is accepted only when the column is nullable;
- non-NULL defaults are rejected with the existing MySQL-shaped BLOB/TEXT/JSON
  default diagnostic extended to include `GEOMETRY`;
- physical row storage uses the current generated table with a nullable SQLite
  BLOB-compatible column; no non-NULL spatial payload is generated here.

## Spatial Index Descriptors

A spatial index descriptor stores:

- logical index name;
- table id;
- `kind = SPATIAL`;
- `is_unique = false`;
- one index-column row with ordinal position `1`;
- no physical SQLite index object;
- no prefix length;
- ascending collation marker for metadata (`A`);
- current index comment and visibility fields from the existing option path.

Creating a spatial index appends warning `3674 / HY000`, using MySQL's message
that the spatial index will not be used by the optimizer because the column has
no SRID attribute. This warning is intentionally retained until MyLite supports
SRID-restricted spatial columns.

`CREATE TABLE ... LIKE` clones spatial columns and spatial indexes without
re-emitting the no-SRID spatial index warning for cloned descriptors, matching
MySQL 8.4.9. `CREATE TABLE ... SELECT` continues to omit indexes and should not
infer spatial column types from arbitrary expressions. `ALTER TABLE ... DROP INDEX|KEY`, standalone
`DROP INDEX`, `ALTER TABLE ... RENAME INDEX|KEY`, and `ALTER TABLE ... ALTER
INDEX ... VISIBLE|INVISIBLE` operate on spatial descriptors through the same
catalog paths used by current secondary and fulltext indexes.

## Introspection

### `SHOW CREATE TABLE`

Spatial columns render in column order. Spatial index descriptors render in
index descriptor order with comments and invisible marker where present:

```sql
`g` geometry DEFAULT NULL
`p` point NOT NULL
SPATIAL KEY `sg` (`g`) COMMENT 'geo' /*!80000 INVISIBLE */
```

### `SHOW COLUMNS`

Spatial columns report:

- `Type`: display type from the descriptor;
- `Null`: `YES` for nullable columns, empty string for non-null columns;
- `Key`: `MUL` when the column is the first part of a spatial index, otherwise
  the existing primary/unique/nonunique precedence rules;
- `Default`: SQL `NULL` for nullable columns and non-null no-default columns;
- `Extra`: empty string for this slice.

### `SHOW INDEX`

Spatial indexes render one row:

- `Non_unique`: `1`;
- `Key_name`: logical index name;
- `Seq_in_index`: `1`;
- `Column_name`: logical column name;
- `Collation`: `A`;
- `Cardinality`: `0` baseline placeholder;
- `Sub_part`: `32`;
- `Packed`: SQL `NULL`;
- `Null`: empty string because indexed spatial columns must be `NOT NULL`;
- `Index_type`: `SPATIAL`;
- `Comment`: empty string;
- `Index_comment`: descriptor comment;
- `Visible`: `YES` / `NO`;
- `Expression`: SQL `NULL`.

### `INFORMATION_SCHEMA.COLUMNS`

Spatial columns appear in the existing limited descriptor column surface:

- `DATA_TYPE` and `COLUMN_TYPE`: descriptor display type;
- `IS_NULLABLE`: `YES` or `NO`;
- `COLUMN_DEFAULT`: SQL `NULL`;
- `SRS_ID`: SQL `NULL` because SRID attributes are deferred;
- character-set, collation, numeric precision, datetime precision, and
  generation-expression fields use the existing non-character/nullable
  placeholders.

### `INFORMATION_SCHEMA.STATISTICS`

Spatial indexes report:

- `NON_UNIQUE`: `1`;
- `SEQ_IN_INDEX`: `1`;
- `COLUMN_NAME`: logical column name;
- `COLLATION`: `A`;
- `SUB_PART`: `32`;
- `NULLABLE`: empty string;
- `INDEX_TYPE`: `SPATIAL`;
- `COMMENT`: empty string;
- `INDEX_COMMENT`: descriptor comment;
- `IS_VISIBLE`: `YES` / `NO`;
- `EXPRESSION`: SQL `NULL`.

`INFORMATION_SCHEMA.ST_GEOMETRY_COLUMNS` remains unsupported and empty/unlisted
according to the existing static metadata policy until SRID-aware spatial
metadata is designed.

## DML Semantics

This slice admits only SQL `NULL`-equivalent spatial row values:

- omitted nullable spatial columns store SQL `NULL`;
- explicit `DEFAULT` for nullable spatial columns stores SQL `NULL`;
- explicit `NULL` for nullable spatial columns stores SQL `NULL`;
- omitted or explicit `DEFAULT` for non-null spatial columns with no default
  fails with `1364 / HY000`;
- explicit `NULL` into non-null spatial columns fails with `3673 / 23000`;
- non-NULL literals, functions, expressions, parameters, and selected values
  targeting spatial columns are unsupported and must fail before SQLite binding
  with a deterministic diagnostic.

`SELECT spatial_column IS NULL`, `SHOW`, and metadata queries may observe the
stored `NULL` values through existing descriptor-backed read paths. Non-NULL
projection, comparison, ordering, grouping, and function evaluation remain
deferred.

## Physical SQLite Handling

No SQLite spatial index is created for `SPATIAL` descriptors:

- SQLite R-tree tables do not implement MySQL geometry storage, SRIDs, or
  optimizer semantics without a broader spatial design.
- A normal SQLite b-tree would be incorrect for spatial search and unnecessary
  for metadata.
- This phase requires no SQLite fork patch.

Generated physical table DDL should continue to quote every identifier and use
stable physical table names. Spatial logical columns may map to a BLOB-affinity
physical column to preserve future byte payload storage.

## Diagnostics

| Case | Diagnostic |
| --- | --- |
| non-spatial column in explicit spatial index | `1687 / 42000`, `A SPATIAL index may only contain a geometrical type column` |
| nullable spatial indexed column | `1252 / 42000`, `All parts of a SPATIAL index must be NOT NULL` |
| more than one spatial key part | `1070 / 42000`, `Too many key parts specified; max 1 parts allowed` |
| spatial prefix key part | `1089 / HY000`, `Incorrect prefix key` message shape |
| explicit spatial key-part order | `1221 / HY000`, `Incorrect usage of spatial/fulltext/hash index and explicit index order` |
| unique or primary spatial index | `3728 / HY000`, `Spatial indexes can't be primary or unique indexes.` |
| successful no-SRID spatial index | warning `3674 / HY000`, spatial index optimizer/SRID message |
| non-NULL spatial default | `1101 / 42000`, `BLOB, TEXT, GEOMETRY or JSON column 'name' can't have a default value` |
| `NOT NULL DEFAULT NULL` spatial column | `1067 / 42000`, `Invalid default value for 'name'` |
| explicit `NULL` into non-null spatial column | `3673 / 23000`, `Column 'name' cannot be null` |
| unknown key column | existing `1072 / 42000` |
| duplicate index name | existing `1061 / 42000` |
| index name `PRIMARY` | existing `1280 / 42000` |
| unsupported `USING` or unsupported grammar | syntax error or existing deterministic unsupported diagnostic |
| non-NULL spatial DML value | `1064 / 42000`, `Spatial DML supports only NULL or DEFAULT values` |
| temporary spatial index | `1064 / 42000`, `Temporary SPATIAL indexes are not yet supported` |
| temporary spatial column | `1064 / 42000`, `Temporary SPATIAL columns are not yet supported` |
| `ALTER TABLE ... ADD COLUMN` spatial definition | `1064 / 42000`, `ALTER TABLE ADD COLUMN does not yet support spatial columns` |
| SQLite/catalog/allocation failure | existing MyLite failure diagnostics and cleanup |

## Tests

Add MySQL-runtime expectation coverage for:

- all admitted spatial column type display names;
- nullable and non-null spatial column `SHOW COLUMNS`,
  `INFORMATION_SCHEMA.COLUMNS`, and `SHOW CREATE TABLE`;
- table-level, alter-added, standalone, and implicit ordinary nonunique spatial
  index creation;
- comments and visibility on spatial indexes;
- `SHOW INDEX` and `INFORMATION_SCHEMA.STATISTICS` rows including `SPATIAL`,
  `A`, `SUB_PART = 32`, comment, and visibility;
- `CREATE TABLE ... LIKE`, drop index, rename index, alter visibility, reopen
  persistence, independent handles, and file-format preamble preservation;
- nullable spatial omitted/`DEFAULT`/`NULL` row values;
- diagnostics for nullable indexed columns, non-spatial key parts, multi-part
  indexes, prefixes, explicit order, unique/primary spatial indexes, unknown
  columns, duplicate names, non-NULL defaults, `NOT NULL DEFAULT NULL`, and
  non-NULL DML values.

Fast C tests should live under `packages/libmylite/tests/` as a new
`runtime_spatial_index_metadata` CTest entry unless nearby fulltext/index tests
provide clearer reuse without reducing readability.

## Compatibility Documentation

Update:

- `COMPATIBILITY.md`;
- `docs/compatibility/sql-indexes-constraints.md`;
- `docs/compatibility/sql-table-ddl.md`;
- `docs/compatibility/sql-show-statements.md`;
- `docs/compatibility/metadata-information-schema.md`;
- `docs/compatibility/type-system-literals-conversion.md`.

The docs must keep limited wording. Do not claim spatial functions, SRID
support, geometry validation, spatial optimizer behavior, or real R-tree
storage.
