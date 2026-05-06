# INSERT SELECT FROM DUAL specification

## Scope

This slice covers the constant single-row MySQL form:

```sql
INSERT [IGNORE] [INTO] tbl_name [(col_name [, col_name] ...)]
SELECT select_expr [, select_expr] ... FROM DUAL
[ON DUPLICATE KEY UPDATE assignment [, assignment] ...]
```

Broader `INSERT ... SELECT` query sources remain deferred, including real table
sources, joins, CTEs, `TABLE`, `VALUES`, locking clauses, query ordering,
query limits, partitions, priority modifiers, and full row-source column
reference semantics.

## MySQL 8.4.9 Behavior

The behavior was verified against the local `mylite-mysql-849` container using:

```sh
docker exec -i mylite-mysql-849 mysql -uroot --batch --raw --show-warnings --force
```

Observed results:

```sql
CREATE TABLE t(a INT, b VARCHAR(20));
INSERT INTO t SELECT 1, 'one' FROM DUAL;
SELECT ROW_COUNT();
INSERT t SELECT 2 AS a, 'two' AS b FROM DUAL;
SELECT ROW_COUNT();
INSERT INTO t (b, a) SELECT 'three', 3 FROM DUAL;
SELECT ROW_COUNT();
SELECT a, b FROM t ORDER BY a;
```

Each insert reports `ROW_COUNT() = 1`. The projection aliases do not affect
target mapping. Explicit target column order controls the destination columns,
so the final rows are `(1, 'one')`, `(2, 'two')`, and `(3, 'three')`.

## MyLite Semantics

MyLite lowers the supported `SELECT ... FROM DUAL` form into the existing
`INSERT ... VALUES` AST and execution path. The lowered statement behaves like
a single `VALUES` row containing the projection expressions in source order.

This deliberately reuses the existing insert implementation for:

- optional `INTO`
- explicit target column lists
- schema/default-schema target resolution
- default and `AUTO_INCREMENT` handling
- duplicate checks
- `INSERT IGNORE`
- scoped `ON DUPLICATE KEY UPDATE`
- affected rows and rollback behavior

Projection aliases are ignored for inserted values. Wildcard projections are
not part of this slice. Unsupported projection expressions reach the same
diagnostics used by `INSERT ... VALUES`.

## Parser Shape

The parser accepts the scoped form with an independently authored Lemon
production equivalent to:

```lemon
insert_values_statement ::= INSERT opt_insert_ignore opt_into table_name
    opt_insert_column_list SELECT select_modifiers select_item_list FROM DUAL
    opt_insert_duplicate_update.
```

The parser rejects explicit select modifiers for this lowered path. Future
full `INSERT ... SELECT` work should introduce a distinct AST node rather than
overloading the values-row executor.

## Compatibility Notes

This slice addresses applications and generated SQL that use `FROM DUAL` as a
portable one-row constant source. It does not claim broad insert-from-query
support.
