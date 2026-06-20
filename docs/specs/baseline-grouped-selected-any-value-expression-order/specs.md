# Baseline Grouped Selected ANY_VALUE Expression Order

## Status

This slice admits grouped `ORDER BY` keys that repeat a selected
`ANY_VALUE(column)` expression in the existing grouped `ANY_VALUE()` envelope:

```sql
SELECT group_column, ANY_VALUE(value_column) AS value
FROM source
GROUP BY group_column
ORDER BY ANY_VALUE(value_column) [ASC|DESC]
```

The repeated expression must match exactly one selected `ANY_VALUE(column)`
result after MyLite descriptor planning. Hidden grouped order keys are covered
by `docs/specs/baseline-hidden-grouped-aggregate-order-keys/specs.md`; this
slice does not broaden grouped `ANY_VALUE()` arguments beyond descriptor
columns.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline `ANY_VALUE()`:
  `docs/specs/baseline-any-value-function/specs.md`
- MySQL 8.4 Reference Manual, miscellaneous functions:
  https://dev.mysql.com/doc/refman/8.4/en/miscellaneous-functions.html#function_any-value
- MySQL 8.4 Reference Manual, `SELECT` syntax:
  https://dev.mysql.com/doc/refman/8.4/en/select.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

`packages/libmylite/tests/mysql_baseline_any_value_function_expectations.sh`
records MySQL 8.4.9 behavior for repeated selected `ANY_VALUE()` expression
order keys. Observed behavior:

- `ORDER BY ANY_VALUE(column) ASC|DESC` is accepted when the selected list also
  contains the same `ANY_VALUE(column)` expression.
- The order key sorts by the selected representative value, with ordinary MySQL
  `NULL` placement for the requested direction.
- `ANY_VALUE()` can choose any value from a group; deterministic tests must use
  groups where every candidate is identical or all candidates are `NULL`.

## Supported Surface

Supported shape:

```sql
SELECT group_projection,
       ANY_VALUE(descriptor_column) [AS alias]
FROM source
[WHERE predicate]
GROUP BY group_key [, ...]
[HAVING supported_group_having]
ORDER BY ANY_VALUE(descriptor_column) [ASC|DESC]
         [, supported_grouped_order_key ...]
[LIMIT supported_limit]
```

The selected and repeated `ANY_VALUE()` calls must match after descriptor
resolution:

- same function kind;
- same unqualified or source-qualified descriptor column;
- same resolved source index.

MyLite Lemon-syntax grammar snippets for this slice:

```lemon
selected_grouped_aggregate_expression ::=
    ANY_VALUE LPAREN expression RPAREN.

select_order_key ::= selected_grouped_aggregate_expression.
```

The runtime keeps enforcing the grouped `ANY_VALUE(column)` descriptor-column
restriction after parsing.

## Runtime Semantics

MyLite plans the repeated `ORDER BY ANY_VALUE(column)` with the same grouped
aggregate-like planner used for selected `ANY_VALUE()` items. If it matches
exactly one selected `ANY_VALUE(column)` result, generated SQLite SQL orders by
the selected output-column ordinal. This reuses MyLite's existing
representative-value selection and result conversion; MyLite does not
materialize groups or recompute the order expression outside SQLite.

No SQLite fork hook is required.

## Non-Goals

This slice does not add:

- repeated expressions that refer to a different descriptor column or source;
- expression arguments in grouped `ANY_VALUE()`;
- mixed ungrouped aggregate `ANY_VALUE()` execution;
- deterministic representative-row selection;
- executable window forms.

Unsupported forms continue to return deterministic MyLite diagnostics.
