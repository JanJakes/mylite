# Baseline CREATE TABLE COMMENT Option

This slice adds table-level `COMMENT` metadata for the existing persistent and
temporary `CREATE TABLE` table-definition paths. The feature is intentionally
limited to storing a MyLite-owned table descriptor comment and exposing it
through existing descriptor-driven metadata surfaces. It does not implement
column comments, index comments, NDB comment options, `ALTER TABLE ... COMMENT`,
or broader table option handling such as `ROW_FORMAT`, `KEY_BLOCK_SIZE`, or
statistics options.

## Compatibility Authority

- MySQL 8.4 Reference Manual, `CREATE TABLE`:
  <https://dev.mysql.com/doc/refman/8.4/en/create-table.html>
- MySQL 8.4 Reference Manual, `SHOW TABLE STATUS`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-table-status.html>
- MySQL 8.4 Reference Manual, `SHOW CREATE TABLE`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-create-table.html>
- Observed MySQL 8.4.9 runtime behavior using the local
  `mylite-mysql-849` container, recorded in
  `packages/libmylite/tests/mysql_baseline_create_table_comment_option_expectations.sh`.

## Supported Surface

The admitted syntax is:

```sql
CREATE [TEMPORARY] TABLE [IF NOT EXISTS] table_name
    (create_table_item[, ...])
    [existing_table_option ...]
    [COMMENT [=] 'comment_text']
    [existing_table_option ...];
```

`COMMENT` may appear anywhere in the current table option list and may be mixed
with the existing supported `ENGINE`, `AUTO_INCREMENT`, `CHARSET` /
`CHARACTER SET`, and `COLLATE` options. MySQL accepts comma separators between
table options; MyLite still rejects comma-separated table options in this slice
because the current table-option grammar does. That parser gap remains tracked
outside table-comment metadata support.

If multiple table `COMMENT` options are present, the last one wins, matching
the observed MySQL 8.4.9 behavior for duplicate table comments.

An empty table comment is accepted and stored as an empty descriptor string.
`SHOW CREATE TABLE` omits the `COMMENT=''` suffix for empty comments; `SHOW
TABLE STATUS.Comment` and `INFORMATION_SCHEMA.TABLES.TABLE_COMMENT` return an
empty string.

`CREATE TABLE ... LIKE source` copies the source table comment. `CREATE TABLE
... SELECT` does not admit table options in the current MyLite grammar and does
not infer a table comment from the source projection.

## MyLite Lemon Syntax

The grammar extension is independently authored for MyLite:

```lemon
table_option(A) ::= COMMENT(C) equal_opt STRING(V). {
    A = mylite_sql_parser_make_table_comment_option(state, C,
        mylite_sql_parser_make_literal(state, V, MYLITE_SQL_AST_LITERAL_STRING));
}
```

No identifier, integer, decimal, hex, bit, national string, expression, or
parameter-valued table comment form is admitted in this slice.

## Ownership And Architecture

- The public API is unchanged. Successful statements use the existing non-row
  result conventions: no result rows, affected rows `0`, and warning count `0`
  for supported in-range comments.
- Statement context and SQL mode state remain handle-owned. String decoding
  observes the existing session `NO_BACKSLASH_ESCAPES` mode when decoding the
  comment literal.
- The lexer already recognizes `COMMENT`. The parser adds a table-option AST
  node with the decoded literal still owned by the AST.
- Planning validates table comment literals before catalog or physical SQLite
  mutation.
- The catalog module owns durable table comment metadata in the table
  descriptor. This requires a catalog schema migration and a corresponding
  field in `struct mylite_catalog_table_descriptor`.
- Runtime DDL writes the descriptor comment through catalog APIs. Physical
  SQLite user tables do not receive a SQL comment because SQLite has no
  matching table comment surface.
- `SHOW CREATE TABLE`, `SHOW TABLE STATUS`, and
  `INFORMATION_SCHEMA.TABLES` render from MyLite descriptors, not from
  SQLite schema text.
- Storage and VFS behavior are unchanged. `.mylite` preamble and shifted
  SQLite payload invariants must remain intact.

## String Semantics

Supported comment values are ordinary MySQL string literals decoded by
MyLite's existing table-option string decoder:

- single-quoted and double-quoted string literals are accepted while the lexer
  admits double-quoted strings;
