# Baseline Connection ID Function

## Status

This feature specifies a narrow scalar system-function slice for
`CONNECTION_ID()`. It builds on `mylite_execute()`, statement context, parser
scaffolding, runtime handle session state, and the existing one-row scalar
session-select execution path.

The feature intentionally exposes a MyLite embedded handle identifier, not a
MySQL server thread, protocol connection, process-list entry, or Performance
Schema thread. It does not add `SHOW PROCESSLIST`, `INFORMATION_SCHEMA`
process-list rows, `performance_schema.threads`, `pseudo_thread_id`, or
protocol connection metadata.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- Baseline current database/current user/version/row-count scalar functions:
  `docs/specs/baseline-current-database-function/specs.md`,
  `docs/specs/baseline-current-user-identity/specs.md`,
  `docs/specs/baseline-version-function/specs.md`,
  `docs/specs/baseline-row-count-function/specs.md`
- Baseline session/system user identity:
  `docs/specs/baseline-session-system-user-identity/specs.md`
- MySQL lexer:
  `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- MySQL 8.4 Reference Manual, information functions:
  https://dev.mysql.com/doc/refman/8.4/en/information-functions.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against the local `mysql:8.4.9` runtime using TCP:

- `CONNECTION_ID()` returns a positive integer connection identifier and
  warning count `0`.
- Repeated `CONNECTION_ID()` calls in the same connection return the same
  value.
- Concurrent independent client connections have distinct `CONNECTION_ID()`
  values.
- Function names are case-insensitive. Whitespace before `(` is accepted for
  this function, unlike MySQL's whitespace-sensitive special function names.
- Comments before `(` and comments inside the empty argument list are accepted.
- Default result column names preserve the expression spelling, with MySQL
  normalizing comments in labels.
- `SELECT CONNECTION_ID() FROM DUAL` returns one row.
- Bare `CONNECTION_ID` is an ordinary column reference in expression context
  and fails with error `1054`, SQLSTATE `42S22`, when no such column exists.
- `CONNECTION_ID(1)`, `CONNECTION_ID(NULL)`, `CONNECTION_ID('x')`, and
  `CONNECTION_ID(1, 2)` fail with error `1582`, SQLSTATE `42000`, reporting
  an incorrect parameter count for native function `CONNECTION_ID`.
- When multiple selected scalar expressions have native-function parameter-count
  errors, MySQL reports the first bad expression in select-list order.
- `USE`, DDL, DML, and failed statements do not change the value for the
  connection.
- MySQL accepts wider scalar forms such as aliases, `LIMIT`, and table-backed
  evaluation; those remain outside this MyLite slice.
- MySQL documents that changing the session `pseudo_thread_id` system variable
  changes `CONNECTION_ID()`. MyLite does not implement that variable in this
  slice.

## Scope

The implementation must add:

- parser and AST support for zero-argument `CONNECTION_ID()`;
- parser support for parsed nonzero-argument `CONNECTION_ID(...)` forms that
  return MySQL-compatible native-function parameter-count diagnostics;
- execution of one-row scalar `SELECT CONNECTION_ID()` and the same form with
  `FROM DUAL`;
- support for mixing `CONNECTION_ID()` with the existing supported scalar
  session functions in the same select list;
- result column names copied from the selected expression source span;
- a nonzero connection-local unsigned integer identifier assigned to each
  opened MyLite handle;
- decimal text formatting of that identifier in result rows;
- deterministic diagnostics for unsupported function arguments, bare
  `CONNECTION_ID`, and wider scalar-select shapes;
- tests and a MySQL 8.4.9 expectation artifact for supported behavior and
  deliberately rejected wider forms.

## Non-Goals

This feature must not implement:

- public API additions or ABI changes;
- a MySQL server thread, socket connection, process-list row, protocol
  connection id, or Performance Schema thread id;
- `SHOW PROCESSLIST`, `INFORMATION_SCHEMA.PROCESSLIST`,
  `performance_schema.threads`, `PROCESSLIST_ID`, or kill/explain-for-
  connection behavior;
- `pseudo_thread_id` system-variable reads or writes;
- aliases, `AS`, expression labels beyond default source-expression text, or
  protocol-grade metadata;
- table-backed evaluation such as `SELECT CONNECTION_ID() FROM table`;
- `WHERE`, `ORDER BY`, `LIMIT`, grouping, joins, subqueries, CTEs, or locking
  clauses on scalar connection-id selects;
- arbitrary SQLite pass-through, SQLite function registration, or SQLite fork
  patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public validation,
  result-handle ownership, statement-boundary row-count updates, and failure
  cleanup.
- Runtime handle initialization owns assigning the connection identifier. The
  identifier is connection-local session state, not catalog or storage state.
- Statement context owns per-statement reset, diagnostics, warning count, and
  existing row-result conventions.
- Lexer/parser/AST own admission of `CONNECTION_ID()` and source spans. They
  stay independent of runtime, catalog, storage, and SQLite.
- Analyzer/planner code recognizes one-row scalar session selects with no
  table source or `FROM DUAL`, including mixes with previous scalar baseline
  functions.
- Runtime execution reads only connection-local session state. It does not
  query or mutate catalog descriptors, physical SQLite schema, descriptor
  caches, selected schema, identity state, catalog generation, or
  `sqlite_schema_generation`.
- The result builder owns one-row text result construction. `CONNECTION_ID()`
  always returns non-`NULL` decimal text in this slice.
- Storage/VFS and SQLite are not involved beyond the already-open handle. The
  `.mylite` preamble and shifted SQLite payload are untouched.

## Supported SQL Grammar

Supported subset:

```sql
SELECT CONNECTION_ID()
SELECT CONNECTION_ID() FROM DUAL
SELECT CONNECTION_ID(), DATABASE(), USER(), ROW_COUNT()
```

Whitespace between `CONNECTION_ID` and parentheses is accepted because MySQL
accepts it and the lexer emits separate function-name and parenthesis tokens.
Comments before `(` and inside the empty argument list are accepted by the
existing comment-skipping lexer/parser pipeline.

Parentheses around the expression are accepted through the existing
parenthesized-expression node:

```sql
SELECT (CONNECTION_ID())
```

MyLite Lemon-syntax grammar snippet:

```lemon
expression ::= connection_id_function.
expression ::= connection_id_argument_count_error.

connection_id_function ::= CONNECTION_ID LPAREN RPAREN.

connection_id_argument_count_error ::=
    CONNECTION_ID LPAREN function_argument_list RPAREN.
function_argument_list ::= expression.
function_argument_list ::= function_argument_list COMMA expression.
```

`CONNECTION_ID` remains usable as an ordinary unquoted identifier in
identifier positions where the parser admits nonreserved function names. Bare
`CONNECTION_ID` is not an admitted connection-id function form.

The argument-count-error branch is admitted only to preserve MySQL's
native-function parameter-count diagnostic. It must not evaluate argument
expressions, expose a general function-call API, or make unsupported scalar
expressions successful. Argument spellings that the current expression grammar
does not tokenize or parse may still fail earlier with the existing syntax
diagnostic until those expression forms are implemented for a real feature.

## Runtime Semantics

`CONNECTION_ID()` returns the handle's `session.connection_id` as unsigned
decimal text.

MyLite assigns each opened handle a nonzero process-local identifier. The value
is stable for the lifetime of the handle, is distinct for simultaneously open
handles in the current process, and is not persisted in `.mylite` files.
Closing and reopening a file creates a new handle with a new process-local
identifier.

The value is MyLite-specific until a server protocol layer, process list, and
thread metadata exist. It follows MySQL's nonzero integer shape and stable
per-connection behavior, but it is not the id of a MySQL server thread and is
not connected to `pseudo_thread_id`.

Successful scalar connection-id selects:

- return one result row;
- return one result column per select item;
- use the source expression text as the column name;
- use `affected_rows == 0` under the existing MyLite row-result convention;
- use `warning_count == 0`;
- update `ROW_COUNT()` state to `-1` because they are result-set statements;
- do not mutate session state, selected schema, catalog state, physical SQLite
  schema, or storage.

`USE`, `CREATE DATABASE`, `DROP DATABASE`, DDL, DML, and failed SQL execution
do not change the connection id.

## Diagnostics

The supported `CONNECTION_ID()` function call does not produce warnings.

Parsed `CONNECTION_ID(...)` calls with one or more arguments fail with MySQL
error `1582`, SQLSTATE `42000`, and a message containing
`Incorrect parameter count in the call to native function 'CONNECTION_ID'`.
This diagnostic is raised before argument evaluation.

Unsupported syntax not admitted by the grammar fails with the existing parse
error, MySQL error `1064`, SQLSTATE `42000`.

Bare `CONNECTION_ID` is outside this slice. It may fail through the existing
unsupported-table-select diagnostic rather than MySQL's exact unknown-column
diagnostic until general scalar name resolution exists.

Unsupported scalar-select shapes may fail either at parse time or with the
existing unsupported-statement diagnostic class, depending on whether the
current MyLite grammar admits the wider form for another feature. Examples
include table-backed connection-id evaluation, aliases, clauses, and mixed
unsupported expressions.

Allocation failures return `MYLITE_NOMEM` and set the existing out-of-memory
diagnostic. Public API misuse behavior is unchanged.

## SQLite And Storage Handling

No generated SQLite SQL is required. The value is connection-local runtime
state, not physical storage state. This feature must not create tables,
register SQLite functions, use SQLite callbacks, change VFS behavior, or patch
SQLite.

## Tests

Fast C tests must cover:

- `CONNECTION_ID()` returning a nonzero decimal integer string;
- repeated calls returning the same value for one handle;
- simultaneously open handles returning distinct values;
- close/reopen creating a new handle id while preserving file contents;
- mixed supported scalar connection-id selects with `DATABASE()`, `USER()`,
  `CURRENT_USER`, `SESSION_USER()`, `SYSTEM_USER()`, `VERSION()`, and
  `ROW_COUNT()`;
- `FROM DUAL` returning the same connection id;
- default column names preserving source-expression text, including lower-case
  spelling, whitespace before parentheses, comments, and surrounding
  parentheses;
- connection id stability across `USE`, schema lifecycle, table lifecycle,
  row DML, and failed statements;
- `CONNECTION_ID(1)`, `CONNECTION_ID(NULL)`, `CONNECTION_ID('x')`, and
  `CONNECTION_ID(1, 2)` returning the exact native-function parameter-count
  error;
- unsupported syntax and admitted unsupported scalar-select shapes failing
  deterministically;
- result row count, affected rows, warning count, non-`NULL` text
  representation, and `ROW_COUNT()` transition behavior.

The MySQL expectation script must verify the supported MySQL result shape,
column names, zero warning count, argument-count errors, bare-name error,
connection-local stability, independent connection uniqueness, and wider forms
against MySQL 8.4.9. A missing MySQL 8.4.9 runtime blocks implementation.
