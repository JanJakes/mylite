# Baseline Descending Index Key Parts Specification

## Summary

This phase adds descriptor-owned `ASC` / `DESC` key-part metadata to MyLite's
current primary, unique secondary, and nonunique secondary index subsets:

```sql
CREATE TABLE t (id INT NOT NULL, PRIMARY KEY (id DESC))
CREATE TABLE t (a INT, b INT, KEY k (a ASC, b DESC))
ALTER TABLE t ADD KEY k (a DESC)
CREATE INDEX k ON t (name(10) DESC)
```

The goal is to accept common MySQL index definitions that explicitly specify
key-part direction, preserve the direction in durable MyLite descriptors, render
it through `SHOW CREATE TABLE`, `SHOW INDEX`, and limited
`INFORMATION_SCHEMA.STATISTICS`, and generate matching physical SQLite index
terms for new descriptor-owned indexes.

This is not a MySQL optimizer-equivalence slice. MyLite records and renders
index direction and lets SQLite use generated indexes where it can, but MyLite
does not promise MySQL-compatible plan choice, reverse-scan reporting, or
performance characteristics for `ORDER BY`.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing index lifecycle specifications:
  `docs/specs/baseline-primary-key-lifecycle/specs.md`,
  `docs/specs/baseline-secondary-index-lifecycle/specs.md`,
  `docs/specs/baseline-alter-table-add-primary-key/specs.md`,
  `docs/specs/baseline-alter-table-add-index-lifecycle/specs.md`,
  `docs/specs/baseline-create-index-lifecycle/specs.md`,
  `docs/specs/baseline-unique-index-lifecycle/specs.md`,
  `docs/specs/baseline-index-prefix-key-parts/specs.md`,
  `docs/specs/baseline-unique-prefix-index-lifecycle/specs.md`
- MySQL 8.4 Reference Manual, descending indexes:
  <https://dev.mysql.com/doc/refman/8.4/en/descending-indexes.html>
