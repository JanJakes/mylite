# Baseline Row-Scalar ORDER BY Expressions

## Summary

This phase widens MyLite's non-grouped table-backed `SELECT ORDER BY` support
from descriptor, ordinal, `FIELD()`, conversion, arithmetic, collation, and
searched-`CASE` keys to the row-scalar expression family already supported by
single-table projection and predicate planning.

Supported user-visible shapes:

```sql
SELECT select_item[, select_item ...]
FROM table_name [AS alias]
[WHERE predicate]
ORDER BY row_scalar_order_key [ASC | DESC][, row_scalar_order_key ...]
[LIMIT row_count]
```

The row-scalar order key must fit MyLite's current row-scalar expression
planner. This includes documented descriptor-backed control-flow functions,
string helpers such as `CONCAT()` and `LOWER()`, JSON extraction/unquoting,
temporal helpers such as `DATEDIFF()`, covered numeric functions, supported
conversions, existing integer arithmetic, postfix `COLLATE`, `LIKE` truth keys,
and the existing searched `CASE` rank-key subset.

This is not a general MySQL expression engine. It does not add grouped
expression order keys, DML expression order keys, tableless row-scalar function
ordering, aggregate/window ordering, scalar subquery order keys, full type
coercion, or expression metadata.

## Compatibility Evidence

Primary references:

- MySQL 8.4 Reference Manual, "SELECT Statement":
  <https://dev.mysql.com/doc/refman/8.4/en/select.html>
- MySQL 8.4 Reference Manual, "Expressions":
  <https://dev.mysql.com/doc/refman/8.4/en/expressions.html>
- MySQL 8.4 Reference Manual, "Built-In Function and Operator Reference":
  <https://dev.mysql.com/doc/refman/8.4/en/built-in-function-reference.html>
- Observed MySQL runtime: Docker container `mylite-mysql-849`, `SELECT
  VERSION()` = `8.4.9`.

MySQL `SELECT` syntax admits `ORDER BY {col_name | expr | position}` entries
with optional direction. Selected expression aliases can also be referenced by
`ORDER BY`. Runtime probes in this slice verify ordering by `CONCAT()`,
`LOWER(CONCAT(...))`, `IFNULL()`, searched `CASE`, `COALESCE()`,
`JSON_UNQUOTE(JSON_EXTRACT(...))`, and `DATEDIFF()` over descriptor-backed rows.

## Ownership Boundaries

- Public API: no ABI change.
- Parser/AST: widen `select_order_key` to reuse the existing
  `predicate_row_scalar_expression` grammar surface for supported row-scalar
  functions. The parser still keeps specialized order-key forms for ordinals,
  descriptors, `FIELD()`, `LIKE`, `CASE`, conversions, collations, and window
  placeholders.
- Runtime: route admitted order keys through existing row-scalar order planning
  and SQL rendering. SQLite executes the generated expressions over physical
  columns, while MyLite validates the expression envelope and binds literals.
- Catalog/storage/SQLite: no catalog mutation, storage-format change, SQLite
  function registration change, or SQLite fork hook.

## Grammar

MyLite Lemon-syntax snippets:

```lemon
select_order_key(A) ::= predicate_row_scalar_expression(K).
select_order_key(A) ::= CASE searched_case_when_list(W) case_else_opt(E) END.
select_order_key(A) ::= predicate_collate_expression(K).
select_order_key(A) ::= FIELD(T) LPAREN RPAREN(R).
```

`FIELD()` with arguments remains admitted through its existing dedicated order
path. The zero-argument `FIELD()` rule keeps the MySQL-shaped argument-count
diagnostic.

Runtime acceptance is narrower than parser admission:

```lemon
row_scalar_order_key(A) ::= supported_row_scalar_function(B).
row_scalar_order_key(A) ::= row_scalar_concat_operator(B).
row_scalar_order_key(A) ::= supported_integer_arithmetic(B).
row_scalar_order_key(A) ::= supported_conversion(B).
row_scalar_order_key(A) ::= supported_collate_expression(B).
row_scalar_order_key(A) ::= supported_like_truth_key(B).
row_scalar_order_key(A) ::= supported_searched_case_rank_key(B).
```

## Semantics

- Each order expression is evaluated once per candidate source row by SQLite
  using MyLite-generated SQL for the admitted row-scalar expression.
- Multiple supported row-scalar order keys may be combined, except for the
  existing `FIELD()` single-hidden-key limitation.
- `ASC` is the default. `DESC` reverses the key order.
- `NULL` values sort before non-`NULL` values ascending and after non-`NULL`
  values descending in the verified subset.
- String keys use the collation selected by the generated expression. Existing
  explicit `COLLATE` support remains limited to MyLite's documented ASCII
  collation subset.
- Joined-source row-scalar ordering remains narrower than single-table
  ordering and is still limited to the existing joined conversion and integer
  arithmetic subset.

## Compatibility Limits

- No grouped aggregate row-scalar expression order keys beyond the existing
  documented grouped descriptor, aggregate-alias, relaxed cast, and selected
  extractor subsets.
- No `TABLE`, `VALUES`, `DELETE`, or `UPDATE` row-scalar expression order keys.
- No tableless/no-source multi-row sorting; source-free scalar `ORDER BY`
  validation is covered by
  [baseline tableless SELECT expression clauses](../baseline-tableless-select-expression-clauses/specs.md).
- No aggregate, window, stored-function, loadable-function, user-variable,
  scalar-subquery, JSON-arrow, tuple, full-text, spatial, or arbitrary
  expression order keys.
- No broader MySQL type aggregation, comparison coercion, Unicode collation
  parity, protocol-grade expression metadata, or tie-order guarantees.
- No SQLite fork hook is needed for this slice; public SQLite expression
  execution plus MyLite validation/rendering is sufficient.

## Tests

MySQL-runtime expectation scripts cover:

- `ORDER BY CONCAT(v, n), id`
- `ORDER BY LOWER(CONCAT(v, n)) DESC, id`
- `ORDER BY IFNULL(i, -1), id`
- `ORDER BY CASE WHEN i > 0 THEN 9 ELSE i END DESC, id`
- multiple row-scalar expression keys in one `ORDER BY` list
- `ORDER BY COALESCE(i, -1), id`
- `ORDER BY JSON_UNQUOTE(JSON_EXTRACT(js, '$.a')) DESC, id`
- `ORDER BY DATEDIFF(dt, '2024-01-01') DESC, id`
- `ORDER BY HEX(binary_column), id`

Runtime C tests mirror those expectations and include the regression that
`IFNULL()` is now accepted as a source-backed `SELECT ORDER BY` key while
grouping and unsupported contexts stay rejected.

Verification before marking done:

1. `sh -n packages/libmylite/tests/mysql_baseline_row_scalar_expressions_expectations.sh`
2. `sh -n packages/libmylite/tests/mysql_baseline_select_row_scalar_predicates_expectations.sh`
3. `packages/libmylite/tests/mysql_baseline_row_scalar_expressions_expectations.sh`
4. `packages/libmylite/tests/mysql_baseline_select_row_scalar_predicates_expectations.sh`
5. `ctest --preset dev -R '^libmylite\.runtime\.(row_scalar_expressions|select_row_scalar_predicates|ifnull_function)$' --output-on-failure`
6. `ctest --preset dev -R '^libmylite\.parser\.' --output-on-failure`
7. `git diff --check`
8. `cmake --workflow --preset check`
