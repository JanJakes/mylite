# Baseline Information Schema Stats Expiry System Variable

## Status

This feature adds a narrow `information_schema_stats_expiry` system-variable
slice. MySQL uses this variable to control expiration of cached dynamic
`INFORMATION_SCHEMA` table statistics. MyLite currently has descriptor-owned
metadata and no MySQL data-dictionary statistics cache, so this slice implements
the observable system-variable surface only:

- scalar `@@information_schema_stats_expiry` reads for default, `SESSION`,
  `LOCAL`, and fixed `GLOBAL` scope;
- `SHOW VARIABLES` rows for default, `SESSION`, and `GLOBAL` scope;
- handle-local session `SET` assignments with MySQL-compatible integer
  conversion, clamping warnings, and diagnostics for unsupported value types.

Mutable server-global state, startup options, persisted variables, privileges,
Performance Schema variable tables, optimizer hints, and actual
`INFORMATION_SCHEMA` statistics caching remain outside this slice.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- Baseline SHOW VARIABLES:
  `docs/specs/baseline-show-variables/specs.md`
- Baseline SHOW VARIABLES WHERE:
  `docs/specs/baseline-show-variables-where/specs.md`
- Runtime system-variable compatibility:
  `docs/compatibility/runtime-system-variables.md`
- MySQL 8.4 Reference Manual, server system variables:
  https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html
- MySQL 8.4 Reference Manual, system variable usage:
  https://dev.mysql.com/doc/refman/8.4/en/using-system-variables.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_information_schema_stats_expiry_system_variable_expectations.sh`
records the runtime probes for this feature. Observed behavior:

- `@@information_schema_stats_expiry`,
  `@@SESSION.information_schema_stats_expiry`,
  `@@LOCAL.information_schema_stats_expiry`, and
  `@@GLOBAL.information_schema_stats_expiry` read `86400` by default.
- `SHOW VARIABLES LIKE 'information_schema_stats_expiry'`, `SHOW SESSION
  VARIABLES LIKE ...`, and `SHOW GLOBAL VARIABLES LIKE ...` return one row
  with value `86400`.
- The official variable metadata reports global/session scope, dynamic status,
  integer type, default `86400`, minimum `0`, maximum `31536000`, and seconds
  as the unit.
- Unscoped, `SESSION`, `LOCAL`, direct `@@variable`, `@@SESSION.variable`, and
  `@@LOCAL.variable` assignments mutate the current session value.
- `SET SESSION information_schema_stats_expiry = DEFAULT` restores the session
  value to the current global value. MyLite's global value remains fixed at
  `86400`, so `DEFAULT` restores `86400`.
- `0`, positive integers up to `31536000`, unary `+`, `TRUE`, `FALSE`, and
  integer-typed user variables are accepted. `TRUE` stores `1`; `FALSE` stores
  `0`.
- Negative integers store `0` and emit warning `1292` with message
  `Truncated incorrect information_schema_stats_expiry value: '<text>'`.
- Integers above `31536000` store `31536000` and emit the same warning shape.
- String, decimal, `NULL`, `ON`, `OFF`, and integer literals above
  `18446744073709551615` fail with error `1232 42000` and message
  `Incorrect argument type to variable 'information_schema_stats_expiry'`.
- MySQL accepts mutable `SET GLOBAL` assignments when the user has sufficient
  privilege. MyLite deliberately keeps global state fixed for this baseline
  slice and accepts only exact fixed no-op global assignments.

## Scope

Supported SQL examples:

```sql
SELECT @@information_schema_stats_expiry
SELECT @@GLOBAL.information_schema_stats_expiry
SELECT @@SESSION.information_schema_stats_expiry
SHOW VARIABLES LIKE 'information_schema_stats_expiry'
SHOW VARIABLES WHERE Variable_name = 'information_schema_stats_expiry'
SET information_schema_stats_expiry = 0
SET SESSION information_schema_stats_expiry = DEFAULT
SET LOCAL information_schema_stats_expiry = +12
SET @@information_schema_stats_expiry = TRUE
SET @stats_expiry = 31536000
SET information_schema_stats_expiry = @stats_expiry
```

The implementation must:

- store `information_schema_stats_expiry` in handle-local session state and
  initialize it to `86400`;
- expose scalar reads for no scope, `SESSION`, `LOCAL`, and fixed `GLOBAL`;
- include the variable in `SHOW VARIABLES`, `SHOW SESSION VARIABLES`, and
  `SHOW GLOBAL VARIABLES`;
- snapshot and restore it across multi-assignment `SET` failure rollback;
- support `DEFAULT`, unsigned integer literals, parenthesized integer
  literals, `+integer`, `-integer`, `TRUE`, `FALSE`, and integer-typed user
  variables;
- clamp negative values to `0` and values above `31536000` to `31536000`, with
  MySQL-compatible warning `1292`;
- reject unsupported value forms with MySQL-compatible error `1232 42000`;
- keep global reads fixed at `86400` and accept only exact fixed no-op global
  assignments through `SET GLOBAL` / `SET @@GLOBAL`;
- leave catalog rows, descriptor versions, descriptor caches, catalog
  generation, SQLite schema generation, and `.mylite` preamble bytes unchanged.

## Non-Goals

This feature does not implement:

- mutable server-global `information_schema_stats_expiry`;
- startup options, option files, persisted variables, privilege semantics,
  Performance Schema variable tables, or `SET_VAR` optimizer hints;
