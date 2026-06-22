# Baseline SQL Quote SHOW CREATE System Variable

## Status

This feature specifies the baseline SHOW CREATE quote-control slice for
`@@sql_quote_show_create`.

It builds on the existing `SYSTEM_VARIABLE` lexer/parser token, scalar
`SELECT` execution, diagnostics lifecycle, generic Boolean session-variable
assignment, `SHOW VARIABLES`, and descriptor-driven `SHOW CREATE DATABASE` /
`SHOW CREATE TABLE` rendering. MySQL exposes this as mutable global and
session state that controls whether simple identifiers are backtick quoted in
`SHOW CREATE` output. MyLite implements the embedded compatibility baseline:
session `SET`/readback, fixed global `ON`, and quote-control behavior for the
current structured database and table renderers.

This is not full SHOW CREATE quote-control support. It does not implement
server-global mutation, startup options, persisted variables, Performance
Schema variable tables, or quote-control behavior for pre-rendered
`SHOW CREATE VIEW`, stored routine, user, trigger, or event output.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- Baseline SHOW CREATE TABLE:
  `docs/specs/baseline-show-create-table/specs.md`
- Baseline SHOW CREATE DATABASE:
  `docs/specs/baseline-show-create-database/specs.md`
- MySQL lexer: `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold: `docs/specs/mysql-parser-scaffold/specs.md`
- MySQL 8.4 Reference Manual, server system variables:
  https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html
- MySQL 8.4 Reference Manual, `SHOW CREATE TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/show-create-table.html
- MySQL 8.4 Reference Manual, `SHOW CREATE DATABASE`:
  https://dev.mysql.com/doc/refman/8.4/en/show-create-database.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_sql_quote_show_create_system_variable_expectations.sh`
records the runtime probes for this feature. Observed behavior:

- `SELECT @@sql_quote_show_create`,
  `@@global.sql_quote_show_create`, `@@session.sql_quote_show_create`,
  `@@local.sql_quote_show_create`, and `@@SQL_QUOTE_SHOW_CREATE` return `1`
  in the tested default runtime.
- The variable has global and session scope. After
  `SET SESSION sql_quote_show_create=0`, unscoped, `session`, and `local`
  reads return `0`, while `global` still returns `1`; resetting the session
  value to `1` restores the default.
- `SHOW VARIABLES LIKE 'sql_quote_show_create'` reflects the session value;
  `SHOW GLOBAL VARIABLES LIKE 'sql_quote_show_create'` reflects the fixed
  global value.
- Variable and scope names are case-insensitive.
- Backtick-quoted final variable-name components are accepted.
- Backtick-quoted scope names, such as
  ``@@`session`.sql_quote_show_create``, are syntax errors.
- Unknown variables fail with error `1193`, SQLSTATE `HY000`, and an
  `Unknown system variable` message.
- A scalar `SELECT` that reads this variable is nondiagnostic. It reads the
  previous diagnostics snapshot for any `@@warning_count` or `@@error_count`
  items in the same select list, then clears diagnostics for following
  diagnostic statements.
- With the default value `1`, `SHOW CREATE DATABASE` and `SHOW CREATE TABLE`
  quote simple identifiers with backticks. With a session value of `0`, MySQL
  omits quotes for simple identifiers but still quotes identifiers that require
  quoting, such as reserved words or names containing spaces.
- MySQL accepts wider expression forms such as
  `SELECT @@sql_quote_show_create + 1`. Those forms remain outside this MyLite
  slice.

The official MySQL system-variable documentation classifies
`sql_quote_show_create` as a dynamic boolean variable with global and session
scope and default value `ON`.

## Scope

The implementation must add:

- runtime recognition of `sql_quote_show_create` inside the existing scalar
  `SELECT` subset;
- support for no scope, `session`, `local`, and `global` scope qualifiers;
- session `SET sql_quote_show_create = 0|1|ON|OFF|TRUE|FALSE|DEFAULT`
  handling through the existing Boolean system-variable override path;
