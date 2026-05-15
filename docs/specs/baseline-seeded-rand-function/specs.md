# Baseline Seeded RAND Function

## Goal

Extend the existing baseline `RAND()` slice to cover deterministic seeded calls
in the no-source scalar envelope:

```sql
SELECT RAND(1) AS r
```

This slice is intentionally narrower than full MySQL `RAND([N])` behavior. It
adds exact seeded values for literal seeds where MyLite already has no-source,
`FROM DUAL`, and `DO` scalar execution, without adding table-backed per-row
random expression evaluation, `ORDER BY RAND()`, expression metadata, or
replication semantics.

## Sources

- Official MySQL 8.4 Reference Manual, mathematical functions:
  <https://dev.mysql.com/doc/refman/8.4/en/mathematical-functions.html>
- Official MySQL 8.4 Reference Manual, function call optimization:
  <https://dev.mysql.com/doc/refman/8.4/en/function-optimization.html>
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_seeded_rand_function_expectations.sh`.
- Existing MyLite baseline `RAND()` implementation and tests.

This specification is independently authored from official MySQL 8.4
documentation, observed MySQL 8.4.9 behavior, public SQLite APIs, and existing
MyLite code. It does not copy MySQL, MariaDB, Percona, SQLite implementation
internals, or other restrictively licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

The MySQL manual defines `RAND([N])` as a random floating-point function
returning `0 <= v < 1.0`, with integer `N` used as a seed. It also states that
a constant seed is initialized once for statement execution, while a nonconstant
seed is initialized for each invocation.

Runtime probes against MySQL 8.4.9 establish the exact values and status
behavior for this slice:

```sql
SELECT RAND(0), RAND(1), RAND(2), RAND(3), RAND(NULL), RAND(TRUE), RAND(FALSE), RAND(-1);
```

returns:

```text
0.15522042769493574  0.40540353712197724  0.6555866465490187
0.9057697559760601   0.15522042769493574  0.40540353712197724
0.15522042769493574  0.9050373219931845
```

Additional probes establish:

- `RAND(4294967295)` matches `RAND(-1)`;
- `RAND(4294967296)` matches `RAND(0)`;
- `RAND(4294967297)` matches `RAND(1)`;
- supported literal seeded calls emit no warnings and keep existing `SELECT`
  and `DO` row-count conventions;
- `SELECT RAND(1), RAND(), RAND(1)` returns exact seeded values for each
  seeded invocation while the zero-argument call remains nondeterministic;
- `SELECT RAND(1, 2)` reports native function parameter-count error
  `1582 / 42000`;
- MySQL accepts table-backed seeded calls such as `SELECT id, RAND(1) FROM t`,
  where a constant seed produces a repeatable per-row sequence. That behavior is
  outside this slice because MyLite does not yet expose general per-row random
  expression state.

The implemented recurrence is derived from these observed MySQL 8.4.9 values
and cross-checked against the exact values above. It is MyLite-owned code.

## Supported Surface

MyLite supports:

- no-source `SELECT RAND(seed)` scalar projection;
- `SELECT RAND(seed) FROM DUAL`;
- `DO RAND(seed)`;
- aliases and default labels using existing scalar projection conventions;
- ordinary parentheses around the seed literal, such as `RAND((1))`;
- multiple top-level seeded and zero-argument `RAND` calls in one no-source
  statement;
- seed operands limited to:
  - decimal integer literals with optional unary `+` or unary `-`;
  - `NULL`, treated as seed `0`;
  - `TRUE`, treated as seed `1`;
  - `FALSE`, treated as seed `0`;
- decimal integer seed magnitudes that fit in `uint64_t`, then truncated to the
  low 32 bits before state initialization;
- negative integer seeds mapped by two's-complement 32-bit wrapping, so `-1`
  becomes `4294967295`;
- successful supported calls with no warnings and existing row-count behavior:
  `-1` after `SELECT`, `0` after `DO`;
- no catalog, descriptor, physical table, statement-context, file-format, VFS,
  SQLite schema-generation, or SQLite fork mutation.

## Deferred Surface

This slice intentionally does not support:

- table-backed `SELECT RAND(seed) FROM table`;
- `WHERE RAND(seed)`, `ORDER BY RAND(seed)`, `GROUP BY RAND(seed)`,
  `HAVING RAND(seed)`, DML assignments, defaults, generated columns, indexes,
  or constraints;
- nonliteral seed expressions such as `RAND(1 + 0)`, `RAND(column)`,
  `RAND(CAST(1 AS SIGNED))`, scalar subqueries, parameters, or functions;
- string, decimal, floating-point, hexadecimal, bit, date, time, or binary seed
  conversion and the warnings those broader conversions require;
- integer seed literals outside the `uint64_t` parsing envelope;
- protocol-grade approximate numeric metadata;
- replication warnings or binlog-format semantics.

## Grammar

MyLite keeps the existing independently authored grammar shape and changes the
one-argument node from an unsupported marker to an admitted seeded function
node:

```lemon
expression(A) ::= RAND(T) LPAREN RPAREN(R).
expression(A) ::= RAND(T) LPAREN expression(B) RPAREN(R).
expression(A) ::= RAND(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R).
identifier(A) ::= RAND(T).
```

The zero-argument production builds `MYLITE_SQL_AST_RAND_FUNCTION`. The
one-argument production builds `MYLITE_SQL_AST_RAND_SEED_FUNCTION` with the seed
expression as its only child. The two-or-more-argument production builds
`MYLITE_SQL_AST_RAND_ARGUMENT_COUNT_ERROR` so execution returns MySQL's native
`1582 / 42000` diagnostic.

## Parser and AST

`RAND` remains a keyword token for function calls while also remaining usable as
an identifier where the current grammar permits identifiers. Parser spans remain
authoritative for default result labels, so labels such as `RAND(1)` and
`RAND ((1))` are preserved from the original SQL text.

The seeded node is admitted only where the current zero-argument `RAND()` scalar
node is admitted: no-source scalar projection, `FROM DUAL`, and `DO`
expression execution. Descriptor row-scalar planning continues to reject it.

## Runtime Behavior

Evaluation is MyLite-owned:

1. Validate the AST shape as either zero-argument `RAND()` or one-argument
   `RAND(seed)`.
2. For zero-argument `RAND()`, keep the current entropy path through
   `sqlite3_randomness()`.
3. For seeded calls, unwrap parentheses around the seed and convert only the
   admitted literal forms to a 32-bit seed value.
4. Initialize the internal two-word random state using the seed value and the
   MyLite-owned recurrence verified against MySQL 8.4.9 observations.
5. Advance the state once and divide the first state word by the recurrence
   modulus to produce the visible `double` value.
6. Format the value through the existing scalar double formatter.

Each supported seeded call is deterministic and independent in this no-source
slice. `RAND(1), RAND(1)` returns the same first seeded value twice. This matches
the observed MySQL behavior for top-level constant seeded calls without a table
source. MyLite does not yet claim the statement-prepared seeded sequence
behavior required for table-backed evaluation.

Successful seeded evaluation emits no warnings. It does not update the statement
context timestamp, row-count state, catalog generation, or storage metadata
beyond existing `SELECT` and `DO` statement conventions.

## Diagnostics

Supported diagnostics:

- `RAND(1, 2)` and wider argument lists: MySQL-compatible native function
  parameter-count error `1582 / 42000`;
- unsupported seed expression or literal form:
  `RAND(seed) supports only integer, boolean, and NULL seed literals`;
- seed integer outside the admitted MyLite parsing envelope:
  `RAND(seed) integer literal is out of range`;
- table-backed or nested contexts: existing scalar-expression unsupported
  diagnostics, updated to include limited seeded `RAND`;
- allocation or formatting failures: existing `MYLITE_NOMEM` or runtime error
  paths;
- public API misuse: unchanged existing `mylite_execute()` behavior.

## Architecture Boundary

- Public API: unchanged; callers use `mylite_execute()` and existing result
  accessors.
- Statement context: unchanged; seeded no-source calls do not need persistent
  per-statement random state.
- Lexer/parser/AST: own the MySQL syntax recognition and the explicit seeded
  AST node.
- Analyzer/planner: unchanged descriptor planning; seeded `RAND` is admitted
  only by scalar-expression admission paths that already admit `RAND()`.
- Runtime/result builder: converts admitted seed literals, evaluates one
  scalar value per call, and formats the result.
- Catalog/storage/VFS: untouched. No descriptors, physical rows, `.mylite`
  preamble bytes, or SQLite schema text are mutated.
- SQLite: zero-argument calls continue to use public `sqlite3_randomness()`.
  Seeded calls use MyLite-owned arithmetic and do not require a SQLite fork
  patch.

## Performance

Seeded no-source evaluation is constant-time and allocation-free apart from the
existing result formatting path. It does not materialize rows, inspect SQLite
metadata, or invoke SQLite SQL. Table-backed random ordering remains deferred
because implementing it correctly and efficiently needs the broader row
expression planner rather than ad hoc MyLite-side materialization.

## Tests

Add MySQL-runtime expectations for:

- MySQL 8.4.9 version guard;
- exact `RAND(0)`, `RAND(1)`, `RAND(2)`, `RAND(3)`, `RAND(NULL)`,
  `RAND(TRUE)`, `RAND(FALSE)`, and `RAND(-1)` values;
- 32-bit wrapping for `4294967295`, `4294967296`, and `4294967297`;
- labels for seeded calls and `FROM DUAL`;
- `DO RAND(seed)` row-count and warning behavior;
- mixed seeded and unseeded calls;
- MySQL acceptance of table-backed seeded calls that MyLite intentionally
  defers;
- native error for two-or-more arguments.

Add C tests for:

- parser AST shapes for one-argument seeded calls and existing arity errors;
- runtime exact seeded values for the supported literal forms;
- no-source, `FROM DUAL`, and `DO` result metadata, row-count, warning count,
  and no-result-set behavior;
- deterministic unsupported diagnostics for unsupported seed forms;
- table-backed seeded calls remain rejected without row materialization;
- no catalog generation or `.mylite` preamble mutation for supported calls.

## Compatibility Documentation

Update `COMPATIBILITY.md` and
`docs/compatibility/functions-numeric-math.md` with limited wording. Mention
literal seeded no-source/`DUAL`/`DO` support, but do not claim table-backed
evaluation, `ORDER BY RAND()`, random integer expressions, expression metadata,
or replication semantics.
