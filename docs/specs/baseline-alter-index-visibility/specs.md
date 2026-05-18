# Baseline ALTER INDEX Visibility

## Summary

This phase adds the narrow index-visibility table action for persistent MyLite
base tables:

```sql
ALTER TABLE table_name ALTER INDEX index_name VISIBLE
ALTER TABLE table_name ALTER INDEX index_name INVISIBLE
```

The operation updates MyLite-owned index descriptor metadata only. It preserves
rows, columns, key parts, uniqueness, foreign-key references, generated SQLite
physical index names, and physical SQLite index maintenance. Visibility is
exposed through descriptor-driven `SHOW INDEX`, `SHOW CREATE TABLE`, and
`INFORMATION_SCHEMA.STATISTICS`.

This phase does not add visibility syntax to `CREATE TABLE`, `CREATE INDEX`, or
`ALTER TABLE ... ADD INDEX`, and does not change SQLite query planning. MyLite
currently treats secondary indexes as compatibility metadata plus physical
constraints and helper indexes; optimizer-use semantics are deferred.

## Sources And Evidence

- MyLite architecture and engineering standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
  - `third_party/sqlite/README.md`
- Existing MyLite index and visibility slices:
  - `docs/specs/baseline-secondary-index-lifecycle/specs.md`
  - `docs/specs/baseline-unique-index-lifecycle/specs.md`
  - `docs/specs/baseline-create-index-lifecycle/specs.md`
  - `docs/specs/baseline-alter-table-add-index-lifecycle/specs.md`
  - `docs/specs/baseline-rename-index-lifecycle/specs.md`
  - `docs/specs/baseline-alter-column-visibility/specs.md`
- Official MySQL 8.4 Reference Manual:
  - Invisible indexes:
    <https://dev.mysql.com/doc/refman/8.4/en/invisible-indexes.html>
  - `SHOW INDEX`:
    <https://dev.mysql.com/doc/refman/8.4/en/show-index.html>
  - `ALTER TABLE`:
    <https://dev.mysql.com/doc/refman/8.4/en/alter-table.html>