- `SHOW VARIABLES` and `SHOW GLOBAL VARIABLES` values for the session/global
  baseline;
- case-insensitive matching for unquoted scope and variable names;
- backtick-quoted final variable-name components;
- one-row scalar result sets with existing source-span column labels;
- fixed global value `1` and session-local values that default to `1`;
- `SHOW CREATE DATABASE` and structured base/temporary-table `SHOW CREATE
  TABLE` rendering that omits backticks for safe simple identifiers when the
  session value is `0` and still quotes identifiers that require quoting;
- MySQL-compatible unknown-variable diagnostics for unsupported names;
- deterministic rejection of quoted scopes;
- fast C tests and a MySQL 8.4.9 expectation artifact.

Supported SQL examples:

```sql
SELECT @@sql_quote_show_create
SELECT @@sql_quote_show_create FROM DUAL
SELECT @@session.sql_quote_show_create, @@local.sql_quote_show_create
SELECT @@global.sql_quote_show_create
SELECT @@session.`sql_quote_show_create`, @@`sql_quote_show_create`
SELECT @@sql_quote_show_create, @@warning_count, ROW_COUNT()
SET SESSION sql_quote_show_create = 0
SHOW VARIABLES LIKE 'sql_quote_show_create'
SHOW CREATE TABLE normal_name
```

## Non-Goals

This feature must not implement:

- startup options, persisted variables, `SET_VAR` hints, or mutable
  server-global `sql_quote_show_create` state;
- variables other than `sql_quote_show_create`;
- quote-control behavior for `SHOW CREATE VIEW`, events, routines, users,
  triggers, or any not-yet-supported `SHOW CREATE` variant;
- Performance Schema variable tables;
- table-backed variable evaluation, aliases, clauses, subqueries, arithmetic,
  functions over variables, parameters, prepared statements, or arbitrary
  SQLite pass-through;
- catalog mutations, storage mutations, SQLite metadata reads, or SQLite fork
  patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public validation,
  parse/execution orchestration, result ownership, row-count state,
  diagnostics snapshot replacement, and failure cleanup.
- Statement context continues to reset live diagnostics at statement start and
  preserve the previous diagnostics snapshot until nondiagnostic successful
  completion replaces it.
- Lexer/parser/AST own syntax admission and source spans for
  `SYSTEM_VARIABLE` expressions. No new grammar is needed beyond the existing
  `expression ::= SYSTEM_VARIABLE` rule.
- Runtime execution owns system-variable path parsing, scope validation,
  session/global value selection, and diagnostics for unsupported names.
- Structured `SHOW CREATE DATABASE` and base/temporary-table `SHOW CREATE
  TABLE` rendering owns identifier quote policy for this slice. Pre-rendered
  view, routine, user, trigger, and event text remains unchanged.
- Result builder owns scalar result column labels and one-row text values.
- Catalog, storage, VFS, and SQLite physical row storage are not involved.
  This feature must not touch `.mylite` preamble bytes or SQLite schema state.

## Supported SQL Grammar

This slice uses the existing system-variable expression atom:

```lemon
expression ::= SYSTEM_VARIABLE.
```

The supported runtime variable paths are:

```sql
@@sql_quote_show_create
@@session.sql_quote_show_create
@@local.sql_quote_show_create
@@global.sql_quote_show_create
```

The existing scalar `SELECT` limits continue to apply:

```lemon
select_statement ::= SELECT select_item_list from_dual_opt.
select_item ::= expression.
from_dual_opt ::= .
from_dual_opt ::= FROM DUAL.
```

System variables are admitted only when every selected expression is in the
existing scalar expression set. Clauses such as `WHERE`, `ORDER BY`, `LIMIT`,
table-backed `FROM`, aliases, and general expressions remain outside this
slice.

## Variable Resolution

Runtime parses the raw token as a `@@` system variable:

