# SHOW CREATE TABLE

## Scope

This feature implements the first executable `SHOW CREATE TABLE` slice for
supported persistent MyLite base tables:

- `SHOW CREATE TABLE tbl_name`
- `SHOW CREATE TABLE db_name.tbl_name`

The statement reads MyLite's table, column, and index catalogs and returns a
single MySQL-shaped result set with columns `Table` and `Create Table`.
Generated text is deterministic and describes the currently supported
`CREATE TABLE` subset: column names and stored column types, nullability,
defaults, `AUTO_INCREMENT`, invisible columns, primary keys, unique indexes,
secondary indexes, key-part prefix lengths and descending order, index
comments, explicit `USING BTREE` display, index visibility, engine,
auto-increment table option, table charset, table collation, and table comment.

Deferred surfaces:

- `SHOW CREATE TABLE` for views, including information-schema views
- `SHOW CREATE VIEW`
- temporary tables and temporary-table shadowing
- foreign keys, checks, generated columns, partitions, functional indexes, and
  storage-engine features not yet represented faithfully in MyLite metadata
- `sql_quote_show_create = 0`; MyLite always emits default quoted identifiers
  in this slice
- privilege-sensitive visibility, metadata locks, warnings, and protocol
  status counters

## Compatibility Sources

- MySQL 8.4 Reference Manual, `SHOW CREATE TABLE` Statement:
  https://dev.mysql.com/doc/en/show-create-table.html
- MySQL 8.4 Reference Manual, `SHOW` Statements:
  https://dev.mysql.com/doc/refman/en/show.html
- MySQL 8.4 Reference Manual, `CREATE TABLE` Statement:
  https://dev.mysql.com/doc/refman/8.4/en/create-table.html
- Runtime observations verified against local `mylite-mysql-849`, MySQL
  `8.4.9`, plus parent-side probes supplied for the same runtime.

This specification is independently authored from official documentation and
observed runtime behavior. It does not copy MySQL grammar, documentation prose,
or implementation sources.

## MySQL 8.4.9 Runtime Observations

The following behavior was verified against MySQL 8.4.9:

| SQL | Result |
| --- | --- |
| `SHOW CREATE TABLE simple` for `CREATE TABLE simple (id INT PRIMARY KEY, v VARCHAR(10))` | Columns `Table`, `Create Table`; one row. The generated definition places `PRIMARY KEY` as a table constraint, renders `id` as `int NOT NULL`, renders nullable `varchar` as `varchar(10) DEFAULT NULL`, and appends `ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci`. |
| `SHOW CREATE TABLE meta` for a table with `AUTO_INCREMENT`, comments, invisible column, unique and nonunique indexes, prefix length, descending order, invisible index, explicit charset/collation, table comment, and `AUTO_INCREMENT=42` | Columns `Table`, `Create Table`; generated text uses backtick-quoted identifiers, `AUTO_INCREMENT` on the column, string-quoted literal defaults, `/*!80023 INVISIBLE */` for invisible columns, primary/unique/nonunique key clauses, `DESC` on descending key parts, index `COMMENT`, `/*!80000 INVISIBLE */` for invisible indexes, and table options `ENGINE=InnoDB AUTO_INCREMENT=42 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin COMMENT='table comment'`. |
| `SHOW CREATE TABLE index_options` for `KEY idx_pre USING BTREE (v)`, `KEY idx_post (v) USING BTREE`, and `KEY idx_hash USING HASH (v)` | Explicit BTREE index type syntax is preserved as `USING BTREE` after the key-part list. Unsupported InnoDB HASH syntax falls back to effective BTREE metadata but does not display a `USING` clause. |
| `SHOW CREATE TABLE text_defaults` for nullable `TEXT`, explicit `TEXT DEFAULT NULL`, and `TEXT NOT NULL` columns | Nullable text family columns omit implicit and explicit `DEFAULT NULL`; non-null text columns append `NOT NULL`. |
| `SHOW CREATE TABLE t_default_col_explicit` for `CREATE TABLE t_default_col_explicit (c VARCHAR(10) COLLATE utf8mb4_bin, d VARCHAR(10)) DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci` | Column `c` renders `varchar(10) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin DEFAULT NULL`; column `d` renders `varchar(10) DEFAULT NULL`. |
| `SHOW CREATE TABLE t_table_bin` for `CREATE TABLE t_table_bin (c VARCHAR(10), d VARCHAR(10) COLLATE utf8mb4_0900_ai_ci) DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin` | Column `c` renders `varchar(10) COLLATE utf8mb4_bin DEFAULT NULL`; column `d` renders `varchar(10) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci DEFAULT NULL`. |
| `SHOW CREATE TABLE t` with no selected database | Error `1046`, SQLSTATE `3D000`, message `No database selected`. |
| `SHOW CREATE TABLE missing_db.t` | Error `1049`, SQLSTATE `42000`, message `Unknown database 'missing_db'`. |
| `SHOW CREATE TABLE missing_table` in a selected schema | Error `1146`, SQLSTATE `42S02`, message `Table '<schema>.missing_table' doesn't exist`. |
| `SHOW CREATE TABLE information_schema.TABLES` | Returns view-shaped columns `View`, `Create View`, `character_set_client`, `collation_connection`; MyLite defers this view surface. |
| `SHOW CREATE TABLE information_schema.missing_info` | Local MySQL 8.4.9 returned error `1146`, SQLSTATE `42S02`, message `Table 'information_schema.MISSING_INFO' doesn't exist`. Existing MyLite information-schema SHOW slices use an `Unknown table 'MISSING_INFO' in information_schema` policy for unknown system objects; this slice follows that internal policy for consistency until detailed system-view diagnostics are unified. |
| `SET SESSION sql_quote_show_create = 0; SHOW CREATE TABLE meta` | Removes backticks where not required. MyLite has no session-variable surface for this yet, so the first slice keeps quoted output. |
| Escaped identifiers such as table ``weird``name``, column ``b``c``, and index ``idx``s`` | Output doubles embedded backticks inside backtick-quoted identifiers. |

