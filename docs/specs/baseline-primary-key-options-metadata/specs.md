# Baseline Primary-Key Options Metadata

## Summary

This phase extends descriptor-owned primary keys with the same narrow
MySQL-compatible option surface already implemented for supported secondary
indexes:

```sql
CREATE TABLE t (..., PRIMARY KEY [USING {BTREE|HASH}] (col[, ...]) [index_option] ...)
ALTER TABLE t ADD PRIMARY KEY [USING {BTREE|HASH}] (col[, ...]) [index_option] ...

index_option:
    USING {BTREE|HASH}
  | COMMENT 'string'
  | VISIBLE
  | INVISIBLE
```

The feature remains metadata-focused. Primary keys continue to be backed by a
generated SQLite unique index over stable MyLite physical names, and MyLite
catalog descriptors remain the authority for `SHOW CREATE TABLE`, `SHOW INDEX`,
and `INFORMATION_SCHEMA.STATISTICS`.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing index option phase:
  `docs/specs/baseline-index-options-metadata/specs.md`
- Existing primary-key lifecycle:
  `docs/specs/baseline-primary-key-lifecycle/specs.md`
- MySQL 8.4 Reference Manual, `CREATE TABLE` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/create-table.html>
- MySQL 8.4 Reference Manual, `ALTER TABLE` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/alter-table.html>
- MySQL 8.4 Reference Manual, `SHOW INDEX` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/show-index.html>
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.STATISTICS` table:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-statistics-table.html>
- Observed MySQL 8.4.9 behavior recorded by
  `packages/libmylite/tests/mysql_baseline_primary_key_options_metadata_expectations.sh`.

This specification is independently authored from official MySQL 8.4
documentation, observed MySQL 8.4.9 behavior, public SQLite APIs, and existing
MyLite source. It does not copy MySQL, MariaDB, Percona, SQLite implementation
internals, or other restrictively licensed implementation sources.

## MySQL 8.4.9 Observations

Runtime probes establish:

- Table-level and `ALTER TABLE ... ADD PRIMARY KEY` forms accept explicit
  `USING BTREE` before the key-part list and as a trailing option.
- `SHOW CREATE TABLE` renders explicit BTREE after the key-part list:
  `PRIMARY KEY (...) USING BTREE`.
- Omitted primary-key type renders no `USING` clause.
- `USING HASH` on InnoDB primary keys succeeds, emits note `3502 / HY000`
  (`This storage engine does not support the HASH index algorithm, storage
  engine default was used instead.`), reports `Index_type = BTREE`, and does
  not render `USING BTREE` in `SHOW CREATE TABLE`.
- `USING HASH` with any explicit key-part direction fails with
  `1221 / HY000` wrong usage.
- `USING RTREE` on the admitted integer primary-key path fails with
  `1687 / 42000`, `A SPATIAL index may only contain a geometrical type
  column`.
- `COMMENT 'text'` is stored in `SHOW INDEX.Index_comment` and
  `INFORMATION_SCHEMA.STATISTICS.INDEX_COMMENT`; `SHOW INDEX.Comment` and
  `INFORMATION_SCHEMA.STATISTICS.COMMENT` remain empty.
- Primary-key index comments may be up to 1024 characters. Longer comments fail
  with `1688 / HY000` and the message shape `Comment for index 'PRIMARY' is too
  long (max = 1024)`.
- `VISIBLE` is accepted and is the default. It is not rendered by
  `SHOW CREATE TABLE`.
- final effective `INVISIBLE` is rejected for primary keys with `3522 / HY000`
  and message `A primary key index cannot be invisible.` A later `VISIBLE`
  option makes the effective state visible, matching the repeated-option rule.
- Repeated options use the final effective value for the same option class.
  `USING HASH ... USING BTREE` stores explicit BTREE with no hash fallback note;
  `USING BTREE ... USING HASH` stores ordinary BTREE metadata and emits note
  `3502`.

## Scope

Supported:

- persistent MyLite base tables through existing `CREATE TABLE` table-level
  primary-key definitions and `ALTER TABLE ... ADD PRIMARY KEY`;
- existing primary-key key-part types, counts, key-length limits, duplicate
  validation, `NOT NULL` conversion, string-key ASCII limits, and `ASC`/`DESC`
  direction metadata;
- leading `USING BTREE` or `USING HASH`;
- trailing repeated `USING BTREE`, `USING HASH`, `COMMENT 'string'`, and
  `VISIBLE`;
- trailing `INVISIBLE` syntax admission followed by MySQL-compatible rejection
  only when the final effective visibility is invisible;
- ordinary decoded string literals for primary-key comments using current
  session SQL-mode escape rules;
- valid UTF-8, NUL-free primary-key comments up to 1024 characters;
- durable catalog storage of primary-key comment text and explicit-BTREE render
  metadata;
- cloning by `CREATE TABLE ... LIKE`, persistence after close/reopen, and
  preservation through supported table rename and drop paths;
- descriptor-driven `SHOW CREATE TABLE`, `SHOW INDEX`, and
  `INFORMATION_SCHEMA.STATISTICS` metadata;
- `USING HASH` InnoDB fallback note `3502` without real hash storage;
- existing no-result DDL public result conventions, affected-row behavior, and
  warning counts plus the new hash fallback note where applicable.

Deferred:

- primary prefix key parts;
- named primary-key constraints;
- inline column primary-key index options;
- functional, expression, table-qualified, ordinal, and multi-valued primary
  key parts;
- real hash index storage, hash optimizer behavior, MEMORY-engine semantics,
  and hash indexes with explicit key-part order;
- RTREE primary keys and spatial primary-key semantics;
- primary comments containing embedded NUL bytes, invalid UTF-8, non-string
  values, character-set introducers, or protocol-grade character metadata;
- optimizer-use guarantees, privilege semantics, and implicit commit emulation.

## Ownership Boundaries

- Public API: no new public ABI. Applications continue to use
  `mylite_execute()` and existing result/diagnostic accessors.
- Statement context: owns diagnostics reset, warning capture, affected rows,
  `ROW_COUNT()`, and cleanup. `USING HASH` fallback notes are statement
  diagnostics.
- Lexer/parser/AST: owns syntax admission and stores primary-key option nodes
  without descriptor, storage, or SQLite access.
- Analyzer/planner/runtime: resolves table and key-part names, validates
  primary-key option semantics, rejects invisible primary keys, decodes comments,
  and stages hash fallback warnings before catalog or SQLite mutation.
- Catalog: MyLite primary-key descriptors are authoritative for logical
  metadata. This phase stores comment text and explicit-BTREE render state in
  existing index descriptor fields.
- Result builder/introspection: renders `SHOW CREATE TABLE`, `SHOW INDEX`, and
  `INFORMATION_SCHEMA.STATISTICS` from descriptors only.
- Storage/VFS: `.mylite` preamble and shifted SQLite payload invariants are
  unchanged.
- SQLite physical storage: physical primary-key enforcement remains an ordinary
  generated SQLite unique index. No SQLite fork patch is required.

## Grammar

MyLite Lemon-syntax sketch:

```lemon
primary_key_definition ::=
    PRIMARY KEY index_type_opt LPAREN primary_key_part_list RPAREN
    index_option_list_opt.

