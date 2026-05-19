# Baseline Column Comments

## Summary

This phase adds MyLite-owned column comment metadata for the existing descriptor
table-definition paths. It is intentionally a metadata slice: comments are
accepted on supported column definitions, persisted in column descriptors, and
rendered through descriptor-driven introspection. The feature does not change
row storage, expression evaluation, query planning, or SQLite user-table schema
text.

Supported syntax:

```sql
CREATE [TEMPORARY] TABLE [IF NOT EXISTS] t (
    column_name supported_type [supported_column_attribute ...] COMMENT 'text'
    [, ...]
);

ALTER TABLE t ADD [COLUMN] column_name supported_type
    [supported_column_attribute ...] COMMENT 'text' [FIRST | AFTER column_name];

ALTER TABLE t MODIFY [COLUMN] column_name supported_type
    [supported_column_attribute ...] [COMMENT 'text'] [FIRST | AFTER column_name];

ALTER TABLE t CHANGE [COLUMN] old_name new_name supported_type
    [supported_column_attribute ...] [COMMENT 'text'] [FIRST | AFTER column_name];
```

`COMMENT` is admitted only where the surrounding `CREATE TABLE`, temporary
table, `ADD COLUMN`, `MODIFY COLUMN`, or `CHANGE COLUMN` statement is already
implemented. This phase does not add unsupported column types or broader `ALTER
TABLE` forms.

## Compatibility Authority

- MySQL 8.4 Reference Manual, `CREATE TABLE`:
  <https://dev.mysql.com/doc/refman/8.4/en/create-table.html>
