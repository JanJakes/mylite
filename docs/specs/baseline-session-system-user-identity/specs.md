# Baseline Session/System User Identity

## Status

This feature specifies the narrow identity-alias slice for `SESSION_USER()`
and `SYSTEM_USER()`. It builds on `mylite_execute()`, statement context,
parser scaffolding, the scalar session-select execution path, and the existing
`USER()` / `CURRENT_USER` identity implementation.

The feature is intentionally not an authentication, account, privilege, stored
function, or general function-resolution implementation. It exposes MySQL's
documented `USER()` aliases through one-row scalar `SELECT` statements without
a table source, plus the same expressions with `FROM DUAL`.

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
- Baseline version and row-count functions:
  `docs/specs/baseline-version-function/specs.md`,
  `docs/specs/baseline-row-count-function/specs.md`
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

Observed against the local `mysql:8.4.9` runtime using TCP:

- `SESSION_USER()` and `SYSTEM_USER()` return the same value as `USER()`.
- The local expectation-test container reports all three values as
  `root@127.0.0.1`; MyLite keeps using its embedded `root@%` identity value.
- `SELECT SESSION_USER(), SYSTEM_USER(), @@warning_count` returns one row and
  warning count `0`.
- Function names are case-insensitive, and default result column names preserve
  the source expression spelling, such as `SESSION_USER()`,
  `session_user()`, and `(SYSTEM_USER())`.
- Comments inside the empty argument list are accepted, such as
  `SESSION_USER(/* inside */)` and `SYSTEM_USER(/* inside */)`.
- `SELECT SESSION_USER() FROM DUAL` and `SELECT SYSTEM_USER() FROM DUAL`
  return one row.
- Bare `SESSION_USER` and bare `SYSTEM_USER` are ordinary column references in
  expression context and fail as unknown columns when no such columns exist.
- `SESSION_USER(1)` and `SYSTEM_USER(1)` fail with syntax error `1064`,
  SQLSTATE `42000`.
- With default SQL mode, `SESSION_USER` and `SYSTEM_USER` are
  whitespace-sensitive function names. `SESSION_USER()` and `SYSTEM_USER()`
  are native function calls, but `SESSION_USER ()`, `SYSTEM_USER ()`, and
  comment-separated forms such as `SESSION_USER/**/()` are resolved as
  stored-function invocations. Without a selected database they fail with
  `1046` (`3D000`); with a selected database they fail with `1630` (`42000`)
  when no stored function exists.
- MySQL accepts wider scalar forms such as `LIMIT` and table-backed
  evaluation; those remain outside this MyLite slice.

## Scope

The implementation must add:

- lexer, parser, and AST support for no-whitespace `SESSION_USER()` and
  `SYSTEM_USER()` expression nodes;
- execution of one-row scalar `SELECT` statements whose select items are
  supported session scalar functions from this slice and previous scalar
  baseline slices;
- support for the same scalar selects with `FROM DUAL`;
- result column names copied from the selected expression source span;
- one result row containing:
  - `SESSION_USER()` as `session.client_user_identity`;
  - `SYSTEM_USER()` as `session.client_user_identity`;
  - existing `USER()`, `CURRENT_USER`, `DATABASE()`, `VERSION()`, and
    `ROW_COUNT()` values when mixed into the same scalar select;
- MyLite's current embedded client identity value, `root@%`;
- deterministic diagnostics for unsupported whitespace-sensitive spellings,
  unsupported arguments, bare names, and unsupported expression mixtures;
- tests and a MySQL 8.4.9 expectation artifact for supported behavior and
  deliberately rejected wider forms.

## Non-Goals

This feature must not implement:

- authentication, accounts, roles, grants, passwords, privilege checks, or host
  matching;
- a distinction between `USER()`, `SESSION_USER()`, and `SYSTEM_USER()` beyond
  returning the existing `session.client_user_identity`;
- `CURRENT_ROLE()`, account categories, or the `SYSTEM_USER` privilege;
- `IGNORE_SPACE` SQL mode or general MySQL function-name resolution;
- stored functions, loadable functions, routine namespaces, or `schema.func()`
  invocation;
- user identity in DDL definers, default expressions, stored routines, views,
  triggers, events, account-management statements, or replication semantics;
- aliases, `AS`, expression labels beyond default source-expression text, or
  protocol-grade metadata;
- table-backed evaluation such as `SELECT SESSION_USER() FROM table`;
- `WHERE`, `ORDER BY`, `LIMIT`, grouping, joins, subqueries, CTEs, or locking
  clauses on scalar identity selects;
- arbitrary SQLite pass-through, SQLite function registration, or SQLite fork
  patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public validation,
  result-handle ownership, statement-boundary row-count updates, and failure
  cleanup.
- Statement context owns per-statement reset, diagnostics, warning count, and
  existing row-result conventions.
- Lexer/parser/AST own admission of the supported no-whitespace identity-alias
  syntaxes and their source spans. They stay independent of runtime, catalog,
  storage, and SQLite.
- Analyzer/planner code recognizes one-row scalar session selects with no
  table source or `FROM DUAL`, including mixes of previous scalar baseline
  functions.
- Runtime execution reads only connection-local session state. It does not
  query or mutate catalog descriptors, physical SQLite schema, catalog
  generation, descriptor caches, selected-schema state, or
  `sqlite_schema_generation`.
