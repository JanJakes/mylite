# SELECT Locking Clauses

## Scope

This feature accepts MySQL 8.4 row-locking clauses on executable `SELECT`
statement shapes:

- `FOR UPDATE`
- `FOR UPDATE OF tbl_name [, tbl_name] ...`
- `FOR UPDATE NOWAIT`
- `FOR UPDATE SKIP LOCKED`
- `FOR SHARE`
- `FOR SHARE OF tbl_name [, tbl_name] ...`
- `FOR SHARE NOWAIT`
- `FOR SHARE SKIP LOCKED`
- `LOCK IN SHARE MODE`

The first slice covers parser acceptance and no-op execution for supported
MyLite scalar, `DUAL`, and table-backed `SELECT` statements. The clause is not
represented as an executable AST child and is not emitted into SQLite SQL.

## MySQL Reference Behavior

MySQL 8.4 uses locking clauses to acquire shared or exclusive row locks for rows
read by a locking read. `NOWAIT` returns immediately if a requested row is
locked, and `SKIP LOCKED` omits locked rows from the result. `LOCK IN SHARE
MODE` is the legacy spelling for a shared locking read.

Reference:

- https://dev.mysql.com/doc/refman/8.4/en/innodb-locking-reads.html

The full behavior depends on InnoDB row locks, transaction isolation, metadata
locks, privilege checks, and concurrent server sessions. MyLite does not yet
have a row-lock manager or server-wide lock wait model.

## MyLite Behavior

MyLite accepts the supported clause spellings after the existing supported
`SELECT` clause order:

```lemon
select_statement ::= SELECT select_list opt_window_clause opt_locking_clause.
select_statement ::= SELECT select_list FROM table_references opt_where_clause
    opt_group_by_clause opt_having_clause opt_window_clause opt_order_by_clause
    opt_limit_clause opt_locking_clause.

opt_locking_clause ::= .
opt_locking_clause ::= locking_clause.

locking_clause ::= FOR UPDATE opt_locking_of_table_list opt_locking_wait.
locking_clause ::= FOR SHARE opt_locking_of_table_list opt_locking_wait.
locking_clause ::= LOCK IN SHARE MODE.

opt_locking_of_table_list ::= .
opt_locking_of_table_list ::= OF table_name_list.

opt_locking_wait ::= .
opt_locking_wait ::= NOWAIT.
opt_locking_wait ::= SKIP LOCKED.
```

Execution ignores the row-locking directive:

- result columns, rows, metadata, affected rows, and `FOUND_ROWS()` accounting
  match the same supported `SELECT` without the locking clause
- no warning is emitted for the accepted no-op
- no row locks are acquired
- `NOWAIT` and `SKIP LOCKED` do not alter row visibility
- `OF` table lists are parsed but not validated beyond normal parser syntax

This is intentionally conservative. It lets MySQL-oriented applications and dump
fragments continue through MyLite when they include locking reads, without
claiming lock semantics that the embedded engine does not yet enforce.

## Deferred

- actual row-lock acquisition and release
- lock wait, timeout, `NOWAIT`, and `SKIP LOCKED` behavior
- transaction-isolation interactions
- privilege checks specific to locking reads
- multiple locking clauses in one query expression
- locking clauses on every deferred query-expression shape
- interaction with future `TABLE`, `VALUES`, CTE, view, and derived-table
  support

## Tests

Parser tests cover `FOR UPDATE`, `FOR SHARE`, `OF` table lists, `NOWAIT`, `SKIP
LOCKED`, `LOCK IN SHARE MODE`, and rejection of unknown `FOR <identifier>`
tails.

Runtime tests cover no-op execution for scalar, `DUAL`, and table-backed
`SELECT` statements, including `ORDER BY`/`LIMIT` combined with `SKIP LOCKED`.
