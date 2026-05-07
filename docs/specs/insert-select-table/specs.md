# INSERT ... SELECT From Tables

This feature expands `INSERT ... SELECT` beyond the existing single-row
`SELECT ... FROM DUAL` lowering. The first executable table-backed slice
inserts rows produced by MyLite-supported `SELECT` statements into supported
persistent and temporary base tables.

## Sources

- MySQL 8.4 Reference Manual, `INSERT ... SELECT`:
  <https://dev.mysql.com/doc/refman/8.4/en/insert-select.html>
- MySQL 8.4 Reference Manual, `SELECT`:
  <https://dev.mysql.com/doc/refman/8.4/en/select.html>
- MySQL 8.4.9 runtime probes run against the local `mylite-mysql-849`
  container, covering column-list and positional inserts, `IGNORE`, duplicate
  rollback, auto-increment allocation, persistent self-insert materialization,
  temporary self-insert diagnostics, column-count mismatches, and scoped
  `ON DUPLICATE KEY UPDATE`.

## Syntax

```lemon
insert_select_statement ::=
    INSERT opt_insert_ignore opt_into table_name opt_insert_column_list
    select_statement opt_insert_duplicate_update.
```

This slice intentionally excludes target `PARTITION` clauses and
`LOW_PRIORITY` / `HIGH_PRIORITY` / `DELAYED` modifiers, matching the current
parser support for executable `INSERT` forms. `INSERT ... SELECT FROM DUAL`
keeps its existing lowering into a single-row values insert.

## Semantics

The source `SELECT` is prepared with the same SELECT support already exposed by
MyLite: scalar and table-backed projections, filtering, ordering, limits,
joins, aggregates, unions, values queries, subqueries, and functions only where
those surfaces are otherwise supported. Unsupported source queries fail before
target mutation.

Execution has two phases:

1. Prepare and execute the source `SELECT`, materializing its visible result
   rows in statement order.
2. Insert the materialized rows through the existing `INSERT ... VALUES`
   executor so target resolution, defaults, type coercion, duplicate handling,
   CHECK constraints, foreign-key checks, `IGNORE`, `ON DUPLICATE KEY UPDATE`,
   auto-increment allocation, affected rows, last insert id, warnings, and
   rollback behavior stay shared with supported values inserts.

Materialization is required for MySQL-compatible persistent self-insert
behavior. For example, `INSERT INTO t SELECT ... FROM t` inserts only rows
visible to the source query before the insert phase starts. MySQL rejects the
same double reference for a temporary table with error 1137; MyLite must detect
this covered first-slice shape and reject it before mutation.

## Column Mapping

If a target column list is present, the source column count must match that
list. Without a target column list, the source column count must match the
target table column count. A mismatch fails with error 1136:

```text
Column count doesn't match value count at row 1
```

Rows are inserted as if each selected row were a values row in source result
order. Omitted target columns use the existing insert default path. Source
`NULL` values remain `NULL`; non-`NULL` values are copied with byte length and
then coerced by the target column's existing DML conversion logic.

## Diagnostics And Side Effects

Observed MySQL 8.4.9 behavior for covered cases:

| Case | Result |
| --- | --- |
| ordinary `INSERT INTO target(cols) SELECT ... FROM source ORDER BY ...` | affected rows equals selected row count |
| no target column list with matching source shape | inserts all source columns positionally |
| column-count mismatch | error 1136, no mutation |
| duplicate key without `IGNORE` or ODKU | error 1062, statement rollback |
| `INSERT IGNORE ... SELECT` duplicate row | warning 1062, duplicate skipped, affected rows count accepted rows |
| `AUTO_INCREMENT` target with omitted id | generated ids allocated in source order, `LAST_INSERT_ID()` is first generated id |
| persistent self-insert | source rows are materialized before insertion |
| temporary self-insert | error 1137, message `Can't reopen table: '<name>'` |
| `ON DUPLICATE KEY UPDATE` using `VALUES(col)` | update branch counts as 2 affected rows and appends warning 1287 |

Successful statements do not expose a result set. `mylite_affected_rows()`
reports the MySQL affected-row count for the insert statement, and
`ROW_COUNT()` observes the same value through the existing row-count state.

## MyLite Scope Decisions

The first slice materializes the full source result in memory. That is simple,
keeps persistent self-insert semantics correct, and reuses mature insert
validation paths. Streaming can be added later for source queries proven not to
read the target table, but it must preserve statement atomicity and warning
ordering.

The first slice rejects unsupported source SELECT shapes with a deterministic
`unsupported INSERT ... SELECT query` diagnostic rather than silently falling
back to SQLite. Target `PARTITION`, source `TABLE table_name`, source
`VALUES ROW(...)`, locking clauses, priority modifiers, and temporary
self-insert aliases beyond the direct covered shape remain deferred.

## Tests

Runtime tests must cover:

- parser acceptance for table-backed `INSERT ... SELECT` with optional `INTO`,
  target column list, `IGNORE`, `ORDER BY`, `LIMIT`, and ODKU
- ordinary column-list insertion from a source table with ordering and defaulted
  target columns
- positional insertion with no target column list
- source column-count mismatch diagnostics
- duplicate-key rollback without `IGNORE`
- `INSERT IGNORE ... SELECT` duplicate skipping and warnings
- `AUTO_INCREMENT` generation and first insert id
- persistent self-insert materialization
- temporary self-insert error 1137 without mutation
- ODKU over selected rows reusing candidate values and warning 1287 for
  `VALUES(col)`
- child foreign-key checks through the existing insert enforcement path

## Compatibility Status

`INSERT ... SELECT` remains partial until query-source `TABLE` and
`VALUES ROW(...)`, partitions, priority modifiers, lock clauses, temporary
self-reference variants, broad source-query coverage, protocol OK-info text,
and broader conversion-warning fidelity are complete.
