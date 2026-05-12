# Baseline Index Prefix Key Parts Specification

## Summary

This phase expands descriptor-owned nonunique secondary indexes from one full
column to ordered key-part lists with optional string prefix lengths:

```sql
CREATE TABLE t (..., KEY k (name(191)))
CREATE TABLE t (..., KEY k (a(3), b(4)))
ALTER TABLE t ADD KEY k (name(191))
CREATE INDEX k ON t (name(191))
```

The goal is to accept common MySQL DDL emitted by MySQL-oriented applications,
including WordPress-style `VARCHAR` prefix indexes, preserve the prefix length
in MyLite catalog descriptors, render it through `SHOW CREATE TABLE`,
`SHOW INDEX`, and limited `INFORMATION_SCHEMA.STATISTICS`, and create matching
SQLite physical indexes from stable descriptors.

This is not full index support. Unique and primary prefix indexes, descending
parts, functional parts, index options, optimizer guarantees, and prefix-aware
duplicate-key semantics remain separate features.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline secondary, alter-add-index, create-index, unique-index, and string
  key lifecycle specs:
  `docs/specs/baseline-secondary-index-lifecycle/specs.md`,
  `docs/specs/baseline-alter-table-add-index-lifecycle/specs.md`,
  `docs/specs/baseline-create-index-lifecycle/specs.md`,
  `docs/specs/baseline-unique-index-lifecycle/specs.md`,
  `docs/specs/baseline-char-varchar-key-lifecycle/specs.md`
