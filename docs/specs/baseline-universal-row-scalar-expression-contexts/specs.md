# Baseline Universal Row-Scalar Expression Contexts

## Goal

MyLite has many scalar function implementations, but several SQL contexts still
admit them through narrow, feature-specific probes. This slice makes the
existing row-scalar expression planner the common admission path for supported
scalar functions and operators in ordinary single-row contexts:

- table-backed `SELECT` projection routing;
- `WHERE`, row-scalar comparison, truth, range, and membership predicates;
- non-grouped single-table `ORDER BY` keys;
- single-table `UPDATE` assignments and `INSERT ... ON DUPLICATE KEY UPDATE`
  assignments for non-key, non-`AUTO_INCREMENT` targets;
- non-`GROUP_CONCAT` aggregate arguments where the SQL builder can lower a
  row-scalar expression into the aggregate call;
- grouped non-`GROUP_CONCAT` aggregate arguments over the same expression
  subset.

The target is not to add new function semantics. It is to stop stranding
already-planned row-scalar functions in arbitrary subsets of contexts.

## Compatibility Authority

Normative behavior comes from official MySQL 8.4 documentation and MySQL 8.4.9
runtime observations:

- `https://dev.mysql.com/doc/refman/8.4/en/expressions.html`
- `https://dev.mysql.com/doc/refman/8.4/en/flow-control-functions.html`
- `https://dev.mysql.com/doc/refman/8.4/en/aggregate-functions.html`
- `https://dev.mysql.com/doc/refman/8.4/en/string-functions.html`
- `https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html`
- `https://dev.mysql.com/doc/refman/8.4/en/json-function-reference.html`

Representative MySQL 8.4.9 probe:

```sql
DROP DATABASE IF EXISTS mylite_expr_contexts_probe;
CREATE DATABASE mylite_expr_contexts_probe;
USE mylite_expr_contexts_probe;
CREATE TABLE t(id INT PRIMARY KEY, i INT, v VARCHAR(20), js JSON, dt DATETIME);
INSERT INTO t VALUES
(1,10,'Alpha',JSON_OBJECT('a', 1),'2024-01-02 03:04:05'),
(2,0,'beta',JSON_OBJECT('a', 2),'2024-01-03 04:05:06'),
(3,NULL,NULL,NULL,NULL);
SELECT id FROM t
 WHERE ABS(i) = GREATEST(i,0)
 ORDER BY JSON_UNQUOTE(JSON_EXTRACT(js,'$.a')) DESC, id;
UPDATE t SET v = CONCAT(LOWER(v), '-', COALESCE(i,0)) WHERE id = 1;
SELECT v FROM t WHERE id = 1;
SELECT SUM(ABS(i)), MIN(LOWER(v)), MAX(CONCAT(v, id)) FROM t;
SELECT COALESCE(i,0) AS g, SUM(ABS(i)), AVG(ABS(i))
  FROM t GROUP BY g ORDER BY g;
DROP DATABASE mylite_expr_contexts_probe;
```

Observed results:

```text
2
1
alpha-10
10 alpha-10 beta2
0 0 0.0000
10 10 10.0000
```

## Semantics

The row-scalar planner is the authority for whether an expression is executable
in this slice. Callers should ask a common syntactic classifier whether an AST
contains a row-scalar expression attempt, route to row-scalar planning, and let
the planner accept or reject the exact expression with the existing
MySQL-shaped diagnostics.

The classifier is a private runtime boundary, not an ABI surface. It separates
ordinary context attempts, predicate-value roots, DML assignment attempts, and
aggregate arguments so each caller can keep its context-specific exclusions
while sharing the same row-scalar family coverage.

The common classifier covers the already-planned families:

- control-flow expressions;
- string, binary-string, digest, compression, UUID, charset/collation, JSON,
  temporal, numeric, base-conversion, numeric-extra, `RAND()`, and conversion
  expressions;
- `CONCAT` operator expressions;
- row arithmetic expressions;
- `LIKE`, `SOUNDS LIKE`, logical conditions, `COLLATE`, and current statement
  time expressions admitted by the row-scalar planner.

Unsupported operands, target storage conversions, binary/collation edge cases,
expression metadata, broad subqueries, and unsupported function modes remain
owned by the underlying function specs. This slice only removes context routing
gaps where an expression is already otherwise supported.

## Aggregate Arguments

For `MIN()`, `MAX()`, `SUM()`, `AVG()`, `BIT_AND()`, `BIT_OR()`, and
`BIT_XOR()`, a supported row-scalar argument should lower to the SQLite
aggregate call over the generated row-scalar SQL. `AVG(expression)` must retain
the existing MyLite `SUM(...), COUNT(...)` result shape so result formatting
remains consistent with current integer AVG behavior.

`GROUP_CONCAT()` remains separate because its MySQL compatibility surface
includes ordering, separator handling, length limits, and string aggregation
state. Supported `GROUP_CONCAT()` row-scalar value arguments are tracked in
`docs/specs/baseline-group-concat-row-scalar-arguments/specs.md`.

## Syntax

Most row-scalar contexts reuse existing expression grammar. Aggregate arguments
use an aggregate-local row-scalar grammar instead of the full expression
grammar, so parser generation stays finite while still admitting the supported
literal, descriptor-column, arithmetic, and representative scalar-function
arguments covered by this slice. Runtime planning remains the authority for
rejecting unsupported aggregate nesting, subqueries, `DISTINCT`, windows, and
unsupported type envelopes.

The intended planning shape is:

```lemon
row_scalar_context_expression ::= row_scalar_expression.
aggregate_argument ::= aggregate_row_scalar_expression.
update_assignment_value ::= row_scalar_context_expression.
duplicate_update_assignment_value ::= row_scalar_context_expression.
```

## SQLite Integration

This slice uses MyLite-side planning and public SQLite SQL/UDF execution. It
does not need a targeted SQLite fork hook.

## Test Plan

Add focused C runtime tests for:

- representative control-flow, numeric, string, JSON, temporal, and binary
  string functions in predicates and `ORDER BY`;
- representative row-scalar expressions in `UPDATE` assignment and
  `INSERT ... ON DUPLICATE KEY UPDATE` assignment;
- non-grouped aggregate row-scalar arguments;
- grouped aggregate row-scalar arguments, including `AVG(expression)`;
- unsupported target categories continuing to reject through existing target
  validation.

## Compatibility Impact

Rows whose only top-level gap was context routing can move from yellow to green
once the relevant family docs and tests confirm that the routing is no longer
limited. Rows with independent semantic gaps, such as incomplete optional
function arguments, full Unicode or collation behavior, protocol expression
metadata, or `GROUP_CONCAT()` expression widening, must remain yellow and state
those specific gaps instead of using a generic context phrase.