- MySQL 8.4 Reference Manual, `SHOW COLUMNS`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-columns.html>
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.COLUMNS`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-columns-table.html>
- MySQL 8.4 Reference Manual, `ALTER TABLE`:
  <https://dev.mysql.com/doc/refman/8.4/en/alter-table.html>
- Observed MySQL 8.4.9 runtime behavior recorded by
  `packages/libmylite/tests/mysql_baseline_column_comments_expectations.sh`.

This specification is independently authored from official MySQL 8.4
documentation, observed MySQL 8.4.9 behavior, public SQLite APIs, and existing
MyLite code. It does not copy MySQL, MariaDB, Percona, SQLite implementation
internals, or other restrictively licensed implementation sources.

## MySQL 8.4.9 Observations

Runtime probes establish:

- `COMMENT 'text'` is accepted as a column attribute and is rendered by
  `SHOW CREATE TABLE`, `SHOW FULL COLUMNS`, and
  `INFORMATION_SCHEMA.COLUMNS.COLUMN_COMMENT`.
- `COMMENT='text'`, `COMMENT 123`, and `COMMENT NULL` are syntax errors for
  column attributes.
- Duplicate column `COMMENT` attributes are accepted; the final comment wins.
- `CREATE TABLE ... LIKE source` copies source column comments.
- `CREATE TABLE ... SELECT` direct source column projections copy source column
  comments.
- `ALTER TABLE ... ADD COLUMN ... COMMENT 'text'` stores the comment on the new
  column.
- `ALTER TABLE ... MODIFY COLUMN ... COMMENT 'text'` and
  `CHANGE COLUMN ... COMMENT 'text'` replace the column descriptor and store the
  supplied comment.
- `MODIFY COLUMN` or `CHANGE COLUMN` without a `COMMENT` clause clears any
  previous comment, because MySQL treats the supplied column definition as the
  replacement definition.
- `ALTER TABLE ... RENAME COLUMN` preserves the existing comment because it is
  not a replacement column definition.
- Character set and collation attributes must appear before `COMMENT` for
  character columns in the observed syntax; `COMMENT 'x' CHARACTER SET ascii`
  is a syntax error, while `CHARACTER SET ascii COMMENT 'x'` is accepted.
- Column comments may contain up to 1024 characters. A longer comment fails
  with `1629 / HY000` and message shape
  `Comment for field 'column_name' is too long (max = 1024)`.
- MySQL can store embedded NUL bytes in comments and renders them with
  `\0`-style escaping in `SHOW CREATE TABLE`. MyLite keeps embedded NUL bytes
  unsupported in this slice because current descriptor strings are C strings.

## Scope

Supported:

- one or more `COMMENT 'string'` column attributes on supported column
  definitions;
- persistent and session-temporary `CREATE TABLE`;
- persistent and session-temporary `CREATE TABLE ... LIKE` descriptor cloning;
- persistent `CREATE TABLE ... SELECT` direct descriptor-column projections
  where descriptor inference is already supported;
- persistent `ALTER TABLE ... ADD COLUMN`, `MODIFY COLUMN`, and
  `CHANGE COLUMN` where those actions are already supported;
- ordinary MySQL string-literal decoding using the current session SQL mode,
  including `NO_BACKSLASH_ESCAPES`;
- valid UTF-8, NUL-free comment text up to 1024 characters;
- duplicate column comments where the final value wins;
- empty comments, represented as an empty descriptor string;
- durable catalog migration for existing files, with existing columns receiving
  empty comments;
- descriptor-driven rendering in:
  - `SHOW CREATE TABLE`;
  - `SHOW FULL COLUMNS`;
  - `INFORMATION_SCHEMA.COLUMNS.COLUMN_COMMENT`;
- preservation across `RENAME TABLE`, `ALTER TABLE ... RENAME COLUMN`, index
  DDL, table rebuilds, close/reopen, and independent file-backed handles;
- clearing comments through `MODIFY COLUMN` or `CHANGE COLUMN` replacement
  definitions that omit `COMMENT`.

Deferred:

- embedded NUL bytes in column comments;
- invalid UTF-8 comment payloads;
- column comments on unsupported types or unsupported table-definition forms;
- `COLUMN_FORMAT`, `STORAGE`, `ENGINE_ATTRIBUTE`, and
  `SECONDARY_ENGINE_ATTRIBUTE`;
- comments on generated columns;
- `ALTER TABLE ... ALTER COLUMN ... COMMENT` because MySQL does not expose that
  as a separate action;
- privilege semantics, character-set conversion, collation effects, and
  protocol-grade metadata beyond current result strings.

## Ownership Boundaries

- Public API: unchanged. Applications continue to use `mylite_execute()` and
  existing result/diagnostic accessors. Successful supported DDL uses existing
  no-row result conventions.
- Statement context: owns statement diagnostics, warning count, affected rows,
  and cleanup. Supported in-range comments produce warning count `0`.
- Lexer/parser/AST: admits `COMMENT 'string'` in the column-attribute list and
  stores the literal as an AST child. Parsing does not consult descriptors or
  SQLite.
- Analyzer/planner/runtime: decodes and validates the final effective comment
  before any catalog or physical SQLite mutation. It also applies MySQL's
  replacement-definition behavior for `MODIFY` and `CHANGE`.
- Catalog: column descriptors are authoritative for logical comment metadata.
  The catalog schema gains a `comment TEXT NOT NULL DEFAULT ''` column in
  `_mylite_catalog_columns`, plus a fixed-size C descriptor field.
- Result builder/introspection: `SHOW CREATE TABLE`, `SHOW FULL COLUMNS`, and
  `INFORMATION_SCHEMA.COLUMNS` read descriptor comments. They do not query
  SQLite metadata for comments.
- Storage/VFS: `.mylite` preamble and shifted SQLite payload invariants are
  unchanged.
- SQLite physical storage: user tables and generated physical SQLite SQL do not
  include logical comment text. No SQLite fork patch is required.

## Grammar

MyLite Lemon-syntax sketch:

```lemon
column_attribute ::= COMMENT STRING.

