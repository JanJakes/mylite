# Baseline Last Insert ID Row Expression

## Scope

This slice extends the existing `LAST_INSERT_ID(expr)` support from source-free
scalar contexts to source-backed row expression contexts.

The MySQL 8.4 Reference Manual documents `LAST_INSERT_ID()` and
`LAST_INSERT_ID(expr)` in the information-functions section:

- <https://dev.mysql.com/doc/refman/8.4/en/information-functions.html>

The official behavior is connection-local: the no-argument form returns the
current session value, while the argument form returns the argument value and
remembers it for the next no-argument call. MySQL also documents the sequence
idiom:

```sql
UPDATE sequence SET id = LAST_INSERT_ID(id + 1)
```

MyLite implements the row-backed baseline for:

- single-table row-scalar `SELECT` projection;
- supported descriptor-backed `WHERE` predicates;
- non-grouped single-table `ORDER BY` expression keys;
- supported single-table `UPDATE` assignment expressions, including the
  documented sequence-update shape over integer columns.

## MySQL 8.4.9 Runtime Observations

Observed against MySQL 8.4.9:

```sql
CREATE TABLE t(id INT, v INT);
INSERT INTO t VALUES (1,10),(2,20),(3,30);
SELECT LAST_INSERT_ID(id), LAST_INSERT_ID() FROM t ORDER BY id;
SELECT LAST_INSERT_ID();
```

returns row pairs `1,1`, `2,2`, `3,3`, and the following read returns `3`.

```sql
SELECT LAST_INSERT_ID(0);
SELECT id FROM t WHERE LAST_INSERT_ID(id) >= 2 ORDER BY id;
SELECT LAST_INSERT_ID();
```

returns ids `2`, `3`, and the following read returns `3`.

```sql
SELECT LAST_INSERT_ID(0);
SELECT id FROM t ORDER BY LAST_INSERT_ID(id) DESC;
SELECT LAST_INSERT_ID();
```

orders rows `3`, `2`, `1`; the following read returns `3`.

```sql
CREATE TABLE seq(id INT NOT NULL);
INSERT INTO seq VALUES (0);
UPDATE seq SET id = LAST_INSERT_ID(id + 1);
SELECT id, LAST_INSERT_ID(), ROW_COUNT(), @@warning_count FROM seq;
```

returns `1`, `1`, `1`, `0`.

## Syntax

This slice uses the existing parser and AST nodes:

```lemon
expression ::= LAST_INSERT_ID LPAREN expression RPAREN.
```

No new grammar is required. The same AST node is admitted by the row-scalar
planner when the argument can be lowered to a supported row-scalar integer
expression.

## Semantics

`LAST_INSERT_ID(expr)` evaluates `expr`, converts the supported baseline value
to MyLite's unsigned 64-bit session value, returns that value, and stores it in
the connection-local `last_insert_id` field.

Zero-argument `LAST_INSERT_ID()` in row-scalar SQL is lowered to a private
dynamic getter when planned through this surface, so a supported
`LAST_INSERT_ID(expr)` evaluated earlier in the same row expression flow is
observable by a following zero-argument read.

Supported row-backed argument values:

- descriptor-backed integer-family columns;
- supported row integer arithmetic over descriptor-backed integer values and
  integer literals;
- `NULL`, which returns SQL `NULL` and stores `0`;
- source-free scalar values already admitted by the existing
  `LAST_INSERT_ID(expr)` implementation.

Negative signed integer values are stored using MySQL-compatible unsigned
two's-complement wrapping. Values that fit in SQLite's signed integer range are
returned to SQLite as integer values; wrapped unsigned values above that range
are returned as decimal text so result rendering preserves the full unsigned
value.

## Diagnostics And Gaps

Unsupported row-backed argument families fail with deterministic MyLite
unsupported diagnostics before execution where the planner can detect them, or
with a runtime SQLite callback error if SQLite exposes an unsupported dynamic
type.

This slice does not implement:

- MySQL's warning-producing string, decimal, float, hex, bit, temporal, JSON,
  or arbitrary expression conversion rules;
- subquery arguments;
- grouped expression keys or aggregate interactions;
- stored routine, stored function, or trigger restoration semantics;
- protocol `mysql_insert_id()` packet/C-API parity beyond existing MyLite
  result metadata.

## Runtime Architecture

The implementation uses private SQLite scalar functions registered during
connection bootstrap. Row-scalar planning lowers `LAST_INSERT_ID(expr)` to the
setter and row-scalar zero-argument `LAST_INSERT_ID()` to the getter. The
callbacks retrieve the owning `mylite_db` from SQLite connection client data,
update or read connection-local session state, and return the converted value.

No public API, catalog, storage format, VFS, generated parser output, or SQLite
fork patch is required.

## Tests

The MySQL expectation script extends the existing
`mysql_baseline_last_insert_id_function_expectations.sh` artifact with
row-backed projection, predicate, ordering, and sequence-update observations.

The C runtime test extends `runtime_last_insert_id_function_test.c` with:

- `SELECT LAST_INSERT_ID(id), LAST_INSERT_ID()` row projection;
- descriptor-backed `WHERE LAST_INSERT_ID(id) >= 2`;
- `ORDER BY LAST_INSERT_ID(id) DESC`;
- `UPDATE seq SET id = LAST_INSERT_ID(id + 1)`;
- following no-argument `LAST_INSERT_ID()` reads to verify the persisted session
  side effect.
