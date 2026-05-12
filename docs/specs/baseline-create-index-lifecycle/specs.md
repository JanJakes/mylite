# Baseline CREATE INDEX Lifecycle

## Summary

This phase adds descriptor-owned standalone index creation for persistent
MyLite base tables:

```sql
CREATE INDEX index_name ON table_name (column_name)
CREATE UNIQUE INDEX index_name ON table_name (column_name)
```

The slice is intentionally small. It gives migrations and schema builders the
common standalone entry point while reusing MyLite-owned index descriptors,
metadata rendering, physical SQLite indexes, and DML duplicate-key enforcement
already used by create-time indexes and `ALTER TABLE ... ADD INDEX`.

This is not full MySQL index DDL. Composite, prefix, descending, functional,
fulltext, spatial, algorithm, lock, visibility, comment, parser, and expression
index surfaces remain separate features.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- SQLite connection bootstrap policy:
  `docs/specs/sqlite-connection-bootstrap-policy/specs.md`
- File-backed MyLite opening VFS:
  `docs/specs/file-backed-mylite-opening-vfs/specs.md`
- MyLite file format: `docs/specs/mylite-file-format/specs.md`
- Baseline secondary and unique index lifecycle specs:
  `docs/specs/baseline-secondary-index-lifecycle/specs.md`,
  `docs/specs/baseline-unique-index-lifecycle/specs.md`
- Baseline `ALTER TABLE ... ADD INDEX` and `DROP INDEX` lifecycle specs:
  `docs/specs/baseline-alter-table-add-index-lifecycle/specs.md`,
  `docs/specs/baseline-alter-table-drop-index-lifecycle/specs.md`
- Baseline `CHAR` / `VARCHAR` key lifecycle:
  `docs/specs/baseline-char-varchar-key-lifecycle/specs.md`
