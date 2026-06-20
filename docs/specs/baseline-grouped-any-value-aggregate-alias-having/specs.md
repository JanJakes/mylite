# Baseline Grouped ANY_VALUE Aggregate Alias HAVING

## Status

This slice documents and verifies grouped `HAVING` null predicates on selected
`ANY_VALUE(column)` aliases:

```sql
SELECT group_column, ANY_VALUE(value_column) AS value
FROM source
GROUP BY group_column
HAVING value IS [NOT] NULL
```

It covers the current descriptor-backed grouped `ANY_VALUE()` envelope and the
existing grouped alias null-predicate `HAVING` subset. The `ANY_VALUE()`
argument must be an unqualified or source-qualified descriptor column.
Comparison predicates are covered by the companion alias-comparison slice.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline `ANY_VALUE()`:
  `docs/specs/baseline-any-value-function/specs.md`
- Baseline grouped aggregate `HAVING`:
  `docs/specs/baseline-having-grouped-aggregate/specs.md`
- Baseline grouped `ANY_VALUE()` alias comparison `HAVING`:
  `docs/specs/baseline-grouped-any-value-aggregate-alias-comparison-having/specs.md`
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
records MySQL 8.4.9 behavior for selected `ANY_VALUE()` alias predicates.
Observed behavior:

- `HAVING selected_any_value_alias IS NULL` filters grouped results by the
  selected representative value.
- `HAVING selected_any_value_alias IS NOT NULL` filters grouped results by the
  selected representative value.
- `ANY_VALUE()` may choose any value from a group, so deterministic tests use
  groups where every candidate value is identical or all candidates are `NULL`.

## Supported Surface

Supported shape:

```sql
SELECT group_projection,
       ANY_VALUE(descriptor_column) AS alias
FROM source
[WHERE predicate]
GROUP BY group_key [, ...]
HAVING alias IS [NOT] NULL
[ORDER BY supported_grouped_order_key]
[LIMIT supported_limit]
```

MyLite Lemon-syntax grammar snippets are unchanged for this slice; the existing
selected grouped aggregate and grouped alias `HAVING` grammar already admits
the shape.

## Runtime Semantics

MyLite lowers grouped `ANY_VALUE(column)` to the resolved physical descriptor
column in the grouped `SELECT` list. The grouped `HAVING` planner resolves the
selected alias to that result expression and applies the documented
`IS NULL`/`IS NOT NULL` predicate before public row conversion.

No SQLite fork hook is required.

## Non-Goals

This slice does not add:

- expression arguments in grouped `ANY_VALUE()`;
- repeated `HAVING ANY_VALUE(column)` expression operands, which MySQL rejects
  with unknown-column diagnostics in the verified envelope;
- deterministic representative-row selection;
- broader grouped `HAVING` predicates beyond the existing supported subset and
  companion alias-comparison slice;
- aggregate windows;
- new source forms beyond the existing grouped `ANY_VALUE()` support.

Unsupported forms continue to return deterministic MyLite diagnostics.

## Validation

Required verification for this slice:

```sh
sh -n packages/libmylite/tests/mysql_baseline_any_value_function_expectations.sh
packages/libmylite/tests/mysql_baseline_any_value_function_expectations.sh
cmake --build --preset dev --target mylite_runtime_any_value_function_test
ctest --preset dev -R '^libmylite\.runtime\.any_value_function$' --output-on-failure
git diff --check
cmake --workflow --preset check
```
