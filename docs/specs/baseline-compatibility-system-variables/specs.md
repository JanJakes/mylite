# Baseline Compatibility System Variables

## Summary

This slice exposes MySQL 8.4.9-shaped metadata placeholders for twelve
compatibility-oriented system variables:

- `completion_type`
- `concurrent_insert`
- `core_file`
- `cte_max_recursion_depth`
- `default_table_encryption`
- `default_week_format`
- `delay_key_write`
- `delayed_insert_limit`
- `delayed_insert_timeout`
- `delayed_queue_size`
- `disabled_storage_engines`
- `div_precision_increment`

MyLite supports scalar reads, `SHOW VARIABLES` rows, scope diagnostics,
read-only diagnostics, deprecation warnings for the delayed-insert variables,
and exact/default no-op `SET` forms where the variable is dynamic in MySQL.
It does not implement mutable server-global state, transaction-completion side
effects, recursive CTE recursion-limit enforcement, default table encryption,
MyISAM delayed-insert or delay-key-write behavior, storage-engine disabling, or
division-precision changes.

## Compatibility Authority

- MySQL 8.4 Reference Manual, server system variables:
  <https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html>
- MySQL 8.4 Reference Manual, `SHOW VARIABLES`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-variables.html>
- MySQL 8.4.9 runtime observations captured by
  `packages/libmylite/tests/mysql_baseline_compatibility_system_variables_expectations.sh`.

The manual establishes the variable names, scope, dynamic/read-only shape, and
feature families. Runtime probes establish pinned 8.4.9 defaults, `SHOW`
display values, warning behavior, assignment diagnostics, and upstream mutable
behavior.

## MySQL 8.4.9 Observations

| Variable | Scalar value | `SHOW VARIABLES` value | Scalar scope |
| --- | --- | --- | --- |
| `completion_type` | `NO_CHAIN` | `NO_CHAIN` | global/session |
| `concurrent_insert` | `AUTO` | `AUTO` | global |
| `core_file` | `0` | `OFF` | global |
| `cte_max_recursion_depth` | `1000` | `1000` | global/session |
| `default_table_encryption` | `0` | `OFF` | global/session |
| `default_week_format` | `0` | `0` | global/session |
| `delay_key_write` | `ON` | `ON` | global |
| `delayed_insert_limit` | `100` | `100` | global |
| `delayed_insert_timeout` | `300` | `300` | global |
| `delayed_queue_size` | `1000` | `1000` | global |
| `disabled_storage_engines` | empty string | empty string | global |
| `div_precision_increment` | `4` | `4` | global/session |

All variables appear in `SHOW VARIABLES`, `SHOW GLOBAL VARIABLES`, and
`SHOW SESSION VARIABLES`. Global-only variables reject explicit
`@@SESSION.name` reads with `1238 / HY000`. `core_file` and
`disabled_storage_engines` reject `SET` with read-only diagnostics.
Non-global `SET` forms for global-only dynamic variables return
`1229 / HY000`. Reading or assigning the delayed-insert variables emits warning
`1287`.

MySQL can mutate the dynamic variables in this batch. MyLite intentionally
accepts only exact current/default assignments as no-ops until the underlying
feature effects are implemented.

## MyLite Scope

MyLite supports:

- unscoped and `GLOBAL` scalar reads for all variables;
- `SESSION` / `LOCAL` scalar reads for the session-scoped variables;
- MySQL-style scalar `SESSION` / `LOCAL` diagnostics for the global-only
  variables;
- `SHOW VARIABLES`, `SHOW GLOBAL VARIABLES`, and `SHOW SESSION VARIABLES`
  rows with MySQL-shaped display values;
- read-only diagnostics for `core_file` and `disabled_storage_engines`;
- exact/default no-op assignment forms for the dynamic variables;
- delayed-insert deprecation warnings on scalar reads and accepted no-op
  assignments;
- deterministic unsupported diagnostics for state-changing assignments and
  user-variable-backed assignments.

MyLite intentionally does not support:

- mutable server-global or session state for this batch;
- `completion_type` transaction completion behavior;
- `cte_max_recursion_depth` enforcement for recursive CTEs;
- `default_table_encryption` table-encryption behavior;
- `default_week_format` effects on date/week functions;
- MyISAM `delay_key_write` behavior;
- delayed-insert queues or delayed-insert statement behavior;
- `disabled_storage_engines` plugin filtering;
- `div_precision_increment` arithmetic precision effects;
- startup options, persisted variables, privilege checks, or Performance
  Schema variable tables.

## Syntax

No new grammar is required. Existing system-variable, `SHOW VARIABLES`, and
`SET` productions already admit the supported forms:

```sql
SELECT @@completion_type, @@GLOBAL.concurrent_insert;
SHOW VARIABLES LIKE 'default_week_format';
SET SESSION cte_max_recursion_depth = DEFAULT;
SET GLOBAL delay_key_write = ON;
```

## Diagnostics

- Unknown variables continue to use `1193 / HY000`.
- `@@SESSION` / `@@LOCAL` reads of global-only variables use
  `1238 / HY000`, `Variable '<name>' is a GLOBAL variable`.
- Non-global `SET` forms for dynamic global-only variables use
  `1229 / HY000`, `Variable '<name>' is a GLOBAL variable and should be set
  with SET GLOBAL`.
- Read-only variables use `1238 / HY000`,
  `Variable '<name>' is a read only variable`.
- Deprecated delayed-insert variables emit warning `1287`,
  `'@@<name>' is deprecated and will be removed in a future release.`
- State-changing values and user-variable-backed assignments use MyLite's
  deterministic unsupported fixed-no-op diagnostics.

## Runtime And Storage

This slice is implemented in MyLite's system-variable registry, scalar
readback, `SHOW VARIABLES` display path, warning path, and `SET` validation. It
does not add public ABI, catalog rows, SQLite SQL, SQLite extension API use,
fork patches, file-format state, VFS behavior, persistent state, or mutable
process-global state.

## Tests

Coverage includes:

- a MySQL 8.4.9 expectation script for defaults, `SHOW` rows, scope
  diagnostics, read-only diagnostics, delayed-insert warnings, no-op
  assignment forms, and upstream mutable behavior;
- a runtime C test for MyLite scalar values, `SHOW` rows, warnings,
  diagnostics, no-op fixed assignments, unsupported state-changing
  assignments, and user-variable assignment rejection;
- full `SHOW VARIABLES` registry regression coverage.
