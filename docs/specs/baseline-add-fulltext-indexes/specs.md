# Baseline Add FULLTEXT Indexes Specification

## Summary

This phase extends the existing metadata-only `FULLTEXT` index slice from
create-time table definitions to the two common add-index entry points:

```sql
ALTER TABLE table_name ADD FULLTEXT [KEY | INDEX] [index_name] (column[, ...])
CREATE FULLTEXT INDEX index_name ON table_name (column[, ...])
```

The feature remains metadata-only. It accepts WordPress- and migration-style
DDL, stores MyLite-owned descriptor metadata, exposes the descriptors through
the existing `SHOW` and limited `INFORMATION_SCHEMA` surfaces, and preserves
the current row storage. It does not implement full-text search, tokenization,
ranking, parser plugins, SQLite FTS tables, or optimizer behavior.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing metadata-only fulltext slice:
  `docs/specs/baseline-fulltext-index-metadata/specs.md`
- Existing add-index and standalone-index lifecycles:
  `docs/specs/baseline-alter-table-add-index-lifecycle/specs.md`,
  `docs/specs/baseline-create-index-lifecycle/specs.md`
- MySQL 8.4 Reference Manual, `ALTER TABLE` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/alter-table.html>
- MySQL 8.4 Reference Manual, `CREATE INDEX` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/create-index.html>
- MySQL 8.4 Reference Manual, `SHOW INDEX` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/show-index.html>
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.STATISTICS` table:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-statistics-table.html>
- Observed MySQL 8.4.9 runtime behavior recorded by
  `packages/libmylite/tests/mysql_baseline_add_fulltext_indexes_expectations.sh`.

This specification is independently authored from official MySQL 8.4
documentation, observed MySQL 8.4.9 runtime behavior, public SQLite APIs, and
existing MyLite code. It does not copy MySQL, MariaDB, Percona, SQLite
implementation internals, or restrictively licensed implementation sources.

## MySQL 8.4.9 Observations

Runtime probes for this phase establish:

- `ALTER TABLE t ADD FULLTEXT KEY ft_body (body)` and
  `ALTER TABLE t ADD FULLTEXT INDEX ft_body (body)` succeed on existing InnoDB
  tables with supported character/text columns.
- `ALTER TABLE t ADD FULLTEXT (body)` succeeds and generates an index name
  from the first key-part column, with `_2`, `_3`, ... suffixes on collisions.
- `CREATE FULLTEXT INDEX ft_body ON t (body)` succeeds and uses the same
  metadata shape as `ALTER TABLE ... ADD FULLTEXT`.
- Adding the first fulltext index to an existing InnoDB table reports
  `ROW_COUNT() == 0`, `@@warning_count == 1`, and `SHOW WARNINGS` returns
  warning `124` with message `InnoDB rebuilding table to add column FTS_DOC_ID`.
- Adding a later fulltext index to the same table reports `ROW_COUNT() == 0`
  and `@@warning_count == 0`.
- If all fulltext indexes are dropped from a table and a fulltext index is
  added again, MySQL 8.4.9 reports `@@warning_count == 0`.
- A `CREATE TABLE` fulltext definition reports zero warnings and a later
  drop/re-add on that table also reports zero warnings.
- Positive prefixes such as `body(10)` are accepted and ignored in
  `SHOW CREATE TABLE`, `SHOW INDEX`, and `INFORMATION_SCHEMA.STATISTICS`.
- `SHOW INDEX` and `INFORMATION_SCHEMA.STATISTICS` report `FULLTEXT`, SQL
  `NULL` collation, and SQL `NULL` sub-part for added fulltext indexes.
- `SHOW COLUMNS` and `INFORMATION_SCHEMA.COLUMNS.COLUMN_KEY` report `MUL` for
  columns that are first key parts of fulltext descriptors.
- Non-character and binary key parts fail with `1283 / HY000` and
  `Column 'name' cannot be part of FULLTEXT index`.
- `CHAR(0)` and `VARCHAR(0)` key parts fail with `1167 / 42000` and
  `The used storage engine can't index column 'name'`.
