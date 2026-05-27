# Baseline Version Function

## Status

This feature specifies a narrow scalar system-function slice for `VERSION()`.
It builds on `mylite_execute()`, statement context, parser scaffolding,
public `mylite_version()`, and the existing scalar session-select execution
path used by `DATABASE()`, `SCHEMA()`, `USER()`, and `CURRENT_USER`.

The original baseline returned MyLite's own engine version string. The later
`baseline-mysql-server-version-identity` slice supersedes only that value:
SQL-visible `VERSION()` now returns the fixed MySQL 8.4.9 compatibility
version string, while the public C API `mylite_version()` continues to return
MyLite's library version. This still does not claim wire-level MySQL
server-version negotiation, protocol handshake behavior, or server status
variable completeness.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- Baseline current database function:
  `docs/specs/baseline-current-database-function/specs.md`
- Baseline current user identity:
  `docs/specs/baseline-current-user-identity/specs.md`
- MySQL lexer:
  `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- MySQL 8.4 Reference Manual, information functions:
  https://dev.mysql.com/doc/refman/8.4/en/information-functions.html
- MySQL 8.4 Reference Manual, server system variables:
  https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against `SELECT VERSION()` returning `8.4.9`:

- `SELECT VERSION(), @@warning_count` returns one row with `8.4.9` and warning
  count `0` in the local expectation-test container.
- The returned string may include a suffix in other MySQL distributions.
- Default result column names are the source expression text, such as
  `VERSION()`, `version()`, `Version()`, `VERSION ()`, and `(VERSION())`.
- `SELECT VERSION() FROM DUAL` returns one row.
- `VERSION(1)`, `VERSION(NULL)`, `VERSION('x')`, and `VERSION(1, 2)` fail
  with error `1582`, SQLSTATE `42000`, reporting an incorrect parameter count
  for native function `VERSION`.
- Bare `VERSION` is not an information function; `SELECT VERSION` fails with
  error `1054`, SQLSTATE `42S22`.
- MySQL accepts wider scalar forms such as `LIMIT`; those are deliberately
  outside this MyLite slice.

## Scope

The implementation must add:

- parser and AST support for zero-argument `VERSION()`;
- execution of one-row scalar `SELECT VERSION()` and the same form with
  `FROM DUAL`;
- support for mixing `VERSION()` with the existing supported scalar session
  functions in the same select list;
- result column names copied from the selected expression source span;
- one result row containing the SQL-visible MySQL 8.4.9 compatibility version;
- MySQL-compatible `1582` diagnostics for parsed `VERSION(...)` calls with a
  nonzero argument count;
- deterministic diagnostics for bare `VERSION` and unsupported scalar-select
  shapes;
- tests and a MySQL 8.4.9 expectation artifact for supported MySQL behavior
  and deliberately rejected wider forms.

## Non-Goals

This feature must not implement:

- protocol handshake version reporting or configurable server-version identity;
- protocol handshake version reporting;
- `@@version`, version-related system variables, status variables, or
  `SHOW VARIABLES`;
- aliases, `AS`, expression labels beyond default source-expression text, or
  protocol-grade metadata;
- table-backed evaluation such as `SELECT VERSION() FROM table`;
- `WHERE`, `ORDER BY`, `LIMIT`, grouping, joins, subqueries, CTEs, or locking
  clauses on scalar version selects;
- arbitrary SQLite pass-through, SQLite function registration, or SQLite fork
  patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public validation,
  result-handle ownership, and failure cleanup.
- Statement context owns statement-boundary reset, diagnostics, warning count,
  and existing row-result conventions.
- Lexer/parser/AST own admission of `VERSION()` and its source span. They stay
  independent of runtime, catalog, storage, and SQLite.
- Analyzer/planner code recognizes one-row scalar selects with no table source
  or `FROM DUAL`, including mixes with existing supported scalar session
  functions.
- Runtime execution returns the SQL-visible MySQL compatibility version string.
  It does not read or mutate catalog descriptors, physical SQLite schema,
  catalog generation, descriptor caches, session selected-schema state,
  identity state, or `sqlite_schema_generation`.
- The result builder owns one-row text result construction. `VERSION()` always
  returns non-`NULL` text in this slice.
- Storage/VFS and SQLite are not involved beyond the already-open handle. The
  `.mylite` preamble and shifted SQLite payload are untouched.

## Supported SQL Grammar

Supported subset:

```sql
SELECT VERSION()
SELECT VERSION() FROM DUAL
SELECT VERSION(), DATABASE(), USER()
```

Whitespace between `VERSION` and parentheses is accepted because MySQL accepts
it and the lexer emits separate function-name and parenthesis tokens.

Parentheses around the expression are accepted through the existing
parenthesized-expression node:

```sql
SELECT (VERSION())
```

MyLite Lemon-syntax grammar snippet:

```lemon
expression ::= version_function.
expression ::= version_argument_count_error.

