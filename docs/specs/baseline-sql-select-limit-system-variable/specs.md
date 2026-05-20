# Baseline SQL Select Limit System Variable

Status note: this earlier fixed-read slice is extended by
`docs/specs/baseline-mutable-sql-select-limit/specs.md`, which adds
handle-local mutable session state and implicit top-level `SELECT` caps while
keeping global state fixed.

## Status

This feature specifies a narrow scalar system-variable slice for
`@@sql_select_limit`.

It builds on the existing `SYSTEM_VARIABLE` lexer/parser token, scalar
`SELECT` execution, diagnostics lifecycle, and descriptor-driven table read
paths. MySQL exposes `sql_select_limit` as mutable global and session state
that caps `SELECT` result rows when a statement has no explicit `LIMIT`.
MyLite does not implement mutable system-variable assignment in the baseline
yet, so this slice exposes only the default no-limit scalar value.

This is not session row-limit execution. It does not implement
`SET sql_select_limit`, mysql client safe-updates initialization, mutable
global/session state, implicit row caps for descriptor-backed `SELECT`, or any
interaction with explicit `LIMIT` clauses.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- Baseline select/order/limit lifecycle:
  `docs/specs/baseline-select-order-limit-lifecycle/specs.md`
- Baseline SQL safe updates system variable:
  `docs/specs/baseline-sql-safe-updates-system-variable/specs.md`
- MySQL lexer: `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold: `docs/specs/mysql-parser-scaffold/specs.md`
- MySQL 8.4 Reference Manual, server system variables:
  https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html
- MySQL 8.4 Reference Manual, mysql client safe-updates tips:
  https://dev.mysql.com/doc/refman/8.4/en/mysql-tips.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_sql_select_limit_system_variable_expectations.sh`
records the runtime probes for this feature. Observed behavior:

- `SELECT @@sql_select_limit`, `@@global.sql_select_limit`,
  `@@session.sql_select_limit`, `@@local.sql_select_limit`, and
  `@@SQL_SELECT_LIMIT` return `18446744073709551615` in the tested default
  runtime.
- The variable has global and session scope. After
  `SET SESSION sql_select_limit=1`, unscoped, `session`, and `local` reads
  return `1`, while `global` still returns `18446744073709551615`; assigning
  `DEFAULT` restores the default session value.
- With the session value set to `1`, a table `SELECT` with no explicit
  `LIMIT` returns at most one row. A table `SELECT` with `LIMIT 2` returns two
  rows because the explicit `LIMIT` takes precedence. With the session value
  set to `0`, a table `SELECT` with no explicit `LIMIT` returns no rows.
- Variable and scope names are case-insensitive.
- Backtick-quoted final variable-name components are accepted.
- Backtick-quoted scope names, such as ``@@`session`.sql_select_limit``, are
  syntax errors.
- Unknown variables fail with error `1193`, SQLSTATE `HY000`, and an
  `Unknown system variable` message.
- A scalar `SELECT` that reads this variable is nondiagnostic. It reads the
  previous diagnostics snapshot for any `@@warning_count` or `@@error_count`
  items in the same select list, then clears diagnostics for following
  diagnostic statements.
- MySQL permits system variables in wider expressions. At the default maximum,
  `SELECT @@sql_select_limit + 0` returns the default value, while adding `1`
  overflows MySQL's unsigned integer expression range. General expressions
  remain outside this MyLite slice.

The official MySQL system-variable documentation classifies
`sql_select_limit` as a dynamic integer variable with global and session scope,
default value `18446744073709551615`, minimum value `0`, and maximum value
`18446744073709551615`. It caps `SELECT` rows only when the statement lacks an
explicit `LIMIT`.

## Scope

The implementation must add:

- runtime recognition of `sql_select_limit` inside the existing scalar
  `SELECT` subset;
- support for no scope, `session`, `local`, and `global` scope qualifiers;
- case-insensitive matching for unquoted scope and variable names;
- backtick-quoted final variable-name components;
- one-row scalar result sets with existing source-span column labels;
- fixed value `18446744073709551615` for all supported scopes;
- MySQL-compatible unknown-variable diagnostics for unsupported names;
- deterministic rejection of quoted scopes;
- fast C tests and a MySQL 8.4.9 expectation artifact.

Supported SQL examples:

```sql
SELECT @@sql_select_limit
SELECT @@sql_select_limit FROM DUAL
SELECT @@session.sql_select_limit, @@local.sql_select_limit
SELECT @@global.sql_select_limit
SELECT @@session.`sql_select_limit`, @@`sql_select_limit`
SELECT @@sql_select_limit, @@warning_count, ROW_COUNT()
```

## Non-Goals

This feature must not implement:

