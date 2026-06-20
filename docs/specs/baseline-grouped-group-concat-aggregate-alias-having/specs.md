# Baseline Grouped GROUP_CONCAT Aggregate Alias HAVING

## Status

This slice extends grouped aggregate `HAVING` so selected `GROUP_CONCAT()`
aggregate aliases may be used in `IS NULL` and `IS NOT NULL` predicates inside
the existing grouped `GROUP_CONCAT()` envelope:

```sql
SELECT group_column,
       GROUP_CONCAT(value ORDER BY order_column SEPARATOR ':') AS names
FROM source
GROUP BY group_column
HAVING names IS [NOT] NULL
```

The aggregate must be selected and uniquely identified by the alias. This does
not add direct `HAVING GROUP_CONCAT(...)` predicates, aggregate-value
comparisons, broader argument support, or new source forms.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline `GROUP_CONCAT()` aggregate:
  `docs/specs/baseline-group-concat-aggregate/specs.md`
- Baseline grouped aggregate `HAVING`:
  `docs/specs/baseline-having-grouped-aggregate/specs.md`
- MySQL 8.4 Reference Manual, aggregate functions:
  https://dev.mysql.com/doc/refman/8.4/en/aggregate-functions.html
- MySQL 8.4 Reference Manual, `SELECT` syntax:
  https://dev.mysql.com/doc/refman/8.4/en/select.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

`packages/libmylite/tests/mysql_baseline_group_concat_aggregate_expectations.sh`
records MySQL 8.4.9 behavior for selected `GROUP_CONCAT()` alias predicates.
Observed behavior:

- `HAVING selected_group_concat_alias IS NOT NULL` keeps groups whose selected
  aggregate result is a non-`NULL` concatenated string.
- `HAVING selected_group_concat_alias IS NULL` keeps groups whose selected
  aggregate result is `NULL`, including all-`NULL` value groups.
- The predicate is evaluated after grouping and before public result ordering.
- Result row count and warning-count behavior remains the same as the existing
  grouped `GROUP_CONCAT()` path for supported short outputs.

## Supported Surface

Supported shape:

```sql
SELECT group_projection,
       GROUP_CONCAT([DISTINCT] value_expression
                    [ORDER BY descriptor_order_column [ASC|DESC]]
                    [SEPARATOR 'literal']) AS alias
FROM source
[WHERE predicate]
GROUP BY group_key [, ...]
HAVING alias IS [NOT] NULL
[ORDER BY supported_grouped_order_key]
[LIMIT supported_limit]
```

The `GROUP_CONCAT()` call remains limited to the current descriptor-backed
grouped envelope:

- one grouped `GROUP_CONCAT()` result;
- one descriptor-backed base-table source for grouped `GROUP_CONCAT()`;
- descriptor value column or already-supported row-scalar value expression;
- optional single descriptor-column aggregate-local order key using the
  existing `GROUP_CONCAT()` order restrictions;
- optional literal separator;
- optional single-value `DISTINCT`.

MyLite Lemon-syntax grammar snippets are unchanged for this slice; the existing
selected grouped aggregate and grouped alias `HAVING` grammar already admits
the shape.

## Runtime Semantics

The grouped `HAVING` planner resolves a selected aggregate alias to the already
planned `GROUP_CONCAT()` item. The SQLite SQL builder emits the same
descriptor-derived aggregate call used for the selected result, without a
public result alias, and appends the `IS NULL` or `IS NOT NULL` predicate.

This preserves the current source, argument, aggregate-local order, separator,
and distinct restrictions. No parser change, public ABI change, file-format
change, or SQLite fork hook is required.

## Non-Goals

This slice does not add:

- direct `HAVING GROUP_CONCAT(...)` expression operands;
- aggregate-value comparisons such as `HAVING names = 'alpha:beta'`;
- nonselected aggregate aliases or ambiguous aliases;
- multiple grouped `GROUP_CONCAT()` results or grouped `GROUP_CONCAT()` mixed
  with other aggregate results;
- broader `GROUP_CONCAT()` value, distinct, separator, or aggregate-local order
  forms;
- aggregate windows;
- source forms outside the current grouped `GROUP_CONCAT()` envelope.

Unsupported forms continue to return deterministic MyLite diagnostics.

## Validation

Required verification for this slice:

```sh
sh -n packages/libmylite/tests/mysql_baseline_group_concat_aggregate_expectations.sh
packages/libmylite/tests/mysql_baseline_group_concat_aggregate_expectations.sh
cmake --build --preset dev --target mylite_runtime_group_concat_aggregate_test
ctest --preset dev -R '^libmylite\.runtime\.group_concat_aggregate$' --output-on-failure
git diff --check
cmake --workflow --preset check
```
