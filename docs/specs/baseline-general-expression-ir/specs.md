# Baseline General Expression IR

## Goal

MyLite currently supports many MySQL expressions through narrow, feature-specific
row-scalar planners. This slice introduces a common row expression planning
surface and starts migrating operator support onto it. The immediate executable
scope is table-backed numeric arithmetic composition for `+`, binary `-`, `*`,
`/`, `DIV`, `%`, and infix `MOD` in the contexts that already use row-scalar
SQL lowering: projection, supported predicates, `ORDER BY` keys, and nested
function arguments.

The long-term purpose is to make MySQL expressions composable across ordinary
SQL positions instead of admitting each function/operator in hand-written
contexts. This slice is intentionally architectural: it creates the first
shared operator substrate while preserving existing SQLite pushdown.

## Compatibility Authority

Normative behavior comes from official MySQL 8.4 Reference Manual pages and
MySQL 8.4.9 runtime observations:

- `https://dev.mysql.com/doc/refman/8.4/en/expressions.html`
- `https://dev.mysql.com/doc/refman/8.4/en/operator-precedence.html`
- `https://dev.mysql.com/doc/refman/8.4/en/type-conversion.html`
- `https://dev.mysql.com/doc/refman/8.4/en/arithmetic-functions.html`

Observed MySQL 8.4.9 probes for this slice:

```sql
CREATE TABLE t(
    id INT PRIMARY KEY,
    a INT,
    b INT,
    s VARCHAR(20),
    d DECIMAL(8,2),
    f DOUBLE,
    z INT NULL
);
INSERT INTO t VALUES
    (1, 7, 2, '8x', 7.50, 7.25, NULL),
    (2, -7, 0, 'x8', -7.50, -7.25, 3),
    (3, NULL, 5, NULL, NULL, NULL, NULL);

SELECT id, a + b * 3, (a + b) * 3, a / b, a DIV b, a % b, MOD(a,b)
FROM t ORDER BY id;
SHOW WARNINGS;

SELECT id, s + 2, s * b, s / b, s % b
FROM t ORDER BY id;
SHOW WARNINGS;

SELECT id FROM t WHERE (a + b * 3) >= 13 ORDER BY id;
SELECT id FROM t WHERE (a / b) IS NULL ORDER BY id;
SELECT id FROM t ORDER BY a / NULLIF(b,0), id;
```

Key observed results:

- `a + b * 3` follows MySQL operator precedence and returns `13`, `-7`, `NULL`
  for the rows above.
- Parentheses override precedence: `(a + b) * 3` returns `27`, `-21`, `NULL`.
- `/` returns fixed-scale decimal-looking text for exact integer operands, such
  as `3.5000`, and returns `NULL` with warning `1365 / 22012 Division by 0`
  when the divisor is zero.
- `DIV`, `%`, and `MOD()` over a zero divisor return `NULL` and append the same
  division-by-zero warning.
- String arithmetic converts strings to numbers with truncation warnings. For
  example, `'8x' + 2` returns `10` and appends `1292 / 22007`.

## Syntax

The existing MyLite grammar already parses the arithmetic operators in scalar
expressions. The intended Lemon-shape for the executable subset is:

```lemon
row_scalar_expression ::= row_scalar_expression PLUS row_scalar_expression.
row_scalar_expression ::= row_scalar_expression MINUS row_scalar_expression.
row_scalar_expression ::= row_scalar_expression STAR row_scalar_expression.
row_scalar_expression ::= row_scalar_expression SLASH row_scalar_expression.
row_scalar_expression ::= row_scalar_expression DIV row_scalar_expression.
row_scalar_expression ::= row_scalar_expression MOD row_scalar_expression.
row_scalar_expression ::= row_scalar_expression PERCENT row_scalar_expression.
row_scalar_expression ::= LPAREN row_scalar_expression RPAREN.
row_scalar_expression ::= literal.
row_scalar_expression ::= identifier.
row_scalar_expression ::= function_call.
```

This is a planning contract, not a new parser copy. Existing parser precedence
continues to determine the AST.

## Semantics

### Planning

The row expression planner should treat supported arithmetic operators as one
common operator node rather than a family-specific integer-only special case.
Each node has:

- operator kind;
- ordered child expressions;
- optional inferred broad result family;
- existing `planned_value` literal leaves;
- descriptor-column leaves with source index information when needed.

The first supported operand domain is:

- integer descriptor columns;
- nonbinary string descriptor columns for MySQL-style numeric-prefix coercion;
- integer, decimal, float, string, boolean, and `NULL` literals;
- nested supported arithmetic operators;
- nested covered row numeric functions and already-supported row control-flow
  expressions where the existing row-scalar planner can lower them.

### Execution

SQLite should continue to scan, filter, sort, and limit rows. MyLite lowers
row expression operators to SQLite SQL using MyLite-owned UDFs when SQLite's
native semantics differ from MySQL, especially for:

- MySQL numeric-prefix string coercion and warnings;
- division-by-zero warnings;
- `DIV` truncation toward zero;
- modulo `NULL` behavior on zero divisor.

Direct SQLite operators may be used only when their behavior is already covered
by the MySQL-compatible operand envelope. The first implementation should prefer
MyLite UDFs for arithmetic operators so diagnostics are handle-owned and
consistent.

### Diagnostics

Warnings produced by expression UDFs belong to the current statement diagnostics
area and must be visible through result warning counts and `SHOW WARNINGS`.
Successful statements must not leak stale error conditions into warning rows.

### Metadata

This slice improves composability, not protocol-grade expression metadata.
Projected expression labels should remain MySQL-shaped for supported existing
paths, but exact column type, flags, charset/collation, and origin metadata
remain documented as deferred expression metadata work.

## SQLite Integration

This slice uses public SQLite extension APIs:

- scalar UDF registration for MyLite arithmetic operators;
- existing SQLite SQL generation and parameter binding;
- existing registered collations where string ordering is already supported.

No targeted SQLite fork hook is required for this slice. A fork hook should only
be considered later if expression metadata or warning propagation cannot be
implemented through public extension APIs without unacceptable overhead.

## Test Plan

Add MySQL-verified expectations and C runtime tests covering:

- table-backed projection with `+`, `-`, `*`, `/`, `DIV`, `%`, and infix `MOD`;
- parentheses and precedence;
- nested numeric functions over arithmetic operands;
- `WHERE` comparison and `IS NULL` predicates over arithmetic expressions;
- `ORDER BY` arithmetic expressions;
- string numeric coercion warnings;
- division-by-zero warnings;
- intentionally unsupported operands such as binary strings or JSON values.

## Compatibility Impact

After this slice, the broad expression rows should still remain yellow because
general expression support is not complete across all function families, DML
assignments, grouping, aggregate arguments, JSON/temporal/binary coercion, and
metadata. The arithmetic operator rows can be narrowed from "no table-backed
expression support" to the implemented row-scalar operator envelope.

## Deferred Work

- Full expression metadata.
- Broad comparison, logical, `BETWEEN`, `IN`, `LIKE`, and `REGEXP` operands.
- General function composition for every scalar function family.
- Aggregate argument expressions and window expression keys.
- DML assignment/value expressions outside the currently planned row-scalar
  envelope.
- JSON, temporal, binary-string, `BIT`, unsigned-width, and exact fixed-decimal
  arithmetic parity.