- MySQL 8.4 Reference Manual, `CREATE TABLE` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/create-table.html>
- MySQL 8.4 Reference Manual, `CREATE INDEX` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/create-index.html>
- MySQL 8.4 Reference Manual, `SHOW INDEX` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/show-index.html>
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.STATISTICS` table:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-statistics-table.html>
- Observed MySQL 8.4.9 runtime behavior recorded by
  `packages/libmylite/tests/mysql_baseline_descending_index_key_parts_expectations.sh`.

This specification is independently authored from official MySQL 8.4
documentation, observed MySQL 8.4.9 behavior, public SQLite APIs, and existing
MyLite code. It does not copy MySQL, MariaDB, Percona, SQLite implementation
internals, or other restrictively licensed implementation sources.

## MySQL 8.4.9 Observations

Runtime probes for this phase establish:

- A key part may end with `ASC` or `DESC`; omitted direction defaults to
  ascending.
- `SHOW CREATE TABLE` omits explicit `ASC` key-part markers and renders
  explicit `DESC` markers.
- `SHOW INDEX.Collation` and `INFORMATION_SCHEMA.STATISTICS.COLLATION` report
  `A` for ascending key parts and `D` for descending key parts.
- Primary, unique secondary, and nonunique secondary indexes all accept
  descending ordinary column key parts in the supported InnoDB baseline.
- Composite indexes can mix ascending and descending key parts; for example
  `KEY k (a ASC, b DESC)` renders as ``KEY `k` (`a`,`b` DESC)``.
- Prefix key parts can also specify direction; for example
  `KEY k (v(5) DESC)` renders as ``KEY `k` (`v`(5) DESC)`` and reports
  `SUB_PART = 5` plus `COLLATION = D`.
- `CREATE TABLE ... LIKE` preserves key-part direction metadata.
- `ALTER TABLE ... ADD PRIMARY KEY (a DESC, b ASC)`,
  `ALTER TABLE ... ADD KEY k (a DESC)`, and
  `CREATE INDEX k ON t (a DESC, b ASC)` report `ROW_COUNT() = 0` and
  `@@warning_count = 0` for supported successful forms.
- Duplicate key-part column names still fail regardless of direction with
  `1060 / 42S21`.
- Prefix syntax on non-string key parts still fails with `1089 / HY000`.
- Qualified key parts such as `KEY k (t.a DESC)` are syntax errors in MySQL
  8.4.9 and remain outside MyLite's admitted grammar.

Official MySQL documentation states that descending indexes are supported for
the InnoDB `BTREE` index surface and for all data types for which ascending
indexes are available. It also documents that `ASC` / `DESC` are not supported
for `HASH`, `FULLTEXT`, `SPATIAL`, or multi-valued indexes. Those index classes
remain outside MyLite's current baseline.

## Scope

Supported:

- persistent MyLite base tables;
- create-time table-level primary keys, unique secondary indexes, and nonunique
  secondary indexes in the current supported descriptor subset;
- single-action `ALTER TABLE table_name ADD PRIMARY KEY (key_part[, ...])`;
- single-action `ALTER TABLE table_name ADD INDEX|KEY [name] (key_part[, ...])`
  and `ALTER TABLE table_name ADD UNIQUE [INDEX|KEY] [name] (key_part[, ...])`;
- standalone `CREATE INDEX index_name ON table_name (key_part[, ...])` and
  `CREATE UNIQUE INDEX index_name ON table_name (key_part[, ...])`;
- unqualified descriptor-column key parts;
- optional string prefix lengths where the existing prefix-index slices admit
  them;
- optional `ASC` or `DESC` after the column or prefix key part;
- omitted direction as ascending;
- descriptor-backed `SHOW CREATE TABLE`, `SHOW INDEX`, and limited
  `INFORMATION_SCHEMA.STATISTICS` direction metadata;
- `CREATE TABLE ... LIKE`, reopen persistence, independent handles, index drop,
  DML duplicate enforcement, foreign-key parent/child index matching, and
  auto-increment key checks through existing descriptor paths;
- no-result DDL result shape with `affected_rows == 0` and
  `warning_count == 0` for supported successful forms.

Deferred:

- inline column `PRIMARY KEY` / `UNIQUE` direction syntax, because MySQL does
  not provide a key-part direction position in inline column attributes;
- primary prefix key parts;
- functional, expression, table-qualified, ordinal, invisible, hidden,
  multi-valued, fulltext, spatial, and hash key parts;
- `USING`, comments, visibility, parser attributes, engine attributes,
  algorithms, locks, partitions, generated columns, temporary tables, views,
  optimizer hints, `EXPLAIN`, and optimizer/index-use guarantees;
- MySQL reverse-scan or filesort observability;
- rebuilding old physical SQLite indexes during catalog migration. Existing
  descriptors default to ascending, so old physical indexes remain compatible.

## Ownership Boundaries

- Public API: no new public ABI. Callers continue to use `mylite_execute()`,
  result accessors, diagnostics, and statement context conventions.
- Statement context: owns diagnostics reset, `ROW_COUNT()`, warning count, and
  result cleanup. No new session or transaction state is introduced.
- Lexer/parser/AST: admits optional key-part direction syntax and preserves it
  as AST metadata without consulting descriptors or SQLite.
- Analyzer/planner/runtime: resolves table, index names, columns, prefix
  admissibility, duplicate key parts, and direction values against MyLite
  descriptors before any SQLite SQL is generated.
- Catalog: MyLite index-column descriptors are authoritative for key-part
  direction. SQLite schema text is a physical artifact only.
- Result builder/introspection: `SHOW CREATE TABLE`, `SHOW INDEX`, and limited
  `INFORMATION_SCHEMA.STATISTICS` render direction from descriptors.
- Storage/VFS: `.mylite` preamble and shifted SQLite payload invariants are
  unchanged. The catalog schema version changes inside the SQLite payload only.
- SQLite physical storage: MyLite emits standard SQLite `CREATE INDEX` terms
  with `DESC` where descriptors request descending order. No SQLite fork patch
  is required.

## Supported SQL Grammar

MyLite admits optional direction after the already supported key part:

```sql
key_part ::= column_name [ '(' prefix_length ')' ] [ ASC | DESC ]
```

This grammar is used in the supported table-level primary-key, unique-key,
secondary-key, `ALTER TABLE ... ADD ...`, and standalone `CREATE INDEX` entry
points. `prefix_length` remains a positive decimal integer literal without sign.

MyLite Lemon-syntax sketch:

```lemon
index_key_direction_opt ::= .
index_key_direction_opt ::= ASC.
index_key_direction_opt ::= DESC.

