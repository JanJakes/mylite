# Baseline Last Insert ID Expression

## Status

This feature extends the existing zero-argument `LAST_INSERT_ID()` scalar
function with the narrow state-setting `LAST_INSERT_ID(expr)` form for
literal scalar use. It builds on MyLite's parser scaffold, scalar `SELECT` and
`DO` execution path, connection-local session state, and existing
auto-increment insert state.

Table-backed row-by-row expression side effects and the sequence-emulation
idiom `UPDATE sequence SET id = LAST_INSERT_ID(id+1)` are covered by the later
`baseline-last-insert-id-row-expression` slice.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- Existing last-insert-id scalar function:
  `docs/specs/baseline-last-insert-id-function/specs.md`
- Baseline auto-increment lifecycle:
  `docs/specs/baseline-auto-increment-lifecycle/specs.md`
- MySQL 8.4 Reference Manual, information functions:
  https://dev.mysql.com/doc/refman/8.4/en/information-functions.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against a local MySQL 8.4.9 server:

- `SELECT LAST_INSERT_ID(), LAST_INSERT_ID(7), LAST_INSERT_ID();` returns
  `0`, `7`, `7` on a fresh connection, showing left-to-right select-list
  evaluation for this simple scalar case.
- `LAST_INSERT_ID(NULL)` returns SQL `NULL`; a later `LAST_INSERT_ID()`
  returns `0`.
- `LAST_INSERT_ID(-1)` returns and stores `18446744073709551615`.
- `LAST_INSERT_ID(0)`, `LAST_INSERT_ID(TRUE)`, and
  `LAST_INSERT_ID(FALSE)` return and store `0`, `1`, and `0`.
- Positive unsigned boundary values through `18446744073709551615` are
  accepted and stored.
- `DO LAST_INSERT_ID(42); SELECT ROW_COUNT(), LAST_INSERT_ID(),
  @@warning_count;` returns `0`, `42`, `0`.
- `SELECT LAST_INSERT_ID(id), LAST_INSERT_ID() FROM t ORDER BY id` evaluates
  per input row and leaves the session value from the last row.
- `LAST_INSERT_ID(1, 2)` fails with error `1582`, SQLSTATE `42000`.
- String and too-large decimal forms are accepted by MySQL through broader
  numeric conversion rules with warning `1292`; those conversion paths are not
  admitted by this slice.

The official information-function documentation states that the no-argument
form returns the connection-local auto-increment value, and that the argument
form returns the argument value and remembers it for the next no-argument call.
It also documents sequence emulation through `UPDATE`, which is covered by a
later row-expression slice.

## Scope

The implementation adds:

- parser and AST support for `LAST_INSERT_ID(expr)` and deterministic native
  parameter-count errors for `LAST_INSERT_ID(expr, ...)`;
- execution in scalar `SELECT` with no source or `FROM DUAL`;
- execution in `DO` expressions;
- left-to-right evaluation in supported scalar select lists and `DO`
  expression lists;
- admitted arguments limited to decimal integer literals with optional unary
  sign, `TRUE`, `FALSE`, and `NULL`, including parenthesized forms;
- unsigned 64-bit positive integer storage for values from `0` through
  `18446744073709551615`;
- MySQL-compatible two's-complement storage for supported negative signed
  literals from `-1` through `-9223372036854775808`;
- `NULL` returning SQL `NULL` while setting the session value to `0`;
- no warnings for supported in-range values;
- no result rows for successful `DO`, using the existing non-query result
  convention with `affected_rows == 0`;
- no public API, catalog, storage, VFS, SQLite schema, or file-format changes.

## Non-Goals

This feature must not implement:

- table-backed `SELECT LAST_INSERT_ID(column)` or row-by-row side effects,
  which are covered by `baseline-last-insert-id-row-expression`;
- use inside `INSERT`, `REPLACE`, defaults, generated columns, checks,
  triggers, or stored routines;
- arithmetic, column, parameter, subquery, string, decimal, float, hex, bit,
  temporal, JSON, function, cast, or general expression arguments;
- MySQL warning-producing conversion for unsupported string or too-large
  numeric argument forms;
- protocol OK-packet insert-id metadata, `mysql_insert_id()` C API state, or
  client capability behavior;
- persistent storage of the manual last-insert-id value;
- SQLite function registration, arbitrary SQLite pass-through, or SQLite fork
  patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public validation,
  result-handle ownership, diagnostics, and statement-boundary behavior.
- Statement context owns per-statement reset and warning/error reporting. It
  does not own connection-local last-insert-id state.
- Connection/session state owns `last_insert_id` as a `uint64_t`. Generated
  auto-increment inserts and `LAST_INSERT_ID(expr)` both write that same
  connection-local field.
- Lexer/parser/AST own admission of the one-argument and argument-count-error
  forms. They preserve source spans for result labels and diagnostics.
- Runtime scalar execution owns validating the admitted literal argument
  subset, converting it to the stored unsigned value, returning SQL `NULL` for
  `LAST_INSERT_ID(NULL)`, and formatting non-`NULL` values as unsigned decimal
  text.
- Catalog, descriptor caches, selected schema, physical SQLite schema, storage
  and VFS are not involved. The `.mylite` preamble and shifted SQLite payload
  are untouched except when unrelated SQL statements already write user data.

