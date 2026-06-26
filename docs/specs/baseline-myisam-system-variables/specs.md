# Baseline MyISAM System Variables

## Summary

This slice exposes MySQL 8.4.9-shaped system-variable metadata for the MyISAM
configuration variables that remain visible to applications even when MyLite
does not implement MyISAM storage behavior:

- `myisam_data_pointer_size`
- `myisam_max_sort_file_size`
- `myisam_mmap_size`
- `myisam_recover_options`
- `myisam_sort_buffer_size`
- `myisam_stats_method`
- `myisam_use_mmap`

The variables are represented as embedded compatibility metadata. MyLite returns
MySQL 8.4.9 defaults, scalar reads, `SHOW VARIABLES` rows, global-only and
read-only diagnostics, fixed global no-op assignment behavior, and handle-local
session state for `myisam_sort_buffer_size` and `myisam_stats_method`.

MyLite does not implement MyISAM table storage, key-cache effects, repair
recovery modes, memory-mapped MyISAM files, MyISAM statistics estimation
effects, or MyISAM-specific sort-buffer allocation behavior.

## Compatibility Authority

- MySQL 8.4 Reference Manual, server system variables:
  <https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html>
- MySQL 8.4 Reference Manual, `SHOW VARIABLES`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-variables.html>
- MySQL 8.4.9 runtime observations captured by
  `packages/libmylite/tests/mysql_baseline_myisam_system_variables_expectations.sh`.

The manual establishes the system-variable surface, scope, dynamic/read-only
classification, and `SHOW VARIABLES` visibility. Runtime probes pin defaults,
scalar display, `SHOW` display, diagnostics, warnings, and assignment behavior
for this embedded baseline.

## MySQL 8.4.9 Observations

| Variable | Scalar value | `SHOW VARIABLES` value | Scalar scope |
| --- | --- | --- | --- |
| `myisam_data_pointer_size` | `6` | `6` | global |
| `myisam_max_sort_file_size` | `9223372036853727232` | `9223372036853727232` | global |
| `myisam_mmap_size` | `18446744073709551615` | `18446744073709551615` | global |
| `myisam_recover_options` | `OFF` | `OFF` | global |
| `myisam_sort_buffer_size` | `8388608` | `8388608` | global/session |
| `myisam_stats_method` | `nulls_unequal` | `nulls_unequal` | global/session |
| `myisam_use_mmap` | `0` | `OFF` | global |

Global-only variables also appear in `SHOW SESSION VARIABLES`. Session scalar
reads for global-only variables return `1238 / HY000`,
`Variable '<name>' is a GLOBAL variable`.

Observed assignment behavior:

- `myisam_data_pointer_size`, `myisam_max_sort_file_size`, and
  `myisam_use_mmap` are global-only and dynamic in MySQL. MyLite accepts
  `SET GLOBAL ... = DEFAULT` and exact default-value no-ops, but rejects
  value-changing global assignments because the storage effects do not exist.
- `myisam_mmap_size` and `myisam_recover_options` are read-only and reject
  assignments with `1238 / HY000`.
- `myisam_sort_buffer_size` is global/session. Session assignments accept
  integer, `TRUE`, `FALSE`, and `DEFAULT`; values below `4096` clamp to `4096`
  and append warning `1292`. String values use `1232 / 42000`.
- `myisam_stats_method` is global/session. Session assignments accept
  `nulls_unequal`, `nulls_equal`, and `nulls_ignored` as identifiers or strings,
  and integers `0`, `1`, and `2` mapping to those values. `TRUE` maps to `1`,
  `FALSE` maps to `0`, and invalid values use `1231 / 42000`.

## MyLite Scope

MyLite supports:

- scalar reads and `SHOW VARIABLES` rows for all variables in this slice;
- global-only scalar diagnostics for the global-only variables;
- read-only diagnostics for `myisam_mmap_size` and `myisam_recover_options`;
- fixed no-op global assignments for mutable global placeholders;
- handle-local session state for `myisam_sort_buffer_size` and
  `myisam_stats_method`;
- MySQL-shaped warnings for clamped `myisam_sort_buffer_size` assignments.

MyLite intentionally does not support:

- process-global mutation for MyISAM variables;
- MyISAM storage, recovery, repair, mmap, statistics, or key-cache behavior;
- allocating sort buffers based on `myisam_sort_buffer_size`;
- changing query planning or metadata statistics based on
  `myisam_stats_method`.

## Syntax

The existing system-variable, `SHOW VARIABLES`, and `SET` productions admit the
supported forms:

```sql
SELECT @@GLOBAL.myisam_data_pointer_size;
SELECT @@SESSION.myisam_sort_buffer_size;
SHOW VARIABLES LIKE 'myisam_stats_method';
SET SESSION myisam_sort_buffer_size = 4096;
SET LOCAL myisam_stats_method = nulls_ignored;
SET GLOBAL myisam_use_mmap = OFF;
```

No new grammar is required.

## Diagnostics

- Unknown variables continue to use `1193 / HY000`.
- Session reads of global-only variables use `1238 / HY000`.
- Non-global `SET` for mutable global-only variables uses `1229 / HY000`.
- Read-only variables use `1238 / HY000`.
- Value-changing global assignments for fixed placeholders return MyLite's
  deterministic unsupported fixed-no-op diagnostics.
- Invalid `myisam_sort_buffer_size` argument types use `1232 / 42000`; clamped
  numeric values append warning `1292 / HY000`.
- Invalid `myisam_stats_method` values use `1231 / 42000`.

## Runtime And Storage

This slice is implemented in MyLite's system-variable registry, scalar readback,
`SHOW VARIABLES` display path, SET validation, and session-state snapshot logic.
It does not introduce public ABI, catalog rows, SQLite SQL, SQLite fork patches,
file-format state, VFS behavior, or mutable process-global state.

## Tests

Coverage includes:

- a MySQL 8.4.9 expectation script for defaults, `SHOW` rows, global-only and
  read-only diagnostics, no-op global assignment forms, mutable upstream
  observations, session assignments, warning behavior, and user-variable forms;
- runtime C tests for scalar values, `SHOW` rows, diagnostics, fixed no-op
  assignments, unsupported state-changing global assignments, session state,
  warning behavior, user-variable assignments, and multi-assignment rollback;
- full `SHOW VARIABLES` registry regression coverage.