- doubled quote characters decode to one quote;
- backslash escapes decode according to the existing session SQL mode;
- `NO_BACKSLASH_ESCAPES` disables backslash escape decoding for later
  statements;
- decoded NUL bytes are rejected with a deterministic MyLite unsupported
  diagnostic because descriptor strings are currently C strings;
- decoded comments longer than 2048 UTF-8 characters are rejected with MySQL-compatible
  `1628 / HY000` table-comment-too-long diagnostics;
- valid UTF-8 bytes are stored and rendered byte-for-byte, subject to the
  existing text handling limits of other table metadata.

`SHOW CREATE TABLE` renders a nonempty comment as:

```sql
 COMMENT='escaped text'
```

The renderer doubles single quotes and uses MySQL-style backslash escapes for
backslash, newline, carriage return, and tab bytes.

## Metadata Semantics

Persistent base tables:

- `SHOW CREATE TABLE` appends the table comment suffix after current engine,
  auto-increment, charset, and collation suffixes when the comment is nonempty.
- `SHOW TABLE STATUS.Comment` reports the descriptor table comment.
- `INFORMATION_SCHEMA.TABLES.TABLE_COMMENT` reports the same descriptor table
  comment.
- Rename preserves the comment because the table descriptor id is preserved.
- Drop removes the comment with the table descriptor.
- Close/reopen preserves comments stored in the durable catalog.
- Independent `.mylite` files and handles keep independent descriptor state.

Temporary tables:

- `CREATE TEMPORARY TABLE ... COMMENT='text'` stores the comment in the
  session-local temporary table descriptor.
- `SHOW CREATE TABLE` for the temporary table renders the comment.
- Temporary tables remain omitted from the current `SHOW TABLE STATUS` and
  durable `INFORMATION_SCHEMA.TABLES` surfaces.

`CREATE TABLE ... LIKE`:

- A persistent clone copies the source persistent table comment.
- A temporary clone copies the visible source table comment into the
  temporary descriptor.

## Diagnostics

Supported successful comment forms report warning count `0`.

Diagnostics for this slice:

- Syntax errors for non-string comment forms, missing values, unsupported
  comma-separated table option lists, column comments, index comments, and
  `ALTER TABLE ... COMMENT` remain parser errors or existing unsupported
  diagnostics.
- Decoded NUL bytes use a deterministic MyLite unsupported diagnostic:
  `table comments do not support NUL bytes`.
- Comments longer than 2048 UTF-8 characters use MySQL-compatible error `1628 / HY000`
  with a message containing `Comment for table '<name>' is too long
  (max = 2048)`.
- Allocation failures return `MYLITE_NOMEM` and set handle diagnostics through
  the existing runtime path.
- Catalog read/write or physical SQLite failures use existing internal/runtime
  diagnostics. No physical SQLite SQL should include the logical comment text.

## Tests

Runtime tests must cover:

- parser acceptance for `COMMENT='text'`, `COMMENT 'text'`, double-quoted
  strings, duplicate comments, and combinations with existing options;
- parser rejection for non-string comment values and currently unsupported
  comma-separated table options;
- successful persistent and temporary table comment creation;
- empty comments omitted from `SHOW CREATE TABLE` but visible as empty metadata;
- escaped quote and backslash rendering;
- duplicate comments where the last value wins;
- `SHOW CREATE TABLE`, `SHOW TABLE STATUS`, and
  `INFORMATION_SCHEMA.TABLES` metadata for persistent tables;
- `CREATE TABLE ... LIKE` and temporary `LIKE` comment cloning;
- rename/drop behavior;
- close/reopen persistence and `.mylite` preamble preservation;
- independent file-backed handles;
- overlength and NUL diagnostics;
- existing create/drop/like/show/status/parser/catalog tests still passing.

## Deferred

- `ALTER TABLE ... COMMENT`
- column comments and index comments
- comma-separated table option grammar
- `ROW_FORMAT`, `KEY_BLOCK_SIZE`, `STATS_PERSISTENT`,
  `STATS_AUTO_RECALC`, `STATS_SAMPLE_PAGES`, and related
  `CREATE_OPTIONS` metadata
- NDB comment option parsing
- comment privilege semantics
- complete character-set conversion or collation behavior for comment text
