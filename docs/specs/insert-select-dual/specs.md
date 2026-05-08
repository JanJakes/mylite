# INSERT SELECT FROM DUAL specification

## Scope

This slice covers MySQL's one-row `DUAL` source and equivalent no-`FROM`
constant source for `INSERT ... SELECT`:

```sql
INSERT [IGNORE] [INTO] tbl_name [(col_name [, col_name] ...)]
SELECT [ALL | DISTINCT] select_expr [, select_expr] ...
[FROM DUAL]
[WHERE expression]
[ORDER BY order_expr [, order_expr] ...]
[LIMIT row_count]
[ON DUPLICATE KEY UPDATE assignment [, assignment] ...]
```

`DUAL` remains a special row source, not a catalog table. Omitting `FROM DUAL`
uses the same single-row constant source. Real table sources, joins, CTEs,
`TABLE`, `VALUES`, grouping, `HAVING`, locking clauses, query expressions,
partitions, priority modifiers, and broad row-source column reference semantics
remain part of broader `INSERT ... SELECT` work.

## MySQL 8.4.9 Behavior

Behavior was verified against the local `mylite-mysql-849` container using:

```sh
docker exec mylite-mysql-849 mysql -uroot --batch --raw --column-type-info -vvv
```

Observed results:

```sql
CREATE TABLE t(a INT, b VARCHAR(20));
INSERT INTO t SELECT 1, 'one' FROM DUAL;
INSERT t SELECT 2 AS a, 'two' AS b FROM DUAL;
INSERT INTO t (b, a) SELECT 'three', 3 FROM DUAL;
INSERT INTO t SELECT 4, 'four' FROM DUAL WHERE TRUE;
INSERT INTO t SELECT 5, 'five' FROM DUAL WHERE FALSE;
INSERT INTO t SELECT 6, 'six' FROM DUAL ORDER BY 1 LIMIT 0;
INSERT INTO t SELECT DISTINCT 7, 'seven' FROM DUAL WHERE TRUE ORDER BY 1 LIMIT 1;
INSERT INTO t SELECT 8, 'eight';
INSERT t SELECT 9, 'nine';
INSERT INTO t (b, a) SELECT 'ten', 10 WHERE TRUE;
```

The first, second, third, fourth, seventh, eighth, ninth, and tenth statements
report one affected row. The false predicate and `LIMIT 0` statements report
zero affected rows and insert no row. Projection aliases do not affect target
mapping. Explicit target column order controls destination columns.

Standalone `SELECT * FROM DUAL` and `INSERT ... SELECT * FROM DUAL` fail with
error 1096, `No tables used`. MySQL rejects aliases after `DUAL` as syntax
errors.

## MyLite Semantics

MyLite parses the supported form as `MYLITE_SQL_AST_INSERT_SELECT_STATEMENT`
whose source is a normal scalar `SELECT` with either `MYLITE_SQL_AST_FROM_DUAL`
or no `FROM` child. Execution materializes the scalar source rows through the
shared `INSERT ... SELECT` path.

The supported path reuses existing insert behavior for:

- optional `INTO`
- explicit target column lists
- schema/default-schema target resolution
- default and `AUTO_INCREMENT` handling
- duplicate checks
- `INSERT IGNORE`
- scoped `ON DUPLICATE KEY UPDATE`
- affected rows and rollback behavior
- child foreign-key checks

`WHERE` is applied before projection evaluation, so a false predicate suppresses
projection warnings and errors such as division by zero. `ORDER BY` is validated
against the scalar result metadata, and `LIMIT 0` suppresses the single DUAL row.
`ALL` and `DISTINCT` are accepted; with a single possible row they do not change
the result set.

Projection aliases are ignored for inserted values. Wildcard projections are
rejected with MySQL's `No tables used` diagnostic.

## Parser Shape

The parser accepts the scoped form with independently authored Lemon productions
equivalent to:

```lemon
insert_select_statement ::= INSERT opt_insert_ignore opt_into table_name
    opt_insert_column_list insert_select_source_statement
    opt_insert_duplicate_update.

insert_select_source_statement ::= SELECT select_modifiers select_item_list
    FROM DUAL opt_where_clause opt_window_clause opt_order_by_clause
    opt_limit_clause.

insert_select_source_statement ::= SELECT select_modifiers select_item_list
    opt_where_clause opt_window_clause opt_order_by_clause opt_limit_clause.
```

`DUAL` aliases are intentionally not accepted by this production. The no-`FROM`
form has the same one-row behavior as `FROM DUAL` for this insert slice.

## Compatibility Notes

This slice addresses applications and generated SQL that use either omitted
`FROM` or `FROM DUAL` as a portable constant row source. It does not claim broad
insert-from-query support.