column_attribute_list ::= column_attribute.
column_attribute_list ::= column_attribute_list column_attribute.
```

`COMMENT` remains a restricted label where the identifier grammar already
admits it. The column-comment form intentionally admits only ordinary string
literals. It does not admit `=`, identifiers, numbers, `NULL`, expressions,
parameters, character-set introducers, or national string literals.

Column comments belong after character set and collation attributes in this
slice. MyLite's existing legacy column-attribute ordering validation must reject
`VARCHAR(5) COMMENT 'x' CHARACTER SET ascii` like observed MySQL 8.4.9
behavior, while still allowing comments before or after nullability, defaults,
primary/unique key attributes, auto-increment, and `ON UPDATE CURRENT_TIMESTAMP`
where the surrounding definition is otherwise accepted.

## Semantics

The effective comment for a column definition starts as the empty string. Each
`COMMENT 'string'` attribute replaces it; the last attribute wins.

Comment literals use the same string decoder as table and index option
comments:

- single-quoted and double-quoted string literals follow the current lexer
  rules;
- doubled quote characters decode to one quote;
- backslash escapes decode unless `NO_BACKSLASH_ESCAPES` is active for the
  statement;
- decoded NUL bytes are rejected with a deterministic MyLite unsupported
  diagnostic;
- decoded text must be valid UTF-8;
- comments longer than 1024 UTF-8 characters fail with MySQL-compatible
  `1629 / HY000`.

`CREATE TABLE ... LIKE` copies descriptor comments exactly. `CREATE TABLE ...
SELECT` copies comments for direct source descriptor-column projections handled
by the current descriptor-inference path. It does not invent comments for
literal, expression, or otherwise unsupported projection forms.

`ALTER TABLE ... ADD COLUMN` stores the planned column comment. `MODIFY COLUMN`
and `CHANGE COLUMN` store the comment from the replacement definition, or the
empty string when the replacement definition omits `COMMENT`. `RENAME COLUMN`
preserves the existing descriptor comment.

## Metadata Mapping

`SHOW CREATE TABLE` renders a nonempty comment after the existing default,
auto-increment, and `ON UPDATE CURRENT_TIMESTAMP` column attributes and before
invisible-column version comments or the trailing comma:

```sql
`c` int DEFAULT NULL COMMENT 'text'
```

The renderer quotes comments with the existing MySQL-style text-quoting helper.
Empty comments are omitted from `SHOW CREATE TABLE`.

`SHOW FULL COLUMNS` maps its `Comment` column to the descriptor comment. Tables
without comments report the empty string.

`INFORMATION_SCHEMA.COLUMNS.COLUMN_COMMENT` maps to the descriptor comment.
System-view rows and columns without comments continue to report the empty
string.

## Diagnostics

- Unsupported non-string comment forms remain syntax errors (`1064 / 42000`) by
  grammar shape.
- Decoded NUL bytes use a deterministic MyLite unsupported diagnostic:
  `column comments do not support NUL bytes`.
- Invalid UTF-8 uses a deterministic MyLite unsupported diagnostic:
  `column comments support only valid UTF-8 text`.
- Too-long comments use MySQL-compatible error `1629 / HY000` with message
  `Comment for field '<column>' is too long (max = 1024)`.
- Unsupported object kinds, missing schemas, missing tables, duplicate columns,
  unsupported column types, invalid defaults, and physical SQLite failures keep
  the current diagnostics of their host statement.
- Allocation failures return `MYLITE_NOMEM` and set handle diagnostics through
  the existing runtime path.

## Tests

Required fast C coverage:

- parser acceptance for `COMMENT 'text'` on column definitions and parser
  rejection of `COMMENT='text'`, numeric comments, and `COMMENT NULL`;
- duplicate comments where the final value wins;
- create-time comments for numeric, string, temporal, and auto-increment
  columns;
- `SHOW CREATE TABLE`, `SHOW FULL COLUMNS`, and
  `INFORMATION_SCHEMA.COLUMNS.COLUMN_COMMENT`;
- empty comments omitted from `SHOW CREATE TABLE` and rendered as empty
  metadata;
- escaped quote/backslash rendering and `NO_BACKSLASH_ESCAPES`;
- `CREATE TABLE ... LIKE`, `CREATE TABLE ... SELECT` direct column projection
  comments, temporary table comments, rename table, rename column,
  add/modify/change column, and replacement definitions that clear prior
  comments;
- close/reopen persistence and `.mylite` preamble preservation;
- independent file-backed handles with independent comments;
- NUL and too-long diagnostics;
- zero-initialized cleanup for any new plan/descriptor paths;
- existing parser, catalog, create/drop/like/show/full-columns/information
  schema/table-rebuild lifecycle tests still passing.

Required MySQL expectation coverage:

- create-time metadata;
- `CREATE TABLE ... LIKE` and `CREATE TABLE ... SELECT` direct column
  projections;
- `ALTER TABLE ... ADD/MODIFY/CHANGE`;
- duplicate comments;
- syntax errors for unsupported column-comment forms;
- 1024-character success and 1025-character failure.
