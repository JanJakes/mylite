# Baseline Grouped ANY_VALUE Aggregate Alias Order

## Status

This slice documents and verifies grouped aggregate `ORDER BY` on selected
`ANY_VALUE(column)` aliases:

```sql
SELECT group_column, ANY_VALUE(value_column) AS value
FROM source
GROUP BY group_column
ORDER BY value [ASC|DESC]
```

It covers the current descriptor-backed grouped `ANY_VALUE()` envelope. The
argument must be an unqualified or source-qualified descriptor column, and the
alias must refer to exactly one selected `ANY_VALUE()` result.

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
records MySQL 8.4.9 behavior for selected `ANY_VALUE()` alias order keys.
Observed behavior:

- `ORDER BY selected_any_value_alias ASC|DESC` is accepted for grouped
  `ANY_VALUE(column)` results.
- The order key sorts by the selected representative value.
- `NULL` representative values use ordinary MySQL aggregate ordering placement:
  first for ascending order and last for descending order.
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
[HAVING supported_group_having]
ORDER BY alias [ASC|DESC]
[LIMIT supported_limit]
```

MyLite Lemon-syntax grammar snippets are unchanged for this slice; the existing
selected grouped aggregate and grouped `ORDER BY` grammar already admits the
shape.

## Runtime Semantics

MyLite lowers grouped `ANY_VALUE(column)` to the resolved physical descriptor
column in the grouped `SELECT` list. For alias ordering, generated SQLite SQL
orders by the selected result ordinal, reusing the same representative value
that MyLite exposes publicly.

No SQLite fork hook is required.

## Non-Goals

This slice does not add:

- expression arguments in grouped `ANY_VALUE()`;
- deterministic representative-row selection;
- alias ordering outside the current grouped aggregate source envelope;
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
