# Baseline Current Role Function

## Status

This feature specifies a narrow scalar information-function slice for
`CURRENT_ROLE()`. It builds on `mylite_execute()`, statement context, parser
scaffolding, the existing scalar session-select execution path, and MyLite's
embedded current-user identity surface.

MyLite does not yet implement accounts, roles, role grants, default roles, or
`SET ROLE`. For this baseline slice, `CURRENT_ROLE()` therefore returns the
MySQL no-active-role value `NONE`.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- Baseline current user identity:
  `docs/specs/baseline-current-user-identity/specs.md`
- Baseline session/system user identity:
  `docs/specs/baseline-session-system-user-identity/specs.md`
- MySQL lexer: `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold: `docs/specs/mysql-parser-scaffold/specs.md`
- MySQL 8.4 Reference Manual, information functions:
  https://dev.mysql.com/doc/refman/8.4/en/information-functions.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against the local `mysql:8.4.9` runtime using the root test account,
which has no active roles:

- `SELECT CURRENT_ROLE()` returns `NONE`.
- `current_role()`, `CURRENT_ROLE ()`, `(CURRENT_ROLE())`, and
  `CURRENT_ROLE() FROM DUAL` return `NONE`.
- Result column names are the source expression text, such as
  `CURRENT_ROLE()`, `current_role()`, `CURRENT_ROLE ()`, and
  `(CURRENT_ROLE())`.
- Bare `CURRENT_ROLE` is not the function; `SELECT CURRENT_ROLE` fails with
  error `1054`, SQLSTATE `42S22`, reporting an unknown column.
- `CURRENT_ROLE(1)`, `CURRENT_ROLE(NULL)`, `CURRENT_ROLE('x')`, and
  `CURRENT_ROLE(1, 2)` fail with error `1582`, SQLSTATE `42000`, reporting an
  incorrect parameter count for native function `CURRENT_ROLE`.
- `CURRENT_ROLE` can be used as an unquoted table and column identifier in the
  probed runtime.
- `SELECT CURRENT_ROLE(), @@warning_count, ROW_COUNT()` after a nondiagnostic
  statement returns `NONE`, warning count `0`, and row count `-1`, then clears
  diagnostics for following diagnostic count reads.
- MySQL accepts wider scalar forms such as `CURRENT_ROLE() LIMIT 1` and
  `CURRENT_ROLE() + 1`; those forms remain outside this MyLite slice.

The official information-function documentation describes `CURRENT_ROLE()` as
returning a comma-separated `utf8mb3` string of active roles, or `NONE` when
there are no active roles.

## Scope

The implementation must add:

- lexer/parser/AST support for zero-argument `CURRENT_ROLE()`;
- execution of one-row scalar `SELECT CURRENT_ROLE()` and the same form with
  `FROM DUAL`;
- support for mixing `CURRENT_ROLE()` with the existing supported scalar
  session functions and system variables in the same select list;
- result column names copied from the selected expression source span;
- one result row containing the non-`NULL` text value `NONE`;
- MySQL-compatible `1582` diagnostics for parsed `CURRENT_ROLE(...)` calls
  with a nonzero argument count;
- deterministic diagnostics for bare `CURRENT_ROLE` and unsupported
  scalar-select shapes;
- tests and a MySQL 8.4.9 expectation artifact for supported MySQL behavior
  and deliberately rejected wider forms.

Supported SQL examples:

```sql
SELECT CURRENT_ROLE()
SELECT current_role() FROM DUAL
SELECT (CURRENT_ROLE())
SELECT CURRENT_ROLE(), CURRENT_USER, USER(), @@warning_count
```

## Non-Goals

This feature must not implement:

- role catalogs, role grants, default roles, active-role state, `SET ROLE`, or
  `SET DEFAULT ROLE`;
- account storage, authentication, privileges, definer semantics, or role graph
  metadata;
- bare `CURRENT_ROLE`;
- table-backed evaluation such as `SELECT CURRENT_ROLE() FROM table`;
- aliases, `WHERE`, `ORDER BY`, `LIMIT`, grouping, joins, subqueries, CTEs, or
  locking clauses on scalar role selects;
- general expression evaluation involving `CURRENT_ROLE()`, string comparison,
  numeric coercion, or warning `1292`;
- protocol-grade character set/collation metadata for the returned string;
- arbitrary SQLite pass-through, SQLite function registration, or SQLite fork
  patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public validation,
  result-handle ownership, and failure cleanup.
- Statement context owns statement-boundary reset, diagnostics, warning count,
  row-count state, and existing row-result conventions.
- Lexer/parser/AST own admission of `CURRENT_ROLE()` and source spans. They
  stay independent of runtime, catalog, storage, and SQLite.
- Runtime execution recognizes one-row scalar selects with no table source or
  `FROM DUAL`, including mixes with existing supported scalar session
  functions and system variables.
- The result builder owns one-row text result construction. `CURRENT_ROLE()`
  always returns non-`NULL` text in this slice.
- Catalog, storage, VFS, and SQLite physical row storage are not involved.
  This feature must not touch `.mylite` preamble bytes or SQLite schema state.

## Supported SQL Grammar

Supported subset:

```sql
SELECT CURRENT_ROLE()
SELECT CURRENT_ROLE() FROM DUAL
SELECT CURRENT_ROLE(), CURRENT_USER, USER()
```

Whitespace between `CURRENT_ROLE` and parentheses is accepted because MySQL
accepts it and the lexer emits separate function-name and parenthesis tokens.

Parentheses around the expression are accepted through the existing
parenthesized-expression node:

```sql
SELECT (CURRENT_ROLE())
```

MyLite Lemon-syntax grammar snippet:

```lemon
expression ::= current_role_function.
expression ::= current_role_argument_count_error.