- MySQL 8.4 Reference Manual, `CREATE TABLE` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/create-table.html>
- MySQL 8.4 Reference Manual, `CREATE INDEX` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/create-index.html>
- MySQL 8.4 Reference Manual, `SHOW INDEX` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/show-index.html>
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.STATISTICS` table:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-statistics-table.html>
- Observed MySQL 8.4.9 runtime behavior to be recorded by
  `packages/libmylite/tests/mysql_baseline_index_prefix_key_parts_expectations.sh`.

This specification is independently authored from official MySQL 8.4
documentation, observed MySQL 8.4.9 behavior, public SQLite APIs, and existing
MyLite code. It does not copy MySQL, MariaDB, Percona, SQLite implementation
internals, or other restrictively licensed implementation sources.

## MySQL 8.4.9 Observations

Runtime probes for this phase establish:

- `KEY meta_key (meta_key(191))` succeeds for `VARCHAR(255)`, renders as
  ``KEY `meta_key` (`meta_key`(191))``, and reports `Sub_part` / `SUB_PART`
  as `191`.
- `KEY body_prefix (body(20))` succeeds for `TEXT`, and `TEXT` without a
  prefix fails with `1170 / 42000`.
- Composite nonunique string prefix indexes such as `KEY k_ab (a(3), b(4))`
  succeed and report one metadata row per ordered part with `Seq_in_index`
  values starting at `1`.
- Duplicate key parts such as `KEY k (a, a)` and `KEY k (a(3), a(5))` fail
  with `1060 / 42S21`.
- `ALTER TABLE t ADD KEY k (a(5))` and `CREATE INDEX k ON t (b(10))` succeed
  for the same prefix subset and are rendered in `SHOW CREATE TABLE`.
- Prefix lengths on integer key parts, such as `object_id(20)`, fail with
  `1089 / HY000`.
- Prefix length `0` fails with `1391 / HY000`.
- A `VARCHAR(10)` prefix length greater than the column length fails with
  `1089 / HY000` in the tested MySQL 8.4.9 InnoDB runtime even after
  `SET SESSION sql_mode = ''`.
- Under the default `utf8mb4_0900_ai_ci` table definition, prefix lengths above
  the InnoDB 3072-byte key limit fail with `1071 / 42000`; for example,
  `VARCHAR(1000)` prefix `768` succeeds, prefix `769` fails, and composite
  string prefix parts fail when their combined prefix bytes exceed the limit.
- A unique prefix index is accepted by MySQL and enforces uniqueness over the
  prefix. For example, `UNIQUE KEY u_v (v(3))` rejects later values sharing
  the same first three characters with `1062 / 23000`. MyLite defers this
  behavior until prefix-aware unique enforcement is designed.

Official MySQL documentation defines prefix lengths for nonbinary string
index parts as character counts. It also defines `SHOW INDEX.Sub_part` and
`INFORMATION_SCHEMA.STATISTICS.SUB_PART` as the indexed prefix length, or
`NULL` when the full column is indexed.

## Scope

Supported:

- persistent MyLite base tables only;
- create-time nonunique table-level `KEY` / `INDEX [index_name] (...)`;
- single-action `ALTER TABLE table_name ADD INDEX|KEY [index_name] (...)`;
- standalone `CREATE INDEX index_name ON table_name (...)`;
- unqualified and schema-qualified target table names using the existing
  selected/default schema policy;
- explicit or generated nonunique index names using the existing descriptor
  policy;
- ordered one-or-more key-part lists for nonunique indexes;
- unqualified descriptor-column key parts;
- full-column key parts for descriptor types already accepted by the existing
  nonunique index lifecycle;
- optional positive decimal integer prefix lengths for `CHAR`, `VARCHAR`, and
  baseline `TEXT` family columns;
- `TEXT` family nonunique indexes only when a valid prefix length is present;
- descriptor-backed `SHOW COLUMNS`, `SHOW CREATE TABLE`, `SHOW INDEX`,
  limited `INFORMATION_SCHEMA.STATISTICS`, `CREATE TABLE ... LIKE`,
  index drops, DML, reopen persistence, independent handles, and `.mylite`
  preamble preservation;
- no-result DDL result shape with `affected_rows == 0` and
  `warning_count == 0` for supported successful forms.

Deferred:

- unique and primary prefix indexes;
- prefix parts on binary string or BLOB columns because those type families are
  not in the current baseline storage surface;
- prefix lengths on non-string columns;
- descending key parts, expression key parts, functional key parts, ordinal key
  parts, qualified key parts, parser options, comments, visibility,
  `KEY_BLOCK_SIZE`, `USING`, algorithms, locks, partitions, fulltext/spatial
  indexes, foreign keys, cascades, triggers, privileges, and optimizer/index-use
  guarantees;
- MySQL's nonstrict truncation-warning path for oversized nonunique prefixes.
  MyLite rejects oversized admitted prefixes with the MySQL 8.4.9 InnoDB error
  observed for this baseline.

## Ownership Boundaries

- Public API: no new public ABI. Callers continue to use `mylite_execute()` and
  existing result/diagnostic accessors.
- Statement context: owns diagnostics reset, warning count, `ROW_COUNT()`, and
  result cleanup. No new session state is introduced.
- Lexer/parser/AST: admits prefix key-part syntax and preserves column names
  plus optional prefix length literal nodes without consulting descriptors or
  SQLite.
- Analyzer/planner/runtime: resolves schema, target table, index name, key
  columns, prefix admissibility, and diagnostics against MyLite descriptors
  before any SQLite SQL is generated.
- Catalog: MyLite index and index-column descriptors are authoritative logical
  metadata, including ordered key parts and optional prefix length. SQLite
  schema text is a physical artifact only.
- Result builder/introspection: existing descriptor-driven `SHOW`,
  `CREATE TABLE ... LIKE`, and `INFORMATION_SCHEMA` surfaces render prefix
  lengths from descriptors.
- Storage/VFS: `.mylite` preamble and shifted SQLite payload invariants are
  unchanged.
- SQLite physical storage: MyLite creates generated SQLite indexes using public
  SQLite DDL support and expression index terms for prefix parts. No SQLite
  fork patch is required.

## Supported SQL Grammar

MyLite admits the same key-part syntax for the nonunique index entry points in
this scope:

```sql
KEY [index_name] (column_name[(prefix_length)][, ...])
INDEX [index_name] (column_name[(prefix_length)][, ...])
ALTER TABLE table_name ADD KEY [index_name] (column_name[(prefix_length)][, ...])
ALTER TABLE table_name ADD INDEX [index_name] (column_name[(prefix_length)][, ...])
CREATE INDEX index_name ON table_name (column_name[(prefix_length)][, ...])
```

`prefix_length` is a positive decimal integer literal without sign. The parser
does not admit parameters, expressions, string literals, floating-point
literals, hex literals, bit literals, qualified columns, sort direction, or
functional key parts in this slice.

MyLite Lemon-syntax sketch:

```lemon
secondary_index_part_list ::= secondary_index_part.
secondary_index_part_list ::= secondary_index_part_list COMMA secondary_index_part.

