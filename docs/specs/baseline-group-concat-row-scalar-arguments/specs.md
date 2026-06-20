# Baseline GROUP_CONCAT Row-Scalar Arguments

## Status

This slice documents and verifies supported row-scalar value expressions inside
the existing descriptor-backed `GROUP_CONCAT()` aggregate envelope:

```sql
SELECT GROUP_CONCAT(IFNULL(name, '') ORDER BY id SEPARATOR ':') FROM source;
SELECT group_column,
       GROUP_CONCAT(CONCAT(name, note) ORDER BY id SEPARATOR '|') AS names
FROM source
GROUP BY group_column
ORDER BY names;
```

It does not change the `GROUP_CONCAT()` aggregate-local `ORDER BY` rules: the
order key remains a supported descriptor column, not a general expression.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline `GROUP_CONCAT()` aggregate:
  `docs/specs/baseline-group-concat-aggregate/specs.md`
- Baseline universal row-scalar expression contexts:
  `docs/specs/baseline-universal-row-scalar-expression-contexts/specs.md`
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
records MySQL 8.4.9 behavior for the supported row-scalar value-expression
subset. Observed behavior:

- `GROUP_CONCAT(IFNULL(name, '') ORDER BY id SEPARATOR ':')` concatenates the
  per-row expression value and preserves empty strings.
- `GROUP_CONCAT(CONCAT(name, notes) ORDER BY id SEPARATOR '|')` skips rows
  whose per-row expression result is `NULL`.
- Single-value `DISTINCT` works for supported row-scalar value expressions.
- Grouped `GROUP_CONCAT()` works with supported row-scalar value expressions.
- Selected grouped aliases and matching repeated selected expressions can order
  the grouped result by the selected aggregate text.

## Supported Surface

Supported shapes:

```sql
SELECT GROUP_CONCAT(value_expression
                    [ORDER BY descriptor_order_column [ASC|DESC]]
                    [SEPARATOR 'literal'])
FROM source
[WHERE predicate]

SELECT GROUP_CONCAT(DISTINCT value_expression
                    [ORDER BY descriptor_order_column [ASC|DESC]]
                    [SEPARATOR 'literal'])
FROM source

SELECT group_projection,
       GROUP_CONCAT(value_expression
                    [ORDER BY descriptor_order_column [ASC|DESC]]
                    [SEPARATOR 'literal']) [AS alias]
FROM source
[WHERE predicate]
GROUP BY group_key [, ...]
[HAVING supported_group_having]
[ORDER BY alias_or_matching_selected_expression]
[LIMIT supported_limit]
```

`value_expression` is limited to descriptor columns and row-scalar expressions
already supported by the `GROUP_CONCAT()` aggregate planner. Multiple
non-`DISTINCT` value expressions keep using per-row `CONCAT()` semantics before
aggregation.

MyLite Lemon-syntax grammar snippets are unchanged for this slice; the existing
`GROUP_CONCAT()` grammar already admits supported row-scalar value expressions.

## Runtime Semantics

MyLite lowers supported row-scalar value expressions through the same private
row-scalar SQL builder used by other descriptor-backed expression contexts.
Each source row produces one aggregate value. `NULL` value-expression results
are skipped by the aggregate, while non-`NULL` empty strings are preserved.

For multiple non-`DISTINCT` value expressions, MyLite concatenates per-row
values with the existing row-scalar `CONCAT()` semantics before feeding the
result to `_mylite_group_concat(...)`. For single-value `DISTINCT`, the
distinct key is the supported row-scalar value expression result.

No SQLite fork hook is required.

## Non-Goals

This slice does not add:

- arbitrary row-scalar expression forms outside the supported
  `GROUP_CONCAT()` value-expression subset;
- aggregate-local expression, ordinal, alias, nullable, string, or multiple
  `ORDER BY` keys;
- multi-value `DISTINCT` tuple semantics;
- `GROUP_CONCAT()` predicates in grouped `HAVING`;
- hidden grouped `ORDER BY GROUP_CONCAT(...)` keys;
- binary result metadata, full grouping, joins, or aggregate windows.

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