- Explicit `ASC` or `DESC` fails with `1221 / HY000` and
  `Incorrect usage of spatial/fulltext/hash index and explicit index order`.
- Prefix length zero fails with `1391 / HY000`.
- Unknown key columns fail with `1072 / 42000`.
- Duplicate explicit index names fail with `1061 / 42000`.
- Index name `PRIMARY`, quoted or unquoted where grammar admits it, fails with
  `1280 / 42000`.
- `ALTER TABLE ... ADD CONSTRAINT c FULLTEXT ...` is a MySQL syntax error.
- MySQL rejects fulltext indexes on temporary InnoDB tables with
  `1796 / HY000`.

## Scope

Supported:

- persistent MyLite base tables;
- `ALTER TABLE table_name ADD FULLTEXT [KEY | INDEX] [index_name]
  (key_part[, ...])`;
- `CREATE FULLTEXT INDEX index_name ON table_name (key_part[, ...])`;
- unqualified and schema-qualified target table names using the existing
  selected/default schema policy;
- optional explicit index names for `ALTER TABLE`, required explicit names for
  standalone `CREATE FULLTEXT INDEX`;
- omitted alter-added names generated from the first key-part column with
  MySQL-compatible suffixes for collisions in the admitted subset;
- one or more unqualified descriptor key-part columns;
- key-part columns limited to `CHAR(1..255)`, `VARCHAR(1..16383)`,
  `TINYTEXT`, `TEXT`, `MEDIUMTEXT`, and `LONGTEXT`;
- optional positive decimal prefix lengths, parsed for compatibility and
  ignored in stored metadata;
- empty and nonempty tables;
- descriptor-backed `SHOW COLUMNS`, `SHOW CREATE TABLE`, `SHOW INDEX`,
  `CREATE TABLE ... LIKE`, `ALTER TABLE ... DROP INDEX|KEY`,
  `ALTER TABLE ... RENAME INDEX|KEY`, standalone `DROP INDEX`, DML, reopen,
  independent file-backed handles, and limited information schema after the
  add;
- no-result DDL result shape with `affected_rows == 0`;
- warning count `1` only when adding the first fulltext index to an existing
  table that has never had MySQL's hidden fulltext document-id state
  introduced, otherwise warning count `0`;
- preservation of `.mylite` preamble and shifted SQLite payload invariants.

Deferred:

- `MATCH ... AGAINST` and all full-text search execution;
- `CREATE FULLTEXT INDEX` options such as parser, comments, visibility,
  algorithms, locks, and engine attributes;
- `ALTER TABLE` multi-action forms;
- `ALTER TABLE ... ADD CONSTRAINT ... FULLTEXT`;
- table-qualified, expression, functional, ordinal, string-literal, duplicate,
  or generated-column key parts;
- explicit key-part ordering;
- temporary fulltext indexes, views, partitioned-table syntax, spatial indexes,
  privilege behavior, and implicit-commit emulation;
- physical SQLite FTS objects, triggers, search indexes, or SQLite fork hooks.

## Ownership Boundaries

- Public API: no new ABI. Callers continue to use `mylite_execute()` and
  existing result/diagnostic accessors.
- Statement context: owns diagnostics reset, warning count, affected rows,
  `ROW_COUNT()`, and cleanup. This phase only appends the MySQL-compatible
  InnoDB rebuild warning for first fulltext additions.
- Lexer/parser/AST: admits the narrow `ALTER TABLE ... ADD FULLTEXT` and
  `CREATE FULLTEXT INDEX` syntax and preserves table, index-name, and key-part
  nodes without consulting descriptors or SQLite.
- Analyzer/planner/runtime: resolves schema, table, index names, and key-part
  columns against MyLite descriptors before any physical SQL is considered.
