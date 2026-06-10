# Parser Corpus ALTER TABLE Table-Option Actions

This slice admits residual MySQL 8.4.9 DDL table-option forms from the
server-test parser corpus:

- `ALTER TABLE ... ENGINE=..., DROP|MODIFY|ADD|ALTER|RENAME ...`
- standalone `ALTER TABLE ...` table-option lists that mix storage/statistics
  options with `COMMENT`
- empty MERGE-table option lists: `UNION=()`

Primary MySQL references:

- https://dev.mysql.com/doc/refman/8.4/en/alter-table.html
- https://dev.mysql.com/doc/refman/8.4/en/create-table.html

## MySQL 8.4.9 Observations

Runtime probes were executed against the local `mylite-mysql-849` MySQL 8.4.9
container.

```sql
CREATE TABLE t (id INT PRIMARY KEY, c INT, d INT) ENGINE=InnoDB;
ALTER TABLE t ENGINE=InnoDB, DROP COLUMN d;
ALTER TABLE t ENGINE=InnoDB, MODIFY c BIGINT;
ALTER TABLE t ENGINE=InnoDB, ADD COLUMN e INT;
ALTER TABLE t
  AVG_ROW_LENGTH=0 CHECKSUM=0 COMMENT='' MIN_ROWS=0 MAX_ROWS=0
  PACK_KEYS=DEFAULT DELAY_KEY_WRITE=0 ROW_FORMAT=DEFAULT;
CREATE TABLE m1 (a INT) ENGINE=MERGE UNION=();
ALTER TABLE m1 UNION=();
```

All statements succeeded. MySQL also accepted table-option-first actions for
column visibility and table rename:

```sql
ALTER TABLE t ENGINE=InnoDB, ALTER my_row_id SET VISIBLE;
ALTER TABLE t ENGINE=InnoDB, ALTER my_row_id SET INVISIBLE;
ALTER TABLE parent ENGINE=InnoDB, RENAME TO parent0;
```

`SHOW COLUMNS` reflected the expected column changes, and `SHOW CREATE TABLE`
for the empty MERGE union table omitted a `UNION` clause.

## Scope

### Empty MERGE UNION Lists

MyLite already parses MERGE-style `UNION=(table[, ...])` and
`UNION (table[, ...])` table-option placeholders. This slice also accepts the
empty-list forms `UNION=()` and `UNION ()` for `CREATE TABLE`,
`CREATE TEMPORARY TABLE`, and `ALTER TABLE` table-option statements. The option
remains a placeholder; MyLite does not implement MERGE storage.

### ALTER TABLE Table-Option-First Multi-Actions

MySQL permits table options as elements in a comma-separated `ALTER TABLE`
action list. MyLite already admits table options after another action, but the
corpus contains table-option-first forms such as:

```sql
ALTER TABLE mysql.user ENGINE='InnoDB', DROP COLUMN max_updates;
ALTER TABLE mysql.plugin ENGINE=InnoDB, MODIFY dl CHAR(64);
ALTER TABLE t ENGINE=MyISAM, ADD COLUMN c2 INT;
ALTER TABLE t ENGINE='InnoDB', ALTER my_row_id SET INVISIBLE;
ALTER TABLE parent ENGINE=InnoDB, RENAME TO parent0;
```

This slice admits the leading `ENGINE` table-option forms through MyLite's
parser placeholder scanner as raw `ALTER_TABLE_MULTI_ACTION_STATEMENT` nodes.
Runtime execution remains conservative: the raw placeholder reaches the current
multi-action unsupported-action diagnostic before catalog or SQLite schema
mutation.

### Mixed Storage and Comment Options

MySQL accepts standalone `ALTER TABLE` table-option lists where storage and
comment options are adjacent. MyLite already implements descriptor-owned
storage/statistics metadata and descriptor-owned table comments. This slice
allows one standalone `ALTER TABLE` storage-option statement to contain
`COMMENT='...'` alongside storage/statistics and placeholder options, updating
both pieces of supported descriptor metadata in one catalog mutation.

`ENGINE=InnoDB` remains validated using the existing engine option rules.
Tablespace, storage, MERGE `UNION`, and `INSERT_METHOD` placeholders remain
accepted no-op metadata placeholders for embedded compatibility. Charset,
collation, and auto-increment mixed into this storage/comment statement remain
outside this slice and continue to return deterministic diagnostics unless
they are parsed by their existing standalone forms.

## MyLite Grammar Snippets

These snippets describe the intended MyLite grammar shape and are
independently authored from MySQL documentation and runtime behavior.

```lemon
table_option ::= UNION equal_opt LPAREN table_name_list RPAREN.
table_option ::= UNION equal_opt LPAREN RPAREN.

alter_table_engine_first_multi_action_placeholder ::=
    ALTER TABLE table_name ENGINE equal_opt option_name COMMA alter_table_action.

alter_table_multi_action ::=
    table_storage_or_placeholder_option.

alter_table_storage_statistics_statement ::=
    ALTER TABLE table_name alter_table_storage_option_list alter_table_option_tail_opt.

alter_table_storage_option_list ::= alter_table_storage_or_comment_option.
alter_table_storage_option_list ::=
    alter_table_storage_option_list alter_table_storage_or_comment_option.
alter_table_storage_option_list ::=
    alter_table_storage_option_list COMMA alter_table_storage_or_comment_option.

alter_table_storage_or_comment_option ::= table_storage_or_placeholder_option.
alter_table_storage_or_comment_option ::= COMMENT equal_opt STRING.
```

## Runtime Behavior

No SQLite fork hook is needed.

- `UNION=()` produces the same placeholder table-option AST shape as nonempty
  MERGE union lists, with no child table list.
- Table-option-first multi-action statements parse as raw placeholders, then
  fail atomically at runtime through the existing unsupported multi-action path.
- Standalone storage/comment option statements update supported
  storage/statistics metadata and table comments in one catalog mutation.
- These changes do not add physical MERGE tables, MyISAM storage, storage
  engine conversion, table copy scheduling, or MySQL metadata-lock behavior.

## Tests

Focused tests cover:

- parser acceptance for empty MERGE `UNION=()` in `CREATE TABLE` and
  `ALTER TABLE`;
- parser acceptance and raw placeholder AST shape for table-option-first
  `ALTER TABLE` multi-actions;
- runtime success for mixed storage/comment standalone table-option statements;
- runtime atomic unsupported diagnostics for table-option-first multi-actions;
- MySQL 8.4.9 expectation evidence for the accepted syntax;
- parser corpus movement over
  `build/perf-data/mysql-server-tests-queries.csv`.

## Compatibility Status

This is a narrow parser and descriptor-metadata compatibility slice. It
improves valid MySQL DDL admission and supported descriptor updates, but does
not make mixed storage-option multi-action ALTER execution, MERGE storage,
MyISAM conversion, or broader table-option semantics supported.
