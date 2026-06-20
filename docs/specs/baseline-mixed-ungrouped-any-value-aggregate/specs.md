# Baseline Mixed Ungrouped ANY_VALUE Aggregate

## Status

This slice admits `ANY_VALUE(column)` in mixed ungrouped aggregate `SELECT`
lists that already contain another supported aggregate:

```sql
SELECT ANY_VALUE(descriptor_column), MAX(integer_column), COUNT(*)
FROM source
[WHERE predicate]
```

The `ANY_VALUE()` argument must be an unqualified or source-qualified
descriptor column from one descriptor-backed base table. The slice does not
change row-scalar `SELECT ANY_VALUE(expr) FROM source`, scalar no-source
`ANY_VALUE(expr)`, or grouped `ANY_VALUE(column)` behavior.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline `ANY_VALUE()`:
  `docs/specs/baseline-any-value-function/specs.md`
- MySQL 8.4 Reference Manual, miscellaneous functions:
  https://dev.mysql.com/doc/refman/8.4/en/miscellaneous-functions.html#function_any-value
- MySQL 8.4 Reference Manual, `GROUP BY` handling:
  https://dev.mysql.com/doc/refman/8.4/en/group-by-handling.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

`packages/libmylite/tests/mysql_baseline_any_value_function_expectations.sh`
records MySQL 8.4.9 behavior for this slice. Observed behavior:

- `SELECT ANY_VALUE(label), MAX(v), COUNT(*) FROM u` is accepted under the
  default `ONLY_FULL_GROUP_BY` mode.
- When every candidate `label` value is identical, the representative result is
  deterministic for tests.
- Empty input returns `NULL` for `ANY_VALUE(label)`, `NULL` for `MAX(v)`, and
  `0` for `COUNT(*)`.
- Unknown argument columns return error 1054 / SQLSTATE `42S22`.

## Supported Surface

Supported shape:

```sql
SELECT ANY_VALUE(descriptor_column) [AS alias],
       supported_aggregate [, ...]
FROM source
[WHERE supported_predicate]
[LIMIT supported_limit]
```

The statement must already route through the existing mixed aggregate planner
because another selected item is a supported aggregate such as `COUNT(*)`,
`MAX(column)`, or `SUM(column)`. A standalone table-backed
`SELECT ANY_VALUE(column) FROM source` remains a row-scalar projection and
returns one row per input row.

MyLite Lemon-syntax grammar does not need a new production for this slice; the
existing function expression production is reused:

```lemon
expression ::= ANY_VALUE LPAREN expression RPAREN.
```

The runtime, not the grammar, enforces that mixed ungrouped aggregate use has a
descriptor-column argument.

## Runtime Semantics

MyLite plans mixed ungrouped `ANY_VALUE(column)` as an aggregate-like select
item. Generated SQLite SQL selects the resolved physical descriptor column as a
bare column in the same aggregate query that computes the other aggregate
results. SQLite still performs source scanning, filtering, and aggregate
execution; MyLite does not materialize input rows or choose the representative
value in memory.

This internal lowering is compatible with MySQL's user-visible contract for the
documented subset because `ANY_VALUE()` does not promise which candidate row
supplies the representative value. Tests that need deterministic assertions
must use identical candidate values or empty input.

No SQLite fork hook is required.

## Non-Goals

This slice does not add:

- expression arguments such as `ANY_VALUE(v + 1)` in mixed aggregate queries;
- standalone aggregate-path handling for `SELECT ANY_VALUE(column) FROM source`;
- no-source mixed aggregate `ANY_VALUE()` forms;
- deterministic representative-row selection;
- hidden aggregate order keys or window forms;
- broader source forms beyond the current one-table mixed aggregate envelope.

Unsupported forms continue to return deterministic MyLite diagnostics.