version_function ::= VERSION LPAREN RPAREN.

version_argument_count_error ::= VERSION LPAREN function_argument_list RPAREN.
function_argument_list ::= expression.
function_argument_list ::= function_argument_list COMMA expression.
```

`VERSION` remains usable as an ordinary unquoted identifier in identifier
positions where the parser already accepts nonreserved keywords. Bare
`VERSION` is not an admitted version-function form.

The `version_argument_count_error` branch is admitted only to preserve MySQL's
native-function parameter-count diagnostic. It must not evaluate argument
expressions, expose a general function-call API, or make unsupported scalar
expressions successful. Argument spellings that the current expression grammar
does not tokenize or parse may still fail earlier with the existing syntax
diagnostic until those expression forms are implemented for a real feature.

## Runtime Semantics

`VERSION()` returns the SQL-visible MySQL 8.4.9 compatibility string as text.
This is intentionally separate from `mylite_version()`, which remains the
public MyLite library version.

Successful scalar version selects:

- return one result row;
- return one result column per select item;
- use the source expression text as the column name;
- use `affected_rows == 0` under the existing MyLite row-result convention;
- use `warning_count == 0`;
- do not mutate session state, catalog state, physical SQLite schema, or
  storage.

`USE`, `CREATE DATABASE`, and `DROP DATABASE` do not change `VERSION()`.
Multiple handles return the same build version string.

## Diagnostics

The supported `VERSION()` function call does not produce warnings.

Parsed `VERSION(...)` calls with one or more arguments fail with MySQL error
`1582`, SQLSTATE `42000`, and a message containing
`Incorrect parameter count in the call to native function 'VERSION'`. This
diagnostic is raised before argument evaluation; `VERSION(NULL)` and
`VERSION(1, 2)` therefore fail the same way as `VERSION(1)`.

Unsupported syntax not admitted by the grammar fails with the existing parse
error, MySQL error `1064`, SQLSTATE `42000`.

Bare `VERSION` is outside this slice. It may fail through the existing
unsupported-table-select diagnostic rather than MySQL's exact unknown-column
diagnostic until general scalar name resolution exists.

Unsupported scalar-select shapes may fail either at parse time or with the
existing unsupported-statement diagnostic class, depending on whether the
current MyLite grammar admits the wider form for another feature. Examples
include table-backed version evaluation, aliases, clauses, and mixed
unsupported expressions.

Allocation failures return `MYLITE_NOMEM` and set the existing out-of-memory
diagnostic. Public API misuse behavior is unchanged.

## SQLite And Storage Handling

No generated SQLite SQL is required. The value is a process build constant,
not physical storage state. This feature must not create tables, register
SQLite functions, use SQLite callbacks, change VFS behavior, or patch SQLite.

## Tests

Fast C tests must cover:

- `VERSION()` returning the SQL-visible MySQL 8.4.9 compatibility string;
- `FROM DUAL` returning the same value;
- mixed supported scalar version, current database, and current identity
  function selects;
- default column names preserving source-expression text, including lower-case
  spelling, whitespace before parentheses, and surrounding parentheses;
- version values remaining stable across `USE`, `CREATE DATABASE`, and
  `DROP DATABASE`;
- close/reopen and independent handle behavior;
- `VERSION(1)`, `VERSION(NULL)`, and `VERSION(1, 2)` returning the exact
  native-function parameter-count error;
- unsupported syntax and admitted unsupported scalar-select shapes failing
  deterministically;
- result row count, affected rows, warning count, and non-`NULL` text
  representation.

The MySQL expectation script must verify the supported MySQL result shape,
column names, zero warning count, argument-count errors, bare-name error, and
wider forms against MySQL 8.4.9. A missing MySQL 8.4.9 runtime blocks
implementation.