- Observed MySQL 8.4.9 behavior is captured in
  `packages/libmylite/tests/mysql_baseline_alter_index_visibility_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## MySQL 8.4.9 Observations

Runtime probes for this phase establish:

- `ALTER TABLE t ALTER INDEX k INVISIBLE` and
  `ALTER TABLE t ALTER INDEX k VISIBLE` succeed for secondary indexes.
- Successful visibility changes and idempotent visibility operations report
  `ROW_COUNT() == 0` and `@@warning_count == 0`.
- `SHOW INDEX.Visible` reports `YES` or `NO`.
- `INFORMATION_SCHEMA.STATISTICS.IS_VISIBLE` reports `YES` or `NO`.
- `SHOW CREATE TABLE` appends `/*!80000 INVISIBLE */` to invisible index
  definitions.
- Indexes are visible by default.
- `ALGORITHM=COPY` reports the current table row count in `ROW_COUNT()` for the
  tested nonempty table, while default/in-place forms report zero.
- `ALTER TABLE ... ALTER KEY ...` is a syntax error.
- Unquoted `PRIMARY` in this grammar position is a syntax error; quoted
  `` `PRIMARY` `` resolves to the primary index and fails with
  `3522 / HY000`, `A primary key index cannot be invisible.`
- Missing indexes fail with `1176 / 42000`.
- Missing default schema, unknown schema, and unknown table follow the existing
  `ALTER TABLE` target diagnostics.
- Unique indexes continue to reject duplicate rows while invisible.
- Child and parent foreign-key indexes can be made invisible; FK enforcement
  still applies.
- MySQL accepts `ALGORITHM=DEFAULT`, `INPLACE`, or `COPY`, and `LOCK=DEFAULT`,
  `NONE`, `SHARED`, or `EXCLUSIVE` option tails for this operation. It rejects
  `ALGORITHM=INSTANT`.
- MySQL accepts `CREATE INDEX ... INVISIBLE` and
  `ALTER TABLE ... ADD INDEX ... INVISIBLE`; MyLite defers those creation
  option forms in this phase.

## Supported Scope

Supported:

- persistent MyLite base tables only;
- one `ALTER TABLE table_name ALTER INDEX index_name VISIBLE` action;
- one `ALTER TABLE table_name ALTER INDEX index_name INVISIBLE` action;
- unqualified and schema-qualified target table names using the current
  selected/default schema policy;
- one identifier or quoted identifier for the index name;
- descriptor-owned secondary indexes, including nonunique, unique, prefix,
  descending, composite, and metadata-only full-text indexes already admitted
  by earlier slices;
- indexes referenced by supported foreign-key descriptors;
- idempotent visible-to-visible and invisible-to-invisible operations;
- limited comma-separated `ALGORITHM` / `LOCK` option tails using the existing
  alter-option parser and action matrix;
- descriptor-backed `SHOW INDEX.Visible`,
  `INFORMATION_SCHEMA.STATISTICS.IS_VISIBLE`, and `SHOW CREATE TABLE`
  rendering;
- `CREATE TABLE ... LIKE` cloning of index visibility metadata;
- reopen persistence, independent file-backed handles, `.mylite` preamble
  preservation, and unchanged row values;
- no-result DDL result shape with `warning_count == 0`; successful default and
  in-place forms report `affected_rows == 0`, while `ALGORITHM=COPY` reports
  the current table row count like MySQL 8.4.9.

Deferred:

- index visibility options inside `CREATE TABLE`, `CREATE INDEX`, and
  `ALTER TABLE ... ADD INDEX`;
- primary-key invisibility;
- implicit-primary-key rules for unique `NOT NULL` indexes when a table has no
  explicit primary key;
- temporary tables, views, partitions, multi-action `ALTER`, `ALTER KEY`,
  functional indexes, spatial indexes, index comments, parser options, engine
  attributes, privilege semantics, optimizer switches, optimizer hints, and
  query-plan metadata;
- using visibility to suppress or enable SQLite physical index use;
- SQLite fork changes.

## Ownership Boundaries

- Public API: unchanged. Callers use `mylite_execute()` and existing result and
  diagnostic accessors.
- Statement context: owns diagnostics reset, warning count, affected rows,
  `ROW_COUNT()`, user-transaction implicit commit boundaries, and cleanup.
- Lexer/parser/AST: admits the narrow single-action grammar and records target
  table, index name, and visibility. It does not inspect descriptors or SQLite
  schema.
- Analyzer/planner/runtime: resolves schemas, tables, and indexes through
  MyLite descriptors before catalog writes. It rejects unsupported object kinds,
  missing indexes, primary indexes, and reserved MyLite targets before physical
  SQL is generated.
- Catalog module: stores `is_visible` on index descriptors, migrates existing
  indexes to visible, and updates descriptor generation atomically.
- Result/introspection builders: render index visibility from descriptors.
- SQLite physical row storage: unchanged. Physical indexes remain present and
  maintained regardless of visibility, matching MySQL's uniqueness and
  maintenance semantics for invisible indexes.
- Storage/VFS/file format: the `.mylite` preamble and shifted SQLite payload
  invariants are unchanged.

## Grammar

Supported SQL:

```sql
ALTER TABLE table_name ALTER INDEX index_name VISIBLE
ALTER TABLE table_name ALTER INDEX index_name INVISIBLE
ALTER TABLE table_name ALTER INDEX index_name VISIBLE, alter_option[, ...]
ALTER TABLE table_name ALTER INDEX index_name INVISIBLE, alter_option[, ...]
```

`table_name` may be unqualified or schema-qualified. `index_name` is one
identifier or quoted identifier. Unquoted `PRIMARY` remains outside the
identifier grammar in this position. `ALTER KEY` is not admitted.

MyLite Lemon-syntax sketch:

```lemon
statement(A) ::= alter_table_index_visibility_statement(B). {
    A = B;
}

alter_table_index_visibility_statement(A) ::=
    ALTER(T) TABLE table_name(N) ALTER INDEX identifier(I) VISIBLE(V)
    alter_table_option_tail_opt(O). {
    A = mylite_sql_parser_make_alter_table_index_visibility_statement(
        state, T, N, I, V, MYLITE_SQL_AST_COLUMN_VISIBILITY_VISIBLE, O);
}

