# Baseline PIPES_AS_CONCAT

## Goal

Add the first narrow behavioral effect for `PIPES_AS_CONCAT`: when the current
session SQL mode includes the flag, `||` is parsed as string concatenation for
the scalar projection paths that already support `CONCAT()`.

This feature is a parser/planner translation slice. It does not add a general
expression engine, full SQL-mode behavior, predicate truth evaluation for
concatenation expressions, DML assignment expressions, expression indexes, or a
SQLite fork patch.

## Sources

- Official MySQL 8.4 Reference Manual, server SQL modes:
  <https://dev.mysql.com/doc/refman/8.4/en/sql-mode.html>
- Official MySQL 8.4 Reference Manual, operator precedence:
  <https://dev.mysql.com/doc/refman/8.4/en/operator-precedence.html>
- Official MySQL 8.4 Reference Manual, logical operators:
  <https://dev.mysql.com/doc/refman/8.4/en/logical-operators.html>
- Existing SQL mode session-state design:
  `docs/specs/baseline-sql-mode-session-state/specs.md`
- Existing `CONCAT()` compatibility surface:
  `docs/compatibility/functions-string.md`
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_pipes_as_concat_expectations.sh`.

The MyLite grammar and implementation are independently authored from official
documentation, observed MySQL 8.4.9 behavior, public SQLite APIs, and existing
MyLite code. Do not copy MySQL, MariaDB, Percona, SQLite implementation
internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the behavior used by this baseline:

- `SET sql_mode = 'PIPES_AS_CONCAT'` makes `||` behave like `CONCAT()`.
- `SET sql_mode = 'ANSI'` includes `PIPES_AS_CONCAT`, so `||` also concatenates
  under `ANSI`.
- The operator returns `NULL` if any operand is `NULL`, matching `CONCAT()`.
- Integer operands are coerced to strings for concatenation. In a later numeric
  context, the concatenated string is converted using ordinary MySQL numeric
  conversion.
- `||` is left-associative.
- With `PIPES_AS_CONCAT`, `||` binds more tightly than `*`, `/`, `DIV`, `%`,
  `+`, `-`, bit shifts, bitwise `&`, bitwise `|`, comparisons, `AND`, `XOR`,
  and `OR`; it binds less tightly than unary operators. For example,
  `1 + 2 || 3` produces `24`, while `1 || 2 + 3` produces `15`.
- Successful supported statements produce no warnings.
- Without `PIPES_AS_CONCAT`, MySQL still treats `||` as deprecated logical OR
  and emits warning `1287`; this feature does not broaden MyLite's scalar
  logical projection surface for that mode.

## Supported Surface

MyLite supports `||` as a concatenation operator only when the parser is running
with `MYLITE_SQL_MODE_PIPES_AS_CONCAT`, which is derived from the connection's
session SQL mode.

Supported statement forms:

- no-source `SELECT` scalar projection;
- `SELECT ... FROM DUAL` scalar projection;
- `DO` expression execution;
- single-table row-scalar `SELECT` projection over one descriptor-backed
  persistent or temporary base table, within the existing row-scalar `SELECT`
  envelope;
- `SET sql_mode = 'PIPES_AS_CONCAT'` and `SET sql_mode = 'ANSI'` activation
  through the existing session SQL-mode state.

Supported operand forms match the current `CONCAT()` argument surface:

- string, integer, boolean, and `NULL` literals;
- optional unary sign for integer literals within the signed 64-bit range;
- existing session scalar values such as `DATABASE()`, `SCHEMA()`, and
  supported `@@...` system variables;
- limited no-source and `FROM DUAL` scalar subqueries where already admitted
  by current `CONCAT()` planning;
- descriptor-backed integer, exact `DECIMAL`, nonbinary string, baseline
  `TEXT` family, `YEAR`, `DATE`, `TIME`, `DATETIME`, and `TIMESTAMP` operands
  in the existing row-scalar `SELECT` path;
- supported row-scalar value functions, including nested `CONCAT()` /
  `CONCAT_WS()` and current string helpers, in the existing row-scalar value
  envelope;
- parentheses around operands and around the full concatenation expression.

For no-source, `DUAL`, and `DO` scalar expressions, a `||` expression may also
feed the current signed-integer arithmetic evaluator when the concatenated
result is a valid signed 64-bit decimal integer string. This covers the
verified precedence examples without adding general MySQL string-to-number
coercion, warnings, or arbitrary expression evaluation.

The generated SQLite expression uses standard SQLite `||` over descriptor-built
operands and bound parameters. SQLite performs row scanning and expression
evaluation; MyLite does not materialize rows in memory for this operator.

## Deferred Surface

This slice intentionally does not support:

- `||` concatenation unless `PIPES_AS_CONCAT` is active;
- scalar `||` logical OR projection when `PIPES_AS_CONCAT` is inactive;
- concatenation expressions in `WHERE`, `HAVING`, `ORDER BY`, `GROUP BY`,
  `UPDATE` assignments, `INSERT` / `REPLACE` values, defaults, generated
  columns, checks, indexes, partitions, triggers, or arbitrary SQLite
  pass-through;
- joined, grouped, distinct, compound, recursive, windowed, or CTE expression
  contexts beyond the current row-scalar projection envelope;
- binary-string result typing, charset/collation metadata parity, non-ASCII
  collation behavior, introducers, collations, parameters, user variables,
  approximate numeric formatting, JSON value typing, arbitrary expression
  operands outside the supported row-scalar value subset, or expression
  metadata.

## Grammar

The supported MyLite subset is described independently as:

```lemon
expression(A) ::= expression(B) CONCAT_OPERATOR(O) expression(C).

