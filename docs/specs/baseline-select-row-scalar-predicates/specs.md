# Baseline SELECT Row-Scalar Predicates

This phase admits common MySQL predicate shapes where a comparison, `IS`, or
range operand is a supported row-scalar function expression rather than only a
bare descriptor column or literal. The target is table-backed
`SELECT ... WHERE` filters such as:

```sql
SELECT id FROM t WHERE HEX(binary_col) = '4142'
SELECT id FROM t WHERE LOWER(name) = 'alpha'
SELECT id FROM t WHERE COALESCE(name, 'fallback') IS NOT NULL
SELECT id FROM t WHERE GREATEST(score, 5) BETWEEN 5 AND 10
SELECT id FROM t WHERE id = IF(1, 1, 0)
```

The slice keeps MyLite's current predicate model. It does not introduce a
general expression VM, row constructors, row-scalar `IN`, aggregate/window
predicates, stored functions, spatial functions, or broad MySQL type coercion.

## Compatibility Evidence

Primary references:

- MySQL 8.4 Reference Manual, "Expressions":
  <https://dev.mysql.com/doc/refman/8.4/en/expressions.html>
- MySQL 8.4 Reference Manual, "Comparison Functions and Operators":
  <https://dev.mysql.com/doc/refman/8.4/en/comparison-operators.html>
- MySQL 8.4 Reference Manual, "Flow Control Functions":
  <https://dev.mysql.com/doc/refman/8.4/en/flow-control-functions.html>
- MySQL 8.4 Reference Manual, "String Functions and Operators":
  <https://dev.mysql.com/doc/refman/8.4/en/string-functions.html>
- MySQL 8.4 Reference Manual, "Functions and Operators":
  <https://dev.mysql.com/doc/refman/8.4/en/functions.html>
- Observed MySQL runtime: Docker container `mylite-mysql-849`, `SELECT
  VERSION()` = `8.4.9`.

MySQL predicates are expressions. A function call can be compared with another
expression, tested with `IS [NOT] NULL`, or used as the `BETWEEN` subject.
Runtime probes verify expected row sets for the supported subset in this slice.

## Ownership Boundaries

- Public API: no ABI change.
- Parser/AST: add a predicate-only row-scalar expression nonterminal that
  reuses existing AST constructors for functions already supported by the
  row-scalar planner.
- Runtime: route admitted row-scalar predicate ASTs to existing
  `PLANNED_SELECT_PREDICATE_ROW_SCALAR_*` predicate nodes and SQL builders.
- Catalog/storage/SQLite: no descriptor, storage format, SQLite SQL function,
  VFS, or SQLite fork changes.

## Grammar

MyLite Lemon-syntax snippets:

```lemon
predicate_atom(A) ::= predicate_row_scalar_expression(B)
        predicate_comparison_operator(O) predicate_row_scalar_comparison_value(V).
predicate_atom(A) ::= predicate_row_scalar_expression(B) IS NULL.
predicate_atom(A) ::= predicate_row_scalar_expression(B) IS NOT NULL.
predicate_atom(A) ::= predicate_row_scalar_expression(B)
        BETWEEN predicate_row_scalar_range_value(L) AND predicate_row_scalar_range_value(U).
predicate_atom(A) ::= predicate_row_scalar_expression(B)
        NOT BETWEEN predicate_row_scalar_range_value(L) AND predicate_row_scalar_range_value(U).

predicate_row_scalar_expression(A) ::= supported_row_scalar_function(B).

predicate_row_scalar_comparison_value(A) ::= predicate_comparison_value(B).
predicate_row_scalar_comparison_value(A) ::= supported_row_scalar_function(B).

predicate_row_scalar_range_value(A) ::= predicate_range_value(B).
predicate_row_scalar_range_value(A) ::= supported_row_scalar_function(B).

supported_row_scalar_function(A) ::= IF(expr, expr, expr).
supported_row_scalar_function(A) ::= IFNULL(expr, expr).
supported_row_scalar_function(A) ::= NULLIF(expr, expr).
supported_row_scalar_function(A) ::= COALESCE(expr_list).
supported_row_scalar_function(A) ::= CONCAT(expr_list).
supported_row_scalar_function(A) ::= CONCAT_WS(expr_list).
supported_row_scalar_function(A) ::= GREATEST(expr_list).
supported_row_scalar_function(A) ::= LEAST(expr_list).
supported_row_scalar_function(A) ::= HEX(expr).
supported_row_scalar_function(A) ::= UNHEX(expr).
supported_row_scalar_function(A) ::= LOWER(expr) | LCASE(expr) | UPPER(expr) | UCASE(expr).
supported_row_scalar_function(A) ::= LTRIM(expr) | RTRIM(expr) | TRIM(...).
supported_row_scalar_function(A) ::= REVERSE(expr) | SOUNDEX(expr) | QUOTE(expr).
supported_row_scalar_function(A) ::= JSON_EXTRACT(expr, expr).
supported_row_scalar_function(A) ::= JSON_UNQUOTE(expr).
supported_row_scalar_function(A) ::= JSON_LENGTH(expr[, expr]).
supported_row_scalar_function(A) ::= JSON_TYPE(expr).
supported_row_scalar_function(A) ::= JSON_QUOTE(expr).
supported_row_scalar_function(A) ::= TIMESTAMP(expr[, expr]).
supported_row_scalar_function(A) ::= UNIX_TIMESTAMP(expr).
supported_row_scalar_function(A) ::= SEC_TO_TIME(expr) | MAKEDATE(expr, expr).
supported_row_scalar_function(A) ::= MAKETIME(expr, expr, expr).
supported_row_scalar_function(A) ::= DATEDIFF(expr, expr).
supported_row_scalar_function(A) ::= TIMEDIFF(expr, expr).
supported_row_scalar_function(A) ::= TIMESTAMPDIFF(unit, expr, expr).
```