current_role_function ::= CURRENT_ROLE LPAREN RPAREN.

current_role_argument_count_error ::=
    CURRENT_ROLE LPAREN function_argument_list RPAREN.
function_argument_list ::= expression.
function_argument_list ::= function_argument_list COMMA expression.
```

`CURRENT_ROLE` remains usable as an ordinary unquoted identifier in identifier
positions where the parser already accepts nonreserved keywords. Bare
`CURRENT_ROLE` is not an admitted role-function form.

The `current_role_argument_count_error` branch is admitted only to preserve
MySQL's native-function parameter-count diagnostic. It must not evaluate
argument expressions, expose a general function-call API, or make unsupported
scalar expressions successful. Argument spellings that the current expression
grammar does not tokenize or parse may still fail earlier with the existing
syntax diagnostic until those expression forms are implemented for a real
feature.

## Runtime Semantics

`CURRENT_ROLE()` returns `NONE` as text. This represents the embedded baseline
with no role graph and no active roles.

Successful scalar current-role selects:

- return one result row;
- return one result column per select item;
- use the source expression text as the column name;
- use `affected_rows == 0` under the existing MyLite row-result convention;
- use `warning_count == 0`;
- make following `ROW_COUNT()` return `-1`;
- clear diagnostics like other successful nondiagnostic scalar selects;
- do not mutate session identity, catalog state, physical SQLite schema,
  storage, or `sqlite_schema_generation`.

`CREATE DATABASE`, `USE`, `DROP DATABASE`, table DDL, DML, close/reopen, and
independent handles do not change the value.

## Diagnostics

The supported `CURRENT_ROLE()` function call does not produce warnings.

Parsed `CURRENT_ROLE(...)` calls with one or more arguments fail with MySQL
error `1582`, SQLSTATE `42000`, and a message containing
`Incorrect parameter count in the call to native function 'CURRENT_ROLE'`.
This diagnostic is raised before argument evaluation.

Bare `CURRENT_ROLE` is outside this slice. It may fail through the existing
unsupported-table-select diagnostic rather than MySQL's exact unknown-column
diagnostic until general scalar name resolution exists.

Unsupported syntax not admitted by the grammar fails with the existing parse
error, MySQL error `1064`, SQLSTATE `42000`.

Unsupported scalar-select shapes may fail either at parse time or with the
existing unsupported-statement diagnostic class, depending on whether the
current MyLite grammar admits the wider form for another feature. Examples
include table-backed role evaluation, aliases, clauses, and mixed unsupported
expressions.

Allocation failures return `MYLITE_NOMEM` and set the existing out-of-memory
diagnostic. Public API misuse behavior is unchanged.

## SQLite And Storage Handling

No generated SQLite SQL is required. The value is a fixed MyLite session
identity placeholder, not physical storage state. This feature must not create
tables, register SQLite functions, use SQLite callbacks, read SQLite metadata,
change VFS behavior, or patch SQLite.

File-backed tests must verify that reading `CURRENT_ROLE()` preserves the
MyLite preamble and does not change catalog or SQLite schema generations.

## Tests

Fast C tests must cover:

- `CURRENT_ROLE()` returning `NONE`;
- `FROM DUAL` returning the same value;
- mixed supported scalar role, current user, version, diagnostics-count, and
  row-count selects;
- default column names preserving source-expression text, including lower-case
  spelling, whitespace before parentheses, and surrounding parentheses;
- values remaining stable across `USE`, `CREATE DATABASE`, `DROP DATABASE`,
  close/reopen, and independent handles;
- `CURRENT_ROLE(1)`, `CURRENT_ROLE(NULL)`, `CURRENT_ROLE('x')`, and
  `CURRENT_ROLE(1, 2)` returning the exact native-function parameter-count
  error when parsed;
- bare `CURRENT_ROLE` and unsupported scalar-select shapes failing
  deterministically;
- result row count, affected rows, warning count, `ROW_COUNT()` interaction,
  diagnostics clearing, and non-`NULL` text representation;
- file preamble preservation and unchanged catalog/schema generations.

The MySQL expectation script must verify the supported MySQL result shape,
column names, zero warning count, argument-count errors, bare-name error, and
wider forms against MySQL 8.4.9. A missing MySQL 8.4.9 runtime blocks
implementation.