- Catalog: MyLite index and index-column descriptors are authoritative logical
  metadata. SQLite schema text is not inspected to discover logical indexes.
- Result builder/introspection: existing descriptor-driven `SHOW`,
  `CREATE TABLE ... LIKE`, and information-schema paths render the new
  descriptors.
- Storage/VFS: `.mylite` file preamble and shifted SQLite payload behavior are
  unchanged.
- SQLite physical storage: no SQLite index, FTS table, trigger, or fork patch
  is created for fulltext descriptors.

## Supported SQL Grammar

MyLite admits:

```sql
ALTER TABLE table_name ADD FULLTEXT [KEY | INDEX] [index_name]
  (fulltext_key_part[, ...])

CREATE FULLTEXT INDEX index_name ON table_name
  (fulltext_key_part[, ...])

fulltext_key_part:
  column_name
  column_name(length)
```

MyLite Lemon-syntax sketch:

```lemon
statement(A) ::= create_index_statement(B). {
    A = B;
}

create_index_statement(A) ::=
    CREATE(C) FULLTEXT INDEX identifier(N) ON table_name(T)
    LPAREN secondary_index_part_list(L) RPAREN. {
    A = mylite_sql_parser_make_create_fulltext_index_statement(
        state, C, N, T, L);
}

alter_table_add_index_statement(A) ::=
    ALTER(A1) TABLE table_name(T) ADD fulltext_index_definition(I). {
    A = mylite_sql_parser_make_alter_table_add_index_statement(state, A1, T, I);
}

fulltext_index_definition(A) ::=
    FULLTEXT(F) fulltext_index_keyword_opt index_name_opt(N)
    LPAREN secondary_index_part_list(L) RPAREN(R). {
    A = mylite_sql_parser_make_fulltext_index_definition(state, F, N, L, R);
}

fulltext_index_keyword_opt ::= .
fulltext_index_keyword_opt ::= KEY.
fulltext_index_keyword_opt ::= INDEX.

secondary_index_part ::= identifier.
secondary_index_part ::= identifier LPAREN INTEGER RPAREN.
```

The implementation reuses the existing secondary key-part list AST. A distinct
top-level statement kind distinguishes standalone `CREATE FULLTEXT INDEX` from
ordinary and unique standalone index creation.

## Resolution Semantics

Target table resolution follows the existing policy:

- an unqualified table requires the selected/default schema;
- a schema-qualified table uses the explicit schema and does not require a
  selected schema;
- unknown schemas fail with `1049 / 42000`;
- unknown persistent tables fail with `1146 / 42S02`;
- reserved `_mylite_*` schema or table names are rejected before any generated
  SQLite SQL;
- information-schema write targets use the existing access-denied policy.

The admitted persistent target must be a base table. Fulltext adds to temporary
tables fail with `1796 / HY000`. Future non-base descriptors fail with the
deterministic unsupported-object diagnostic for this surface.

Index names are table-local:

- explicit names must not collide case-insensitively with existing primary,
  unique, secondary, or fulltext descriptors;
- duplicate explicit names fail with `1061 / 42000`;
- `PRIMARY` fails with `1280 / 42000`;
- omitted names for alter-added fulltext indexes derive from the first
  key-part column and append `_2`, `_3`, ... until no descriptor name collides.

Column names are descriptor-owned:

- key parts must be existing unqualified descriptor columns;
- unknown columns fail with `1072 / 42000`;
- duplicate columns inside one fulltext index use the existing duplicate-column
  diagnostic;
- descriptor identifier comparison follows the current catalog
  case-insensitive policy. This phase does not add collation-aware identifier
  comparison.

## Descriptor and Catalog Semantics

On success, MyLite creates:

- one `_mylite_catalog_indexes` row with `kind = FULLTEXT` and
  `is_unique = 0`;
