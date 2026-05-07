# SHOW INDEX / SHOW INDEXES / SHOW KEYS

This feature implements the first executable `SHOW INDEX` metadata slice for
MyLite. It targets persistent MyLite base-table indexes already represented in
`__mylite_index_catalog`.

Supported forms:

- `SHOW INDEX FROM tbl_name`
- `SHOW INDEXES FROM tbl_name`
- `SHOW KEYS FROM tbl_name`
- `SHOW [EXTENDED] {INDEX | INDEXES | KEYS} {FROM | IN} tbl_name`
- `SHOW [EXTENDED] {INDEX | INDEXES | KEYS} {FROM | IN} tbl_name {FROM | IN} db_name`
- `SHOW [EXTENDED] {INDEX | INDEXES | KEYS} {FROM | IN} db_name.tbl_name`
- `SHOW [EXTENDED] {INDEX | INDEXES | KEYS} {FROM | IN} tbl_name WHERE expr`

Deferred:

- hidden storage-engine key parts exposed by MySQL `SHOW EXTENDED INDEX`
- generated invisible primary key rows controlled by
  `show_gipk_in_create_table_and_information_schema`
- functional key parts with non-`NULL` `Expression`
- user views, temporary tables, privilege-sensitive visibility, locks, and
  optimizer/statistics updates

## References

- MySQL 8.4 Reference Manual, `SHOW INDEX` Statement:
  https://dev.mysql.com/doc/refman/8.4/en/show-index.html
- MySQL 8.4 Reference Manual, `SHOW` Statements:
  https://dev.mysql.com/doc/refman/8.4/en/show.html
- MySQL 8.4 Reference Manual, Extensions to `SHOW` Statements:
  https://dev.mysql.com/doc/refman/8.4/en/extended-show.html

## MySQL 8.4.9 Runtime Findings

The following expectations were verified against MySQL 8.4.9.

| Case | Observed behavior |
| --- | --- |
| `SHOW INDEX FROM meta` | Returns columns `Table`, `Non_unique`, `Key_name`, `Seq_in_index`, `Column_name`, `Collation`, `Cardinality`, `Sub_part`, `Packed`, `Null`, `Index_type`, `Comment`, `Index_comment`, `Visible`, `Expression`. |
| `SHOW INDEXES FROM meta` | Same result shape and rows as `SHOW INDEX FROM meta`. |
| `SHOW KEYS FROM meta` | Same result shape and rows as `SHOW INDEX FROM meta`. |
| `SHOW EXTENDED INDEX FROM meta` | Includes normal rows plus hidden InnoDB key parts. |
| `SHOW INDEX FROM meta WHERE Seq_in_index = 2` | Parses and filters rows using displayed `SHOW INDEX` column names. |
| `SHOW INDEX FROM meta` with no selected database | Error `1046`, SQLSTATE `3D000`, message `No database selected`. |
| `SHOW INDEX FROM missing_db.meta` | Error `1049`, SQLSTATE `42000`, message `Unknown database 'missing_db'`. |
| `SHOW INDEX FROM missing_table FROM db` | Error `1146`, SQLSTATE `42S02`, message `Table 'db.missing_table' doesn't exist`. |
| `SHOW INDEX FROM TABLES FROM information_schema` | Succeeds with the standard result columns and no rows in the verified runtime. |
| `SHOW INDEX FROM missing_info FROM information_schema` | Error `1109`, SQLSTATE `42S02`, message `Unknown table 'MISSING_INFO' in information_schema`. |
| `SHOW INDEX FROM db1.t FROM db2` | The explicit schema after the table target wins; MySQL reports indexes for `db2.t`. |
| `SHOW INDEX FROM db.t LIKE 'x%'` | Syntax error; this statement supports `WHERE`, not `LIKE`. |

For a table created with a primary key, a unique index, and a nonunique
multi-part index with prefix, descending order, comment, and invisibility,
MySQL returns:

- primary key: `Non_unique = 0`, `Key_name = PRIMARY`, `Seq_in_index = 1`,
  `Collation = A`, `Cardinality = 0` before table statistics are available,
  `Sub_part = NULL`, `Null = ''`, `Index_type = BTREE`, `Visible = YES`,
  `Expression = NULL`
