# Window Functions and WINDOW Clauses

## Scope

This slice adds parser and AST recognition for MySQL window-function syntax, but
does not implement analytic execution yet. Preparing executable statements that
contain window functions or a `WINDOW` clause returns a deterministic
unsupported diagnostic.

In scope:

- `OVER ()`, `OVER window_name`, and `OVER (window_specification)`
- `WINDOW name AS (window_specification)` definitions after `HAVING` and before
  query-level `ORDER BY`
- `PARTITION BY`, window-local `ORDER BY`, and `ROWS`/`RANGE` frame clauses
- frame bounds using `CURRENT ROW`, `UNBOUNDED PRECEDING`,
  `UNBOUNDED FOLLOWING`, `expr PRECEDING`, and `expr FOLLOWING`
- ranking, distribution, offset, and value window function names
- aggregate function calls followed by `OVER`
- `RESPECT NULLS` and `IGNORE NULLS` syntax on value/offset window functions

Out of scope:

- partition materialization, peer-group handling, frame evaluation, and result
  calculation
- named-window inheritance validation
- execution-time MySQL errors for unsupported `IGNORE NULLS`
- optimizer behavior, spill behavior, and metadata for window outputs
- window functions inside DML expressions beyond parser recognition

## Sources

- MySQL 8.4 Reference Manual, Window Function Descriptions:
  https://dev.mysql.com/doc/refman/8.4/en/window-function-descriptions.html
- MySQL 8.4 Reference Manual, Window Function Concepts and Syntax:
  https://dev.mysql.com/doc/refman/8.4/en/window-functions-usage.html

Observed behavior was checked against MySQL 8.4.9 in Docker container
`mylite-mysql-849`, using:

```text
docker exec -i mylite-mysql-849 mysql -uroot --password= --protocol=TCP --table --show-warnings
```

This specification is independently authored from official documentation and
observed runtime behavior. It does not copy MySQL grammar, documentation prose,
or implementation sources.

## MySQL 8.4.9 Behavior Summary

Window functions are evaluated over a row's window after the row source,
filtering, grouping, and aggregate phases have produced the input rows. The
`OVER` clause either references a named window or defines an inline window.
Window specifications can partition rows, order rows within each partition, and
restrict the current frame.

Example ranking behavior:

```sql
SELECT id, grp, n,
       ROW_NUMBER() OVER (PARTITION BY grp ORDER BY id) AS rn
FROM t
ORDER BY grp, id;
```

With rows `(1,1,10)`, `(2,1,20)`, `(3,2,5)`, `(4,2,NULL)`,
`(5,2,15)`, MySQL 8.4.9 returns row numbers `1,2` for group `1` and
`1,2,3` for group `2`.

Named windows are declared in the `WINDOW` clause:

```sql
SELECT id,
       SUM(n) OVER w AS running_sum
FROM t
WINDOW w AS (
    PARTITION BY grp
    ORDER BY id
    ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW
)
ORDER BY grp, id;
```

For the same rows, MySQL returns running sums `10`, `30`, `5`, `5`, and
`20`; `SUM()` skips `NULL` inputs inside the frame.

`RESPECT NULLS` is accepted for value/offset window functions:

```sql
SELECT LAG(n, 1, 0) RESPECT NULLS OVER (ORDER BY id) FROM t;
```

`IGNORE NULLS` is parsed by MySQL 8.4.9 but preparing the statement returns
error 1235, "This version of MySQL doesn't yet support 'IGNORE NULLS'".

## MyLite Behavior

### Parser and AST

The parser recognizes window syntax and records it in AST nodes:

- `MYLITE_SQL_AST_WINDOW_FUNCTION_CALL`
- `MYLITE_SQL_AST_OVER_CLAUSE`
- `MYLITE_SQL_AST_WINDOW_SPECIFICATION`
- `MYLITE_SQL_AST_WINDOW_CLAUSE`
- `MYLITE_SQL_AST_WINDOW_DEFINITION_LIST`
- `MYLITE_SQL_AST_WINDOW_DEFINITION`
- `MYLITE_SQL_AST_WINDOW_PARTITION_CLAUSE`
- `MYLITE_SQL_AST_WINDOW_FRAME_CLAUSE`
- `MYLITE_SQL_AST_WINDOW_FRAME_BOUND`
- `MYLITE_SQL_AST_WINDOW_NULL_TREATMENT`

Recognized non-aggregate window functions:

- `CUME_DIST()`
- `DENSE_RANK()`
- `FIRST_VALUE(expr)`
- `LAG(expr[, offset[, default]])`
- `LAST_VALUE(expr)`
- `LEAD(expr[, offset[, default]])`
- `NTH_VALUE(expr, n)`
- `NTILE(n)`
- `PERCENT_RANK()`
- `RANK()`
- `ROW_NUMBER()`

Aggregate function AST nodes are wrapped as window-function calls when followed
by `OVER`. Non-aggregate scalar functions followed by `OVER`, such as
`CONCAT('a') OVER ()`, are rejected during parser action validation.

### Grammar Sketch

The implemented MyLite Lemon shape is intentionally smaller than MySQL's full
grammar, but keeps the syntax surfaces needed for future execution:

```lemon
select_statement ::= SELECT ... opt_having_clause opt_window_clause
                    opt_order_by_clause opt_limit_clause.

opt_window_clause ::= .
opt_window_clause ::= WINDOW window_definition_list.

window_definition ::= identifier AS LPAREN window_specification RPAREN.

over_clause ::= OVER identifier.
over_clause ::= OVER LPAREN window_specification RPAREN.

window_specification ::= opt_identifier opt_partition_clause
                         opt_order_by_clause opt_window_frame_clause.

window_frame_clause ::= window_frame_unit window_frame_bound.
window_frame_clause ::= window_frame_unit BETWEEN window_frame_bound
                        AND window_frame_bound.

window_function_call ::= window_function_name LPAREN opt_arguments RPAREN
                         opt_window_null_treatment over_clause.
primary_expression ::= aggregate_call over_clause.
```

### Execution Placeholder

During `SELECT` preparation, MyLite scans the query AST for window-function
calls and `WINDOW` clauses. If either is present, preparation fails with
`MYLITE_UNSUPPORTED` and the diagnostic text:

```text
Unsupported window functions or WINDOW clause
```

This keeps applications from seeing a misleading partial result while still
allowing the grammar to advance toward MySQL coverage.

## Test Coverage

Parser tests cover:

- `ROW_NUMBER() OVER ()`
- aggregate window functions with named `WINDOW` clauses
- `LAG(... ) RESPECT NULLS OVER (...)`
- `NTH_VALUE(... ) IGNORE NULLS OVER (...)`
- invalid arity for `ROW_NUMBER()` and `LAG()`
- invalid null treatment on `ROW_NUMBER()`
- invalid scalar-function `OVER`

Runtime tests cover:

- unsupported diagnostics for direct window-function execution
- unsupported diagnostics for named `WINDOW` clauses