- MySQL 8.4 Reference Manual, `CREATE INDEX` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/create-index.html>
- MySQL 8.4 Reference Manual, `ALTER TABLE` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/alter-table.html>
- MySQL 8.4 Reference Manual, `SHOW INDEX` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/show-index.html>
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.STATISTICS` table:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-statistics-table.html>
- Observed MySQL 8.4.9 runtime behavior recorded by
  `packages/libmylite/tests/mysql_baseline_create_index_lifecycle_expectations.sh`.

This specification is independently authored from official MySQL 8.4
documentation, observed MySQL 8.4.9 behavior, public SQLite APIs, and existing
MyLite source. It does not copy MySQL, MariaDB, Percona, SQLite implementation
internals, or other restrictively licensed implementation sources.

## MySQL 8.4.9 Observations

Runtime probes for this phase establish:

- `CREATE INDEX k_v ON t (v)` and `CREATE UNIQUE INDEX u_v ON t (v)`
  succeed on existing InnoDB tables, report `ROW_COUNT() == 0`, and leave
  `@@warning_count == 0` for admitted forms.
- `CREATE INDEX` requires an explicit index name. `CREATE INDEX ON t (v)` and
  `CREATE INDEX IF NOT EXISTS k_v ON t (v)` fail with `1064 / 42000`.
- Target table resolution uses normal default-schema behavior: no selected
  database fails with `1046 / 3D000`, unknown schemas fail with
  `1049 / 42000`, and unknown tables fail with `1146 / 42S02`.
- `SHOW CREATE TABLE`, `SHOW INDEX`, and `INFORMATION_SCHEMA.STATISTICS`
  expose standalone-created nonunique indexes with `NON_UNIQUE = 1` and
  standalone-created unique indexes with `NON_UNIQUE = 0`.
- Duplicate explicit index names fail with `1061 / 42000`.
- A quoted index name `PRIMARY` fails with `1280 / 42000`; unquoted `PRIMARY`
  in the index-name position is a syntax error.
- Unknown key columns fail with `1072 / 42000`.
- `TEXT` without a prefix fails with `1170 / 42000`.
- `CHAR(0)` and `VARCHAR(0)` cannot be indexed by the tested InnoDB runtime and
  fail with `1167 / 42000`.
- Creating a unique index over existing duplicate non-`NULL` values fails with
  `1062 / 23000`. Duplicate `NULL` values are permitted.
- Under the default `utf8mb4_0900_ai_ci` collation, ASCII `VARCHAR` duplicate
  checks are case-insensitive and preserve `NO PAD` trailing-space behavior:
  existing `'a'` / `'A'` rows fail, while existing `'a'` / `'a '` rows succeed.
  Existing `CHAR` values are canonicalized before indexing, so `'a'` and
  `'a '` fail as duplicates.
- MySQL accepts wider forms including multiple key parts, prefix key parts,
  descending key parts, functional key parts, `USING`, comments, visibility,
  algorithm, lock, fulltext, and spatial forms. They remain deferred here.

## Scope

Supported:

- persistent MyLite base tables only;
- `CREATE INDEX index_name ON table_name (column_name)`;
- `CREATE UNIQUE INDEX index_name ON table_name (column_name)`;
- unqualified and schema-qualified target table names using the existing
  selected/default schema policy;
- one explicit index name, quoted or unquoted, using the existing descriptor
  identifier policy;
- exactly one unqualified descriptor column in the key-part list;
- supported key target descriptors:
  - integer-family and integer aliases, including `BOOL` / `BOOLEAN`;
  - exact `DECIMAL` / `NUMERIC` / `FIXED`;
  - canonical `DATE`, `DATETIME`, and `TIMESTAMP`;
  - `CHAR(1..255)` and `VARCHAR(1..255)`;
- nullable and `NOT NULL` key target columns;
- empty and nonempty tables;
- unique-index duplicate validation over existing rows before committing
  descriptor mutation;
- nullable unique indexes permitting multiple `NULL` values;
- ASCII-valued `CHAR` / `VARCHAR` unique key values using the fixed MyLite
  `utf8mb4_0900_ai_ci` ASCII collation behavior;
- descriptor-backed `SHOW COLUMNS`, `SHOW CREATE TABLE`, `SHOW INDEX`,
  `CREATE TABLE ... LIKE`, DML, `ALTER TABLE ... DROP INDEX|KEY`,
  `ALTER TABLE ... DROP PRIMARY KEY` auto-increment key checks, and limited
  `INFORMATION_SCHEMA.STATISTICS`, `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`, and
  `INFORMATION_SCHEMA.KEY_COLUMN_USAGE` after the create;
- row-value preservation, reopen persistence, independent file-backed handles,
  and `.mylite` preamble preservation;
- no-result DDL result shape with `affected_rows == 0` and
  `warning_count == 0` for supported successful forms.

Deferred:

- omitted index names and `IF NOT EXISTS`;
- `CREATE FULLTEXT INDEX`, `CREATE SPATIAL INDEX`, and primary-key creation;
- standalone `DROP INDEX` and index rename;
- temporary tables, views, generated columns, invisible columns beyond explicit
  descriptor column matching, foreign keys, cascades, triggers, privileges,
  and implicit-commit emulation;
- multiple key parts, duplicate key parts, prefix lengths, descending key
  parts, functional key parts, expression key parts, table-qualified key parts,
  ordinal key parts, and string-literal key parts;
- index type clauses, comments, parser options, `KEY_BLOCK_SIZE`, visibility,
  engine attributes, algorithms, locks, and partitions;
- `TEXT` family key parts until prefix-length semantics are implemented;
- non-ASCII string key values and full Unicode collation weights;
- optimizer/index-use guarantees beyond creating a physical SQLite index that
  SQLite may use.

## Ownership Boundaries

- Public API: no new public ABI. Callers continue to use `mylite_execute()` and
  existing result/diagnostic accessors.
- Statement context: owns diagnostics reset, warning count, affected rows,
  `ROW_COUNT()`, and cleanup.
- Lexer/parser/AST: owns syntax admission for the narrow standalone DDL and
  preserves index name, table name, and key column nodes without consulting
  descriptors or SQLite.
- Analyzer/planner/runtime: resolves schema, table, index name, and key column
  against MyLite descriptors before generating physical SQL.
- Catalog: MyLite index and index-column descriptors are authoritative logical
  metadata. SQLite schema text is not inspected to discover logical indexes.
- Result builder/introspection: existing descriptor-driven `SHOW`,
  `CREATE TABLE ... LIKE`, and `INFORMATION_SCHEMA` surfaces render the new
  descriptors.
- Storage/VFS: `.mylite` preamble and shifted SQLite payload invariants are
  unchanged.
- SQLite physical storage: MyLite creates one ordinary SQLite index using
  stable generated physical table, index, and column identifiers. No SQLite
  fork patch is required.

## Supported SQL Grammar

MyLite admits only the explicit-name one-column forms:

```sql
CREATE INDEX index_name ON table_name (column_name)
CREATE UNIQUE INDEX index_name ON table_name (column_name)
```

The target table may be unqualified or schema-qualified. The index name is one
identifier or quoted identifier. The key column is one unqualified identifier or
quoted identifier.

MyLite Lemon-syntax sketch:

```lemon
statement(A) ::= create_index_statement(B). {
    A = B;
}

