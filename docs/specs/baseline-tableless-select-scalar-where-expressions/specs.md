# Baseline Tableless SELECT Scalar WHERE Expressions

## Summary

This slice widens source-free and `FROM DUAL` `WHERE` predicates from the
earlier scalar-literal subset to the current source-free row-scalar expression
envelope. It applies to top-level tableless `SELECT` and to the shared
row-scalar `FROM DUAL` source used by `INSERT ... SELECT` and `REPLACE ...
SELECT`.

Supported user-visible shapes:

```sql
SELECT select_item[, select_item ...]
[WHERE source_free_predicate]

SELECT select_item[, select_item ...]
FROM DUAL
[WHERE source_free_predicate]
```

`source_free_predicate` includes scalar-literal truth, comparison,
`IS [NOT] NULL`, `[NOT] BETWEEN`, and `[NOT] IN` predicates; current
source-free row-scalar function truth/comparison/`IS`/range/membership
predicates; keyword and symbolic logical `NOT`, `AND`/`&&`, `OR`/`||`, and
`XOR`; and the existing `[NOT] EXISTS` filter for `FROM DUAL`.

## Compatibility Evidence

Primary references:

- MySQL 8.4 Reference Manual, "SELECT Statement":
  <https://dev.mysql.com/doc/refman/8.4/en/select.html>
- MySQL 8.4 Reference Manual, "Expressions":
  <https://dev.mysql.com/doc/refman/8.4/en/expressions.html>
- MySQL 8.4 Reference Manual, "Type Conversion in Expression Evaluation":
  <https://dev.mysql.com/doc/refman/8.4/en/type-conversion.html>
- Observed MySQL runtime: Docker container `mylite-mysql-849`, `SELECT
  VERSION()` = `8.4.9`.

Runtime probes for this slice verified:

- `SELECT 1 WHERE IF(1, TRUE, FALSE)` returns one row.
- `SELECT 1 WHERE IF(0, TRUE, FALSE)` returns zero rows.
- `SELECT 1 WHERE COALESCE(NULL, 1)` returns one row.
- `SELECT 1 WHERE IFNULL(NULL, 3) = 3` returns one row.
- `SELECT 1 WHERE NULLIF(1, 1) IS NULL` returns one row.
- `SELECT 1 WHERE ISNULL(NULL)` returns one row.
- `SELECT 1 WHERE CASE WHEN 1 THEN TRUE ELSE FALSE END` returns one row.
- `SELECT 1 WHERE 2 BETWEEN 1 AND 3` returns one row.
- `SELECT 1 WHERE COALESCE(NULL, 2) IN (1, 2)` returns one row.
- `SELECT 1 FROM DUAL WHERE GREATEST(1, 2) IN (1, 2)` returns one row.
- `SELECT 1 WHERE missing` returns `1054 / 42S22` with `where clause`
  context.

## Ownership Boundaries

- Public API: no ABI change.
- Parser/AST: scalar-literal `BETWEEN` and `IN` predicate atoms are admitted so
  tableless `WHERE 2 BETWEEN 1 AND 3` and `WHERE 1 IN (1, 2)` use the normal
  predicate planner instead of parse failure.
- Runtime: tableless row-scalar planning reuses the existing predicate planner
  and SQL builder, then validates that planned expressions contain no source
  columns, aggregates, or window expressions.
- Catalog/storage/SQLite: no catalog mutation, file-format change, SQLite
  registration change, or SQLite fork hook.

## Grammar

MyLite Lemon-syntax snippets:

```lemon
predicate_atom(A) ::= predicate_scalar_literal(C) BETWEEN(B)
    predicate_range_value(L) AND predicate_range_value(U).

predicate_atom(A) ::= predicate_scalar_literal(C) NOT(N) BETWEEN(B)
    predicate_range_value(L) AND predicate_range_value(U).

predicate_atom(A) ::= predicate_scalar_literal(C) IN(I)
    LPAREN predicate_in_value_list(V) RPAREN(R).

predicate_atom(A) ::= predicate_scalar_literal(C) NOT(N) IN(I)
    LPAREN predicate_in_value_list(V) RPAREN(R).
```

The tableless runtime support gate accepts planned predicate nodes only when all
row-scalar expression operands are source-free:

```text
source_free_expression =
    planned row-scalar expression except COLUMN, COUNT_STAR, SUM_COLUMN,
    WINDOW_FUNCTION, or any expression tree containing those kinds
```

## Semantics

- The predicate is evaluated against the single candidate tableless row. True
  returns that row; false or `NULL` returns zero rows.
- Scalar-literal `BETWEEN` and `IN` use the same row-scalar predicate SQL
  builder as function-left predicates.
- Symbolic logical `&&` and `||` are admitted in the same contexts as keyword
  `AND` and `OR`. Existing MyLite warning handling remains responsible for
  deprecation warnings.
- Unknown identifiers still resolve through the predicate planner and produce
  MySQL-shaped `where clause` diagnostics.
- Source-dependent planned expressions, aggregates, and windows are rejected
  before SQL generation in tableless filters.

## Compatibility Limits

- No broad system/session scalar predicate classification beyond functions
  admitted by later focused slices. `DATABASE()` and `SCHEMA()` predicate
  expressions are covered by
  `docs/specs/baseline-current-database-row-scalar-contexts/specs.md`.
- No arithmetic-left or arbitrary expression-left predicate atoms such as
  `WHERE 1 + 2 BETWEEN 3 AND 4`; those remain part of the broader expression
  predicate work.
- No arbitrary subquery predicates outside the existing `[NOT] EXISTS` filter
  and previously admitted scalar-subquery `IS NULL` tableless form.
- No tableless `GROUP BY`, `HAVING`, aggregate evaluation, or multiple-row
  source-free row generation.
- No SQLite fork hook is needed.

## Tests

MySQL-runtime expectation coverage:

- `packages/libmylite/tests/mysql_baseline_select_row_scalar_predicates_expectations.sh`
- `packages/libmylite/tests/mysql_baseline_tableless_select_expression_clauses_expectations.sh`
- `packages/libmylite/tests/mysql_baseline_insert_select_dual_scalar_where_expectations.sh`

Runtime C coverage:

- `runtime_select_row_scalar_predicates_test.c` covers source-free
  function/scalar-literal truth, comparison, range, membership, and logical
  filters.
- `runtime_select_literal_projection_test.c` covers source-free literal
  `WHERE` with `ORDER BY` / `LIMIT`.
- `runtime_insert_select_lifecycle_test.c` covers the shared `FROM DUAL`
  insert source path for scalar-literal `BETWEEN`, `&&`, and `||`.
