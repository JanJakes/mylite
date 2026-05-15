# Baseline RENAME INDEX Lifecycle

## Summary

This phase adds the narrow index-rename table action for persistent MyLite base
tables:

```sql
ALTER TABLE table_name RENAME INDEX old_index_name TO new_index_name
ALTER TABLE table_name RENAME KEY old_index_name TO new_index_name
```

The operation changes the MyLite logical descriptor name for one existing
secondary index and preserves table rows, index key parts, uniqueness,
foreign-key references by descriptor id, and the generated SQLite physical
index. It builds on the descriptor-owned create/add/drop index lifecycles,
descending and prefix key-part metadata, limited foreign keys, file-backed
storage, and descriptor-driven `SHOW` / `INFORMATION_SCHEMA` metadata.

This is intentionally not full MySQL `ALTER TABLE`. Multi-action `ALTER`,
`ALGORITHM`, `LOCK`, visibility changes, primary-key rename, temporary tables,
views, partitions, privileges, and optimizer guarantees remain separate work.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline secondary, unique, prefix, and descending index specs:
  `docs/specs/baseline-secondary-index-lifecycle/specs.md`,
  `docs/specs/baseline-unique-index-lifecycle/specs.md`,
  `docs/specs/baseline-index-prefix-key-parts/specs.md`,
  `docs/specs/baseline-descending-index-key-parts/specs.md`
- Baseline `ALTER TABLE ... ADD/DROP INDEX` specs:
  `docs/specs/baseline-alter-table-add-index-lifecycle/specs.md`,
  `docs/specs/baseline-alter-table-drop-index-lifecycle/specs.md`
- MySQL 8.4 Reference Manual, `ALTER TABLE` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/alter-table.html>
- Observed MySQL 8.4.9 runtime behavior recorded by
  `packages/libmylite/tests/mysql_baseline_rename_index_lifecycle_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Observations

Runtime probes for this phase establish:

- `ALTER TABLE t RENAME INDEX old TO new` and
  `ALTER TABLE t RENAME KEY old TO new` are synonyms.
- Successful renames report `ROW_COUNT() == 0` and `@@warning_count == 0`.
- Table contents are unchanged.
- The renamed index appears under the new name in `SHOW CREATE TABLE`,
  `SHOW INDEX`, and `INFORMATION_SCHEMA.STATISTICS`.
- Unique, nonunique, prefix, and descending index attributes are preserved.
- Duplicate checks for renamed unique indexes report the new key name.
- Exact same-name and case-only renames are accepted and report zero affected
  rows/warnings; case-only renames update the displayed spelling.
- The old name must resolve to an existing table-local index. Missing names
  fail with `1176 / 42000`.
- The new name must not duplicate another resulting index name. Duplicate
  names fail with `1061 / 42000`.
- Quoted `PRIMARY` as either the old or new name fails with
  `1280 / 42000`; unquoted `PRIMARY` in either position is a syntax error
  because `PRIMARY` is reserved in this grammar position.
- MySQL accepts multi-action forms and `ALGORITHM` / `LOCK` clauses; they are
  deliberately outside this phase.

## Scope

Supported:

- persistent MyLite base tables only;
- one `ALTER TABLE table_name RENAME INDEX old_index_name TO new_index_name`
  action;
- one `ALTER TABLE table_name RENAME KEY old_index_name TO new_index_name`
  action;
- unqualified and schema-qualified target table names using the existing
  selected/default schema policy;
- one unqualified old index name or quoted old index name resolving to an
  existing descriptor-owned secondary index;
- one unqualified new index name or quoted new index name that does not
  collide case-insensitively with another table-local index descriptor;
- exact same-name no-op and case-only spelling changes;
- supported secondary descriptors, including both `is_unique = 0` nonunique
  indexes and `is_unique = 1` unique indexes;
- existing full-column, composite, prefix, and descending secondary descriptors
  already admitted by prior slices;
- indexes referenced by supported foreign-key descriptors, because foreign keys
  reference index ids rather than logical names;
- descriptor-backed metadata after the rename through `SHOW COLUMNS`,
  `SHOW CREATE TABLE`, `SHOW INDEX`, limited `INFORMATION_SCHEMA.STATISTICS`,
  `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`, `KEY_COLUMN_USAGE`, and
  `REFERENTIAL_CONSTRAINTS`;
- DML behavior after the rename, including unique duplicate diagnostics using
  the renamed key name;
- reopen persistence, table rename/drop interaction, independent file-backed
  handles, and `.mylite` preamble preservation;
- no-result DDL result shape with `affected_rows == 0` and
  `warning_count == 0` for successful supported renames.

Deferred:

- primary-key rename through `RENAME INDEX PRIMARY`, `RENAME KEY PRIMARY`, or
  quoted `` `PRIMARY` `` names;
- multi-action `ALTER TABLE`;
- `ALGORITHM`, `LOCK`, partition clauses, index visibility, comments, parser
  options, functional indexes, fulltext/spatial indexes, temporary tables,
  views, triggers, cascades, privileges, metadata locks, and optimizer/index-use
  guarantees;
- standalone rename syntax, because MySQL exposes this action through
  `ALTER TABLE`;
- SQLite fork patches.

## Ownership Boundaries

- Public API: no ABI change. Applications use `mylite_execute()` and existing
  result/diagnostic accessors.
- Statement context: owns diagnostics reset, warning count, affected rows,
  `ROW_COUNT()`, implicit user-transaction commit boundaries, and cleanup.
- Lexer/parser/AST: admits the narrow single-action syntax and preserves the
  target table, old index name, and new index name nodes. It does not inspect
  descriptors or SQLite schema.
- Analyzer/planner/runtime: resolves schema, table, old index, and new name
  against MyLite descriptors before any catalog write. It rejects unsupported
  object kinds, primary indexes, duplicate names, and reserved MyLite names.
- Catalog: MyLite index descriptors are authoritative for logical index state.
  Renaming updates the descriptor name and descriptor generation; index-column
  rows and physical names are unchanged.
- Result/introspection builders: existing descriptor-driven metadata paths
  render the post-rename descriptor state.
- Storage/VFS: `.mylite` preamble and shifted SQLite payload invariants are
  unchanged.
- SQLite physical storage: no physical DDL is needed. The generated SQLite
  index keeps its stable `_mylite_user_index_<index_id>` name.

## Supported Grammar

MyLite admits only:

```sql
ALTER TABLE table_name RENAME INDEX old_index_name TO new_index_name
ALTER TABLE table_name RENAME KEY old_index_name TO new_index_name
```

The table name may be unqualified or schema-qualified. Old and new index names
are each one identifier or quoted identifier. Unquoted `PRIMARY` is not an
identifier in this grammar position and remains a parse error.

MyLite Lemon-style sketch:

```lemon
statement(A) ::= alter_table_rename_index_statement(B). {
    A = B;
}

