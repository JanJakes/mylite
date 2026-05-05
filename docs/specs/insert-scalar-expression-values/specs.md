# `INSERT` and `REPLACE` scalar expression values

## Scope

This slice extends the current `INSERT ... VALUES`, `INSERT ... SET`,
`REPLACE ... VALUES`, and `REPLACE ... SET` execution paths so source values may
use MyLite's supported scalar expression evaluator instead of being limited to
literals, `DEFAULT`, `NULL`, `CURRENT_TIMESTAMP`, and the earlier arithmetic
subset.

In scope:

- scalar built-in functions already supported by Task 24 in DML-safe expression
  contexts, including statement/session functions such as `NOW()`, `RAND()`,
  `UUID()`, `ROW_COUNT()`, and `LAST_INSERT_ID()`
- `CASE`, casts, supported operators, and nested scalar expressions when their
  operands are supported by the expression evaluator
- strict DML promotion of expression warnings and error-level conditions
- statement-stable current temporal values for all rows in one statement
- assignment-order column references already supported by `INSERT ... SET` and
  `REPLACE ... SET`
- existing `VALUES(col)` handling in `ON DUPLICATE KEY UPDATE`

Out of scope for this slice:

- full MySQL `INSERT ... VALUES` row-source column-reference semantics, where a
  value can observe earlier explicit assignments and default values for omitted
  columns
- scalar expressions in the `ON DUPLICATE KEY UPDATE` assignment list beyond the
  existing column, candidate-row, `VALUES(col)`, literal, and arithmetic subset
- insert-from-query sources, prepared statement parameters, user variables, and
  subqueries in insert source expressions
- complete conversion/range/truncation warning demotion for `INSERT IGNORE`

## Sources

- MySQL 8.4 Reference Manual, `INSERT` Statement:
  https://dev.mysql.com/doc/refman/8.4/en/insert.html
- MySQL 8.4 Reference Manual, `REPLACE` Statement:
  https://dev.mysql.com/doc/refman/8.4/en/replace.html
- MySQL 8.4 Reference Manual, Built-In Function and Operator Reference:
  https://dev.mysql.com/doc/refman/8.4/en/built-in-function-reference.html
- Existing MyLite specs:
  - `docs/specs/insert-values/specs.md`
  - `docs/specs/insert-set/specs.md`
  - `docs/specs/replace/specs.md`
  - `docs/specs/scalar-built-in-functions/specs.md`

Observed behavior was verified against MySQL 8.4.9 in Docker container
`mylite-mysql-849`, using:

```sh
docker exec -i mylite-mysql-849 mysql -uroot --force --batch --raw --show-warnings
```

This specification is independently authored from official documentation and
observed MySQL runtime behavior.

## MySQL 8.4.9 Behavior

With:

```sql
SET time_zone = '+00:00';
SET timestamp = 1700000000;
CREATE TABLE t (
  id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
  s VARCHAR(40),
  n INT,
  d DECIMAL(8,2),
  dt DATETIME(6),
  r DOUBLE NULL
);
```

MySQL accepts scalar functions in `VALUES` rows:

```sql
INSERT INTO t (s,n,d,dt,r)
VALUES (CONCAT('a','b'), ABS(-3), ROUND(12.345, 2), NOW(6), RAND(7));
```

The inserted row is:

```text
1, ab, 3, 12.35, 2023-11-14 22:13:20.000000, 0.9065021936842261
```

`INSERT ... SET` evaluates assignment expressions left to right. In:

```sql
INSERT INTO t SET s = CONCAT('x','y'), n = 2, d = n + 1.5,
  dt = TIMESTAMP('2024-02-29','01:02:03'), r = RAND(7);
```

`d` observes the earlier assignment to `n`, and the inserted values are
`xy`, `2`, `3.50`, `2024-02-29 01:02:03.000000`, and
`0.9065021936842261`.

Multiple rows in one statement share the statement timestamp:

```sql
INSERT INTO t (dt,r,s)
VALUES (NOW(6), RAND(7), UUID()), (NOW(6), RAND(7), UUID());
```

Both `dt` values are identical. `RAND(7)` returns the same seeded value for
each textual seeded call, while `UUID()` returns different identifiers.

Strict DML promotes expression warnings and error-level conditions:

```sql
INSERT INTO t (s) VALUES (REGEXP_LIKE('abc','('));
```

returns error `3691 (HY000)` with message
`Mismatched parenthesis in regular expression.`

```sql
INSERT INTO t (s) VALUES (SQRT('foo'));
```

returns error `1292 (22007)` with message
`Truncated incorrect DOUBLE value: 'foo'`.

MySQL also supports richer row-source column references in `VALUES` rows. With
`CREATE TABLE t (a INT DEFAULT 5, b INT DEFAULT 7)`, these statements insert
`(8,1)`, `(3,2)`, and `(8,7)`:

```sql
INSERT INTO t (a,b) VALUES (b + 1, 1);
INSERT INTO t (b,a) VALUES (2, b + 1);
INSERT INTO t (a) VALUES (b + 1);
```

That row-source reference model remains deferred for MyLite until insert source
evaluation is rebuilt around assignment-order row state.

## MyLite Semantics

MyLite evaluates source expressions through `mylite_expression_eval_with_context`
when an insert or replace value cannot be represented by the existing literal
path and does not use the special `VALUES(col)` duplicate-update function.

The evaluator uses the same statement/session function callbacks as `UPDATE`
and `DELETE`, so current temporal functions remain statement-stable and
session functions read the executing statement state.

For `INSERT ... SET` and `REPLACE ... SET`, expression identifiers keep the
current assignment-order semantics:

- references to assigned columns read the value assigned so far
- references to unassigned columns read the implicit/default value already
  materialized for the row
- unknown references return MySQL-compatible unknown-column diagnostics

For `INSERT ... VALUES` and `REPLACE ... VALUES`, this slice supports scalar
functions and expressions whose identifiers do not require the deferred
row-source reference model.

Expression results are converted into insert-bound values before storage:

- `NULL` follows existing explicit-`NULL` handling, including auto-increment
  allocation
- signed and unsigned integers bind as integer values when in range
- real values bind as real values except for auto-increment targets, where the
  existing unsupported diagnostic is retained unless the value is `0`
- text values are routed through existing column conversion unless the target
  column uses text/blob storage

Expression warnings and error-level conditions are promoted for strict DML in
the same way as current `UPDATE` and `DELETE` assignment expressions.

## Tests

Runtime tests should cover:

- `INSERT ... VALUES` with `CONCAT`, `ABS`, `ROUND`, `NOW(6)`, `RAND(seed)`,
  `UUID()`, and `STRCMP`
- `INSERT ... SET` with function expressions and an assignment-order reference
  feeding a later expression
- `REPLACE ... VALUES` and `REPLACE ... SET` with function expressions
- multi-row statement-stable `NOW(6)` and per-call `UUID()` behavior
- strict promotion for invalid regular expressions and conversion warnings
- unsupported row-source column-reference behavior remains deterministic until
  the dedicated row-source reference model is implemented
