# Baseline `GROUP BY ... WITH ROLLUP`

## Scope

This slice implements a bounded executable `GROUP BY ... WITH ROLLUP` subset for
MyLite's existing grouped aggregate runtime:

- one descriptor-backed group key;
- the current grouped aggregate source envelope, including supported table,
  joined, and derived sources that already plan successfully;
- supported projection-only grouped queries and supported aggregate functions in
  the current grouped aggregate envelope;
- supported source `WHERE` predicates;
- MySQL-compatible omission of the rollup row when the filtered source produces
  no ordinary grouped rows.

The supported rollup row is appended after the ordinary grouped rows. Every
projected group expression cell in the rollup row is SQL `NULL`; aggregate cells
are computed by an aggregate-only query over the same source and `WHERE`
predicate.

## MySQL Behavior

The authoritative documentation source is the MySQL 8.4 Reference Manual,
section 14.19.2, `GROUP BY` Modifiers:

https://dev.mysql.com/doc/refman/8.4/en/group-by-modifiers.html

Observed MySQL 8.4.9 behavior for this slice:

- `SELECT g, COUNT(*) FROM t GROUP BY g WITH ROLLUP` returns ordinary grouped
  rows followed by a final row where `g` is `NULL` and the count is the total.
- Ordinary `NULL` group values remain ordinary rows; `GROUPING(g)` returns `0`
  for those rows and `1` for the rollup total row.
- `WHERE` filters are applied before grouping and rollup aggregation.
- If the filtered input has no groups, no rollup row is returned.
- `SELECT g FROM t GROUP BY g WITH ROLLUP` returns ordinary grouped keys plus a
  final `NULL` row.

The runtime expectations are recorded in
`packages/libmylite/tests/mysql_baseline_group_by_rollup_expectations.sh` and
verified against the `mylite-mysql-849` MySQL 8.4.9 container.

## Syntax

MyLite already parses the supported syntax:

```lemon
group_clause_opt ::= GROUP BY group_key_list group_rollup_opt.
group_rollup_opt ::= .
group_rollup_opt ::= WITH ROLLUP.
```

The alternative MySQL syntax `GROUP BY ROLLUP (expr [, expr]...)` is not part of
this slice.

## Runtime Design

The implementation stays in MyLite's grouped aggregate planner and runtime:

1. Planning records a `with_rollup` bit when the parser marker is present.
2. The slice rejects unsupported combinations before lowering:
   `DISTINCT`, `SQL_CALC_FOUND_ROWS`, multiple group keys, `HAVING`, `ORDER BY`,
   and `LIMIT`.
3. Normal grouped rows continue to execute through the existing SQLite grouped
   aggregate query.
4. If at least one grouped row was emitted, MyLite executes a second
   aggregate-only SQLite query over the same source and source predicate.
5. The rollup result row is assembled by setting projection cells to SQL `NULL`
   and reusing the existing aggregate-cell formatter for aggregate cells.

This uses public SQLite execution APIs and MyLite's SQL lowering layer. No
targeted SQLite fork hook is required.

## Unsupported Behavior

The broad MySQL feature remains limited:

- multi-key rollups with intermediate subtotal rows;
- `GROUPING()` execution;
- alternative `GROUP BY ROLLUP(...)` syntax;
- rollup with `HAVING`, `ORDER BY`, `LIMIT`, `DISTINCT`, or
  `SQL_CALC_FOUND_ROWS`;
- full late-stage rollup sorting/filtering/limiting semantics;
- rollup inside `INFORMATION_SCHEMA` grouped metadata queries.

These combinations continue to return explicit MyLite diagnostics rather than
silently producing approximate rows.

## Tests

- MySQL 8.4.9 expectation script:
  `packages/libmylite/tests/mysql_baseline_group_by_rollup_expectations.sh`
- Runtime coverage:
  `packages/libmylite/tests/runtime_group_by_single_column_aggregate_test.c`

Focused verification:

```sh
sh -n packages/libmylite/tests/mysql_baseline_group_by_rollup_expectations.sh
packages/libmylite/tests/mysql_baseline_group_by_rollup_expectations.sh
cmake --build --preset dev --target mylite_runtime_group_by_single_column_aggregate_test
ctest --preset dev -R '^libmylite\.runtime\.group_by_single_column_aggregate$' --output-on-failure
```
