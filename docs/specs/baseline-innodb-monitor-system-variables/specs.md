# Baseline InnoDB Monitor System Variables

## Scope

This baseline covers MySQL 8.4.9-shaped metadata and embedded no-op handling
for these InnoDB monitor switch variables:

- `innodb_monitor_disable`
- `innodb_monitor_enable`
- `innodb_monitor_reset`
- `innodb_monitor_reset_all`

The official MySQL 8.4 Reference Manual lists these variables as global,
dynamic InnoDB system variables used to enable, disable, or reset InnoDB
metrics counters. MyLite has no live InnoDB metrics engine, so this baseline
exposes the variables for compatibility while intentionally omitting counter
side effects.

Reference:

- <https://dev.mysql.com/doc/refman/8.4/en/innodb-parameters.html>

## Observed MySQL 8.4.9 Behavior

Runtime probes against `mysql:8.4.9` showed:

- `SELECT @@<variable>` and `SELECT @@GLOBAL.<variable>` return `NULL` after
  `SET GLOBAL <variable> = DEFAULT`.
- `SHOW VARIABLES LIKE '<variable>'`, `SHOW GLOBAL VARIABLES LIKE ...`, and
  `SHOW SESSION VARIABLES LIKE ...` include the variable with an empty value
  after default reset.
- `SELECT @@SESSION.<variable>` and `SELECT @@LOCAL.<variable>` fail with
  error `1238/HY000`, reporting that the variable is global.
- Unqualified, `SESSION`, and `LOCAL` assignment forms fail with
  `1229/HY000`, reporting that the variable must be set with `SET GLOBAL`.
- `SET GLOBAL <variable> = DEFAULT` succeeds.
- `SET GLOBAL <variable> = 'all'`, `'latch'`, `'module_buffer'`, and
  `'buffer%'` succeed as switch actions. Unquoted `all`, `latch`, and
  `module_buffer` forms are also accepted.
- `SET GLOBAL <variable> = NULL` fails with `1231/42000`.
- `SET GLOBAL <variable> = 1` fails with `1232/42000`.
- `SET GLOBAL <variable> = 'bogus'` fails with `1231/42000`.

## MyLite Semantics

MyLite implements these as fixed global placeholders:

- Scalar unqualified/global reads return SQL `NULL`.
- `SHOW VARIABLES`, `SHOW GLOBAL VARIABLES`, and `SHOW SESSION VARIABLES`
  return an empty string value.
- `SESSION`/`LOCAL` scalar reads return the MySQL-shaped global-only diagnostic.
- Unqualified, `SESSION`, and `LOCAL` assignments return the MySQL-shaped
  `SET GLOBAL` required diagnostic.
- `SET GLOBAL <variable> = DEFAULT` succeeds and has no side effects.
- `SET GLOBAL <variable> = 'all'`, `'latch'`, `'module_buffer'`, and
  `'buffer%'` succeeds as no-op switch actions. Unquoted `all`, `latch`, and
  `module_buffer` forms also succeed.
- Unsupported string switch values use MySQL-shaped `1231/42000` diagnostics.
- Non-string non-default values use MySQL-shaped `1232/42000` diagnostics.

The baseline deliberately does not implement live InnoDB metrics counters,
`INFORMATION_SCHEMA.INNODB_METRICS` state transitions, `SHOW ENGINE INNODB
MUTEX` state changes, persisted global values, privilege checks, cross-handle
global mutation, or Performance Schema variable tables.

## Parser And Runtime Design

No new grammar is required. Existing `SET` and system-variable scalar syntax
covers the baseline:

```lemon
set_statement ::= SET set_assignment_list.
set_assignment ::= system_variable_target EQ set_value.
system_variable_target ::= GLOBAL ident.
scalar_expression ::= system_variable_reference.
system_variable_reference ::= AT_AT ident.
system_variable_reference ::= AT_AT GLOBAL DOT ident.
```

Runtime implementation adds descriptors for the four variables, marks them as
global-only, returns fixed `NULL` scalar cells and empty `SHOW` values, and
routes global assignments through the InnoDB storage-variable SET handler.

This is pure MyLite runtime logic. It does not require a SQLite public
extension API, wrapper translation, file-format change, or targeted SQLite fork
hook.

## Tests

- `packages/libmylite/tests/mysql_baseline_innodb_monitor_system_variables_expectations.sh`
  verifies the observed MySQL 8.4.9 expectations.
- `packages/libmylite/tests/runtime_innodb_monitor_system_variables_test.c`
  verifies MyLite scalar reads, `SHOW VARIABLES`, global-only diagnostics,
  accepted no-op global switch actions, rejected values, and user-variable
  assignments.
