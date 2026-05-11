# Baseline Secondary Index Lifecycle Specification

## Summary

This phase adds the first non-primary index slice for persistent MyLite base
tables: descriptor-owned, single-column, nonunique secondary indexes declared
inside `CREATE TABLE`. The goal is to accept the common MySQL DDL emitted by
applications, preserve index metadata through descriptor operations, render it
through `SHOW CREATE TABLE`, `SHOW INDEX`, and the limited
`INFORMATION_SCHEMA.STATISTICS` surface, and create matching physical SQLite
indexes so SQLite can use them for compatible scans.

This is intentionally not full MySQL index support. The slice does not add
standalone `CREATE INDEX`, `ALTER TABLE ADD/DROP/RENAME INDEX`, unique indexes,
foreign keys, prefix indexes, invisible indexes, descending indexes, expression
indexes, optimizer hints, or MySQL optimizer-plan guarantees.

## Compatibility Sources

Normative behavior comes from official MySQL 8.4 documentation and observed
MySQL 8.4.9 runtime probes:

- MySQL 8.4 Reference Manual, `CREATE TABLE` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/create-table.html>
- MySQL 8.4 Reference Manual, `SHOW INDEX` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/show-index.html>
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.STATISTICS` table:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-statistics-table.html>
- Runtime expectations are recorded in
  `packages/libmylite/tests/mysql_baseline_secondary_index_lifecycle_expectations.sh`
  and must pass against a MySQL 8.4.9 server.

The `CREATE TABLE` manual defines table create definitions that include
`INDEX`/`KEY` entries and key parts; it also documents that unnamed indexes use
the first key column name with numeric suffixes when needed, and that
`PRIMARY` is reserved for the primary key name. `SHOW INDEX` and
`INFORMATION_SCHEMA.STATISTICS` define the index metadata columns this slice
renders for admitted descriptors.

## Ownership Boundaries

- Public API: no new public ABI. Applications continue to use `mylite_open()`,
  `mylite_execute()`, and existing result accessors.
- Statement context: no new session state. Existing selected/default schema and
  diagnostics rules apply.
- Parser/AST: admits a narrow table-level secondary-index grammar in
  `CREATE TABLE` and represents it separately from primary-key descriptors.
- Analyzer/planner/runtime: resolves index names and key columns against
  MyLite's planned column descriptors before any SQLite SQL is generated.
- Catalog: MyLite catalog descriptors remain authoritative for logical indexes;
  SQLite schema text is only a physical execution artifact.
- Result builder: existing non-row DDL result conventions remain unchanged.
- Storage/VFS: `.mylite` preamble and shifted SQLite payload invariants are
  unchanged.
- SQLite physical storage: MyLite creates ordinary SQLite indexes on stable
  physical table and column identifiers. Compatibility rules and diagnostics
  stay in MyLite.

## Supported Syntax

For this phase, the admitted grammar is:

```sql
CREATE TABLE table_name (
  column_definition,
  ...,
  [KEY | INDEX] [index_name] (column_name)
) [supported_table_options]
```

Details:

- persistent base tables only;
- table-level secondary index declarations only;
- `KEY` and `INDEX` are synonyms;
- optional unqualified index name;
- exactly one unqualified key column;
- key column must name an existing descriptor column in the same `CREATE TABLE`;
- admitted key columns may be current descriptor-supported integer-family,
  exact `DECIMAL`, canonical `DATE`, `CHAR`, `VARCHAR`, or bare `TEXT` family
  columns, except bare `TEXT` family indexes require prefix support and are
  rejected for now;
- no index prefix length, `ASC`/`DESC`, `USING`, `COMMENT`, visibility,
  `KEY_BLOCK_SIZE`, parser attributes, engine attributes, expression key parts,
  functional indexes, `FULLTEXT`, `SPATIAL`, `UNIQUE`, named constraints,
  foreign keys, or check constraints.

MyLite Lemon-syntax sketch:

```lemon
create_table_item ::= column_definition.
create_table_item ::= primary_key_definition.
create_table_item ::= secondary_index_definition.

secondary_index_definition ::= secondary_index_keyword index_name_opt
                               LPAREN secondary_index_part RPAREN.