- MySQL data-dictionary statistics tables, statistics refresh, shared
  cross-session statistics cache, `ANALYZE TABLE` effects, or optimizer
  statistics behavior;
- any change to existing `INFORMATION_SCHEMA` table contents or row estimates;
- variables other than `information_schema_stats_expiry`;
- arbitrary SQLite pass-through or SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns statement
  orchestration, public result ownership, diagnostics snapshots, and cleanup on
  failure.
- Statement context owns live and previous diagnostics. Out-of-range
  assignment warnings are appended to the current statement diagnostics before
  successful non-query result finalization.
- Lexer/parser/AST already admit `SYSTEM_VARIABLE` expressions, `SHOW
  VARIABLES`, and `SET` system-variable targets. This feature adds no new
  tokens or grammar.
- Runtime system-variable handling owns path parsing, scope validation, value
  parsing, clamping warnings, session-state mutation, scalar readback, fixed
  global readback, and `SHOW VARIABLES` display.
- The catalog module remains the descriptor authority. This feature does not
  read or mutate schema, table, index, or column descriptors.
- Result building uses the existing scalar result and non-query result
  conventions. Successful `SET` returns no rows, `affected_rows = 0`, and
  warnings only for clamped values.
- SQLite physical row storage is not involved. No generated SQLite SQL,
  SQLite metadata reads, SQLite extension registration, VFS change, or SQLite
  fork patch is required.
- Storage/VFS continues to own the `.mylite` preamble and shifted SQLite
  payload boundary. This feature must not write through byte range `[0, 4096)`.

## Grammar

This slice uses the existing MyLite grammar shape:

```lemon
expression ::= SYSTEM_VARIABLE.

show_variables_statement ::=
    SHOW show_variables_scope_opt VARIABLES show_variables_filter_opt.

set_assignment ::= set_system_variable_target EQUAL set_system_variable_value.
set_system_variable_target ::= identifier.
set_system_variable_target ::= SESSION identifier.
set_system_variable_target ::= LOCAL identifier.
set_system_variable_target ::= GLOBAL identifier.
set_system_variable_target ::= SYSTEM_VARIABLE.

set_system_variable_value ::= DEFAULT.
set_system_variable_value ::= INTEGER.
set_system_variable_value ::= PLUS INTEGER.
set_system_variable_value ::= MINUS INTEGER.
set_system_variable_value ::= TRUE.
set_system_variable_value ::= FALSE.
set_system_variable_value ::= user_variable.
set_system_variable_value ::= LPAREN set_system_variable_value RPAREN.
```

The grammar admits broader literal tokens for other variables. This feature's
runtime validator accepts only the value forms listed above and returns
MySQL-compatible diagnostics for unsupported forms.

## Semantics

Session storage:

- New handles start with `information_schema_stats_expiry = 86400`.
- Close/reopen starts a new session with the default value; the value is not
  stored in `.mylite` files.
- Independent handles have independent session values.
- Global reads always return `86400`.

Assignment:

- `DEFAULT` stores `86400`.
- `TRUE` stores `1`; `FALSE` stores `0`.
- A nonnegative integer literal stores the exact parsed value when it is within
  `0..31536000`.
- A positive sign is ignored.
- A negative integer literal stores `0` and appends warning `1292`.
- An integer literal above `31536000` stores `31536000` and appends warning
  `1292`.
- An integer-typed user variable uses the same conversion and warning rules.
- Unsupported literal kinds and unsigned overflow fail before mutation.
- Multi-assignment `SET` remains atomic: if any assignment fails, earlier
  session mutations in the same `SET` statement are rolled back.

`SET GLOBAL information_schema_stats_expiry = DEFAULT`,
`SET GLOBAL information_schema_stats_expiry = 86400`, and the corresponding
`@@GLOBAL` forms are accepted as fixed no-op assignments. Other global
assignments fail with a deterministic MyLite unsupported-feature diagnostic.

Successful scalar reads and `SHOW VARIABLES` statements return through the
existing result API conventions and clear diagnostics like other successful
statements. Successful in-range `SET` statements return `warning_count = 0`;
clamped `SET` statements return `warning_count = 1`.

## Diagnostics

The implementation must preserve existing diagnostics for:

- malformed SQL: parser diagnostic;
- unknown system variable: MySQL error `1193 HY000`;
- quoted system-variable scope: deterministic unsupported-feature diagnostic;
- unsupported assignment value forms: MySQL error `1232 42000`;
- integer overflow beyond the unsigned literal parser range: MySQL error
  `1232 42000`;
- clamped integer assignment: warning `1292`;
- unsupported non-no-op global assignment: deterministic MyLite
  unsupported-feature diagnostic;
- allocation failure: existing `MYLITE_NOMEM` diagnostic;
- public API misuse: unchanged existing public execution/result misuse
  behavior.

## Test Plan

- Add a MySQL 8.4.9 expectation script covering default/global/session/local
  reads, `SHOW VARIABLES`, supported `SET` forms, warning clamping, rejected
  value types, user variables, and exact no-op global assignment.
- Add fast C runtime tests for scalar reads, `SHOW VARIABLES` rows,
  assignments, warnings, rollback on failed multi-assignment, independent
  handles, reopen reset, and `.mylite` preamble preservation.
- Extend `SHOW VARIABLES` baseline tests with the new row.
- Run the new expectation script, focused runtime tests, `cmake --build
  --preset dev`, and `cmake --workflow --preset check`.
