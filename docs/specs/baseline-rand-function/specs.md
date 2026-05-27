# Baseline RAND Function

## Goal

Add the first narrow random scalar function slice:

```sql
SELECT RAND() AS r
```

This feature exists to accept common application probes and lightweight scalar
queries that expect a MySQL-shaped random value. It is not a full nondeterministic
expression implementation. Seeded random sequences, per-row evaluation,
`ORDER BY RAND()`, and expression metadata are left for later expression-engine
work.

## Sources

- Official MySQL 8.4 Reference Manual, mathematical functions:
  <https://dev.mysql.com/doc/refman/8.4/en/mathematical-functions.html>
- Official MySQL 8.4 Reference Manual, function call optimization:
  <https://dev.mysql.com/doc/refman/8.4/en/function-optimization.html>
- SQLite public C API, `sqlite3_randomness()`
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_rand_function_expectations.sh`.

This specification is independently authored from official MySQL 8.4
documentation, observed MySQL 8.4.9 behavior, public SQLite APIs, and existing
MyLite code. It does not copy MySQL, MariaDB, Percona, SQLite implementation
internals, or other restrictively licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish these expectations for this slice:

- `RAND()` returns a non-`NULL` approximate numeric value `v` where
  `0 <= v < 1`.
- `RAND()` is nondeterministic. Two invocations in the same `SELECT` may return
  different values; callers must not treat it as a statement constant.
- `RAND ()` with whitespace before `(` is accepted in default SQL mode.
- `SELECT RAND() FROM DUAL` returns one row.
- `DO RAND(), RAND()` succeeds, returns no result set, leaves
  `ROW_COUNT() == 0`, and emits no warnings.
- `SELECT RAND` is parsed as a bare identifier and fails with unknown column
  `1054 / 42S22` when no such column is in scope.
- Unquoted `rand` remains usable as an identifier in current tested DDL
  contexts, including `CREATE TABLE rand (id INT)`.
- `RAND(seed)` is valid MySQL behavior with repeatable seeded sequences, but
  is outside this baseline slice.
- `RAND(1, 2)` fails with native function argument-count error
  `1582 / 42000`.
- MySQL evaluates table-backed `RAND()` once per row and accepts
  `ORDER BY RAND()`, but both are outside this baseline slice.

Later seeded and table-backed `RAND()` baseline phases supersede the initial
deferred notes above only for their explicitly documented subsets.

## Supported Surface

MyLite supports:

- no-source `SELECT RAND()` scalar projection;
- `SELECT RAND() FROM DUAL`;
- `DO RAND()`;
- multiple top-level `RAND()` projections or `DO` expressions in one statement;
- ordinary expression parentheses around the `RAND()` call;
- aliases and default column labels using the existing scalar projection result
  conventions;
- whitespace between `RAND` and `(`, matching observed MySQL behavior;
- successful supported calls with no warnings and with existing `ROW_COUNT()`
  conventions: `-1` after `SELECT`, `0` after `DO`;
- no catalog, descriptor, physical table, statement-context, file-format, VFS,
  or SQLite schema-generation mutation.

Each invocation produces a MyLite-owned visible double string for a value in
`[0, 1)`. MyLite uses SQLite's public `sqlite3_randomness()` API as the entropy
source, converts random bits to a double in the admitted range, and reuses the
existing scalar double formatter. This feature does not require a SQLite fork
patch.

## Deferred Surface

This slice intentionally does not support:

- seeded `RAND(seed)` or exact MySQL seeded sequence parity;
- nonconstant seed expressions, column seeds, string seed conversion, or seed
  warnings;
- table-backed `SELECT RAND() FROM table`;
- `WHERE RAND()`, `ORDER BY RAND()`, `GROUP BY RAND()`, `HAVING RAND()`, DML
  assignments, defaults, generated columns, indexes, or constraints;
- arithmetic around `RAND()` such as `RAND() * 5`;
- `FLOOR(7 + RAND() * 5)` or random integer helper expressions;
- use inside `CONCAT()`, `FIELD()`, `CASE`, comparisons, predicates, aggregate
  inputs, scalar subqueries, or update subqueries;
- replication warnings or binlog-format semantics;
- protocol-grade approximate numeric metadata.

## Grammar

The MyLite grammar admits a zero-argument built-in function node and keeps
unsupported seed forms distinguishable:

```lemon
expression(A) ::= RAND(T) LPAREN RPAREN(R).
expression(A) ::= RAND(T) LPAREN expression(B) RPAREN(R).
expression(A) ::= RAND(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R).
identifier(A) ::= RAND(T).
```

The zero-argument production builds `MYLITE_SQL_AST_RAND_FUNCTION`. The
one-argument production builds a seed-specific unsupported node so execution
can return a deterministic MyLite diagnostic instead of a syntax error. The
two-or-more-argument production builds an argument-count marker for MySQL's
native `1582 / 42000` diagnostic.

## Parser and AST

`RAND` is added as a keyword token so function calls parse without ambiguity,
while the identifier production keeps `rand` usable as an unquoted identifier
where the current grammar permits identifiers. The parser must preserve source
spans for default result labels, including whitespace in `RAND ()`.

`RAND()` participates only in the existing no-source/`DUAL` scalar projection
and `DO` expression admission path. It is not admitted by descriptor row-scalar
planning.

## Runtime Behavior

Evaluation is MyLite-owned:

1. Validate the AST shape is a zero-argument `RAND()` node.
2. Request random bytes through `sqlite3_randomness()`.
3. Convert the random bits to a `double` in `[0, 1)` using a fixed internal
   transformation.
4. Format the value through the existing scalar double formatting helper.
5. Return the formatted text in the scalar result cell.

Successful evaluation emits no warnings. `RAND()` itself is not tied to the
statement context timestamp and does not update row-count state beyond the
existing `SELECT` and `DO` statement conventions.

## Diagnostics

Supported diagnostics:

- `RAND(1, 2)` and wider argument lists: MySQL-compatible native function
  parameter-count error `1582 / 42000`;
- `RAND(seed)`: MyLite-specific unsupported diagnostic,
  `RAND(seed) is not supported`;
- table-backed or nested contexts: existing scalar-expression unsupported
  diagnostics, updated to mention limited `RAND()`;
- allocation or formatting failures: existing `MYLITE_NOMEM` or runtime error
  paths;
- public API misuse: unchanged existing `mylite_execute()` behavior.

## Architecture Boundary

- Public API: unchanged; callers use `mylite_execute()` and existing result
  accessors.
- Statement context: unchanged; randomness is not stored in the statement
  context.
- Lexer/parser/AST: own MySQL syntax recognition and deterministic unsupported
  seed/arity shapes.
- Analyzer/planner: no descriptor planning; only scalar expression admission is
  extended.
- Runtime/result builder: evaluates and formats one scalar value per admitted
  invocation.
- Catalog/storage/VFS: untouched. No descriptors, physical rows, `.mylite`
  preamble bytes, or SQLite schema text are mutated.
- SQLite: used only through public `sqlite3_randomness()`; no fork patches.

## Tests

Add MySQL-runtime expectations for:

- MySQL 8.4.9 version guard;
- `RAND()` no-source range, warning count, and `ROW_COUNT()` behavior;
- `RAND ()` label and value range with `FROM DUAL`;
- `DO RAND(), RAND()` row-count and warning behavior;
- `RAND` as an identifier and bare `SELECT RAND` unknown-column behavior;
- MySQL acceptance of seeded and table-backed forms that MyLite intentionally
  defers;
- native error for two-or-more arguments.

Add C tests for:

- parser AST shapes for `RAND()`, `RAND ()`, aliases, `FROM DUAL`, `DO`, seed
  unsupported node, two-argument count marker, and identifier use;
- runtime range checks for no-source, `FROM DUAL`, multiple projections, and
  `DO`;
- result labels, affected rows, row counts, warning counts, and absence of
  result rows for `DO`;
- deterministic diagnostics for seeded and table-backed/nested unsupported
  contexts;
- no catalog generation or `.mylite` preamble mutation for supported calls.

## Compatibility Documentation

Use limited wording in `COMPATIBILITY.md` and
`docs/compatibility/functions-numeric-math.md`. Do not claim seeded `RAND(N)`,
table-backed evaluation, random ordering, random integer expressions, protocol
metadata, or replication semantics.