secondary_index_part ::= identifier.
secondary_index_part ::= identifier LPAREN integer_literal RPAREN.
```

The AST should represent each key part as a key-part node with one identifier
child and an optional prefix-length literal child. Existing callers must treat
legacy identifier-only nodes as full-column key parts only until all production
paths use the new node shape.

## Resolution and Validation

Target table resolution follows existing policy:

- an unqualified table requires the selected/default schema;
- a schema-qualified table uses the explicit schema and does not require a
  selected schema;
- unknown schemas fail with `1049 / 42000`;
- unknown tables fail with `1146 / 42S02`;
- reserved `_mylite_*` schema or table names are rejected before physical SQL
  is generated;
- target objects must be persistent base tables.

Index names remain table-local:

- explicit names must not collide case-insensitively with any existing primary,
  unique, or nonunique descriptor index;
- duplicate explicit names fail with `1061 / 42000`;
- quoted `PRIMARY` fails with `1280 / 42000`;
- omitted names for `CREATE TABLE` and `ALTER TABLE ... ADD INDEX|KEY` derive
  from the first key column and append `_2`, `_3`, ... for collisions.

Each key part resolves against MyLite column descriptors:

- key-part column names must be unqualified descriptor column names;
- unknown columns fail with `1072 / 42000`;
- current descriptor lookup remains ASCII case-insensitive, matching the
  existing catalog policy;
- duplicate key parts fail with `1060 / 42S21`;
- full-column key parts keep the existing accepted type set for nonunique
  indexes;
- `TEXT` family key parts require a prefix and otherwise fail with
  `1170 / 42000`;
- prefix lengths are allowed only for `CHAR`, `VARCHAR`, and baseline `TEXT`
  family descriptors;
- prefix length `0` fails with `1391 / HY000`;
- prefix length greater than a bounded `CHAR(n)` / `VARCHAR(n)` descriptor
  fails with `1089 / HY000`;
- admitted string key-part byte contributions must fit MySQL's observed
  3072-byte InnoDB key limit for the fixed `utf8mb4` baseline; MyLite counts
  nonbinary string prefix lengths as characters and uses four bytes per
  character for this cap;
- prefix length on non-string descriptors fails with `1089 / HY000`;
- supported successful forms produce no warnings.

For `CHAR` / `VARCHAR`, the prefix length is checked against the descriptor's
logical character length. For `TEXT` family descriptors, this baseline accepts
positive decimal prefix lengths that fit the same 3072-byte key-length cap.

## Descriptor and Catalog Semantics

The catalog gains nullable prefix-length metadata on index-column descriptors.
`NULL` means a full-column key part; a positive integer means a prefix part.

On successful nonunique index creation, MyLite creates:

- one `_mylite_catalog_indexes` row with secondary kind and `is_unique = 0`;
- one `_mylite_catalog_index_columns` row per key part, preserving ordinal
  position and optional prefix length;
- a generated physical index name `_mylite_user_index_<index_id>`;
- updated descriptor/catalog generations and SQLite schema generation because a
  physical SQLite index changed.

`CREATE TABLE ... LIKE` clones prefix lengths together with existing index
descriptors. `DROP INDEX`, `ALTER TABLE ... DROP INDEX|KEY`, `DROP TABLE`,
`RENAME TABLE`, DML, reopen, and independent handles observe prefix descriptors
through existing catalog loading paths. Failed planning or execution must leave
no catalog rows and no physical SQLite index.

Catalog migration from the previous schema version must add the prefix-length
column with `NULL` for every existing index-column row, preserving old
descriptors as full-column key parts.

## Physical SQLite Handling

MyLite generates SQLite index DDL only from descriptors and stable physical
names:

```sql
CREATE INDEX "_mylite_user_index_<index_id>"
ON "_mylite_user_table_<table_id>"
("full_col", substr("prefix_col", 1, 191) COLLATE "_mylite_string_key")
```

Rules:

- quote every generated identifier;
- interpolate only validated descriptor-owned prefix integers, never raw SQL
  text;
- full-column string `CHAR` / `VARCHAR` parts keep the existing
  `_mylite_string_key` collation annotation;
- prefix `CHAR`, `VARCHAR`, and `TEXT` parts use a SQLite expression index term
  based on `substr(column, 1, prefix_length)` and the same string-key collation;
- non-string full-column parts render as ordinary quoted physical columns;
- create physical indexes after descriptor rows are allocated and before the
  catalog mutation commits;
- no query rewriting is required for correctness. SQLite may use the physical
  index where applicable, but MyLite does not expose optimizer guarantees.

Expression indexes and `substr()` are public SQLite features in the bundled
runtime, so this phase does not require a SQLite fork patch.

## Introspection

`SHOW CREATE TABLE` renders nonunique indexes with ordered key parts:

```sql
KEY `k` (`a`(3),`b`)
```

`SHOW INDEX` and `INFORMATION_SCHEMA.STATISTICS` render one row per key part:

- `Non_unique` / `NON_UNIQUE`: `1`;
- `Seq_in_index` / `SEQ_IN_INDEX`: descriptor ordinal position starting at `1`;
- `Column_name` / `COLUMN_NAME`: descriptor column name;
- `Collation`: `A`;
- `Sub_part` / `SUB_PART`: decimal prefix length for prefix parts, `NULL` for
  full-column parts;
- `Index_type` / `INDEX_TYPE`: `BTREE`;
- nullable, visibility, cardinality, packed, comment, and expression fields keep
  the existing limited index metadata policy.

`SHOW COLUMNS` `Key` values continue to reflect descriptor index presence; this
phase does not add protocol flag changes.

## Diagnostics

Supported diagnostics include:

- syntax errors and unsupported grammar: existing parser/unsupported diagnostic;
- missing default schema: existing `1046 / 3D000`;
- unknown schema: existing `1049 / 42000`;
- unknown table: existing `1146 / 42S02`;
- reserved target schema/table/index names: existing reserved-name diagnostics;
- duplicate index name: `1061 / 42000`;
- duplicate key part column name: `1060 / 42S21`;
- incorrect index name `PRIMARY`: `1280 / 42000`;
- unknown key column: `1072 / 42000`;
- `TEXT` family key without prefix: `1170 / 42000`;
- zero prefix length: `1391 / HY000`;
- prefix on non-string or oversized bounded string column: `1089 / HY000`;
- combined key length over the current 3072-byte cap: `1071 / 42000`;
- unsupported unique or primary prefix key parts: deterministic MyLite
  unsupported diagnostic until those lifecycles are implemented;
- unsupported key-part forms such as qualified columns, expressions, directions,
  parameters, hex/bit/string/float/decimal prefix literals, or options:
  deterministic parse or unsupported diagnostics;
- physical SQLite failures, allocation failures, and public API misuse:
  existing MyLite runtime conventions.

## Performance and Storage

This slice keeps index work close to SQLite:

- MyLite validates DDL and maintains descriptors, but it does not materialize
  table rows to build nonunique indexes itself;
- SQLite builds and maintains the generated physical indexes;
- prefix string expressions are evaluated by SQLite's expression-index support;
- MyLite does not promise optimizer use or expose query-plan differences.

## Tests

Add a MySQL-runtime expectation script covering:

- valid `VARCHAR` prefix, `TEXT` prefix, composite prefix, full-column plus
  prefix mixtures, `ALTER TABLE ... ADD KEY`, and standalone `CREATE INDEX`;
- `SHOW CREATE TABLE`, `SHOW INDEX`, and
  `INFORMATION_SCHEMA.STATISTICS.SUB_PART`;
- generated index names from the first key part;
- missing default schema, unknown schema/table, duplicate index names, quoted
  `PRIMARY`, and unknown key columns;
- invalid integer prefix, zero prefix, oversized `VARCHAR` prefix, `TEXT`
  without prefix, unique prefix duplicate behavior as deferred evidence, and
  primary prefix behavior as deferred evidence.

Add fast C tests under `packages/libmylite/tests/` for:

- parser admission and rejection of the supported and unsupported grammar;
- `CREATE TABLE`, `ALTER TABLE ... ADD INDEX|KEY`, and `CREATE INDEX`
  nonunique prefix/composite descriptors;
- metadata rendering through `SHOW CREATE TABLE`, `SHOW INDEX`, and limited
  `INFORMATION_SCHEMA.STATISTICS`;
- `CREATE TABLE ... LIKE`, drop index, table rename/drop, reopen persistence,
  independent file handles, and preamble preservation;
- deterministic diagnostics for unsupported unique/primary prefix parts and
  invalid prefix lengths;
- existing secondary, unique, primary, alter-add-index, create-index, drop-index,
  DML, catalog, VFS, parser, and registration tests.

## Compatibility Documentation

After implementation, update:

- `COMPATIBILITY.md`;
- `docs/compatibility/sql-indexes-constraints.md`;
- `docs/compatibility/sql-table-ddl.md`;
- other compatibility detail pages only if the implementation changes their
  documented surface.

The docs must state that this is limited nonunique prefix/composite index
metadata and physical-index support, not full MySQL index behavior.
