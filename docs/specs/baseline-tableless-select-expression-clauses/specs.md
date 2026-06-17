# Baseline Tableless SELECT Expression Clauses

## Summary

This slice admits the current source-free and `FROM DUAL` row-scalar `SELECT`
expression envelope with `WHERE`, `ORDER BY`, and `LIMIT` clauses.

Supported user-visible shapes:

```sql
SELECT select_item[, select_item ...]
[WHERE scalar_predicate]
[ORDER BY {position | selected_alias | scalar_expression} [ASC | DESC][, ...]]
[LIMIT limit_clause]

SELECT select_item[, select_item ...]
FROM DUAL
[WHERE scalar_predicate]
[ORDER BY {position | selected_alias | scalar_expression} [ASC | DESC][, ...]]
[LIMIT limit_clause]
```

The selected items and scalar predicates are still limited to MyLite's current
source-free row-scalar planner. `ORDER BY` over a source-free one-row query is
validated for MySQL-shaped name/ordinal/expression behavior but is executed as a
no-op because there is no row set to sort beyond the single computed row.

## Compatibility Evidence

Primary references:

- MySQL 8.4 Reference Manual, "SELECT Statement":
  <https://dev.mysql.com/doc/refman/8.4/en/select.html>
- MySQL 8.4 Reference Manual, "Problems with Column Aliases":
  <https://dev.mysql.com/doc/refman/8.4/en/problems-with-alias.html>
- MySQL 8.4 Reference Manual, "Expressions":
  <https://dev.mysql.com/doc/refman/8.4/en/expressions.html>
- Observed MySQL runtime: Docker container `mylite-mysql-849`, `SELECT
  VERSION()` = `8.4.9`.

MySQL `SELECT` syntax admits `WHERE`, `ORDER BY`, and `LIMIT` around query
blocks, including query blocks that compute rows without a table. MySQL also
accepts `DUAL` as an optional dummy source for source-free expressions. MySQL
allows selected aliases in `ORDER BY`.

Runtime probes for this slice verified:

- `SELECT 1 ORDER BY 1` returns one row.
- `SELECT 1 AS x ORDER BY x DESC` returns one row.
- `SELECT IF(1,2,3) ORDER BY IF(1,2,3)` returns `2`.
- `SELECT CASE WHEN 1 THEN 2 ELSE 3 END ORDER BY 1` returns `2`.
- `SELECT 1 WHERE TRUE ORDER BY 1` returns one row.
- `SELECT 1 WHERE FALSE ORDER BY 1` returns zero rows.
- `SELECT 1 FROM DUAL ORDER BY 1 LIMIT 1` returns one row.
- `SELECT 1 ORDER BY 2` returns `1054 / 42S22`.

## Ownership Boundaries

- Public API: no ABI change.
- Parser/AST: no-source and `FROM DUAL` `SELECT` productions admit existing
  `WHERE`, `ORDER BY`, and `LIMIT` clause nodes.
- Runtime: tableless row-scalar planning validates `ORDER BY` ordinals,
  selected aliases, and supported scalar expression keys. The generated SQLite
  SQL omits physical `ORDER BY` for no-source plans.
- Catalog/storage/SQLite: no catalog mutation, file-format change, SQLite
  registration change, or SQLite fork hook.

## Grammar

MyLite Lemon-syntax snippets:

```lemon
select_statement(A) ::= SELECT select_modifiers select_item_list
    where_clause_opt window_clause_opt select_order_clause_opt limit_clause_opt
    select_locking_clause_opt.

select_statement(A) ::= SELECT select_modifiers select_item_list FROM DUAL
    where_clause_opt window_clause_opt select_order_clause_opt limit_clause_opt
    select_locking_clause_opt select_into_opt.
```

The executable tableless order-key subset is:

```lemon
tableless_order_key(A) ::= integer_position.
tableless_order_key(A) ::= selected_alias.
tableless_order_key(A) ::= supported_source_free_row_scalar_expression.
```

## Semantics

- `WHERE` is evaluated against the single source-free candidate row. A true
  predicate returns that row; false or `NULL` returns zero rows.
- `ORDER BY` is validated but does not add a physical SQLite sort for
  source-free queries.
- Integer positions are 1-based and must refer to selected items. Invalid
  positions produce MySQL's unknown-column order-clause diagnostic shape.
- Selected aliases are matched ASCII case-insensitively. For duplicate aliases,
  MyLite accepts the first matching selected alias in this source-free context.
- Non-alias expression keys must be accepted by the source-free row-scalar
  expression planner. Unsupported expressions keep the existing targeted
  unsupported diagnostics.
- Because tableless `ORDER BY` is validation-only in this slice, expression
  keys are not evaluated for additional warnings or side effects.
- `LIMIT` keeps the existing MySQL-compatible tableless limit behavior.

## Compatibility Limits

- No tableless `GROUP BY`, `HAVING`, `WINDOW` execution, aggregate evaluation,
  or multiple-row source-free row generation.
- No arbitrary scalar expression engine beyond the current source-free
  row-scalar planner.
- No user-variable references in `ORDER BY` until the source-free order-key
  planner admits them explicitly.
- No physical optimizer behavior; tableless `ORDER BY` remains a semantic
  validation step.
- No SQLite fork hook is needed.

## Tests

MySQL-runtime expectation script:

- `packages/libmylite/tests/mysql_baseline_tableless_select_expression_clauses_expectations.sh`

Runtime C coverage:

- `runtime_select_literal_projection_test.c` covers source-free literal
  `ORDER BY`, alias ordering, `WHERE FALSE`, `FROM DUAL ORDER BY ... LIMIT`,
  and invalid order keys.
- `runtime_scalar_expression_projection_test.c` covers source-free scalar
  function ordering by expression keys.
- Existing control-flow, comparison, arithmetic, and session scalar tests cover
  stale `WHERE TRUE` and `ORDER BY 1` regressions.
