# Baseline ALTER TABLE COMMENT

This slice adds the narrow table-comment mutation path for existing MyLite table
descriptors. It reuses the table comment metadata already introduced for
`CREATE TABLE ... COMMENT` and exposes the changed descriptor through the
existing descriptor-driven `SHOW CREATE TABLE`, `SHOW TABLE STATUS`, and
`INFORMATION_SCHEMA.TABLES` surfaces.

## Compatibility Authority

- MySQL 8.4 Reference Manual, `ALTER TABLE`:
  <https://dev.mysql.com/doc/refman/8.4/en/alter-table.html>
- MySQL 8.4 Reference Manual, `CREATE TABLE` table options:
  <https://dev.mysql.com/doc/refman/8.4/en/create-table.html>
- MySQL 8.4 Reference Manual, `SHOW CREATE TABLE`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-create-table.html>
- MySQL 8.4 Reference Manual, `SHOW TABLE STATUS`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-table-status.html>
- Observed MySQL 8.4.9 runtime behavior recorded in
  `packages/libmylite/tests/mysql_baseline_alter_table_comment_expectations.sh`.

This specification is independently authored from official MySQL documentation,
observed MySQL 8.4.9 behavior, public SQLite APIs, and existing MyLite code.
It does not copy MySQL, MariaDB, Percona, SQLite implementation internals, or
other restrictively licensed implementation sources.

## Supported Surface

The admitted syntax is:

```sql
ALTER TABLE table_name COMMENT [=] 'comment_text'
ALTER TABLE table_name COMMENT [=] 'comment_text', ALGORITHM [=] value
ALTER TABLE table_name COMMENT [=] 'comment_text', LOCK [=] value
ALTER TABLE table_name COMMENT [=] 'comment_text',
    ALGORITHM [=] value, LOCK [=] value
```

`table_name` may be unqualified or schema-qualified and follows MyLite's
existing selected/default schema policy. The statement targets visible
descriptor tables. A same-schema temporary table shadows a persistent table for
this metadata-only mutation.

`comment_text` is limited to the existing MyLite table-option string literal
surface. Single-quoted strings are always admitted. Double-quoted strings are
admitted while the current SQL mode still treats them as strings. `ANSI_QUOTES`
makes a later double-quoted comment value a syntax error.

The comment value replaces any existing table comment. `COMMENT=''` clears the
comment: `SHOW CREATE TABLE` omits the suffix and metadata surfaces return an
empty string.

The supported comma tail is the existing limited `ALGORITHM` / `LOCK` option
tail. MyLite accepts `ALGORITHM=DEFAULT`, `ALGORITHM=COPY`,
`ALGORITHM=INPLACE`, `LOCK=DEFAULT`, `LOCK=NONE`, `LOCK=SHARED`, and
`LOCK=EXCLUSIVE`; `ALGORITHM=INSTANT` is rejected for this action, matching the
observed MySQL 8.4.9 behavior for table comment changes.

Successful supported statements return through the existing non-row result
conventions: zero result rows, affected rows `0`, and warning count `0`.

## MyLite Lemon Syntax

The grammar extension is independently authored for MyLite:

```lemon
statement(A) ::= alter_table_comment_statement(B). {
    A = B;
}

alter_table_comment_statement(A) ::=
    ALTER(A1) TABLE table_name(T) COMMENT(C) equal_opt STRING(V)
    alter_table_option_tail_opt(O). {
    A = mylite_sql_parser_make_alter_table_comment_statement(
        state,
        A1,
        T,
        mylite_sql_parser_make_table_comment_option(
            state,
            C,
            mylite_sql_parser_make_literal(state, V, MYLITE_SQL_AST_LITERAL_STRING)),
        O);
}
```

The grammar intentionally excludes numeric, identifier, `NULL`, hex, bit,
national string, parameter, and expression comment values. It also excludes
multi-action `ALTER TABLE` lists such as `COMMENT='a', COMMENT='b'` and table
options unrelated to the existing `ALGORITHM` / `LOCK` tail.

## Ownership Boundary

- The public API is unchanged. `mylite_execute()` owns the statement boundary,
  result handle, diagnostics reset, warning count, and row-count state through
  the existing statement context.
- Lexer/parser/AST own syntax admission and produce an
  `ALTER_TABLE_COMMENT` statement node with a child table-name node and a child
  table-comment option node.
- Analyzer/planner code resolves the target using catalog descriptors, validates
  the comment literal, validates the optional algorithm/lock tail, and decides
  whether the target is persistent or temporary.
- The durable catalog remains authoritative for persistent table comments.
  Mutating a persistent comment updates only MyLite catalog metadata and
  descriptor generation. SQLite schema text is not used as the source of truth.
- The session temporary catalog owns temporary table comments. Temporary comment
  mutation updates only the handle-local temporary descriptor.
- `SHOW CREATE TABLE`, `SHOW TABLE STATUS`, and `INFORMATION_SCHEMA.TABLES`
  continue rendering from descriptors, so no separate introspection path is
  added.
- Storage/VFS behavior is unchanged. The `.mylite` preamble and shifted SQLite
  payload invariants must not change.
- SQLite owns physical rows. This feature does not require physical row SQL,
  physical table rebuilds, triggers, indexes, or SQLite fork patches.

