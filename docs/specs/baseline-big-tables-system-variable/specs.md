# Baseline Big Tables System Variable

## Status

This feature adds a narrow `big_tables` system-variable slice. MySQL uses
`big_tables` to prefer disk-backed internal temporary tables for large `SELECT`
operations. MyLite does not yet expose MySQL's internal temporary-table storage
choice, so this slice implements the observable variable surface only:

- scalar reads for `@@big_tables`, `@@SESSION.big_tables`, `@@LOCAL.big_tables`,
  and fixed `@@GLOBAL.big_tables`;
- `SHOW VARIABLES` rows for default, `SESSION`, and `GLOBAL` scope;
- handle-local session `SET` assignment and readback with MySQL-shaped boolean
  value conversion and diagnostics.

Mutable server-global state, startup options, persisted variables, privileges,
`SET_VAR` hints, Performance Schema variable tables, and actual temporary-table
storage changes remain outside this slice.

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
- MySQL 8.4 Reference Manual, server system variable reference:
  https://dev.mysql.com/doc/refman/8.4/en/server-system-variable-reference.html
- MySQL 8.4 Reference Manual, using system variables:
  https://dev.mysql.com/doc/refman/8.4/en/using-system-variables.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_big_tables_system_variable_expectations.sh`
records the runtime probes for this feature. Observed behavior:

- `@@big_tables`, `@@GLOBAL.big_tables`, `@@SESSION.big_tables`, and
  `@@LOCAL.big_tables` read `0` by default.
- `SHOW VARIABLES LIKE 'big_tables'`, `SHOW SESSION VARIABLES LIKE ...`, and
  `SHOW GLOBAL VARIABLES LIKE ...` return one row with value `OFF`.
- The official variable metadata reports global/session scope, dynamic status,
  boolean type, and default `OFF`.
- Unscoped, `SESSION`, `LOCAL`, direct `@@variable`, `@@SESSION.variable`, and
  `@@LOCAL.variable` assignments mutate the current session value.
- `SET SESSION big_tables = DEFAULT` restores the session value to the current
  global value. MyLite's global value remains fixed at `0`, so `DEFAULT`
  restores `0`.
- Boolean tokens `ON`, `OFF`, `TRUE`, and `FALSE`, integer literals `0` and
  `1`, signed `+0`, `+1`, and `-0`, and parenthesized integer or string-literal
  values are accepted where MySQL accepts the syntax.
- String literals `'ON'` and `'OFF'` are accepted. String literals `'1'`,
  `'TRUE'`, and `'FALSE'` fail with error `1231 42000`.
- Integer literals outside `0` and `1`, including negative values other than
  `-0`, fail with error `1231 42000`.
- Decimal values fail with error `1232 42000`.
- Integer user variables `0` and `1` and string user variables `ON` and `OFF`
  are accepted. String user variables `1`, `1.0`, `TRUE`, and `FALSE`, integer
  user variables outside `0` and `1`, decimal user variables, and `NULL` user
  variables fail. Decimal user variables fail with `1232 42000`; decimal-looking
  string user variables fail with `1231 42000`.
- MySQL accepts mutable `SET GLOBAL` assignments when the user has sufficient
  privilege. MyLite deliberately keeps global state fixed for this baseline
  slice and accepts only exact fixed no-op global assignments.

## Scope

Supported SQL examples:

```sql
SELECT @@big_tables
SELECT @@GLOBAL.big_tables
SELECT @@SESSION.big_tables
SHOW VARIABLES LIKE 'big_tables'
SHOW VARIABLES WHERE Variable_name = 'big_tables'
SET big_tables = ON
SET SESSION big_tables = DEFAULT
SET LOCAL big_tables = +1
SET @@big_tables = FALSE
SET @@SESSION.big_tables = 'ON'
SET @big_tables = 'OFF'
SET big_tables = @big_tables
```

The implementation must:

- store `big_tables` in handle-local session state and initialize it to `false`;
- expose scalar reads for no scope, `SESSION`, `LOCAL`, and fixed `GLOBAL`;
- include the variable in `SHOW VARIABLES`, `SHOW SESSION VARIABLES`, and
  `SHOW GLOBAL VARIABLES`;
- snapshot and restore it across multi-assignment `SET` failure rollback;
- support `DEFAULT`, boolean tokens, integer literal `0` / `1` values with
  optional unary sign where MySQL accepts them, string literals `'ON'` /
  `'OFF'`, and supported integer/string user variables while rejecting decimal
  user variables with MySQL's wrong-argument-type diagnostic;
- reject unsupported values with MySQL-shaped diagnostics;
- keep global reads fixed at `0` and accept only exact fixed no-op global
  assignments through `SET GLOBAL` / `SET @@GLOBAL`;
