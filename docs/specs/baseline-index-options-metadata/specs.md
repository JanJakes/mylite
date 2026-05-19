# Baseline Index Options Metadata

## Summary

This phase extends existing descriptor-owned index lifecycle support with a
small MySQL-compatible index-option metadata slice:

```sql
CREATE TABLE t (..., KEY k [USING {BTREE|HASH}] (col[, ...]) [index_option] ...)
CREATE TABLE t (..., UNIQUE KEY k [USING {BTREE|HASH}] (col[, ...]) [index_option] ...)
CREATE TABLE t (..., FULLTEXT KEY k (col[, ...]) [comment_or_visibility] ...)
ALTER TABLE t ADD INDEX k [USING {BTREE|HASH}] (col[, ...]) [index_option] ...
ALTER TABLE t ADD UNIQUE KEY k [USING {BTREE|HASH}] (col[, ...]) [index_option] ...
ALTER TABLE t ADD FULLTEXT KEY k (col[, ...]) [comment_or_visibility] ...
CREATE INDEX k [USING {BTREE|HASH}] ON t (col[, ...]) [index_option] ...
CREATE UNIQUE INDEX k [USING {BTREE|HASH}] ON t (col[, ...]) [index_option] ...
CREATE FULLTEXT INDEX k ON t (col[, ...]) [comment_or_visibility] ...
```

For this slice, `index_option` is limited to `USING BTREE`, `USING HASH`,
`COMMENT 'string'`, `VISIBLE`, and `INVISIBLE`. MyLite continues to expose
InnoDB-shaped metadata: explicit `USING BTREE` is preserved for
`SHOW CREATE TABLE`; `USING HASH` succeeds with MySQL's InnoDB fallback note and
stores a normal BTREE descriptor without rendering an explicit `USING` clause.

This is not full MySQL index-option support. `SPATIAL`, `WITH PARSER`,
`KEY_BLOCK_SIZE`, engine attributes, algorithm/lock options on standalone
`CREATE INDEX`, functional key parts, expression key parts, optimizer semantics,
and real hash or spatial storage remain deferred.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing index lifecycle specs:
  `docs/specs/baseline-secondary-index-lifecycle/specs.md`,
  `docs/specs/baseline-unique-index-lifecycle/specs.md`,
  `docs/specs/baseline-create-index-lifecycle/specs.md`,
  `docs/specs/baseline-alter-table-add-index-lifecycle/specs.md`,
  `docs/specs/baseline-fulltext-index-metadata/specs.md`
