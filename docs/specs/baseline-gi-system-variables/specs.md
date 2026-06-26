# Baseline G/I System Variables

## Summary

This slice exposes MySQL 8.4.9-shaped support for the remaining small G/I
runtime system variables that can be represented without a server background
subsystem:

- `generated_random_password_length`
- `group_replication_consistency`
- `gtid_executed_compression_period`
- `gtid_next`
- `histogram_generation_max_mem_size`
- `identity`
- `immediate_server_version`
- `init_connect`
- `init_file`
- `init_replica`
- `init_slave`
- `last_insert_id`

Most variables are fixed metadata placeholders with exact/default no-op `SET`
forms. `identity` and `last_insert_id` read and update MyLite's existing
handle-local last-insert-id state. This slice does not implement password
generation, group replication, GTID ownership, histogram memory budgeting,
connection initialization SQL execution, or startup-file execution.

`insert_id` is intentionally outside this slice because MySQL uses it to affect
the next generated `AUTO_INCREMENT` value. That is a separate DML allocation
semantics feature.

## Compatibility Authority

- MySQL 8.4 Reference Manual, server system variables:
  <https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html>
- MySQL 8.4 Reference Manual, replication and GTID system variables:
  <https://dev.mysql.com/doc/refman/8.4/en/replication-options-gtids.html>
- MySQL 8.4 Reference Manual, `SHOW VARIABLES`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-variables.html>
- MySQL 8.4.9 runtime observations captured by
  `packages/libmylite/tests/mysql_baseline_compatibility_system_variables_expectations.sh`.

The manual establishes variable families, scope, and mutability. Runtime probes
establish pinned defaults, blank-vs-`NULL` display behavior, scope diagnostics,
read-only/default diagnostics, and session last-insert-id interactions.

## MySQL 8.4.9 Observations

| Variable | Scalar value | `SHOW VARIABLES` value | Scalar scope |
| --- | --- | --- | --- |
| `generated_random_password_length` | `20` | `20` | global/session |
| `group_replication_consistency` | `BEFORE_ON_PRIMARY_FAILOVER` | `BEFORE_ON_PRIMARY_FAILOVER` | global/session |
| `gtid_executed_compression_period` | `0` | `0` | global |
| `gtid_next` | `AUTOMATIC` | `AUTOMATIC` | session |
| `histogram_generation_max_mem_size` | `20000000` | `20000000` | global/session |
| `identity` | current last insert id | current last insert id | session |
| `immediate_server_version` | `999999` | `999999` | session |
| `init_connect` | empty string | empty string | global |
| `init_file` | SQL `NULL` | empty string | global |
| `init_replica` | empty string | empty string | global |
| `init_slave` | empty string | empty string | global |
| `last_insert_id` | current last insert id | current last insert id | session |

Global-only variables also appear in `SHOW SESSION VARIABLES`. Session-only
variables do not appear in `SHOW GLOBAL VARIABLES`. `init_file` returns SQL
`NULL` for scalar reads and an empty `SHOW VARIABLES` string.

Default assignment behavior is variable-specific:

- dynamic global/session placeholders accept `SET ... = DEFAULT` and exact
  current/default assignments in the appropriate scopes;
- global-only variables reject non-global `SET` with `1229 / HY000`;
- session-only variables reject `SET GLOBAL` with `1228 / HY000`;
- `init_file` rejects any `SET` form with read-only diagnostic `1238 / HY000`;
- scalar reads and accepted `SET GLOBAL` forms for deprecated `init_slave`
  emit warning `1287 / HY000` and advise `init_replica`;
- `identity` and `last_insert_id` reject `DEFAULT` with `1230 / 42000`,
  because MySQL has no default value for those aliases.

`identity` and `last_insert_id` share MySQL's session last-insert-id state:
generated auto-increment inserts update both variables, and assigning either
one updates `LAST_INSERT_ID()` readback.

## MyLite Scope

MyLite supports:

- scalar reads and `SHOW VARIABLES` rows for all variables in the table above;
- MySQL-shaped global-only and session-only diagnostics;
- fixed/default no-op assignments for dynamic placeholder variables;
- read-only diagnostics for `init_file`;
- `init_slave` deprecation warnings on scalar reads and accepted no-op SETs;
- `identity` and `last_insert_id` readback from MyLite's existing
  handle-local last-insert-id state;
- literal and user-variable `SET identity = value` /
  `SET last_insert_id = value` assignments that update that state.

MyLite intentionally does not support:

- mutable server-global placeholder state for password-generation length,
  group-replication consistency, GTID compression period, histogram memory, or
  connection initialization SQL text;
- password-generation side effects;
- group-replication membership or consistency behavior;
- `gtid_next = ANONYMOUS` or explicit GTID ownership and release semantics;
- histogram memory budgeting;
- executing `init_connect`, `init_replica`, `init_slave`, or `init_file`
  statements;
- startup options, persisted variables, privilege checks, or Performance
  Schema variable tables for this batch.

## Syntax

The existing system-variable, `SHOW VARIABLES`, and `SET` productions admit the
supported forms:

```sql
SELECT @@gtid_next, @@identity, @@last_insert_id;
SHOW SESSION VARIABLES LIKE 'identity';
SET SESSION last_insert_id = 9;
SET GLOBAL init_connect = DEFAULT;
```

No new grammar is required.

## Diagnostics

- Unknown variables continue to use `1193 / HY000`.
- Session reads of global-only variables use `1238 / HY000`,
  `Variable '<name>' is a GLOBAL variable`.
- Global reads of session-only variables use `1238 / HY000`,
  `Variable '<name>' is a SESSION variable`.
- Non-global `SET` for global-only dynamic variables uses `1229 / HY000`,
  `Variable '<name>' is a GLOBAL variable and should be set with SET GLOBAL`.
- `SET GLOBAL` for session-only variables uses `1228 / HY000`,
  `Variable '<name>' is a SESSION variable and can't be used with SET GLOBAL`.
- `SET identity = DEFAULT` and `SET last_insert_id = DEFAULT` use
  `1230 / 42000`, `Variable '<name>' doesn't have a default value`.
- Scalar reads and accepted `SET GLOBAL init_slave` no-op forms emit warning
  `1287 / HY000`, `@@init_slave` is deprecated in favor of `init_replica`.
- State-changing placeholder assignments outside exact/default no-op forms use
  MyLite's deterministic unsupported fixed-no-op diagnostics.

## Runtime And Storage

Placeholder variables are implemented in MyLite's system-variable registry,
scalar readback, `SHOW VARIABLES` display path, and fixed SET validation path.
`identity` and `last_insert_id` reuse `database->session.last_insert_id`; they
do not introduce new storage, public ABI, catalog rows, SQLite SQL, SQLite fork
patches, file-format state, VFS behavior, or mutable process-global state.

## Tests

Coverage includes:

- a MySQL 8.4.9 expectation script for defaults, `SHOW` rows, scope
  diagnostics, read-only/default diagnostics, no-op assignment forms, upstream
  mutable placeholder behavior, and `identity` / `last_insert_id` interaction;
- runtime C tests for scalar values, `SHOW` rows, scope diagnostics, fixed
  no-op assignments, unsupported state-changing assignments, and
  last-insert-id state updates;
- full `SHOW VARIABLES` registry regression coverage.