- one `_mylite_catalog_index_columns` row per key part with descriptor column
  id and ordinal position;
- no stored prefix length, because MySQL ignores fulltext prefixes;
- ascending sort direction as an internal placeholder, never exposed for
  fulltext metadata;
- a generated physical-name value for catalog shape stability, although no
  SQLite object is created;
- a durable `_mylite_catalog_tables.fulltext_doc_id_initialized` flag the first
  time a fulltext descriptor is introduced for the table;
- updated table descriptor/catalog generation so descriptor caches observe the
  new index and first fulltext document-id state.

`sqlite_schema_generation` is not incremented for fulltext adds, because the
physical SQLite schema is unchanged. Existing descriptor-only drop and rename
paths continue to operate on fulltext descriptors and do not clear
`fulltext_doc_id_initialized`. Catalog migration sets the flag for tables that
already have a fulltext descriptor; older dropped-all fulltext history cannot
be reconstructed if it predates this catalog column.

## Physical SQLite Handling

No physical `CREATE INDEX` or FTS DDL is emitted. This is intentional:

- a SQLite b-tree index would not implement MySQL full-text behavior;
- SQLite FTS tables need a separate synchronization, parser, tokenization, and
  file-format design;
- the required metadata behavior is fully owned by MyLite descriptors;
- public SQLite APIs are sufficient for the row table and catalog mutations,
  and no SQLite fork hook is required.

Ordinary nonunique and unique index additions continue to create physical
SQLite indexes through the existing path.

## Warnings, Results, and Diagnostics

Successful admitted fulltext additions return through the existing non-row DDL
result conventions:

- `column_count == 0`;
- `row_count == 0`;
- `affected_rows == 0`;
- `warning_count == 1` only when adding the first fulltext descriptor to a
  table whose durable `fulltext_doc_id_initialized` flag is false;
- `warning_count == 0` when the table already has the durable fulltext
  document-id state, including after all fulltext indexes have been dropped and
  later re-added.

The first-add warning is:

| Level | Code | SQLSTATE | Message |
| --- | --- | --- | --- |
| Warning | `124` | `HY000` | `InnoDB rebuilding table to add column FTS_DOC_ID` |

Diagnostics for invalid admitted-syntax inputs:

| Case | Diagnostic |
| --- | --- |
| missing selected schema | existing `1046 / 3D000` |
| unknown schema | existing `1049 / 42000` |
| unknown table | existing `1146 / 42S02` |
| reserved target name | existing reserved-name diagnostic |
| information-schema write target | existing access-denied diagnostic |
| temporary fulltext index | `1796 / HY000` |
| duplicate index name | existing `1061 / 42000` |
| index name `PRIMARY` | existing `1280 / 42000` |
| unknown key column | existing `1072 / 42000` |
| non-character or binary key part | `1283 / HY000` |
| zero-length `CHAR` / `VARCHAR` key part | `1167 / 42000` |
| explicit `ASC` or `DESC` | `1221 / HY000` |
| prefix length zero | existing `1391 / HY000` |
| unsupported grammar | syntax error or existing deterministic unsupported diagnostic |
| allocation/catalog/SQLite failure | existing MyLite allocation/internal diagnostics |

## Testing

The feature must add:

- MySQL 8.4.9 expectation coverage for accepted alter-added and standalone
  fulltext forms, warning counts, `SHOW WARNINGS`, generated names, ignored
  prefixes, metadata rows, and diagnostics;
- parser tests for `ALTER TABLE ... ADD FULLTEXT` and
  `CREATE FULLTEXT INDEX`;
- runtime tests for metadata, warnings, zero affected rows, physical-index
  separation, persistence/reopen, `CREATE TABLE ... LIKE`, DML after add,
  independent handles, and diagnostics;
- regression coverage that existing fulltext create-time, ordinary add-index,
  standalone create-index, parser, catalog, VFS, and file-format tests still
  pass.