- MySQL 8.4 Reference Manual, `CREATE TABLE` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/create-table.html>
- MySQL 8.4 Reference Manual, `ALTER TABLE` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/alter-table.html>
- MySQL 8.4 Reference Manual, `CREATE INDEX` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/create-index.html>
- MySQL 8.4 Reference Manual, `SHOW INDEX` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/show-index.html>
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.STATISTICS` table:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-statistics-table.html>
- Observed MySQL 8.4.9 behavior recorded by
  `packages/libmylite/tests/mysql_baseline_index_options_metadata_expectations.sh`.

This specification is independently authored from official MySQL 8.4
documentation, observed MySQL 8.4.9 behavior, public SQLite APIs, and existing
MyLite source. It does not copy MySQL, MariaDB, Percona, SQLite implementation
internals, or other restrictively licensed implementation sources.

## MySQL 8.4.9 Observations

Runtime probes establish:

- Table-level, `ALTER TABLE ... ADD`, and standalone `CREATE INDEX` forms accept
  explicit `USING BTREE` before the key-part list and as a post-key option.
- MySQL prefers rendering `USING BTREE` after the key-part list in
  `SHOW CREATE TABLE`, even when it was written before the list.
- Omitted index type renders no `USING` clause for ordinary InnoDB BTREE
  secondary and unique indexes.
- `USING HASH` on InnoDB succeeds, emits note `3502 / HY000`
  (`This storage engine does not support the HASH index algorithm, storage
  engine default was used instead.`), reports `Index_type = BTREE`, and does
  not render `USING BTREE` in `SHOW CREATE TABLE`.
- `COMMENT 'text'` is stored in `SHOW INDEX.Index_comment` and
  `INFORMATION_SCHEMA.STATISTICS.INDEX_COMMENT`; `SHOW INDEX.Comment` and
  `INFORMATION_SCHEMA.STATISTICS.COMMENT` remain empty for this slice.
- Index comments may be up to 1024 characters. A longer comment fails with
  `1688 / HY000` and message shape `Comment for index 'name' is too long
  (max = 1024)`.
- `VISIBLE` is accepted and is the default visible state. It is not rendered by
  `SHOW CREATE TABLE`.
- `INVISIBLE` is accepted for supported non-primary indexes and renders as
  MySQL's versioned invisible-index comment. `SHOW INDEX.Visible` and
  `INFORMATION_SCHEMA.STATISTICS.IS_VISIBLE` report `NO`.
- Fulltext creation accepts `COMMENT`, `VISIBLE`, and `INVISIBLE`. Adding the
  first fulltext index to an existing InnoDB table still emits warning `124`
  for the FTS document-id rebuild.
- `CREATE FULLTEXT INDEX ... USING BTREE` and `FULLTEXT KEY ... USING BTREE`
  are syntax errors in the observed runtime.
- Explicit `DESC` key parts with `USING HASH` fail with
  `1221 / HY000` wrong usage. Because MyLite normalizes `USING HASH` to the
  InnoDB default, it must reject hash plus explicit key-part direction before
  catalog mutation.

## Scope

Supported:

- persistent MyLite base tables through existing table-level,
  `ALTER TABLE ... ADD`, and standalone `CREATE [UNIQUE|FULLTEXT] INDEX`
  lifecycle paths;
- existing supported key-part counts, prefix lengths, and `ASC`/`DESC` limits
  for secondary and unique indexes;
- existing supported fulltext metadata-only key parts;
- leading `USING BTREE` or `USING HASH` for non-fulltext secondary and unique
  definitions;
- trailing repeated index options for non-fulltext secondary and unique
  definitions:
  - `USING BTREE`;
  - `USING HASH`;
  - `COMMENT 'string'`;
  - `VISIBLE`;
  - `INVISIBLE`;
- trailing `COMMENT 'string'`, `VISIBLE`, and `INVISIBLE` for fulltext
  definitions;
- duplicate options use the final effective value for the same logical property,
  matching the observed MySQL behavior for repeated comments and visibility in
  nearby DDL surfaces;
- ordinary decoded string literals for index comments using the current session
  SQL-mode escape rules;
- valid UTF-8, NUL-free index comments up to 1024 characters;
- durable catalog storage of index comment text and explicit BTREE rendering
  metadata;
- cloning by `CREATE TABLE ... LIKE`, persistence after close/reopen, and
  preservation through existing rename, drop, and visibility-toggle paths;
- descriptor-driven `SHOW CREATE TABLE`, `SHOW INDEX`, and
  `INFORMATION_SCHEMA.STATISTICS` metadata;
- `USING HASH` InnoDB fallback note `3502` without real hash storage;
- existing no-result DDL public result conventions, affected-row behavior, and
  warning counts plus the new hash fallback note where applicable.

Deferred:

- `SPATIAL` indexes and `CREATE SPATIAL INDEX`;
- primary-key index options;
- `WITH PARSER`, `KEY_BLOCK_SIZE`, `ENGINE_ATTRIBUTE`,
  `SECONDARY_ENGINE_ATTRIBUTE`, standalone `CREATE INDEX` `ALGORITHM` / `LOCK`,
  and broader online-DDL option semantics;
- fulltext parser plugins, fulltext search, tokenizer metadata, and fulltext
  `USING` clauses;
- real hash index storage, hash optimizer behavior, MEMORY-engine semantics,
  RTREE metadata, and hash indexes with explicit key-part order;
- functional, expression, table-qualified, ordinal, and multi-valued key parts
  beyond existing supported key-part slices;
- index comments containing embedded NUL bytes, invalid UTF-8, non-string
  values, character-set introducers, or protocol-grade character metadata;
- privilege semantics, implicit commit emulation, optimizer-use guarantees,
  and storage-engine-specific index statistics.

## Ownership Boundaries

- Public API: no new public ABI. Applications continue to use
  `mylite_execute()` and existing result/diagnostic accessors.
- Statement context: owns diagnostics reset, warning capture, affected rows,
  `ROW_COUNT()`, and cleanup. `USING HASH` fallback notes are statement
  diagnostics.
- Lexer/parser/AST: owns syntax admission and stores index-option nodes without
  descriptor or SQLite access.
- Analyzer/planner/runtime: resolves table, index names, key parts, option
  conflicts, comment text, visibility, and InnoDB hash fallback behavior before
  catalog or SQLite mutation.
- Catalog: MyLite index descriptors are authoritative for logical metadata.
  This phase extends them with comment text and explicit-BTREE render state.
- Result builder/introspection: renders `SHOW CREATE TABLE`, `SHOW INDEX`, and
  `INFORMATION_SCHEMA.STATISTICS` from descriptors only.
- Storage/VFS: `.mylite` preamble and shifted SQLite payload invariants are
  unchanged.
- SQLite physical storage: physical index creation remains ordinary SQLite
  `CREATE INDEX` / `CREATE UNIQUE INDEX` over stable generated names. No SQLite
  fork patch is required.

## Grammar

MyLite Lemon-syntax sketch:

```lemon
secondary_index_definition ::=
    KEY index_name_opt index_type_opt LPAREN secondary_index_part_list RPAREN
    secondary_index_option_list_opt.