secondary_index_keyword ::= KEY.
secondary_index_keyword ::= INDEX.
index_name_opt ::= .
index_name_opt ::= identifier.
secondary_index_part ::= identifier.
```

Unsupported forms remain syntax errors when they are not admitted by the parser,
or deterministic MyLite runtime errors when the parser accepts enough structure
to provide a clearer compatibility diagnostic.

## Name Resolution

The target table uses existing schema resolution:

- unqualified target names use the selected/default schema;
- schema-qualified target names use the explicit schema;
- missing default schema, unknown schema, reserved `_mylite_*` names, and
  existing table conflicts use existing diagnostics.

Index names and column names are resolved within the planned table descriptor:

- index names use the current descriptor identifier policy and capacity;
- explicit index names must be unique within the table and must not be
  `PRIMARY` case-insensitively;
- unnamed indexes derive from the first key column name, using `_2`, `_3`, ...
  suffixes when needed, matching observed MySQL naming for the admitted subset;
- key column names are unqualified and resolve against planned descriptor
  columns; unknown columns fail before catalog or physical SQLite mutation;
- duplicate column names in the table continue to fail through existing column
  descriptor validation.

## Descriptor and Catalog Model

The existing catalog already has index and index-column tables. This phase
extends that model from primary-only to primary plus nonunique secondary
indexes:

- add a secondary index kind to the internal index-kind enum;
- migrate/catalog-initialize `_mylite_catalog_indexes.kind` to admit both
  primary and secondary kinds;
- retain `is_unique` for primary and future unique indexes; this slice stores
  secondary indexes with `is_unique = 0`;
- store one index-column row per admitted secondary index with ordinal position
  `1`;
- do not store prefix length, direction, visibility, comments, parser clauses,
  or expression text yet.

The catalog generation, descriptor versions, and SQLite schema generation update
on successful DDL. Failed DDL must leave no catalog rows, no physical table, and
no physical index.

`CREATE TABLE ... LIKE` clones secondary index descriptors for supported source
tables and creates new physical indexes with new stable physical names. The
clone preserves logical index names and key-column mapping by ordinal/name;
like existing auto-increment cloning, counters and physical names are target
specific.

`CREATE TABLE ... SELECT` continues not to copy indexes or constraints.
`DROP TABLE`, `RENAME TABLE`, `TRUNCATE`, row DML, and reopen must preserve the
existing behavior. Physical table/index names remain stable internal names and
are not shown to users.

## Physical SQLite Handling

For each admitted secondary index, MyLite generates a standard SQLite index:

```sql
CREATE INDEX "_mylite_user_index_<index_id>"
ON "_mylite_user_table_<table_id>" ("physical_column_name");
```

Rules:

- quote every generated identifier;
- generate SQL only from descriptors and stable physical names;
- create physical secondary indexes after the physical table exists and before
  committing the catalog mutation;
- on failure, roll back the catalog mutation and drop any physical artifacts the
  create path already produced;
- use normal SQLite index support through public SQLite APIs; no SQLite fork
  patch is required;
- no query rewriting is needed for correctness. Existing descriptor-driven
  `SELECT`, `UPDATE`, and `DELETE` continue to generate ordinary SQLite SQL;
  SQLite may choose the physical index where applicable, but MyLite does not
  expose optimizer guarantees in this slice.

## Introspection

### `SHOW CREATE TABLE`

For admitted secondary indexes, append one line per descriptor after the primary
key line, preserving MySQL's observed create-definition order for the supported
subset: columns first, then primary key if present, then secondary indexes in
logical definition order. Render:

```sql
KEY `index_name` (`column_name`)
```

No `USING`, comments, visibility, prefix length, or direction is rendered in
this slice.

### `SHOW INDEX`

`SHOW INDEX`, `SHOW INDEXES`, and `SHOW KEYS` render one row per admitted index
column. For secondary indexes:

- `Table`: logical table name;
- `Non_unique`: `1`;
- `Key_name`: logical index name;
- `Seq_in_index`: `1`;
- `Column_name`: logical column name;
- `Collation`: `A`;
- `Cardinality`: `0` for this baseline, matching the existing primary-key
  placeholder strategy;
- `Sub_part`: `NULL`;
- `Packed`: `NULL`;
- `Null`: `YES` if the descriptor column is nullable, otherwise empty string;
- `Index_type`: `BTREE`;
- `Comment`, `Index_comment`: empty strings;
- `Visible`: `YES`;
- `Expression`: `NULL`.

Primary-key rows keep existing behavior. `SHOW INDEX` ordering follows MySQL's
visible output shape for this subset: primary key first, then secondary indexes
in descriptor order, with single-column indexes having only sequence `1`.

### `INFORMATION_SCHEMA.STATISTICS`

This phase adds limited queryable `INFORMATION_SCHEMA.STATISTICS` rows for
supported system views and MyLite base-table index descriptors. It uses the
existing limited information-schema query engine for projections, aliases,
`WHERE`, `ORDER BY`, `LIMIT`, and `COUNT(*)`.

The base-table secondary-index row values mirror `SHOW INDEX` and the MySQL
column names:

- `TABLE_CATALOG`: `def`;
- `TABLE_SCHEMA`, `TABLE_NAME`: logical descriptor names;
- `NON_UNIQUE`: `1` for secondary indexes, `0` for primary keys;
- `INDEX_SCHEMA`: table schema name;
- `INDEX_NAME`: logical index name;
- `SEQ_IN_INDEX`: `1`;
- `COLUMN_NAME`: logical column name;
- `COLLATION`: `A`;
- `CARDINALITY`: `0`;
- `SUB_PART`, `PACKED`, `EXPRESSION`: `NULL`;
- `NULLABLE`: `YES` for nullable columns or empty string for non-null columns;
- `INDEX_TYPE`: `BTREE`;
- `COMMENT`, `INDEX_COMMENT`: empty strings;
- `IS_VISIBLE`: `YES`.

System rows for `information_schema.STATISTICS` are included in
`INFORMATION_SCHEMA.TABLES` and `INFORMATION_SCHEMA.COLUMNS` like the existing
system-view rows.

## Diagnostics

Provide deterministic diagnostics for:

- syntax errors and unsupported grammar;
- missing default schema, unknown schema, unknown table, and reserved table
  names through existing create-table paths;
- duplicate explicit index names with MySQL error `1061` / SQLSTATE `42000`;
- explicit index name `PRIMARY` with MySQL error `1280` / SQLSTATE `42000`;
- unknown key columns with MySQL error `1072` / SQLSTATE `42000`;
- bare `TEXT` family key columns without prefix length with MySQL error `1170`
  / SQLSTATE `42000`;
- unsupported key prefix lengths, non-string key prefixes, key direction,
  `USING`, options, unique/fulltext/spatial/foreign/check constraints,
  expression key parts, multiple key parts, named constraints, and standalone
  index DDL with MyLite-specific unsupported or existing syntax diagnostics;
- physical SQLite DDL failures, catalog failures, allocation failures, and
  public API misuse through existing runtime paths.

## Tests

Add fast C tests under `packages/libmylite/tests/`, preferably a new
`runtime_secondary_index_lifecycle` binary, and register it with CTest as
`libmylite.runtime.secondary_index_lifecycle`.

Coverage:

- parser acceptance for `KEY name (col)`, `INDEX name (col)`, unnamed
  `KEY (col)`, and coexistence with primary key and current table options;
- parser or runtime rejection for unique/fulltext/spatial/foreign/check/named
  constraint forms, prefix lengths, directions, `USING`, comments/options,
  expression parts, qualified columns, and multiple key parts;
- successful secondary indexes on admitted integer, decimal, date, char, and
  varchar columns;
- deterministic rejection for bare text-family columns until prefix support is
  implemented;
- explicit duplicate index names, generated unnamed suffixes, and explicit
  `PRIMARY` name rejection;
- `SHOW CREATE TABLE`, `SHOW INDEX`, and `INFORMATION_SCHEMA.STATISTICS`
  metadata;
- `CREATE TABLE ... LIKE` cloning and `CREATE TABLE ... SELECT` omission;
- persistence after close/reopen;
- drop/rename/truncate/DML compatibility;
- physical `.mylite` preamble unchanged;
- independent file-backed handles with independent index descriptors;
- zero-initialized cleanup for new planner/catalog objects;
- existing parser/runtime/catalog/DDL/DML/introspection tests still pass.

`packages/libmylite/tests/mysql_baseline_secondary_index_lifecycle_expectations.sh`
records MySQL 8.4.9 expected metadata and diagnostics for the admitted and
explicitly deferred surface.

## Compatibility Documentation

Update only the exact supported subset:

- `COMPATIBILITY.md` rows for `CREATE TABLE`, indexes, `SHOW INDEX`, and
  `INFORMATION_SCHEMA`;
- `docs/compatibility/sql-table-ddl.md` for secondary indexes in `CREATE TABLE`
  and cloning behavior;
- `docs/compatibility/sql-indexes-constraints.md` for the limited secondary
  index lifecycle;
- `docs/compatibility/sql-show-statements.md` for secondary-index rows;
- `docs/compatibility/metadata-information-schema.md` for limited
  `STATISTICS` support;
- type docs only to mention that supported non-`TEXT` descriptors may appear in
  nonunique secondary indexes, without claiming full index semantics.

Do not overclaim unique indexes, prefix indexes, text keys, full optimizer
behavior, standalone index DDL, `ALTER TABLE` index DDL, foreign keys, check
constraints, generated columns, invisible indexes, descending indexes, or
functional indexes.