/* Token mapping is SQL-mode sensitive. */
CONCAT_OPERATOR ::= "||" when PIPES_AS_CONCAT is active.
LOGICAL_OR      ::= "||" when PIPES_AS_CONCAT is inactive.
```

`CONCAT_OPERATOR` has higher precedence than `BITWISE_XOR` and lower precedence
than unary operators in MyLite's Lemon grammar, matching the verified MySQL
8.4.9 precedence for the supported expressions. These snippets describe
MyLite's supported subset, not MySQL's full grammar.

## Runtime Semantics

Planning:

1. `SET sql_mode` keeps using the existing session state and canonical text
   storage. `PIPES_AS_CONCAT` and `ANSI` already occupy session-mode bits.
2. Statement parsing converts the current session SQL mode to parser modes.
   This feature adds `MYLITE_SQL_MODE_PIPES_AS_CONCAT` when the session bit is
   active.
3. The token mapper maps `||` to `CONCAT_OPERATOR` only under that parser mode.
   Otherwise the existing deprecated logical-OR token remains unchanged.
4. The parser creates an ordinary binary-expression AST node with a new
   `concat` operator kind.
5. The row-scalar planner recognizes top-level concat-operator trees, flattens
   left/right associative chains into the existing planned `CONCAT` expression
   shape, and plans each leaf through the current non-`CONCAT()` argument
   planner.
6. Unsupported operand forms fail deterministically through the existing
   row-scalar diagnostic surface.

Generated SQL shape:

```sql
SELECT (?1 || "v" || ?2) FROM "_mylite_user_table_1"
```

The exact physical table name is internal. The invariants are descriptor-derived
column references, quoted identifiers, numbered parameter placeholders, and
bound literal/session values. No SQLite fork hook is required.

Execution:

- MyLite binds literal and session scalar operands using the existing planned
  value binder.
- Descriptor columns remain direct SQLite column references.
- SQLite evaluates `||`, including `NULL` propagation, for the generated
  expression.
- Successful `SELECT` follows existing result conventions:
  `affected_rows == 0`, `ROW_COUNT()` becomes `-1`, and `warning_count == 0`.
- Successful `DO` follows existing non-row result conventions:
  zero result columns, zero rows, `affected_rows == 0`, and `warning_count == 0`.

## Ownership Boundaries

- Public API: unchanged. Users call `mylite_execute()` and inspect the existing
  result object.
- Statement context: unchanged except for normal `SELECT`/`DO` status.
- Lexer/parser/AST: adds a parser-mode bit, a mode-sensitive token mapping for
  `||`, and a binary AST operator for concatenation.
- Analyzer/planner: narrows concat-operator support to the existing row-scalar
  projection envelope and reuses planned `CONCAT` storage.
- Catalog: read-only descriptor authority for row-scalar column operands. No
  descriptor rows, descriptor versions, descriptor caches, catalog generation,
  or `sqlite_schema_generation` are mutated.
- Result builder: unchanged; concatenation is a projected scalar expression.
- Storage/VFS/file format: unchanged. `.mylite` preamble and shifted SQLite
  payload invariants are preserved.
- SQLite: standard generated SQLite expression using public SQLite preparation
  and binding APIs. No targeted SQLite fork patch is needed.

## Diagnostics

- Inactive `PIPES_AS_CONCAT`: scalar `SELECT 1||0` keeps the current MyLite
  unsupported syntax behavior; descriptor-backed `WHERE ... || ...` keeps the
  existing deprecated logical-OR predicate behavior.
- Unsupported concat contexts return MyLite's current parse or unsupported
  expression diagnostics for that context.
- Unsupported operands return the current row-scalar `CONCAT()` argument
  diagnostics.
- Unknown descriptor columns use existing descriptor-resolution diagnostics.
- Table-backed row-scalar integer operand overflow uses the existing signed
  64-bit row-scalar literal diagnostic. In no-source, `DUAL`, and `DO` scalar
  concatenation, otherwise supported integer literals are converted as strings
  before concatenation, matching the observed MySQL behavior for wide unsigned
  literals.
- Allocation failures return `MYLITE_NOMEM` and leave cleanup paths
  zero-initialization safe.

## Test Plan

- MySQL expectation script:
  - validates MySQL 8.4.9 runtime version;
  - verifies `PIPES_AS_CONCAT` and `ANSI` activation;
  - records `NULL` propagation, integer operand coercion, left associativity,
    and verified precedence examples;
  - verifies no-source, `DUAL`, scalar-subquery, table-backed, and `DO`
    behavior;
  - records inactive-mode logical-OR behavior and warnings as a scoped
    incompatibility.
- Parser tests:
  - `||` remains a syntax error in scalar projection without the parser mode;
  - `||` parses under `MYLITE_SQL_MODE_PIPES_AS_CONCAT`;
  - precedence and left-associative AST shape are checked for representative
    expressions.
- Runtime tests:
  - no-source and `DUAL` projection;
  - `SET sql_mode = 'PIPES_AS_CONCAT'` and `SET sql_mode = 'ANSI'`;
  - `NULL` propagation and integer/string operands;
  - precedence examples verified against MySQL;
  - `DO` result conventions;
  - table-backed row-scalar projection and reopen persistence;
  - unsupported contexts and operands;
  - inactive-mode scalar syntax remains unchanged;
  - `.mylite` preamble remains unchanged for file-backed tests.