The implemented parser may keep existing special predicate productions for
`FIND_IN_SET()`, `REGEXP_LIKE()`, `JSON_VALID()`, JSON containment, string
length, substring, temporal extractors, conversions, and collations. This slice
adds the broader row-scalar comparison, `IS NULL`, and `BETWEEN` dispatch
without changing those specialized paths. Bare row-scalar truth predicates
remain outside this slice.

## Semantics

- Comparisons support the current MyLite row-scalar comparison operators:
  `=`, `<=>`, `<>`, `!=`, `<`, `<=`, `>`, and `>=`, plus the existing
  row-scalar `LIKE` / `REGEXP` paths where already supported.
- The comparison right operand may be a supported predicate literal, descriptor
  column where the existing planner supports it, `COLLATE` expression, or a
  supported row-scalar expression. The executable single-table subset includes
  descriptor-column RHS values and direct supported row-scalar function RHS
  values for row-scalar comparison subjects, plus direct supported row-scalar
  function RHS values for descriptor-column comparison subjects.
- `IS NULL` and `IS NOT NULL` use the row-scalar predicate node so functions
  can participate without resolving as columns.
- `BETWEEN` and `NOT BETWEEN` reuse existing literal/range conversion for
  literal bounds and plan supported row-scalar function bounds through the same
  row-scalar SQL builder used for comparison RHS expressions.
- Supported expressions are evaluated by SQLite through MyLite's row-scalar SQL
  expression builder; MyLite only plans and validates the expression envelope.

## Compatibility Limits

- No row-scalar `IN (...)` or `NOT IN (...)` in this slice.
- No bare row-scalar truth predicates such as `WHERE IF(...)` in this slice.
- No arbitrary `predicate_atom ::= expression` grammar widening.
- No row constructors, `MATCH ... AGAINST`, full-text predicates, spatial
  constructors, stored functions, loadable functions, or user-defined
  functions.
- No aggregate or window-function predicates in `WHERE`.
- No additional MySQL type coercion beyond the existing row-scalar and
  predicate conversion helpers.
- No SQLite fork hook is needed; this is a MyLite parser/runtime wrapper over
  existing row-scalar planning and SQLite expression execution.

## Tests

Add MySQL-runtime expectations and focused runtime tests for:

- binary/string expression comparisons, including supported row-scalar RHS
  values;
- descriptor-column and direct row-scalar function RHS values for row-scalar
  comparison subjects;
- direct row-scalar function RHS values for descriptor-column comparison
  subjects;
- control-flow and comparison functions in `WHERE`;
- `IS NULL` / `IS NOT NULL` over row-scalar functions;
- `BETWEEN` / `NOT BETWEEN` over row-scalar functions, including direct
  row-scalar function bounds;
- temporal row-scalar functions in predicates;
- JSON row-scalar functions in predicates;
- unsupported `IN` and aggregate/window predicate behavior stays outside this
  slice.

Verification before marking done:

1. `packages/libmylite/tests/mysql_baseline_select_row_scalar_predicates_expectations.sh`
2. Focused CTest entry for the runtime test.
3. Parse-corpus benchmark comparison.
4. `git diff --check`
5. `cmake --workflow --preset check`
