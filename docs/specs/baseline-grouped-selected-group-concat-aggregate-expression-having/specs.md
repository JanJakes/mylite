# Baseline Grouped Selected GROUP_CONCAT Aggregate Expression HAVING

## Status

This slice extends grouped aggregate `HAVING` so a selected `GROUP_CONCAT()`
aggregate expression may be repeated as an `IS NULL` or `IS NOT NULL` operand
inside the existing grouped `GROUP_CONCAT()` envelope:

```sql
SELECT group_column,
       GROUP_CONCAT(value ORDER BY order_column SEPARATOR ':') AS names
FROM source
GROUP BY group_column
HAVING GROUP_CONCAT(value ORDER BY order_column SEPARATOR ':') IS [NOT] NULL
```

The repeated expression must match exactly one selected `GROUP_CONCAT()`
aggregate after descriptor planning. This does not add nonselected
`GROUP_CONCAT()` HAVING operands, aggregate-value comparisons, broader
argument support, or new source forms.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline `GROUP_CONCAT()` aggregate:
  `docs/specs/baseline-group-concat-aggregate/specs.md`
- Baseline grouped `GROUP_CONCAT()` alias HAVING:
  `docs/specs/baseline-grouped-group-concat-aggregate-alias-having/specs.md`
- Baseline grouped selected `GROUP_CONCAT()` expression ordering:
  `docs/specs/baseline-grouped-selected-group-concat-aggregate-expression-order/specs.md`
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
records MySQL 8.4.9 behavior for repeated selected `GROUP_CONCAT()`
expression predicates. Observed behavior:

- `HAVING GROUP_CONCAT(...) IS NOT NULL` keeps groups whose selected aggregate
  result is a non-`NULL` concatenated string.
- `HAVING GROUP_CONCAT(...) IS NULL` keeps groups whose selected aggregate
  result is `NULL`, including all-`NULL` value groups.
- The repeated expression must be semantically aligned with the selected
  aggregate result for MyLite's supported subset: value expression, `DISTINCT`,
  aggregate-local order key, order direction, and separator are all part of the
  selected-expression match.

## Supported Surface

Supported shape:

```sql
SELECT group_projection,
       GROUP_CONCAT([DISTINCT] value_expression
                    [ORDER BY descriptor_order_column [ASC|DESC]]
                    [SEPARATOR 'literal']) [AS alias]
FROM source
[WHERE predicate]
GROUP BY group_key [, ...]
HAVING GROUP_CONCAT([DISTINCT] value_expression
                    [ORDER BY descriptor_order_column [ASC|DESC]]
                    [SEPARATOR 'literal']) IS [NOT] NULL
[ORDER BY supported_grouped_order_key]
[LIMIT supported_limit]
```

The selected and repeated `GROUP_CONCAT()` calls must match after descriptor
planning. The aggregate remains limited to the current descriptor-backed
grouped envelope:

- one grouped `GROUP_CONCAT()` result;
- one descriptor-backed base-table source for grouped `GROUP_CONCAT()`;
- descriptor value column or already-supported row-scalar value expression;
- optional single descriptor-column aggregate-local order key using the
  existing `GROUP_CONCAT()` order restrictions;
- optional literal separator;
- optional single-value `DISTINCT`.

MyLite Lemon-syntax grammar snippets are unchanged for this slice; the existing
selected grouped aggregate and grouped `HAVING` grammar already admits the
shape.

## Runtime Semantics

The grouped `HAVING` planner parses the repeated `GROUP_CONCAT()` call into a
temporary grouped aggregate item and compares it to the selected aggregate
items. A match is accepted only when exactly one selected aggregate has the
same function, distinct flag, value operand, aggregate-local order, and
separator. The SQLite SQL builder then emits the same descriptor-derived
aggregate call for the `HAVING` operand and appends `IS NULL` or `IS NOT NULL`.

No parser change, public ABI change, file-format change, or SQLite fork hook is
required.

## Non-Goals

This slice does not add:

- nonselected `HAVING GROUP_CONCAT(...)` expression operands;
- aggregate-value comparisons such as `HAVING GROUP_CONCAT(...) = 'alpha:beta'`;
- selected aliases, which are covered by
  `docs/specs/baseline-grouped-group-concat-aggregate-alias-having/specs.md`;
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
