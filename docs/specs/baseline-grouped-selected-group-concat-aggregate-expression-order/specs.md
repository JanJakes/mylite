# Baseline Grouped Selected GROUP_CONCAT Aggregate Expression Order

## Status

This slice admits grouped `ORDER BY` keys that repeat a selected
`GROUP_CONCAT()` aggregate expression in the existing grouped
`GROUP_CONCAT()` envelope:

```sql
SELECT group_column,
       GROUP_CONCAT(value ORDER BY order_column SEPARATOR ':') AS names
FROM source
GROUP BY group_column
ORDER BY GROUP_CONCAT(value ORDER BY order_column SEPARATOR ':') [ASC|DESC]
```

The repeated expression must match exactly one selected `GROUP_CONCAT()`
result after MyLite descriptor planning. This is separate from hidden aggregate
order keys and does not broaden the underlying `GROUP_CONCAT()` argument or
source support.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline `GROUP_CONCAT()` aggregate:
  `docs/specs/baseline-group-concat-aggregate/specs.md`
- Baseline grouped `GROUP_CONCAT()` alias ordering:
  `docs/specs/baseline-grouped-group-concat-aggregate-alias-order/specs.md`
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
records MySQL 8.4.9 behavior for repeated selected `GROUP_CONCAT()` expression
order keys. Observed behavior:

- `ORDER BY GROUP_CONCAT(...) ASC` sorts by the selected aggregate text result,
  with `NULL` aggregate results first.
- `DESC` reverses non-`NULL` text order and places `NULL` aggregate results
  last.
- Repeated selected expressions work with the supported aggregate-local
  descriptor `ORDER BY`, explicit literal separators, row-scalar value
  expressions, and single-value `DISTINCT`.
- The selected result value and warning state are the same as alias ordering in
  the probed short-output subset.

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
[HAVING supported_group_having]
ORDER BY GROUP_CONCAT([DISTINCT] value_expression
                      [ORDER BY descriptor_order_column [ASC|DESC]]
                      [SEPARATOR 'literal']) [ASC|DESC]
         [, supported_grouped_order_key ...]
[LIMIT supported_limit]
```

The selected and repeated `GROUP_CONCAT()` calls must match after descriptor
resolution:

- same aggregate kind and `DISTINCT` modifier;
- same descriptor or supported row-scalar value expression;
- same aggregate-local descriptor order key and direction;
- same explicit separator literal, or both using the default separator.

MyLite Lemon-syntax grammar snippets for this slice:

```lemon
selected_grouped_aggregate_expression ::=
    GROUP_CONCAT LPAREN expression group_concat_order_opt
    group_concat_separator_opt RPAREN.

selected_grouped_aggregate_expression ::=
    GROUP_CONCAT LPAREN expression COMMA function_argument_list
    group_concat_order_opt group_concat_separator_opt RPAREN.

selected_grouped_aggregate_expression ::=
    GROUP_CONCAT LPAREN DISTINCT expression group_concat_order_opt
    group_concat_separator_opt RPAREN.

select_order_key ::= selected_grouped_aggregate_expression.
```

## Runtime Semantics

MyLite plans the repeated `ORDER BY GROUP_CONCAT(...)` with the same grouped
aggregate planner used for selected aggregate items. If it matches exactly one
selected `GROUP_CONCAT()` result, generated SQLite SQL orders by the selected
aggregate output-column ordinal with MyLite's MySQL-compatible text collation.
This preserves selected-result `NULL` placement, text comparison, and warning
behavior without recomputing the aggregate expression.

No SQLite fork hook is required.

## Non-Goals

This slice does not add:

- hidden aggregate order keys, such as
  `SELECT g FROM t GROUP BY g ORDER BY GROUP_CONCAT(v)`;
- repeated expressions that differ by value expression, local order key,
  separator, or `DISTINCT`;
- multiple grouped `GROUP_CONCAT()` results;
- broader `GROUP_CONCAT()` value, distinct, separator, aggregate-local order,
  grouping, source, predicate, or window forms.

Unsupported forms continue to return deterministic MyLite diagnostics.