- `SET`, startup options, persisted variables, `SET_VAR` hints, mysql client
  safe-updates initialization, or mutable global/session
  `sql_select_limit` state;
- implicit row caps for descriptor-backed `SELECT` statements;
- variables other than `sql_select_limit`;
- `sql_safe_updates`, `max_join_size`, safe-updates mode, or changed
  `UPDATE`/`DELETE` behavior;
- `SHOW VARIABLES` or Performance Schema variable tables;
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
- Runtime execution owns system-variable path parsing, scope validation, fixed
  value selection, and diagnostics for unsupported names.
- Descriptor-driven `SELECT` execution remains unchanged because no mutable
  session `sql_select_limit` state exists. The existing explicit
  `LIMIT`/`OFFSET` implementation remains authoritative for row caps.
- The catalog remains authoritative for descriptors. This variable slice does
  not create optimizer metadata, session variable storage, or table lifecycle
  behavior.
- Result builder owns scalar result column labels and one-row text values.
- Storage, VFS, and SQLite physical row storage are not involved. This feature
  must not touch `.mylite` preamble bytes or SQLite schema state.

## Supported SQL Grammar

This slice uses the existing system-variable expression atom:

```lemon
expression ::= SYSTEM_VARIABLE.
```

The supported runtime variable paths are:

```sql
@@sql_select_limit
@@session.sql_select_limit
@@local.sql_select_limit
@@global.sql_select_limit
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
- it rejects malformed paths and unsupported variables with MySQL error `1193`,
  SQLSTATE `HY000`;
- it preserves the original source text as the scalar result column label.

For this slice, all scopes return the same fixed value. This is a deliberate
MyLite limitation: no mutable `sql_select_limit` state exists yet.

## Runtime Semantics

The supported variable returns:

| Variable | Value |
| --- | --- |
| `sql_select_limit` | `18446744073709551615` |

The value is independent of selected schema, close/reopen, table DDL, DML, and
independent handles. It is a compatibility scalar only. Because no mutable
session state exists, existing descriptor-driven `SELECT` result counts are
unchanged and remain controlled only by explicit query `LIMIT` clauses.

Successful scalar reads:

- return one row and one text column for each selected expression;
- use the original source expression as the column label unless the general
  scalar-select path later adds alias support;
- leave `warning_count == 0` for supported forms;
- do not mutate catalog rows, descriptor versions, descriptor caches, catalog
  generation, physical SQLite schema, or `.mylite` preamble bytes;
- follow existing scalar `SELECT` row-count behavior, so `ROW_COUNT()` after a
  successful scalar row result is `-1`.

## Diagnostics

This slice uses existing diagnostics for:

- syntax errors, including quoted scopes and unsupported scalar-select
  clauses;
- unknown system variables: error `1193`, SQLSTATE `HY000`;
- unsupported expressions such as arithmetic over system variables;
- public API misuse through the existing execution/result API behavior;
- allocation failures through existing MyLite allocation diagnostics.

Supported reads of `@@sql_select_limit` do not emit warnings. This slice does
not implement MySQL's mutable `SET SESSION sql_select_limit=...` surface, so
assignment diagnostics and implicit row-limit behavior are out of scope.

## Tests

Tests must cover:

- unscoped, `global`, `session`, and `local` forms;
- fixed `18446744073709551615` value for all supported scopes;
- case-insensitive names and scopes;
- backtick-quoted final variable names;
- quoted scope rejection;
- exact column labels for representative source spellings;
- `FROM DUAL`;
- mixed scalar reads with existing diagnostics, charset, engine, autocommit,
  quote-control, foreign-key-check, unique-check, updatable-view, safe-updates,
  warning-reporting, and version variables;
- diagnostics read-and-clear behavior after warnings and errors;
- unknown unscoped and scoped variable names;
- unsupported wider expressions;
- selected schema, close/reopen, table DDL, DML, and independent handles do not
  change the fixed value;
- representative descriptor-backed `SELECT` statements still return their full
  result set unless they have an explicit `LIMIT`;
- `.mylite` preamble preservation and unchanged catalog/SQLite generation after
  variable reads;
- existing parser/runtime/system-variable and table lifecycle tests still pass.

The MySQL expectation script verifies the MySQL 8.4.9 reference behavior for
the supported SQL forms and explicitly records mutable upstream row-limiting
behavior that this slice leaves unsupported.

## Compatibility Documentation

Update:

- `COMPATIBILITY.md`;
- `docs/compatibility/runtime-system-variables.md`;
- `docs/compatibility/sql-query-expressions.md`.

Do not overclaim mutable system variables, `SET`, `SHOW VARIABLES`,
implicit `SELECT` row caps, safe-updates mode, `max_join_size`, or changed
`UPDATE`/`DELETE` behavior.