index_key_part ::= identifier index_prefix_length_opt index_key_direction_opt.
index_prefix_length_opt ::= .
index_prefix_length_opt ::= LPAREN integer_literal RPAREN.

secondary_index_part_list ::= index_key_part.
secondary_index_part_list ::= secondary_index_part_list COMMA index_key_part.

primary_key_part_list ::= index_key_part.
primary_key_part_list ::= primary_key_part_list COMMA index_key_part.
```

The parser does not admit parameters, arbitrary expressions, string literals,
floating-point literals, hex literals, bit literals, qualified key columns,
functional key parts, `USING`, index comments, or visibility attributes in this
slice.

## Resolution and Validation

Target table resolution follows existing policy:

- unqualified targets require the selected/default schema;
- schema-qualified targets use the explicit schema;
- unknown schemas fail with `1049 / 42000`;
- unknown tables fail with `1146 / 42S02`;
- reserved `_mylite_*` schema or table names are rejected before physical SQL
  is generated;
- target objects must be persistent base tables for persistent index DDL.

Index names remain table-local and follow existing primary, unique, and
nonunique descriptor rules. Explicit names must not collide case-insensitively
with existing descriptor indexes, and quoted `PRIMARY` remains reserved for
primary keys.

Each key part resolves against MyLite descriptors:

- key-part column names must be unqualified descriptor column names;
- unknown columns fail with `1072 / 42000`;
- current descriptor lookup remains ASCII case-insensitive, matching the
  existing catalog policy;
- duplicate key-part column names fail with `1060 / 42S21` even when directions
  differ;
- full-column key parts keep each existing accepted type set for primary,
  unique, and nonunique indexes;
- prefix key parts keep the existing string-prefix validation and diagnostics;
- omitted direction stores ascending;
- explicit `ASC` stores ascending but is not rendered in `SHOW CREATE TABLE`;
- explicit `DESC` stores descending and is rendered as ` DESC`.

Direction does not alter duplicate-key equality semantics, `NULL` handling,
foreign-key compatibility, auto-increment eligibility, or descriptor-column
type conversion. It is ordering metadata and physical-index term metadata only.

## Descriptor and Catalog Semantics

The catalog gains `sort_direction` metadata on index-column descriptors:

- `1` means ascending;
- `2` means descending;
- new descriptors always store one of these values;
- migration of old descriptors sets `sort_direction = 1`;
- a check constraint rejects any other durable value.

On successful index creation, MyLite writes `sort_direction` for every
`_mylite_catalog_index_columns` row. `CREATE TABLE ... LIKE` clones direction
together with existing index-column metadata. Drops, table rename, reopen,
temporary catalog mirroring, descriptor cache reload, and independent handles
observe the field through the existing index loading paths.

The MyLite file-format preamble version remains unchanged because the change is
inside the SQLite-backed catalog schema, not the outer `.mylite` header. The
catalog schema version and minimum reader version advance so older MyLite builds
do not silently misread descending descriptors as ascending.

## Physical SQLite Handling

For new indexes, MyLite generates physical SQLite index DDL from descriptors and
stable physical names:

```sql
CREATE INDEX "_mylite_user_index_<index_id>"
ON "_mylite_user_table_<table_id>" ("physical_column" DESC)
```

Prefix terms remain expression index terms, with direction appended after the
expression:

```sql
substr("physical_column", 1, 5) COLLATE "utf8mb4_0900_ai_ci" DESC
```

Rules:

- quote every generated SQLite identifier;
- generate SQL only from descriptors and stable physical names;
- append no direction keyword for ascending physical terms unless the local
  builder path already needs it for clarity;
- append `DESC` only for descending descriptors;
- do not append direction in duplicate-validation, foreign-key, DML conflict,
  or equality-probe expressions, because direction is not part of key equality;
- use SQLite public DDL support; no SQLite fork patch is required;
- failed planning or physical DDL must leave no new catalog rows and no new
  physical index.

## Introspection

### `SHOW CREATE TABLE`

For each descriptor key part:

- render `` `column` `` for ascending full-column key parts;
- render `` `column` DESC`` for descending full-column key parts;
- render `` `column`(N)`` for ascending prefix key parts;
- render `` `column`(N) DESC`` for descending prefix key parts.

Explicit `ASC` is normalized away, matching observed MySQL output.

### `SHOW INDEX`

`SHOW INDEX`, `SHOW INDEXES`, and `SHOW KEYS` render one row per descriptor key
part. The `Collation` column is:

- `A` for ascending descriptors;
- `D` for descending descriptors.

All other columns keep the existing MyLite descriptor-driven behavior, including
the current deterministic cardinality placeholder and `Sub_part` prefix
metadata.

### `INFORMATION_SCHEMA.STATISTICS`

The limited `INFORMATION_SCHEMA.STATISTICS.COLLATION` projection uses the same
`A` / `D` mapping as `SHOW INDEX`. `SUB_PART`, `INDEX_TYPE`, `IS_VISIBLE`, and
`EXPRESSION` keep the existing supported subset.

## Diagnostics

The slice reuses existing diagnostics where possible:

- syntax error for unsupported grammar such as qualified key parts, functional
  key parts, `USING`, options, comments, visibility, fulltext, or spatial forms
  not admitted by the parser;
- missing default schema: `1046 / 3D000`;
- unknown schema: `1049 / 42000`;
- unknown table: `1146 / 42S02`;
- reserved `_mylite_*` target names: existing reserved-name diagnostic;
- duplicate explicit index name: `1061 / 42000`;
- reserved `PRIMARY` index name: `1280 / 42000`;
- unknown key column: `1072 / 42000`;
- duplicate key-part column: `1060 / 42S21`;
- unsupported indexed column type: existing MySQL-shaped storage-engine
  diagnostic for the relevant index class;
- `TEXT` family key without prefix: `1170 / 42000`;
- prefix length `0`: `1391 / HY000`;
- prefix length on non-string descriptor or beyond bounded string length:
  `1089 / HY000`;
- aggregate key too long: `1071 / 42000`;
- `NULL` or duplicate existing rows when adding a primary or unique index:
  existing primary/unique diagnostics;
- SQLite physical DDL failure: existing physical SQL failure diagnostic;
- allocation failure: `MYLITE_NOMEM` with handle-owned diagnostics.

Supported in-range forms produce `warning_count == 0`.

## Tests

Add MySQL-runtime-verified expectations and plain C runtime/parser tests for:

- create-time primary, unique, nonunique, composite, and prefix key parts with
  omitted direction, explicit `ASC`, and explicit `DESC`;
- `ALTER TABLE ... ADD PRIMARY KEY`, `ALTER TABLE ... ADD KEY`, and standalone
  `CREATE INDEX` / `CREATE UNIQUE INDEX` with descending parts;
- `SHOW CREATE TABLE`, `SHOW INDEX.Collation`, and
  `INFORMATION_SCHEMA.STATISTICS.COLLATION` / `SUB_PART`;
- `CREATE TABLE ... LIKE` preservation and reopen persistence;
- duplicate unique enforcement and DML behavior unchanged by direction;
- duplicate key-part names with mixed directions;
- prefix validation with descending direction;
- missing default schema, unknown schema/table, unknown columns, duplicate
  names, and reserved names through existing index entry points;
- unsupported syntax such as table-qualified key parts, functional key parts,
  `USING`, fulltext/spatial forms, and unsupported options;
- `.mylite` preamble preservation and independent file-backed handles;
- existing parser, index lifecycle, prefix, unique, primary, foreign-key,
  create-like, show, information-schema, DML, file-format, and workflow tests.

## Review Notes

- The implementation must keep descriptor metadata authoritative and must not
  infer direction from SQLite schema text.
- Existing catalog rows must default to ascending on migration.
- Direction must not leak into equality probes or duplicate-validation SQL.
- Any generated SQLite `DESC` belongs only to index DDL terms.
- The compatibility docs must describe the feature as limited metadata and
  physical-index support, not full optimizer behavior.