- unique key: `Non_unique = 0`, index name as created, normal key-part order
- nonunique key: `Non_unique = 1`; key parts are ordered by `Seq_in_index`;
  descending parts use `Collation = D`, ascending parts use `A`; nullable
  columns use `Null = YES`; prefix lengths appear in `Sub_part`; invisible
  indexes use `Visible = NO`; comments appear in `Index_comment`

MySQL `SHOW INDEX ... WHERE Key_name = 'mixedcase'` matched a mixed-case index
name in the verified runtime. The filter uses the displayed `SHOW INDEX` column
names.

The verified MySQL 8.4.9 field descriptors for `SHOW INDEX` are also stable for
newly created tables. MyLite attaches these descriptors for supported
catalog-backed targets:

| Result column | Field type | Length | Charset | Required metadata flags |
| --- | --- | --- | --- | --- |
| `Table` | `VAR_STRING` | `64` | `utf8mb3` (`8`) | `NOT_NULL`, `BINARY`, `NO_DEFAULT_VALUE`; visible table `SHOW_STATISTICS`, origin table `tables`. |
| `Non_unique` | `LONG` | `2` | binary (`63`) | `NOT_NULL`, `NUM`. |
| `Key_name` | `VAR_STRING` | `64` | `utf8mb3` (`8`) | Nullable. |
| `Seq_in_index` | `LONG` | `10` | binary (`63`) | `NOT_NULL`, `UNSIGNED`, `NO_DEFAULT_VALUE`, `NUM`; origin table `index_column_usage`. |
| `Column_name` | `VAR_STRING` | `64` | `utf8mb3` (`8`) | Nullable. |
| `Collation` | `VAR_STRING` | `1` | `utf8mb3` (`8`) | Nullable. |
| `Cardinality` | `LONGLONG` | `21` | binary (`63`) | Nullable, `NUM`. |
| `Sub_part` | `LONGLONG` | `21` | binary (`63`) | Nullable, `NUM`. |
| `Packed` | `NULL` | `0` | binary (`63`) | Nullable, `BINARY`, `NUM`. |
| `Null` | `VAR_STRING` | `3` | `utf8mb3` (`8`) | `NOT_NULL`. |
| `Index_type` | `VAR_STRING` | `11` | `utf8mb3` (`8`) | `NOT_NULL`, `BINARY`. |
| `Comment` | `VAR_STRING` | `8` | `utf8mb3` (`8`) | `NOT_NULL`. |
| `Index_comment` | `VAR_STRING` | `2048` | `utf8mb3` (`8`) | `NOT_NULL`, `BINARY`, `NO_DEFAULT_VALUE`; origin table `indexes`. |
| `Visible` | `VAR_STRING` | `3` | `utf8mb3` (`8`) | `NOT_NULL`. |
| `Expression` | `BLOB` | `4294967295` | `utf8mb3` (`8`) | Nullable, `BLOB`, `BINARY`. |

## Syntax And AST

MyLite grammar accepts the MySQL statement shape and records the spelling only
as one statement kind:

```lemon
statement ::= show_index_statement.

show_index_statement ::= SHOW opt_extended show_index_keyword FROM table_name
                         opt_show_index_schema opt_show_index_filter.
show_index_statement ::= SHOW opt_extended show_index_keyword IN table_name
                         opt_show_index_schema opt_show_index_filter.

show_index_keyword ::= INDEX.
show_index_keyword ::= INDEXES.
show_index_keyword ::= KEYS.

opt_show_index_schema ::= .
opt_show_index_schema ::= FROM identifier.
opt_show_index_schema ::= IN identifier.

opt_show_index_filter ::= .
opt_show_index_filter ::= where_clause.
```

`INDEXES` remains usable as an unquoted identifier outside the `SHOW INDEX`
keyword position. `KEYS` follows MySQL reserved-keyword behavior and is accepted
as an object identifier only when quoted or otherwise already accepted by
existing MyLite grammar rules.

The AST node is `MYLITE_SQL_AST_SHOW_INDEX_STATEMENT`. Children are appended in
this order:

