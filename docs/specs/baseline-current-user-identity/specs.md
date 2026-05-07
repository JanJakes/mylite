# Baseline Current User Identity

## Status

This feature specifies a narrow runtime identity slice for MyLite scalar
information functions. It adds `USER()`, `CURRENT_USER()`, and bare
`CURRENT_USER` on top of `mylite_execute()`, statement context, parser
scaffolding, the existing connection-local session identity placeholders, and
the scalar current-database execution path.

The feature is intentionally not an authentication, user-account, privilege,
or general scalar-expression implementation. It exposes MyLite's current
embedded identity policy through one-row `SELECT` statements without a table
source, plus the same expressions with `FROM DUAL`.

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
- MySQL lexer:
  `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- MySQL 8.4 Reference Manual, information functions:
  https://dev.mysql.com/doc/refman/8.4/en/information-functions.html
- MySQL 8.4 Reference Manual, function name parsing and resolution:
  https://dev.mysql.com/doc/refman/8.4/en/function-resolution.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against `SELECT VERSION()` returning `8.4.9` using TCP from inside
the local MySQL expectation-test container:

- `SELECT USER(), CURRENT_USER(), CURRENT_USER, @@warning_count` returns one
  row with the client user identity, authenticated current user identity, the
  same authenticated identity, and warning count `0`.
- In the local MySQL 8.4.9 root container used by MyLite expectation tests,
  TCP reports `USER()` as `root@127.0.0.1` and `CURRENT_USER()` /
  `CURRENT_USER` as `root@%`. Socket access reports `root@localhost` for both,
  so TCP is the preferred probe path for proving that the values can differ.
- `CURRENT_USER` is accepted without parentheses. `USER` is not accepted as a
  bare information function; `SELECT USER` fails with error `1054`, SQLSTATE
  `42S22`.
- `SELECT USER() FROM DUAL` and `SELECT CURRENT_USER FROM DUAL` return one
  row.
- Default result column names are the source expression text, such as
  `USER()`, `CURRENT_USER()`, `CURRENT_USER`, `user()`, `current_user`,
  `USER ()`, and `(CURRENT_USER)`.
- `USER(1)` and `CURRENT_USER(1)` fail with syntax error `1064`, SQLSTATE
  `42000`.
- `USE` does not change user identity function values.
- MySQL accepts wider scalar forms such as `LIMIT` and table-backed
  evaluation; those are deliberately outside this MyLite slice.

MySQL documents `SESSION_USER()` and `SYSTEM_USER()` as synonyms for
`USER()`, but MySQL function-name parsing treats those names differently when
whitespace appears before `(`. This slice defers them so the initial identity
path stays small and does not overclaim function-name parsing support.

## Scope

The implementation must add:

- parser and AST support for `USER()`, `CURRENT_USER()`, and bare
  `CURRENT_USER` expression nodes;
- execution of one-row scalar `SELECT` statements whose select items are
  supported session scalar functions from this slice and the existing
  `DATABASE()` / `SCHEMA()` slice;
- support for the same scalar selects with `FROM DUAL`;
- result column names copied from the selected expression source span;
- one result row containing:
  - `USER()` as `session.client_user_identity`;
  - `CURRENT_USER()` and bare `CURRENT_USER` as
    `session.current_user_identity`;
  - existing `DATABASE()` / `SCHEMA()` values when mixed into the same scalar
    select;
- MyLite's current embedded identity value, `root@%`, for both client and
  current user placeholders;
- diagnostics for unsupported function arguments, bare `USER`, and unsupported
  expression mixtures through existing parse or unsupported-statement errors;
- tests and a MySQL 8.4.9 expectation artifact for supported behavior and
  deliberately rejected wider forms.

## Non-Goals

This feature must not implement:

- authentication, accounts, roles, grants, passwords, privilege checks, or host
  matching;
- a distinction between connected client identity and authenticated identity
  beyond using the two existing session fields;
- `SESSION_USER()`, `SYSTEM_USER()`, `CURRENT_ROLE()`, or other user/role
  functions;
- user identity in DDL definers, default expressions, stored routines, views,
  triggers, events, account-management statements, or replication semantics;
- general function calls or a function registry;
- aliases, `AS`, expression labels beyond default source-expression text, or
  protocol-grade metadata;
- table-backed evaluation such as `SELECT USER() FROM table`;
- `WHERE`, `ORDER BY`, `LIMIT`, grouping, joins, subqueries, CTEs, or locking
  clauses on scalar identity selects;
- arbitrary SQLite pass-through, SQLite function registration, or SQLite fork
  patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public validation,
  result-handle ownership, and failure cleanup.
- Statement context owns statement-boundary reset, diagnostics, warning count,
  and existing row-result conventions.
- Lexer/parser/AST own admission of the supported identity syntaxes and their
  source spans. They stay independent of runtime, catalog, storage, and
  SQLite.
- Analyzer/planner code recognizes one-row scalar session selects with no
  table source or `FROM DUAL`, including mixes of current database and current
  user identity functions.
- Runtime execution reads only connection-local session state. It does not
  query or mutate catalog descriptors, physical SQLite schema, catalog
  generation, descriptor caches, or `sqlite_schema_generation`.
- The result builder owns one-row text/`NULL` result construction. Identity
  functions always return non-`NULL` text in this slice.
- Storage/VFS and SQLite are not involved beyond the already-open handle. The
  `.mylite` preamble and shifted SQLite payload are untouched.

## Supported SQL Grammar

Supported subset:

```sql
SELECT USER()
SELECT CURRENT_USER()
SELECT CURRENT_USER
SELECT USER(), CURRENT_USER(), CURRENT_USER
SELECT USER() FROM DUAL
SELECT CURRENT_USER FROM DUAL
```

Whitespace between `USER` or `CURRENT_USER` and parentheses is accepted for
the parenthesized forms because MySQL accepts it for these function names and
the lexer emits separate function-name and parenthesis tokens.

Parentheses around the expression are accepted through the existing
parenthesized-expression node:

```sql
SELECT (USER()), (CURRENT_USER()), (CURRENT_USER)
```

MyLite Lemon-syntax grammar snippet:

```lemon
expression ::= current_identity_function.
expression ::= CURRENT_USER.

current_identity_function ::= USER LPAREN RPAREN.
current_identity_function ::= CURRENT_USER LPAREN RPAREN.
```

`USER` remains usable as an ordinary unquoted identifier in identifier
positions where the parser already accepts nonreserved keywords. Bare `USER`
is not an admitted identity-function form.

The parser must reject argument forms such as `USER(1)` and
`CURRENT_USER(1)` as syntax errors for this slice.

## Runtime Semantics

`USER()` returns the connection's `session.client_user_identity` as text.

`CURRENT_USER()` and bare `CURRENT_USER` return the connection's
`session.current_user_identity` as text.

Both session fields are initialized to `root@%` by the runtime-handle slice.
That value is a MyLite embedded compatibility decision until authentication
and account management exist. It follows MySQL's documented `user@host` shape,
but it is not derived from a client connection, grant table, or host matcher.

Successful scalar identity selects:

- return one result row;
- return one result column per select item;
- use the source expression text as the column name;
- use `affected_rows == 0` under the existing MyLite row-result convention;
- use `warning_count == 0`;
- do not mutate session state, catalog state, physical SQLite schema, or
  storage.

`USE`, `CREATE DATABASE`, and `DROP DATABASE` do not change the identity
values. Multiple handles each expose their own session identity state. The
current implementation initializes those states identically.

## Diagnostics

The supported identity function calls do not produce warnings.

Unsupported syntax not admitted by the grammar fails with the existing parse
error, MySQL error `1064`, SQLSTATE `42000`.

Bare `USER` is outside this slice. It may fail through the existing
unsupported-table-select diagnostic rather than MySQL's exact unknown-column
diagnostic until general scalar name resolution exists.

Unsupported scalar-select shapes may fail either at parse time or with the
existing unsupported-statement diagnostic class, depending on whether the
current MyLite grammar admits the wider form for another feature. Examples
include table-backed identity evaluation, aliases, clauses, and mixed
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

- `USER()`, `CURRENT_USER()`, and bare `CURRENT_USER` returning `root@%`;
- mixed supported scalar identity and current-database function selects;
- `FROM DUAL` returning the same identity values;
- default column names preserving source-expression text, including lower-case
  spelling, whitespace before parentheses, and surrounding parentheses;
- identity values remaining stable across `USE`, `CREATE DATABASE`, and
  `DROP DATABASE`;
- close/reopen preserving the initialized `root@%` identity behavior;
- two handles against one file exposing independent but equal initialized
  identity values;
- unsupported syntax and admitted unsupported scalar-select shapes failing
  deterministically;
- result row count, affected rows, warning count, and non-`NULL` text
  representation.

The MySQL expectation script must verify the supported result shape, column
names, zero warning count, syntax errors, and deliberately deferred wider
forms against MySQL 8.4.9. A missing MySQL 8.4.9 runtime blocks
implementation.