## Syntax

MyLite owns the grammar below; it is intentionally authored for MyLite's Lemon
parser rather than copied from MySQL sources:

```lemon
statement ::= show_create_table_statement.

show_create_table_statement ::= SHOW CREATE TABLE table_name.
```

`SHOW CREATE VIEW` and `SHOW CREATE TABLE ...` extensions outside this form are
not accepted by this slice.

## AST

Add a `MYLITE_SQL_AST_SHOW_CREATE_TABLE_STATEMENT` node with one child: the
table target. The child is a normal `table_name`, so it may be one-part or
schema-qualified. The statement span runs from `SHOW` through the table target.

## Runtime Semantics

Target resolution:

- Two-part table names provide schema and table.
- One-part table names use the session selected schema.
- If no selected schema is available, return `No database selected`.
- If the target schema is missing, return `Unknown database '<schema>'`.
- If the target table is missing in a user schema, return
  `Table '<schema>.<table>' doesn't exist`.
- `information_schema` is matched case-insensitively and normalized to
  lowercase for validation.
- Known `information_schema` tables return `MYLITE_UNSUPPORTED` with message
  `SHOW CREATE TABLE for information_schema tables is not supported`.
- Unknown `information_schema` tables return MyLite's current deterministic
  unknown-system-table diagnostic.

Result shape:

- Successful base-table execution returns exactly two columns:
  `Table`, `Create Table`.
- The row value for `Table` is the unqualified table name.
- `Create Table` contains one complete statement.
- Successful execution produces no warnings.
- `mylite_affected_rows()` remains `-1` because the result is read-only.

Formatting:

- Identifiers are always quoted with backticks.
- Embedded backticks inside identifiers are doubled.
- String literals are quoted with single quotes.
- Embedded single quotes and backslashes inside string literals are escaped
  with a backslash, matching the current default MySQL display style.
- The first line is `CREATE TABLE <quoted-table-name> (`.
- Column lines appear in `ordinal_position` order and are indented by two
  spaces.
- Column type text comes from `__mylite_column_catalog.column_type`.
- Character columns include no charset/collation suffix when they inherit
  MySQL's default table collation. They include `COLLATE <table-collation>`
  when they inherit a non-default table collation. When a character column's
  stored collation differs from the table collation, it renders
  `CHARACTER SET <column-character-set> COLLATE <column-collation>`.
- Non-nullable columns append `NOT NULL`.
- Nullable columns with no explicit default append `DEFAULT NULL`, except text
  and blob family columns where MySQL omits the implicit default.