secondary_index_definition ::=
    INDEX index_name_opt index_type_opt LPAREN secondary_index_part_list RPAREN
    secondary_index_option_list_opt.

unique_index_definition ::=
    UNIQUE unique_index_keyword_opt index_name_opt index_type_opt
    LPAREN secondary_index_part_list RPAREN secondary_index_option_list_opt.

fulltext_index_definition ::=
    FULLTEXT fulltext_index_keyword_opt index_name_opt
    LPAREN secondary_index_part_list RPAREN fulltext_index_option_list_opt.

create_index_statement ::=
    CREATE INDEX identifier index_type_opt ON table_name
    LPAREN secondary_index_part_list RPAREN secondary_index_option_list_opt.

create_index_statement ::=
    CREATE UNIQUE INDEX identifier index_type_opt ON table_name
    LPAREN secondary_index_part_list RPAREN secondary_index_option_list_opt.

create_index_statement ::=
    CREATE FULLTEXT INDEX identifier ON table_name
    LPAREN secondary_index_part_list RPAREN fulltext_index_option_list_opt.

index_type_opt ::= .
index_type_opt ::= index_type_option.
index_type_option ::= USING BTREE.
index_type_option ::= USING HASH.

secondary_index_option_list_opt ::= .
secondary_index_option_list_opt ::= secondary_index_option_list.
secondary_index_option_list ::= secondary_index_option.
secondary_index_option_list ::= secondary_index_option_list secondary_index_option.
secondary_index_option ::= index_type_option.
secondary_index_option ::= COMMENT STRING.
secondary_index_option ::= VISIBLE.
secondary_index_option ::= INVISIBLE.