## Resolution Semantics

Unqualified targets require a selected schema. Schema-qualified targets use the
explicit schema and do not require a selected default schema.

Writes to `information_schema` are rejected through the existing system-schema
write guard. Reserved `_mylite_*` schema and table names are rejected before
physical SQL generation.

For explicit schemas, an unknown schema reports the existing MySQL-compatible
unknown-database diagnostic. Unknown tables report the existing unknown-table
diagnostic. Same-schema temporary tables shadow persistent tables for the
supported comment mutation. Persistent targets must be base-table descriptors;
temporary targets must be session temporary table descriptors. Future non-base
object descriptors must be rejected with a deterministic unsupported-object
diagnostic.

Identifier matching follows the current MyLite catalog name policy. This slice
does not add case-folding or collation changes.

## String Semantics

Supported comment values are decoded by MyLite's existing table-option string
decoder:

- doubled quotes decode to one quote;
- backslash escapes decode according to the current SQL mode;
- `NO_BACKSLASH_ESCAPES` disables backslash escape decoding for later
  statements;
- decoded NUL bytes are rejected with a deterministic MyLite diagnostic because
  descriptor comments are stored as C strings;
- invalid UTF-8 is rejected with a deterministic MyLite diagnostic;
- comments longer than 2048 UTF-8 characters are rejected with MySQL-compatible
  `1628 / HY000` table-comment-too-long diagnostics.

`SHOW CREATE TABLE` renders a nonempty comment as ` COMMENT='escaped text'`
using the existing MyLite quoted-text renderer. Empty comments are omitted.

## Metadata Semantics

Persistent base tables:

- `ALTER TABLE t COMMENT='text'` replaces the descriptor comment.
- `SHOW CREATE TABLE` renders the new nonempty comment.
- `SHOW TABLE STATUS.Comment` reports the new comment.
- `INFORMATION_SCHEMA.TABLES.TABLE_COMMENT` reports the new comment.
- Rename preserves the comment because the table descriptor id is preserved.
- Drop removes the descriptor and its comment.
- Close/reopen preserves durable comments.
- Independent file-backed handles keep independent descriptor state.

Temporary tables:

- `ALTER TABLE t COMMENT='text'` updates the shadowing temporary descriptor
  when a same-schema temporary table is visible.
- `SHOW CREATE TABLE` for the temporary table renders the changed comment.
- Temporary tables remain omitted from the current `SHOW TABLE STATUS` and
  durable `INFORMATION_SCHEMA.TABLES` surfaces.
- MyLite rejects temporary table comment mutation inside an active user
  transaction, matching the current temporary-DDL policy and preserving the
  active transaction.
- Temporary comments are discarded when the handle closes.

## Diagnostics

Successful supported statements report warning count `0`.

Diagnostics for this slice:

- syntax errors for non-string comment forms, missing values, and unsupported
  multi-action forms;
- existing missing-default-schema, unknown-schema, unknown-table, reserved-name,
  system-schema write, and unsupported-object diagnostics;
- existing MyLite temporary-DDL diagnostics for temporary table comment mutation
  inside an active user transaction;
- existing algorithm/lock diagnostics, including rejection of
  `ALGORITHM=INSTANT` for this metadata mutation;
- deterministic MyLite diagnostics for decoded NUL bytes and invalid UTF-8;
- MySQL-compatible `1628 / HY000` diagnostics for comments longer than 2048
  UTF-8 characters;
- allocation failure returns `MYLITE_NOMEM` and sets handle diagnostics through
  the existing runtime path;
- catalog or temporary-catalog failures use existing internal/runtime
  diagnostics.

## Tests

Runtime and parser tests must cover:

- parser acceptance for `COMMENT='text'`, `COMMENT 'text'`, double-quoted
  comment strings, and optional `ALGORITHM` / `LOCK` tails;
- parser/runtime rejection for numeric, identifier, `NULL`, and unsupported
  multi-action comment forms;
- persistent comment replacement, empty-comment clearing, escaping, and
  `NO_BACKSLASH_ESCAPES` behavior;
- `SHOW CREATE TABLE`, `SHOW TABLE STATUS`, and
  `INFORMATION_SCHEMA.TABLES` after mutation;
- schema-qualified targets without a default schema;
- missing default schema, unknown schema, unknown table, and reserved target
  diagnostics;
- temporary table shadowing and temporary `SHOW CREATE TABLE` metadata;
- persistence after close/reopen for durable tables;
- rename preservation and drop cleanup;
- independent file-backed handles;
- overlength and NUL diagnostics;
- `.mylite` preamble preservation;
- existing parser, table comment, show-create-table, show-table-status,
  information-schema, temporary table, and alter-table tests still passing.

## Deferred

- Multi-action `ALTER TABLE` lists, including duplicate `COMMENT` actions where
  MySQL lets the last one win.
- Broader table options such as `ROW_FORMAT`, `KEY_BLOCK_SIZE`,
  `STATS_PERSISTENT`, `ENGINE_ATTRIBUTE`, partition comments, and NDB comment
  options.
- Privileges, metadata locks, binary logging, and online-DDL concurrency
  semantics.
- Character-set conversion or collation behavior for comment text.
