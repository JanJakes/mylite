# Baseline J/L System Variables

## Summary

This slice exposes MySQL 8.4.9-shaped support for small J/L runtime system
variables that can be represented without adding server subsystems:

- `internal_tmp_mem_storage_engine`
- `join_buffer_size`
- `key_buffer_size`
- `key_cache_age_threshold`
- `key_cache_block_size`
- `key_cache_division_limit`
- `keyring_operations`
- `large_files_support`
- `large_page_size`
- `large_pages`
- `lc_messages`
- `lc_messages_dir`
- `local_infile`
- `locked_in_memory`
- `mandatory_roles`

`internal_tmp_mem_storage_engine` and `join_buffer_size` get handle-local
session readback and assignment. The remaining variables are fixed embedded
metadata placeholders with MySQL-shaped scope, read-only, and no-op assignment
diagnostics.

This slice intentionally does not include:

- `insert_id`, because it affects next `AUTO_INCREMENT` allocation;
- `lc_time_names`, because it changes temporal function output and belongs in
  a locale-aware temporal formatting slice;
- full MyISAM key cache behavior, keyring behavior, localized diagnostics,
  `LOAD DATA LOCAL` transport behavior, memory locking, or mandatory-role
  privilege effects.

## Compatibility Authority

- MySQL 8.4 Reference Manual, server system variables:
  <https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html>
- MySQL 8.4 Reference Manual, `SHOW VARIABLES`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-variables.html>
- MySQL 8.4.9 runtime observations captured by
  `packages/libmylite/tests/mysql_baseline_jl_system_variables_expectations.sh`.

The manual establishes variable scope, mutability, and intended side effects.
Runtime probes establish pinned defaults, display values, scope diagnostics,
read-only diagnostics, assignment coercions, and placeholder decisions.

## MySQL 8.4.9 Observations

| Variable | Scalar value | `SHOW VARIABLES` value | Scalar scope |
| --- | --- | --- | --- |
| `internal_tmp_mem_storage_engine` | `TempTable` | `TempTable` | global/session |
| `join_buffer_size` | `262144` | `262144` | global/session |
| `key_buffer_size` | `8388608` | `8388608` | global |
| `key_cache_age_threshold` | `300` | `300` | global |
| `key_cache_block_size` | `1024` | `1024` | global |
| `key_cache_division_limit` | `100` | `100` | global |
| `keyring_operations` | `1` | `ON` | global |
| `large_files_support` | `1` | `ON` | global |
| `large_page_size` | `0` | `0` | global |
| `large_pages` | `0` | `OFF` | global |
| `lc_messages` | `en_US` | `en_US` | global/session |
| `lc_messages_dir` | `/usr/share/mysql-8.4/` | `/usr/share/mysql-8.4/` | global |
| `local_infile` | `0` | `OFF` | global |
| `locked_in_memory` | `0` | `OFF` | global |
| `mandatory_roles` | empty string | empty string | global |

Global-only variables also appear in `SHOW SESSION VARIABLES`. Session reads
for global-only variables return `1238 / HY000`, `Variable '<name>' is a GLOBAL
variable`.

Observed assignment behavior for the implemented session variables:

- `internal_tmp_mem_storage_engine` accepts `DEFAULT`, `TempTable`, `MEMORY`,
  and numeric enum aliases `1` and `0` respectively. Unsupported values such as
  `InnoDB` fail with `1231 / 42000`.
- `join_buffer_size` accepts integer and integer user-variable session
  assignments. Values below `128` clamp to `128` with warning `1292`, and
  strings or `ON`/`OFF` tokens fail with `1232 / 42000`.
- Both variables have mutable MySQL global state. MyLite keeps global readback
  fixed and accepts only exact/default global no-op assignments.

Observed assignment behavior for fixed placeholders:

- global-only dynamic placeholders reject non-global `SET` with
  `1229 / HY000`;
- exact/default global no-op assignments are accepted where MySQL accepts
  mutable global assignments;
- read-only placeholders reject any assignment with `1238 / HY000`;
- value-changing global assignments return deterministic MyLite unsupported
  diagnostics instead of mutating embedded server state.

## MyLite Scope

MyLite supports:

- scalar reads and `SHOW VARIABLES` rows for all variables in this slice;
- MySQL-shaped global-only diagnostics;
- session-local `internal_tmp_mem_storage_engine` readback and SET for
  `DEFAULT`, `TempTable`, `MEMORY`, and the numeric enum aliases `1` and `0`;
- session-local `join_buffer_size` readback and SET for integer literals,
  booleans, unary signs, and integer user variables, with MySQL-compatible
  lower-bound clamping to `128`;
- fixed global readback for `internal_tmp_mem_storage_engine` and
  `join_buffer_size`;
- exact/default no-op global assignments for dynamic placeholders;
- read-only diagnostics for `large_files_support`, `large_page_size`,
  `large_pages`, `lc_messages_dir`, and `locked_in_memory`;
- fixed English `lc_messages` readback with no localized diagnostics.

MyLite intentionally does not support:

- mutable process-global state for this slice;
- internal temporary-table engine routing;
- join-buffer allocation or optimizer planning effects;
- MyISAM key cache allocation, flushing, or partitioning;
- keyring service enable/disable effects;
- large-page memory allocation or process memory locking;
- localized diagnostics or message file loading;
- `LOAD DATA LOCAL` client/server transport changes;
- mandatory-role grants, activation, revocation protection, or privilege checks.

## Syntax

The existing system-variable, `SHOW VARIABLES`, and `SET` productions admit the
supported forms:

```sql
SELECT @@join_buffer_size, @@GLOBAL.key_buffer_size;
SHOW VARIABLES LIKE 'local_infile';
SET SESSION internal_tmp_mem_storage_engine = MEMORY;
SET join_buffer_size = 128;
SET GLOBAL key_buffer_size = DEFAULT;
```

No new grammar is required.

## Diagnostics

- Unknown variables continue to use `1193 / HY000`.
- Session reads of global-only variables use `1238 / HY000`,
  `Variable '<name>' is a GLOBAL variable`.
- Non-global `SET` for global-only dynamic variables uses `1229 / HY000`,
  `Variable '<name>' is a GLOBAL variable and should be set with SET GLOBAL`.
- Read-only variables use MySQL-shaped `1238 / HY000` diagnostics.
- Invalid `internal_tmp_mem_storage_engine` values use `1231 / 42000`.
- Invalid `join_buffer_size` argument types use `1232 / 42000`.
- Clamped `join_buffer_size` values append warning `1292 / HY000`.
- Unsupported value-changing placeholder assignments use MyLite's
  deterministic unsupported fixed-no-op diagnostics.

## Runtime And Storage

The slice is implemented in MyLite's system-variable registry, scalar readback,
`SHOW VARIABLES` display path, `SET` validation path, and session snapshot
state for multi-assignment rollback. It does not introduce public ABI, catalog
rows, SQLite SQL, SQLite fork patches, file-format state, VFS behavior, or
mutable process-global state.

## Tests

Coverage includes:

- a MySQL 8.4.9 expectation script for defaults, `SHOW` rows, scope
  diagnostics, read-only diagnostics, no-op assignment forms, upstream mutable
  global observations, and session assignment behavior;
- runtime C tests for scalar values, `SHOW` rows, scope diagnostics, fixed
  no-op assignments, unsupported state-changing assignments, and session
  readback;
- full `SHOW VARIABLES` registry regression coverage.