- leave query planning, temporary-table placement, catalog rows, descriptor
  versions, descriptor caches, catalog generation, SQLite schema generation,
  and `.mylite` preamble bytes unchanged.

## Non-Goals

This feature does not implement:

- mutable server-global `big_tables`;
- startup options, option files, persisted variables, privilege semantics,
  Performance Schema variable tables, or `SET_VAR` optimizer hints;
- disk-backed internal temporary-table selection, memory-pressure behavior,
  optimizer plan changes, row-materialization changes, or temporary-table
  storage diagnostics;
- variables other than `big_tables`;
- arbitrary SQLite pass-through or SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns statement
  orchestration, result ownership, diagnostics snapshots, and cleanup on
  failure.
- Statement context owns live and previous diagnostics. Unsupported assignment
  values report through the existing system-variable diagnostic helpers.
- Lexer/parser/AST already admit `SYSTEM_VARIABLE` expressions, `SHOW
  VARIABLES`, and `SET` system-variable targets. This feature reuses those
  paths and extends user-variable assignment values just enough for decimal
  source-text probes such as `SET @bt = 1.0; SET big_tables = @bt` to reach the
  runtime validator and produce the MySQL-observed diagnostic.
- Runtime system-variable handling owns path parsing, scope validation, value
  parsing, session-state mutation, scalar readback, fixed global readback, and
  `SHOW VARIABLES` display.
- The catalog module remains the descriptor authority. This feature does not
  read or mutate schema, table, index, or column descriptors.
- Result building uses the existing scalar result and non-query result
  conventions. Successful `SET` returns no rows, `affected_rows = 0`, and
  `warning_count = 0`.
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
set_system_variable_value ::= STRING.
set_system_variable_value ::= user_variable.
set_system_variable_value ::= LPAREN set_system_variable_value RPAREN.
```

The grammar admits broader literal tokens for other variables. This feature's
runtime validator accepts only the value forms listed above and returns
MySQL-shaped diagnostics for unsupported forms.

## Semantics

Session storage:

- New handles start with `big_tables = false`.
- Close/reopen starts a new session with the default value; the value is not
  stored in `.mylite` files.
- Independent handles have independent session values.
- Global reads always return `0`.

Assignment:

- `DEFAULT` stores `false`.
- `ON`, `TRUE`, `1`, `+1`, and equivalent supported user-variable values store
  `true`.
- `OFF`, `FALSE`, `0`, `+0`, `-0`, and equivalent supported user-variable
  values store `false`.
- String literals or string user-variable values `ON` and `OFF` are accepted
  case-insensitively.
- Unsupported strings, decimal literals or decimal user variables, `NULL`,
  integer values outside `0` / `1`, and negative nonzero integers fail before
  mutation. Decimal-looking string user-variable values remain strings and fail
  with the invalid-value diagnostic rather than the wrong-argument-type
  diagnostic.
- Multi-assignment `SET` remains atomic: if any assignment fails, earlier
  session mutations in the same `SET` statement are rolled back.

`SET GLOBAL big_tables = DEFAULT`, `SET GLOBAL big_tables = 0`,
`SET GLOBAL big_tables = OFF`, and equivalent `@@GLOBAL` forms are accepted as
fixed no-op assignments. Other global assignments fail with a deterministic
MyLite unsupported-feature diagnostic.

Successful scalar reads and `SHOW VARIABLES` statements return through the
existing result API conventions and clear diagnostics like other successful
statements. Successful `SET` statements return `warning_count = 0`.

## Diagnostics

The implementation must preserve existing diagnostics for:

- malformed SQL: parser diagnostic;
- unknown system variable: MySQL error `1193 HY000`;
- quoted system-variable scope: deterministic unsupported-feature diagnostic;
- unsupported boolean values that MySQL treats as invalid variable values:
  MySQL error `1231 42000`;
- unsupported expression or decimal value forms that MySQL treats as wrong
  argument types: MySQL error `1232 42000`;
- unsupported non-no-op global assignment: deterministic MyLite
  unsupported-feature diagnostic;
- allocation failure: existing `MYLITE_NOMEM` diagnostic;
- public API misuse: unchanged existing public execution/result misuse
  behavior.

## Test Plan

- Add a MySQL 8.4.9 expectation script covering default/global/session/local
  reads, `SHOW VARIABLES`, supported `SET` forms, accepted string forms,
  rejected values, user variables, fixed no-op global assignment, and
  multi-assignment rollback expectations.
- Add fast C runtime tests for scalar reads, `SHOW VARIABLES` rows,
  assignments, diagnostics, rollback on failed multi-assignment, independent
  handles, reopen reset, and `.mylite` preamble preservation.
- Extend `SHOW VARIABLES` baseline tests with the new row.
- Run the new expectation script, focused runtime tests, `cmake --build
  --preset dev`, and `cmake --workflow --preset check`.