fulltext_index_option_list_opt ::= .
fulltext_index_option_list_opt ::= fulltext_index_option_list.
fulltext_index_option_list ::= fulltext_index_option.
fulltext_index_option_list ::= fulltext_index_option_list fulltext_index_option.
fulltext_index_option ::= COMMENT STRING.
fulltext_index_option ::= VISIBLE.
fulltext_index_option ::= INVISIBLE.
```

The parser must not admit `USING` in fulltext index definitions for this slice.
Unsupported option keywords outside the admitted lists remain syntax errors or
existing deterministic unsupported diagnostics.

## Planning Semantics

The planner resolves options after resolving the target table and key parts:

- `USING BTREE` sets `show_create_explicit_btree = true`.
- Omitted type sets `show_create_explicit_btree = false`.
- `USING HASH` sets `show_create_explicit_btree = false`, stages warning 3502,
  and uses the same SQLite physical index shape as ordinary BTREE indexes.
- `USING HASH` with any explicit key-part direction (`ASC` or `DESC`) is
  rejected with `1221 / HY000`.
- A later `USING BTREE` or `USING HASH` option overrides an earlier type option
  for the final render state and warning decision.
- `COMMENT 'string'` replaces any earlier index comment in the same definition.
- `VISIBLE` / `INVISIBLE` replace any earlier visibility option in the same
  definition.
- Fulltext definitions inherit the existing metadata-only physical behavior and
  may store comment and visibility.

Index comments are decoded with the same string-literal policy as table
comments, then validated as NUL-free UTF-8. The limit is 1024 characters and
the internal descriptor capacity is sized for 1024 `utf8mb4` characters plus
the NUL terminator.

## Catalog and Migration

The catalog schema advances by adding two columns to `_mylite_catalog_indexes`:

- `comment TEXT NOT NULL DEFAULT ''`;
- `show_create_explicit_btree INTEGER NOT NULL DEFAULT 0 CHECK(... IN (0,1))`.

The in-memory index descriptor gains matching fields. Existing files migrate
with empty comments and `show_create_explicit_btree = 0`, preserving current
metadata exactly.

`CREATE TABLE ... LIKE` copies index comments, explicit-BTREE render state, and
visibility along with existing index descriptors. Rename and drop operations
preserve or remove the new fields through the existing descriptor rows.

## Introspection

`SHOW CREATE TABLE` renders:

- `USING BTREE` after the key-part list only when the descriptor stores
  `show_create_explicit_btree`;
- `COMMENT 'text'` after the key-part list or `USING BTREE` when the descriptor
  comment is nonempty;
- the existing versioned invisible-index comment after the comment clause when
  the descriptor is invisible.

For example:

```sql
KEY `k` (`v`(3)) USING BTREE COMMENT 'hello' /*!80000 INVISIBLE */
FULLTEXT KEY `ft` (`body`) COMMENT 'full' /*!80000 INVISIBLE */
```

`SHOW INDEX` and `INFORMATION_SCHEMA.STATISTICS` continue to report
`Index_type` / `INDEX_TYPE` as `BTREE` for ordinary supported secondary and
unique indexes and `FULLTEXT` for fulltext descriptors. They report the
descriptor comment in `Index_comment` / `INDEX_COMMENT`. `Comment` /
`COMMENT` stays empty for this slice.

## Physical SQLite Handling

Generated SQLite remains descriptor-built and quoted:

```sql
CREATE [UNIQUE] INDEX "_mylite_user_index_<index_id>"
ON "_mylite_user_table_<table_id>" ("physical_column"...);
```

Index comments, visibility, and explicit-BTREE render state do not change the
physical SQLite DDL. `USING HASH` never creates a hash index in this slice.
No SQLite optional `CREATE INDEX` extensions, PRAGMAs, or fork hooks are used.

## Diagnostics and Warnings

Supported success:

- non-hash supported forms keep existing affected-row behavior and
  `warning_count == 0`, except fulltext add keeps existing warning 124 when it
  first initializes fulltext document-id state;
- `USING HASH` appends note `3502 / HY000` and otherwise succeeds when no
  explicit key-part order is present.

Diagnostics:

- syntax errors: existing `1064 / 42000` parser diagnostics;
- missing default schema, unknown schema/table, reserved names, duplicate index
  names, unknown key columns, unsupported key columns, duplicate key parts,
  key length, existing unique duplicate rows, and fulltext limitations: existing
  diagnostics from the underlying lifecycle paths;
- too-long index comment: `1688 / HY000`, `Comment for index 'name' is too long
  (max = 1024)`;
- index comment with embedded NUL: deterministic MyLite unsupported diagnostic
  `index comments do not support NUL bytes`;
- invalid UTF-8 index comment: deterministic MyLite unsupported diagnostic
  `index comments support only valid UTF-8 text`;
- `USING HASH` plus explicit key-part order: `1221 / HY000`,
  `Incorrect usage of spatial/fulltext/hash index and explicit index order`;
- unsupported options such as `WITH PARSER`, `KEY_BLOCK_SIZE`, engine
  attributes, and `SPATIAL`: syntax errors or deterministic unsupported
  diagnostics, but not silent acceptance.

## Tests

Add a fast C runtime test, preferably `runtime_index_options_metadata`, and a
MySQL 8.4.9 expectation script. Cover:

- `CREATE TABLE`, `ALTER TABLE ... ADD`, and `CREATE INDEX` with explicit
  `USING BTREE` and comments for nonunique, unique, and fulltext indexes;
- leading and trailing `USING BTREE`, and rendering normalization to trailing
  `USING BTREE`;
- `VISIBLE` default/explicit and `INVISIBLE` creation-time metadata;
- `USING HASH` success, warning 3502, BTREE metadata, and omitted `USING` in
  `SHOW CREATE TABLE`;
- `USING HASH` plus explicit key-part order rejected with 1221;
- comment escaping, empty comments, duplicate comment options with final value,
  too-long comments, NUL comments, and invalid non-string comment forms;
- `SHOW CREATE TABLE`, `SHOW INDEX`, and
  `INFORMATION_SCHEMA.STATISTICS` values;
- close/reopen persistence, `CREATE TABLE ... LIKE` cloning, rename/drop
  preservation/removal, and independent file-backed handles;
- physical `.mylite` preamble preservation;
- existing index lifecycle, fulltext, visibility, parser, catalog, and file
  format tests still passing.