- it accepts no scope, `session`, `local`, or `global`;
- it treats unquoted names ASCII case-insensitively;
- it accepts a backtick-quoted final variable-name component and unescapes
  doubled backticks before comparison;
- it rejects backtick-quoted scope names with a deterministic syntax
  diagnostic;
- it rejects malformed paths and unsupported variables with MySQL error
  `1193`, SQLSTATE `HY000`;
- it preserves the original source text as the scalar result column label.

For this slice, global scope returns the fixed embedded default `1`. Unscoped,
`session`, and `local` scope read the current session override when one exists,
and otherwise return `1`.

## Runtime Semantics

The supported variable returns:

| Scope | Value |
| --- | --- |
| Global | fixed `1` |
| Session/local/unscoped default | `1` |
| Session/local/unscoped after `SET SESSION sql_quote_show_create = 0` | `0` |

The session value is independent per handle and resets on close/reopen. It
affects only the structured database and table SHOW CREATE renderers in this
slice.

Successful scalar reads:

- return one row and one text column for each selected expression;
- use the original source expression as the column label unless the general
  scalar-select path later adds alias support;
- leave `warning_count == 0` for supported forms;
- do not mutate catalog rows, descriptor versions, descriptor caches, catalog
  generation, physical SQLite schema, or `.mylite` preamble bytes;
- follow existing scalar `SELECT` row-count behavior, so `ROW_COUNT()` after a
  successful scalar row result is `-1`.

## SHOW CREATE Interaction

When the session value is `1`, `SHOW CREATE DATABASE` and structured
`SHOW CREATE TABLE` output backtick-quotes identifiers. When the session value
is `0`, those renderers omit quotes for simple nonreserved ASCII identifiers
and retain quotes for identifiers that require quoting, including reserved
keywords and names containing spaces or other special characters.

This quote policy is intentionally conservative for the current embedded
baseline. It does not rewrite pre-rendered view, stored routine, user,
trigger, or event SHOW CREATE text.

## Diagnostics

This slice uses existing diagnostics for:

- syntax errors, including quoted scopes and unsupported scalar-select clauses;
- unknown system variables: error `1193`, SQLSTATE `HY000`;
- unsupported expressions such as arithmetic over system variables;
- public API misuse through the existing execution/result API behavior;
- allocation failures through existing MyLite allocation diagnostics.

Supported reads and session assignments of `@@sql_quote_show_create` do not
emit warnings.

## Tests

Tests must cover:

- unscoped, `global`, `session`, and `local` forms;
- case-insensitive names and scopes;
- backtick-quoted final variable names;
- quoted scope rejection;
- exact column labels for representative source spellings;
- `FROM DUAL`;
- mixed scalar reads with existing diagnostics, charset, engine, version, and
  autocommit variables;
- diagnostics read-and-clear behavior after warnings and errors;
- unknown unscoped and scoped variable names;
- unsupported wider expressions;
- default/session/global values, session assignment, `SHOW VARIABLES`, and
  close/reopen reset behavior;
- selected schema and independent handles keep session state isolated;
- quoted and unquoted `SHOW CREATE DATABASE` and structured `SHOW CREATE
  TABLE` output for supported descriptors, including still-quoted identifiers
  that require quoting;
- `.mylite` preamble preservation and unchanged catalog/SQLite generation
  after variable reads;
- existing parser/runtime/system-variable and SHOW CREATE tests still pass.

The MySQL expectation script verifies the MySQL 8.4.9 reference behavior for
the supported SQL forms and explicitly records wider MySQL behavior that this
slice leaves unsupported.

## Compatibility Documentation

Update:

- `COMPATIBILITY.md`;
- `docs/compatibility/runtime-system-variables.md`;
- `docs/compatibility/sql-show-statements.md`;
- existing SHOW CREATE feature specs where they previously listed
  `sql_quote_show_create` as entirely missing.

Do not overclaim server-global mutation, persisted variables, Performance
Schema variable tables, or quote-control coverage for pre-rendered SHOW CREATE
variants.