1. table target
2. optional explicit schema target
3. optional `WHERE` clause

The node records `show_index_extended` when the optional `EXTENDED` keyword is
present.

## Runtime Semantics

Target resolution mirrors `SHOW COLUMNS`:

- one-part table names use the selected schema
- two-part table names provide schema and table
- an explicit trailing `{FROM | IN} db_name` overrides a schema qualifier in
  `db.table`
- more than two table-name parts are rejected with a deterministic unsupported
  diagnostic
- `information_schema` is normalized case-insensitively to lowercase in
  diagnostics and generated SQL

Validation order:

1. resolve schema/table target
2. require selected schema for unqualified table names
3. validate schema existence
4. for `information_schema`, validate known system table names and return an
   empty result set for known tables
5. validate persistent base table existence for user schemas

Successful execution selects from `__mylite_index_catalog` with MySQL-compatible
display columns and ordering:

1. `Table`
2. `Non_unique`
3. `Key_name`
4. `Seq_in_index`
5. `Column_name`
6. `Collation`
7. `Cardinality`
8. `Sub_part`
9. `Packed`
10. `Null`
11. `Index_type`
12. `Comment`
13. `Index_comment`
14. `Visible`
15. `Expression`

Rows are ordered by the internal catalog row order, which preserves the
DDL-defined index order for currently supported `CREATE TABLE`, `ALTER TABLE`,
and standalone `CREATE INDEX` paths. The existing catalog already stores
supported primary, unique, ordinary, prefix, collation, comment, visibility,
nullable, and effective index-type metadata. Cardinality uses MySQL's observed
zero placeholder for newly created rows until statistics maintenance exists.

`SHOW INDEX ... WHERE expr` filters rows after mapping the displayed column names
to the result shape. The executable subset includes string and numeric literals,
`NULL`/boolean literals, unary signs and `NOT`, comparison predicates, `LIKE`,
`IN`/`NOT IN`, `IS NULL`/`IS NOT NULL`, parentheses, `AND`, and `OR`. Unsupported
filter expressions return `MYLITE_UNSUPPORTED` with
`SHOW INDEX WHERE expression is not supported`.

`SHOW EXTENDED INDEX` currently returns the same rows as `SHOW INDEX`, because
MyLite does not have hidden storage-engine key parts.

## Tests

Parser tests cover:

- `INDEX`, `INDEXES`, and `KEYS` synonyms
- `FROM` and `IN`
- selected schema, explicit schema, and `db.table`
- explicit-schema override with `db.table FROM other_db`
- `EXTENDED`
- `WHERE` parsing
- rejected `LIKE`
- rejected missing table target
- rejected duplicate schema clauses
- `INDEXES` as an unquoted identifier where MySQL permits it

Runtime tests cover:

- no selected schema diagnostic
- MySQL 8.4.9-derived result-column descriptors
- primary, unique, nonunique, and multi-part index rows
- prefix lengths, ascending/descending collation, comments, nullable flag,
  zero cardinality placeholders, index visibility, and index type values
  already preserved by the catalog
- `INDEXES` and `KEYS` synonyms
- `FROM` and `IN`
- explicit schema and `db.table`
- explicit-schema override behavior
- tables with no indexes returning no rows with metadata columns
- missing schema and missing table diagnostics
- `WHERE` equality, `IN`, and boolean disjunction filtering
- known `information_schema` tables returning no rows
- unknown `information_schema` tables using MySQL's uppercase object-name
  diagnostic
- `INDEXES` as a table/index name where MySQL permits it

## Compatibility Decisions

This slice is intentionally catalog-backed. It does not inspect SQLite's
physical indexes because MyLite metadata is the compatibility source of truth
for MySQL names, comments, visibility, key-part ordering, and future functional
key parts.

`SHOW EXTENDED INDEX` is executable rather than rejected. Returning visible
catalog rows is correct for MyLite's current storage model because hidden InnoDB
metadata columns do not exist.

`WHERE` filtering is implemented for the expression subset above by rendering the
parsed predicate over the MySQL display-column aliases. Broader expression
coverage remains a future shared `SHOW` filtering task.