alter_table_rename_index_statement(A) ::=
    ALTER TABLE table_name(T) RENAME rename_index_keyword old_identifier(O)
    TO new_identifier(N). {
    A = mylite_sql_parser_make_alter_table_rename_index_statement(state, T, O, N);
}

rename_index_keyword ::= INDEX.
rename_index_keyword ::= KEY.
old_identifier ::= identifier.
new_identifier ::= identifier.
```

The AST kind remains distinct from table and column renames even though the
runtime may reuse descriptor resolution helpers.

## Resolution Semantics

Target table resolution follows existing table policy:

- unqualified table names require a selected/default schema;
- schema-qualified table names use the explicit schema and do not require a
  selected schema;
- missing default schema fails with `1046 / 3D000`;
- unknown schemas fail with `1049 / 42000`;
- unknown tables fail with `1146 / 42S02`;
- reserved `_mylite_*` schema or table names are rejected before any catalog
  write;
- only persistent base-table descriptors are supported.

Index resolution is descriptor-owned:

- load current table columns and index descriptors from the MyLite catalog;
- match the old logical index name case-insensitively against table-local
  descriptor index names;
- reject a missing old name with `1176 / 42000`;
- reject a resolved primary-key descriptor with `1280 / 42000` for quoted
  `PRIMARY` and keep unquoted `PRIMARY` as a parser error;
- reject a new name that case-insensitively matches any other table-local
  index descriptor with `1061 / 42000`;
- allow a new name that matches the same resolved index, including exact
  same-name and case-only spelling changes;
- reject a new name equal to `PRIMARY` with `1280 / 42000`;
- reject reserved `_mylite_*` new index names before any catalog write;
- do not inspect SQLite schema metadata to discover, validate, classify, or
  rename indexes.

Current descriptor identifier comparison remains the existing ASCII
case-insensitive policy. This phase does not add collation-aware identifier
matching.

## Descriptor and Catalog Semantics

On success:

- update the resolved `_mylite_catalog_indexes.name` value;
- increment the index descriptor version and set its updated catalog
  generation;
- update the owning table descriptor identity/generation so descriptor caches
  observe the renamed index set;
- preserve the index id, kind, uniqueness, physical name, index-column
  descriptors, prefix lengths, sort directions, row values, table physical
  name, column descriptors, primary-key descriptor, foreign-key descriptors,
  auto-increment counter, and schema descriptor;
- do not increment `sqlite_schema_generation`, because the physical SQLite
  schema is unchanged.

`CREATE TABLE ... LIKE` after the rename clones the renamed descriptor. Existing
foreign-key descriptors remain valid because they reference index ids. Failed
planning or execution must leave catalog descriptors unchanged.

## Physical SQLite Handling

No SQLite SQL is generated for a supported rename. The physical index already
uses a stable generated name:

```text
_mylite_user_index_<index_id>
```

Rules:

- do not run SQLite `ALTER` or `DROP/CREATE INDEX` for logical renames;
- do not materialize rows in MyLite and do not rebuild physical tables;
- keep the catalog update inside a MyLite catalog mutation transaction;
- surface `MYLITE_NOMEM` for allocation failure and deterministic internal
  diagnostics for unexpected catalog failures;
- use public SQLite APIs only. No SQLite fork patch is required.

## Result Semantics

Successful execution returns through existing non-row statement conventions:

- no row result set;
- `affected_rows == 0`;
- `warning_count == 0`;
- statement diagnostics remain clear.

## Diagnostics

The implementation must cover deterministic diagnostics for:

- parser errors and unsupported grammar (`PRIMARY` unquoted, missing `TO`,
  qualified index names, multi-action `ALTER`, `ALGORITHM`, `LOCK`, index
  visibility, partition/options syntax, and other unsupported clauses);
- missing default schema, unknown schema, unknown table, and reserved
  `_mylite_*` schema/table names;
- unsupported target object kind once non-base-table descriptors exist;
- unknown old index names with MySQL-compatible `1176 / 42000`;
- new names that duplicate another index with `1061 / 42000`;
- old or new quoted `PRIMARY` names with `1280 / 42000`;
- reserved `_mylite_*` index names with a MyLite-specific reserved-name
  diagnostic;
- catalog update failures, allocation failures, and public API misuse through
  existing public API behavior.

## Performance and Architecture Notes

This is an O(number of table indexes) planner plus one catalog row update. It
does not scan or materialize user rows, rebuild tables, or recreate SQLite
indexes. Keeping the physical index name stable preserves SQLite's b-tree and
keeps the operation close to metadata-only cost while MyLite descriptors remain
the logical authority.

## Test Plan

Fast C tests should cover:

- parser acceptance for `RENAME INDEX` and `RENAME KEY` with unqualified and
  schema-qualified table targets and quoted names;
- parser rejection for unquoted `PRIMARY`, missing `TO`, qualified index names,
  multiple actions, `ALGORITHM`, and `LOCK`;
- successful rename of nonunique, unique, prefix, and descending indexes;
- exact same-name and case-only rename;
- metadata through `SHOW CREATE TABLE`, `SHOW INDEX`, and limited
  `INFORMATION_SCHEMA.STATISTICS`, `TABLE_CONSTRAINTS`, and
  `KEY_COLUMN_USAGE`;
- unique duplicate enforcement and duplicate diagnostics after rename;
- foreign-key-backed index rename without breaking enforcement or FK metadata;
- missing default schema, unknown schema, unknown table, reserved table names,
  unknown index, duplicate new name, quoted `PRIMARY`, and reserved index-name
  diagnostics;
- reopen persistence, table rename/drop interaction, independent handles,
  `.mylite` preamble preservation, unchanged physical SQLite index count, and
  unchanged `sqlite_schema_generation` for successful renames;
- zero-initialized cleanup for the new plan object.

MySQL expectation coverage must verify every user-visible behavior introduced
by this phase against MySQL 8.4.9 before implementation.