- The result builder owns one-row text result construction. These aliases
  always return non-`NULL` text in this slice.
- Storage/VFS and SQLite are not involved beyond the already-open handle. The
  `.mylite` preamble and shifted SQLite payload are untouched.

## Supported SQL Grammar

Supported subset:

```sql
SELECT SESSION_USER()
SELECT SYSTEM_USER()
SELECT SESSION_USER(), SYSTEM_USER(), USER()
SELECT SESSION_USER() FROM DUAL
SELECT SYSTEM_USER() FROM DUAL
```

Whitespace or comments between `SESSION_USER` or `SYSTEM_USER` and `(` are not
admitted. MySQL's default parser treats those forms as stored-function
resolution, and stored-function namespaces are outside this slice. MyLite
therefore rejects those forms deterministically rather than pretending to
resolve a routine.

Parentheses around the expression are accepted through the existing
parenthesized-expression node:

```sql
SELECT (SESSION_USER()), (SYSTEM_USER())
```

MyLite Lemon-syntax grammar snippet:

```lemon
expression ::= current_identity_alias_function.

current_identity_alias_function ::= SESSION_USER LPAREN RPAREN.
current_identity_alias_function ::= SYSTEM_USER LPAREN RPAREN.
```

`SESSION_USER` and `SYSTEM_USER` remain usable as ordinary unquoted
identifiers in identifier positions where the parser admits nonreserved
function names. Bare `SESSION_USER` and bare `SYSTEM_USER` are not admitted
identity-alias function forms.

The parser must reject argument forms such as `SESSION_USER(1)` and
`SYSTEM_USER(1)` as syntax errors for this slice.

## Runtime Semantics

`SESSION_USER()` and `SYSTEM_USER()` return the connection's
`session.client_user_identity` as text.

The session field is initialized to `root@%` by the runtime-handle slice. That
value is a MyLite embedded compatibility decision until authentication and
account management exist. It follows MySQL's documented `user@host` shape, but
it is not derived from a client connection, grant table, or host matcher.

Successful scalar identity-alias selects:

- return one result row;
- return one result column per select item;
- use the source expression text as the column name;
- use `affected_rows == 0` under the existing MyLite row-result convention;
- use `warning_count == 0`;
- update `ROW_COUNT()` state to `-1` because they are result-set statements;
- do not mutate session identity, selected schema, catalog state, physical
  SQLite schema, or storage.

`USE`, `CREATE DATABASE`, and `DROP DATABASE` do not change the alias values.
Multiple handles each expose their own session identity state. The current
implementation initializes those states identically.

## Diagnostics

The supported identity-alias function calls do not produce warnings.

Unsupported syntax not admitted by the grammar fails with the existing parse
error, MySQL error `1064`, SQLSTATE `42000`.

Whitespace-sensitive stored-function spellings, such as `SESSION_USER ()` and
`SYSTEM_USER ()`, are outside this slice. MyLite may reject them with the
existing syntax diagnostic instead of emulating MySQL stored-function
resolution errors.

Bare `SESSION_USER` and bare `SYSTEM_USER` are outside this slice. They may
fail through the existing unsupported-table-select diagnostic rather than
MySQL's exact unknown-column diagnostic until general scalar name resolution
exists.

Unsupported scalar-select shapes may fail either at parse time or with the
existing unsupported-statement diagnostic class, depending on whether the
current MyLite grammar admits the wider form for another feature. Examples
include table-backed alias evaluation, aliases, clauses, and mixed unsupported
expressions.

Allocation failures return `MYLITE_NOMEM` and set the existing out-of-memory
diagnostic. Public API misuse behavior is unchanged.

## SQLite And Storage Handling

No generated SQLite SQL is required. The value is connection-local runtime
state, not physical storage state. This feature must not create tables,
register SQLite functions, use SQLite callbacks, change VFS behavior, or patch
SQLite.

## Tests

Fast C tests must cover:

- `SESSION_USER()` and `SYSTEM_USER()` returning `root@%`;
- mixed supported scalar identity aliases with `USER()`, `CURRENT_USER`,
  `DATABASE()`, `VERSION()`, and `ROW_COUNT()`;
- `FROM DUAL` returning the same identity values;
- default column names preserving source-expression text, including lower-case
  spelling, comments inside the empty argument list, and surrounding
  parentheses;
- rejection of whitespace-sensitive `SESSION_USER ()`, `SYSTEM_USER ()`, and
  comment-separated forms;
- identity values remaining stable across `USE`, `CREATE DATABASE`, and
  `DROP DATABASE`;
- close/reopen preserving the initialized `root@%` identity behavior;
- two handles against one file exposing independent but equal initialized
  identity values;
- unsupported syntax and admitted unsupported scalar-select shapes failing
  deterministically;
- result row count, affected rows, warning count, non-`NULL` text
  representation, and `ROW_COUNT()` transition behavior.

The MySQL expectation script must verify the supported result shape, column
names, zero warning count, syntax errors, whitespace-sensitive stored-function
behavior, bare-name errors, and deliberately deferred wider forms against
MySQL 8.4.9. A missing MySQL 8.4.9 runtime blocks implementation.