- Explicit defaults are rendered as `DEFAULT CURRENT_TIMESTAMP` for
  `CURRENT_TIMESTAMP` defaults and as quoted literals for other cataloged
  defaults.
- `auto_increment` in `extra` appends `AUTO_INCREMENT`.
- `on update CURRENT_TIMESTAMP` in `extra` appends
  `ON UPDATE CURRENT_TIMESTAMP`.
- `INVISIBLE` in `extra` appends `/*!80023 INVISIBLE */`.
- Column comments append `COMMENT '<comment>'`.
- Index clauses follow columns and are ordered primary key first, then unique
  indexes, then nonunique indexes, preserving catalog order within each group.
- The primary key renders as `PRIMARY KEY (<key-parts>)`.
- Unique indexes render as `UNIQUE KEY <quoted-index-name> (<key-parts>)`.
- Nonunique indexes render as `KEY <quoted-index-name> (<key-parts>)`.
- A key part renders as a quoted column name, optional `(prefix_length)`, and
  optional ` DESC` when the catalog collation is `D`.
- Key parts are separated with `,` without a following space, matching MySQL's
  default `SHOW CREATE TABLE` display.
- Indexes created with explicit `USING BTREE` render `USING BTREE` after the
  key-part list. Default BTREE indexes do not render the clause. InnoDB HASH
  fallback indexes expose effective BTREE metadata but do not render an index
  type clause.
- Index comments append `COMMENT '<comment>'`.
- Invisible non-primary indexes append `/*!80000 INVISIBLE */`.
- The closing line appends table options in deterministic MySQL order:
  `ENGINE=<engine>`, optional `AUTO_INCREMENT=<value>`, `DEFAULT CHARSET=<charset>`,
  `COLLATE=<collation>`, optional `COMMENT='<comment>'`.

## Storage And Performance

This feature is read-only. It must not mutate schema, table, column, index, or
physical SQLite storage. Runtime execution materializes one formatted row at
prepare time and then prepares a tiny SQLite `SELECT` over string literals.
That keeps the first slice simple and deterministic without introducing a new
custom result-set implementation.

## Tests

Parser coverage:

- `SHOW CREATE TABLE t`
- `SHOW CREATE TABLE db.t`
- keyword and escaped identifiers when accepted by the existing table-name
  grammar
- syntax rejection for missing target, `SHOW CREATE VIEW`, `SHOW FULL CREATE
  TABLE`, and trailing filters or schema clauses

Runtime coverage:

- selected-schema result shape and simple primary-key formatting
- schema-qualified target
- full supported metadata formatting with defaults, comments, invisible column,
  primary key, unique key, secondary key, prefix length, descending key part,
  index comment, invisible index, explicit charset/collation, table comment,
  and table `AUTO_INCREMENT`
- explicit `USING BTREE` display for create-table, standalone `CREATE INDEX`,
  and `ALTER TABLE ADD KEY`, including HASH fallback suppression
- text family columns suppressing implicit and explicit `DEFAULT NULL`
- escaped backticks in table, column, and index identifiers
- missing selected schema diagnostic
- missing schema diagnostic
- missing table diagnostic
- known `information_schema` table unsupported diagnostic
- unknown `information_schema` table diagnostic
- unsupported names with more than two identifier parts

## Compatibility Decisions

`SHOW CREATE TABLE` is intentionally catalog-backed. MyLite does not parse or
store original DDL text as the source of truth because future DDL may rewrite
metadata through `ALTER TABLE`, standalone index DDL, and compatibility
normalization.

Known information-schema tables are rejected as unsupported rather than
returning view-shaped `SHOW CREATE TABLE` output. MySQL uses the table
statement for views too, but MyLite does not yet model `CREATE VIEW` text,
definers, security, or view connection character-set metadata.

`sql_quote_show_create` is out of scope. Until MyLite has a compatible
session-variable model for it, default quoted output is the only supported
surface.

MyLite stores a private `display_index_type` flag alongside effective
`__mylite_index_catalog.index_type` metadata. This keeps
`INFORMATION_SCHEMA.STATISTICS.INDEX_TYPE` normalized to the effective MySQL
value while still allowing `SHOW CREATE TABLE` to preserve explicit
`USING BTREE` syntax when MySQL displays it.