alter_table_index_visibility_statement(A) ::=
    ALTER(T) TABLE table_name(N) ALTER INDEX identifier(I) INVISIBLE(V)
    alter_table_option_tail_opt(O). {
    A = mylite_sql_parser_make_alter_table_index_visibility_statement(
        state, T, N, I, V, MYLITE_SQL_AST_COLUMN_VISIBILITY_INVISIBLE, O);
}
```

The existing AST visibility enum may be reused because it already captures the
same visible/invisible binary state. The statement kind remains distinct from
column visibility.

## Resolution And Diagnostics

Target table resolution follows the existing MyLite table policy:

- unqualified names require a selected/default schema;
- schema-qualified names do not require a selected schema;
- missing default schema fails with `1046 / 3D000`;
- unknown schema fails with `1049 / 42000`;
- unknown table fails with `1146 / 42S02`;
- reserved `_mylite_*` schema or table names are rejected before mutation;
- only persistent base-table descriptors are supported.

Index resolution is descriptor-owned:

- match the requested index name case-insensitively against table-local MyLite
  index descriptors;
- unknown names fail with `1176 / 42000`, `Key '<name>' doesn't exist in table
  '<table>'`;
- primary-key descriptors fail with `3522 / HY000` when the grammar can reach
  them through a quoted identifier;
- secondary descriptors are eligible regardless of uniqueness, full-text
  metadata-only status, prefix parts, descending parts, composite parts, or
  supported FK references;
- visibility changes are idempotent and still update through the successful
  DDL result path without changing rows.

Current descriptor identifier comparison remains MyLite's existing ASCII
case-insensitive policy. This phase does not add collation-aware identifier
matching.

## Catalog And Physical Semantics

The catalog gains `is_visible` on `_mylite_catalog_indexes`, defaulting to `1`
for newly created and migrated existing descriptors. Successful changes:

- update only `_mylite_catalog_indexes.is_visible`,
  `descriptor_version`, and `updated_catalog_generation` for the target index;
- mark the owning table generation changed through the existing catalog
  mutation flow so descriptor caches observe the new metadata;
- preserve index id, table id, name, kind, uniqueness, physical name, key
  parts, column descriptors, rows, table physical name, schema descriptors,
  foreign-key descriptors, check descriptors, and auto-increment counters;
- do not generate SQLite physical DDL and do not increment
  `sqlite_schema_generation`.

`CREATE TABLE ... LIKE` clones visibility with other supported index metadata.
Failed planning or execution leaves descriptors unchanged.

Generated SQLite identifiers remain stable:

```text
_mylite_user_index_<index_id>
```

No SQLite fork patch is required.

## Metadata Rendering

`SHOW INDEX` renders:

- `Visible = 'YES'` when `is_visible` is true;
- `Visible = 'NO'` when `is_visible` is false.

`INFORMATION_SCHEMA.STATISTICS` renders:

- `IS_VISIBLE = 'YES'` when `is_visible` is true;
- `IS_VISIBLE = 'NO'` when `is_visible` is false.

`SHOW CREATE TABLE` renders visible indexes exactly as before and appends
` /*!80000 INVISIBLE */` to invisible secondary and full-text index
definitions. Primary keys are always visible.

## Option Tail Semantics

For this phase, admitted alter-option tails are compatibility assertions only:

- `ALGORITHM=DEFAULT`, `ALGORITHM=INPLACE`, and `ALGORITHM=COPY` are accepted;
- `ALGORITHM=INSTANT` is rejected using the existing unsupported-algorithm
  diagnostic path;
- `LOCK=DEFAULT`, `LOCK=NONE`, `LOCK=SHARED`, and `LOCK=EXCLUSIVE` are
  accepted;
- repeated or conflicting options follow the existing MyLite alter-option
  diagnostics.

The options do not change physical execution because the operation is
catalog-only. MyLite still reports MySQL-compatible affected rows for the
admitted option subset: `COPY` reports the current table row count and other
accepted algorithms report zero.

## Tests

Fast C tests cover:

- parser acceptance for visible/invisible syntax, quoted names,
  schema-qualified table names, and option tails;
- parser rejection of `ALTER KEY`, unquoted `PRIMARY`, qualified index names,
  and multi-action shapes;
- successful visible-to-invisible, invisible-to-visible, and idempotent
  operations;
- `SHOW INDEX`, `INFORMATION_SCHEMA.STATISTICS`, and `SHOW CREATE TABLE`
  metadata;
- persistence across close/reopen and `CREATE TABLE ... LIKE`;
- rename preserving visibility;
- full-text metadata-only indexes;
- unique duplicate enforcement and foreign-key enforcement while invisible;
- missing default schema, unknown schema, unknown table, unknown index, and
  quoted-primary diagnostics;
- catalog migration from the previous schema version defaulting existing
  indexes to visible;
- `.mylite` preamble preservation.

The MySQL expectation script verifies the user-visible behavior above against
MySQL 8.4.9. Existing parser, catalog, index lifecycle, show/index metadata,
foreign-key, and workflow checks remain part of the release gate.

## Compatibility Notes

This feature is partial. MyLite now accepts and persists explicit index
visibility changes for existing persistent base-table secondary descriptors, but
does not claim full invisible-index optimizer behavior or creation-time
visibility options. Documentation must continue to mark `CREATE INDEX` options
and broader index option grammar as unsupported until those surfaces are
specified and tested separately.