create_index_statement(A) ::=
    CREATE(C) INDEX identifier(N) ON table_name(T)
    LPAREN secondary_index_part_list(L) RPAREN. {
    A = mylite_sql_parser_make_create_index_statement(
        state, C, false, N, T, L);
}

create_index_statement(A) ::=
    CREATE(C) UNIQUE INDEX identifier(N) ON table_name(T)
    LPAREN secondary_index_part_list(L) RPAREN. {
    A = mylite_sql_parser_make_create_index_statement(
        state, C, true, N, T, L);
}

secondary_index_part ::= identifier.
```

The implementation may reuse the existing secondary key-part list AST node, but
the top-level statement kind must preserve whether the standalone DDL is unique
or nonunique.

## Resolution Semantics

Target table resolution follows the existing policy:

- an unqualified table requires the selected/default schema;
- a schema-qualified table uses the explicit schema and does not require a
  selected schema;
- unknown schemas fail with `1049 / 42000`;
- unknown tables fail with `1146 / 42S02`;
- reserved `_mylite_*` schema or table names are rejected before physical SQL
  is generated.

The target object must be a persistent base table. Future non-base descriptors
must fail deterministically for this supported surface.

Index name resolution is table-local:

- explicit names must not collide case-insensitively with any existing primary,
  unique, or nonunique index descriptor;
- duplicate explicit names fail with `1061 / 42000`;
- quoted `PRIMARY` fails with `1280 / 42000`;
- unquoted `PRIMARY` remains a syntax error in the admitted grammar.

Column resolution is descriptor-owned:

- the key column must be an existing unqualified descriptor column;
- unknown columns fail with `1072 / 42000`;
- `TEXT` family columns fail with `1170 / 42000`;
- `CHAR(0)` and `VARCHAR(0)` fail with `1167 / 42000`;
- unsupported descriptor kinds fail with a MyLite unsupported diagnostic until
  their MySQL semantics are implemented.

Current descriptor catalog name matching remains ASCII case-insensitive for
schema, table, index, and column lookup. This phase does not add collation-aware
identifier semantics.

## Unique Duplicate Semantics

`CREATE UNIQUE INDEX` validates existing rows before committing descriptor
metadata:

- `NULL` key values are ignored for duplicate detection, so multiple `NULL`
  rows remain valid;
- non-`NULL` key values are grouped by the same physical expression used in
  generated unique indexes;
- `CHAR` / `VARCHAR` key expressions include the MyLite
  `utf8mb4_0900_ai_ci` ASCII collation annotation;
- non-ASCII string key values fail with the existing MyLite unsupported
  diagnostic before descriptor mutation;
- the first duplicate group in physical row order is reported with
  `1062 / 23000` and the message shape
  `Duplicate entry 'value' for key 'table.index'`.

The slice does not overclaim duplicate-group tie ordering beyond the
MySQL-runtime-verified single-column cases. It preserves existing row data if
validation fails.

## Physical SQLite Handling

Generated SQL is descriptor-built and uses public SQLite APIs:

```sql
CREATE [UNIQUE] INDEX "_mylite_user_index_<index_id>"
ON "_mylite_user_table_<table_id>" ("column_name" [COLLATE "utf8mb4_0900_ai_ci"])
```

Every generated identifier is quoted. Values are not interpolated into the
physical DDL; this feature generates only identifiers and fixed SQL keywords.
Duplicate-validation queries are also descriptor-built, use quoted identifiers,
and do not inspect SQLite metadata as a source of logical truth.

Catalog mutation allocates the index id, inserts index and index-column
descriptors, executes the physical SQLite `CREATE INDEX`, updates table
identity metadata, and commits atomically. On failure, MyLite rolls back catalog
mutation and leaves existing descriptors and rows unchanged. Successful
standalone index creation increments the SQLite schema-generation cache marker.

## Result Behavior

Successful supported `CREATE INDEX` and `CREATE UNIQUE INDEX` return through the
existing non-row statement result conventions:

- no result rows;
- `affected_rows == 0`;
- `warning_count == 0`;
- `ROW_COUNT()` observes `0`.

The statements do not mutate catalog rows unrelated to the new index
descriptor, do not rewrite user rows, and do not change table descriptor
versions except through the existing table identity update used for schema DDL.

## Diagnostics

Required diagnostics for this slice:

- syntax errors and unsupported grammar: existing parser syntax error or
  deterministic unsupported diagnostic;
- missing default schema: `1046 / 3D000`;
- unknown schema: `1049 / 42000`;
- unknown table: `1146 / 42S02`;
- reserved `_mylite_*` target names: existing MyLite reserved-name diagnostic;
- unsupported object kind: deterministic unsupported diagnostic;
- duplicate index name: `1061 / 42000`;
- quoted `PRIMARY` index name: `1280 / 42000`;
- unknown key column: `1072 / 42000`;
- `TEXT` key without prefix: `1170 / 42000`;
- `CHAR(0)` / `VARCHAR(0)` key part: `1167 / 42000`;
- existing duplicate rows for unique indexes: `1062 / 23000`;
- non-ASCII string unique key values: existing MyLite unsupported diagnostic;
- physical SQLite failures: mapped through existing internal/physical-row
  diagnostic policy after catalog rollback;
- allocation failures: existing `MYLITE_NOMEM` diagnostic behavior.

## Test Plan

Add `packages/libmylite/tests/mysql_baseline_create_index_lifecycle_expectations.sh`
and fast C coverage under `packages/libmylite/tests/`.

The MySQL script verifies MySQL 8.4.9 expectations for accepted forms, metadata,
diagnostics, duplicate unique validation, duplicate `NULL`, schema resolution,
unsupported-but-upstream-accepted syntax, and row/warning counts.

The C runtime test covers:

- successful nonunique and unique standalone index creation over integer,
  unsigned integer, decimal, date, datetime, timestamp, `CHAR`, and `VARCHAR`
  descriptors;
- descriptor metadata through `SHOW CREATE TABLE`, `SHOW INDEX`,
  `INFORMATION_SCHEMA.STATISTICS`, `TABLE_CONSTRAINTS`, and
  `KEY_COLUMN_USAGE`;
- duplicate unique validation over existing rows and duplicate `NULL`
  acceptance;
- later `INSERT`, `INSERT IGNORE`, `UPDATE`, and `ALTER TABLE ... DROP INDEX`
  behavior using standalone-created descriptors;
- schema-qualified and unqualified targets, missing default schema, unknown
  schema, unknown table, reserved names, duplicate names, quoted `PRIMARY`,
  unknown columns, `TEXT` without prefix, and zero-length string key columns;
- parser tests for admitted forms and deferred syntax;
- reopen persistence, table rename/drop interaction, independent handles,
  `.mylite` preamble preservation, and zero-initialized cleanup paths.
