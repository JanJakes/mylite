# Baseline SQL Buffer Result System Variable

## Status

This feature specifies a narrow scalar system-variable slice for
`@@sql_buffer_result`.

It builds on the existing `SYSTEM_VARIABLE` lexer/parser token, scalar
`SELECT` execution, diagnostics lifecycle, and descriptor-driven table read
paths. MySQL exposes `sql_buffer_result` as mutable global and session state
that forces `SELECT` results into temporary tables. MyLite does not implement
mutable system-variable assignment or MySQL server-side result buffering in
the baseline yet, so this slice exposes only the default disabled scalar
value.

This is not result-buffering support. It does not implement
`SET sql_buffer_result`, mutable global/session state, temporary result table
materialization, lock-release behavior, optimizer effects, or protocol changes.

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
- MySQL lexer: `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold: `docs/specs/mysql-parser-scaffold/specs.md`
- MySQL 8.4 Reference Manual, server system variables:
  https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_sql_buffer_result_system_variable_expectations.sh`
records the runtime probes for this feature. Observed behavior:

- `SELECT @@sql_buffer_result`, `@@global.sql_buffer_result`,
  `@@session.sql_buffer_result`, `@@local.sql_buffer_result`, and
  `@@SQL_BUFFER_RESULT` return `0` in the tested default runtime.
- The variable has global and session scope. After
  `SET SESSION sql_buffer_result=1`, unscoped, `session`, and `local` reads
  return `1`, while `global` still returns `0`; assigning `DEFAULT` restores
  the default session value.
- Variable and scope names are case-insensitive.
- Backtick-quoted final variable-name components are accepted.
- Backtick-quoted scope names, such as ``@@`session`.sql_buffer_result``, are
  syntax errors.
- Unknown variables fail with error `1193`, SQLSTATE `HY000`, and an
  `Unknown system variable` message.
- A scalar `SELECT` that reads this variable is nondiagnostic. It reads the
  previous diagnostics snapshot for any `@@warning_count` or `@@error_count`
  items in the same select list, then clears diagnostics for following
  diagnostic statements.
- MySQL accepts wider expression forms such as
  `SELECT @@sql_buffer_result + 1`. Those forms remain outside this MyLite
  slice.

The official MySQL system-variable documentation classifies
`sql_buffer_result` as a dynamic boolean variable with global and session
scope and default value `OFF`. When enabled, MySQL materializes `SELECT`
results into temporary tables so table locks can be released earlier. MyLite
returns the fixed default disabled value `0`, so this slice does not alter
query execution.

## Scope

The implementation must add:

- runtime recognition of `sql_buffer_result` inside the existing scalar
  `SELECT` subset;
- support for no scope, `session`, `local`, and `global` scope qualifiers;
- case-insensitive matching for unquoted scope and variable names;
- backtick-quoted final variable-name components;
- one-row scalar result sets with existing source-span column labels;
- fixed value `0` for all supported scopes;
- MySQL-compatible unknown-variable diagnostics for unsupported names;
- deterministic rejection of quoted scopes;
- fast C tests and a MySQL 8.4.9 expectation artifact.

Supported SQL examples:

```sql
SELECT @@sql_buffer_result
SELECT @@sql_buffer_result FROM DUAL
SELECT @@session.sql_buffer_result, @@local.sql_buffer_result
SELECT @@global.sql_buffer_result
SELECT @@session.`sql_buffer_result`, @@`sql_buffer_result`
SELECT @@sql_buffer_result, @@warning_count, ROW_COUNT()
```

## Non-Goals

This feature must not implement:

- `SET`, startup options, persisted variables, `SET_VAR` hints, or mutable
  global/session `sql_buffer_result` state;
- temporary result table materialization, server cursor buffering,
  lock-release behavior, optimizer effects, memory/disk spill policy, or
  protocol changes;
- variables other than `sql_buffer_result`;
- changed descriptor-backed `SELECT` result production;
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
- Descriptor-driven `SELECT` execution remains unchanged because the fixed
  value is disabled and no mutable `sql_buffer_result` state exists.
- The catalog remains authoritative for descriptors. This variable slice does
  not affect table lifecycle or query planning.
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
@@sql_buffer_result
@@session.sql_buffer_result
@@local.sql_buffer_result
@@global.sql_buffer_result
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
MyLite limitation: no mutable `sql_buffer_result` state exists yet.

## Runtime Semantics

The supported variable returns:

| Variable | Value |
| --- | --- |
| `sql_buffer_result` | `0` |

The value is independent of selected schema, close/reopen, table DDL, DML, and
independent handles. It is a compatibility scalar only. Because the fixed
value is disabled, existing descriptor-driven `SELECT` behavior must not
change.

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

Supported reads of `@@sql_buffer_result` do not emit warnings. This slice does
not implement MySQL's mutable `SET SESSION sql_buffer_result=...` surface, so
assignment diagnostics and result-buffering side effects are out of scope.

## Tests

Tests must cover:

- unscoped, `global`, `session`, and `local` forms;
- fixed `0` value for all supported scopes;
- case-insensitive names and scopes;
- backtick-quoted final variable names;
- quoted scope rejection;
- exact column labels for representative source spellings;
- `FROM DUAL`;
- mixed scalar reads with existing diagnostics, charset, engine, autocommit,
  quote-control, foreign-key-check, unique-check, updatable-view,
  safe-updates, select-limit, notes, warning-reporting, and version variables;
- diagnostics read-and-clear behavior after warnings and errors;
- unknown unscoped and scoped variable names;
- unsupported wider expressions;
- selected schema, close/reopen, table DDL, DML, and independent handles do not
  change the fixed value;
- representative descriptor-backed `SELECT` statements still return normal
  result rows;
- `.mylite` preamble preservation and unchanged catalog/SQLite generation after
  variable reads;
- existing parser/runtime/system-variable and table lifecycle tests still pass.

The MySQL expectation script verifies the MySQL 8.4.9 reference behavior for
the supported SQL forms and explicitly records mutable result-buffering
behavior that this slice leaves unsupported.

## Compatibility Documentation

Update:

- `COMPATIBILITY.md`;
- `docs/compatibility/runtime-system-variables.md`;
- `docs/compatibility/sql-query-expressions.md`.

Do not overclaim mutable system variables, `SET`, `SHOW VARIABLES`,
temporary result tables, lock-release behavior, optimizer effects, or changed
descriptor-backed `SELECT` behavior.