index_type_opt ::= .
index_type_opt ::= USING identifier.

index_option_list_opt ::= .
index_option_list_opt ::= index_option_list.
index_option_list ::= index_option.
index_option_list ::= index_option_list index_option.

index_option ::= USING identifier.
index_option ::= COMMENT STRING.
index_option ::= VISIBLE.
index_option ::= INVISIBLE.
```

The parser admits option syntax; the runtime planner supports `BTREE` and
`HASH`, rejects `RTREE` on the admitted non-spatial primary-key subset with
MySQL's spatial non-geometric diagnostic, and rejects other values with the
existing deterministic parse diagnostic.

## Planning Semantics

The planner resolves primary-key options after resolving the target table
context and before catalog mutation:

- `USING BTREE` sets `show_create_explicit_btree = true`.
- omitted type sets `show_create_explicit_btree = false`.
- `USING HASH` sets `show_create_explicit_btree = false`, stages warning 3502,
  and uses the same SQLite physical index shape as ordinary BTREE primary keys.
- `USING HASH` with any explicit key-part direction (`ASC` or `DESC`) is
  rejected with `1221 / HY000`.
- a later `USING BTREE` or `USING HASH` option overrides an earlier type option
  for final render state and warning decision.
- final effective `USING RTREE` is rejected with `1687 / 42000` on the
  admitted integer and ASCII-string primary-key subset.
- `COMMENT 'string'` replaces any earlier primary-key comment in the same
  definition.
- `VISIBLE` preserves or restores the required visible state.
- final effective `INVISIBLE` is rejected with `3522 / HY000`.

All existing primary-key validation remains in force: unknown columns, reserved
column names, duplicate parts, nullable key parts, unsupported type families,
unsupported string-key values, key length, existing duplicate rows, and existing
`NULL` rows continue to use current diagnostics.

## Metadata and SQL Rendering

Catalog insertion for the primary index stores:

- name `PRIMARY`;
- generated physical index name;
- kind `PRIMARY`;
- uniqueness `true`;
- visibility `true`;
- decoded comment text;
- explicit-BTREE render flag.

`SHOW CREATE TABLE` renders:

```sql
PRIMARY KEY (`id`) USING BTREE COMMENT 'text'
```

when the descriptor has explicit BTREE and a nonempty comment. It omits visible
state and never renders an invisible primary-key marker because invisible
primary keys are rejected.

`SHOW INDEX` and `INFORMATION_SCHEMA.STATISTICS` continue to report
`Index_type = BTREE`, empty storage-engine `Comment`, descriptor
`Index_comment`, and `Visible` / `IS_VISIBLE = YES` for primary keys.

## Diagnostics

This phase reuses existing MyLite/MySQL-shaped diagnostics:

- syntax errors and unsupported option spellings: existing `1064 / 42000`;
- missing default schema, unknown schema, unknown table, non-base target table,
  and reserved target names: existing table-resolution diagnostics;
- duplicate primary key: existing `1068 / 42000`;
- unknown key columns: existing `1072 / 42000`;
- nullable primary-key part: existing `1171 / 42000`;
- duplicate key tuples during `ALTER TABLE ... ADD PRIMARY KEY`: existing
  duplicate-key diagnostic;
- `USING HASH` with explicit key-part order: `1221 / HY000`;
- primary-key `USING RTREE` over the admitted non-spatial key-part subset:
  `1687 / 42000`;
- primary-key `INVISIBLE`: `3522 / HY000`;
- overlong primary-key comments: `1688 / HY000`;
- comment NUL or invalid UTF-8: deterministic MyLite unsupported diagnostics;
- physical SQLite, catalog, allocation, or public API failures: existing
  internal/allocation/misuse diagnostics.

Supported in-range statements return zero affected rows, no result rows, and
zero warnings except for the documented `USING HASH` note.

## Performance and Storage

The new options do not materialize table rows. CREATE paths only extend logical
metadata captured during planning. `ALTER TABLE ... ADD PRIMARY KEY` continues
to perform the existing validation scans and generated SQLite unique-index
creation required for enforcement. `SHOW` and `INFORMATION_SCHEMA` paths read
the small descriptor sets already used for index introspection.

No new dependency or SQLite fork patch is required.