## Supported SQL Grammar

Supported subset:

```sql
SELECT LAST_INSERT_ID(7)
SELECT LAST_INSERT_ID(NULL), LAST_INSERT_ID()
SELECT LAST_INSERT_ID(-1) FROM DUAL
DO LAST_INSERT_ID(42)
```

MyLite Lemon-syntax grammar snippet:

```lemon
expression ::= LAST_INSERT_ID LPAREN RPAREN.
expression ::= LAST_INSERT_ID LPAREN expression RPAREN.
expression ::= LAST_INSERT_ID LPAREN expression COMMA function_argument_list RPAREN.
```

The first form creates the existing read-only AST node. The second creates a
distinct state-setting AST node. The third creates an argument-count-error AST
node that runtime maps to MySQL error `1582`, SQLSTATE `42000`.

Bare `LAST_INSERT_ID` remains an identifier-compatible token in identifier
positions and is not a function call.

## Argument Conversion

The runtime unwraps parentheses around the argument.

`NULL`:

- returns SQL `NULL`;
- stores `0` in session `last_insert_id`;
- emits no warnings.

Boolean literals:

- `TRUE` stores and returns `1`;
- `FALSE` stores and returns `0`;
- unary signs on booleans are not admitted.

Unsigned decimal integer literals:

- values from `0` through `18446744073709551615` are stored and returned as
  unsigned decimal text;
- values outside `uint64_t` are rejected with a deterministic MyLite
  unsupported diagnostic and leave the previous session value unchanged.

Signed decimal integer literals:

- unary `+` uses the unsigned decimal path;
- unary `-0` stores and returns `0`;
- unary `-N` for `1 <= N <= 9223372036854775808` stores `2^64 - N` and
  returns that unsigned decimal value;
- negative magnitudes larger than `9223372036854775808` are rejected with a
  deterministic MyLite unsupported diagnostic and leave the previous session
  value unchanged.

Unsupported argument forms fail before mutating session state.

## Runtime Semantics

Successful scalar `SELECT LAST_INSERT_ID(expr)`:

- returns one row and one column for the expression;
- uses the source expression or alias as the column label, preserving existing
  scalar projection conventions;
- evaluates select-list items left to right for this MyLite-owned scalar path;
- updates `ROW_COUNT()` to `-1` after the result-set statement completes;
- records `warning_count == 0` for supported in-range values.

Successful `DO LAST_INSERT_ID(expr)`:

- evaluates expression-list items left to right;
- returns no rows and no columns;
- reports `affected_rows == 0`;
- leaves `ROW_COUNT()` as `0` for the following statement;
- records `warning_count == 0` for supported in-range values.

Generated auto-increment insert behavior remains authoritative for insert
statements. A later successful generated insert overwrites a manual
`LAST_INSERT_ID(expr)` value with the first generated id. Explicit non-magic
auto-increment inserts and non-auto-increment DML leave the manual value
unchanged.

Independent handles have independent session values. Close/reopen starts a
new connection-local value; the manual value is not persisted in the file.

## Diagnostics

Supported one-argument calls do not produce warnings.

Diagnostics:

- `LAST_INSERT_ID(expr, ...)`: MySQL-compatible native function parameter
  count error `1582`, SQLSTATE `42000`;
- unsupported one-argument expression type: MyLite unsupported-feature error;
- integer literal outside this slice's admitted range: MyLite
  unsupported-feature error;
- bare `LAST_INSERT_ID`: existing unknown-column or unsupported scalar path;
- table-backed use or DML expression use: existing unsupported descriptor or
  expression diagnostics;
- allocation failures: `MYLITE_NOMEM` with the existing out-of-memory
  diagnostic;
- public API misuse: unchanged.

All one-argument conversion failures must leave the previous session
`last_insert_id` value unchanged.

## SQLite And Storage Handling

No generated SQLite SQL is required for `LAST_INSERT_ID(expr)`. The value is
connection-local MyLite session state. The implementation must not register a
SQLite function, inspect SQLite metadata, create catalog rows, mutate
descriptor versions, or patch SQLite.

## Tests

Fast C tests must cover:

- parser AST nodes and source spans for zero-argument, one-argument, and
  multi-argument forms;
- scalar `SELECT` with integer, signed integer, unsigned boundary, boolean,
  `NULL`, `FROM DUAL`, aliases, and parenthesized arguments;
- left-to-right scalar select evaluation and following zero-argument reads;
- `DO LAST_INSERT_ID(expr)` and following `ROW_COUNT()`, `LAST_INSERT_ID()`,
  and warning count;
- unsupported strings, unsupported arithmetic/column/subquery/function
  arguments, out-of-range positive and negative integer literals, and
  multiple arguments;
- failure preserving the previous session value;
- manual value overwritten by a later generated auto-increment insert, while
  explicit inserts and non-auto DML preserve it;
- close/reopen and independent-handle state;
- file preamble preservation and zero-initialized cleanup.

MySQL expectation artifacts must verify the supported scalar and `DO` forms,
argument-count error, table-backed MySQL behavior documented as deferred, and
representative unsupported conversion forms that MyLite intentionally does not
admit yet.
