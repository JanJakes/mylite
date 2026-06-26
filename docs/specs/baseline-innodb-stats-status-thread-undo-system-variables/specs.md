# Baseline InnoDB Stats, Status, Thread, And Undo System Variables

## Scope

This slice adds MySQL 8.4.9-shaped metadata and embedded placeholder handling
for the remaining adjacent InnoDB system variables in the baseline matrix:

- `innodb_stats_auto_recalc`
- `innodb_stats_include_delete_marked`
- `innodb_stats_method`
- `innodb_stats_on_metadata`
- `innodb_stats_persistent`
- `innodb_stats_persistent_sample_pages`
- `innodb_stats_transient_sample_pages`
- `innodb_status_output`
- `innodb_status_output_locks`
- `innodb_strict_mode`
- `innodb_sync_array_size`
- `innodb_sync_spin_loops`
- `innodb_table_locks`
- `innodb_temp_data_file_path`
- `innodb_temp_tablespaces_dir`
- `innodb_thread_concurrency`
- `innodb_thread_sleep_delay`
- `innodb_tmpdir`
- `innodb_undo_directory`
- `innodb_undo_log_encrypt`
- `innodb_undo_log_truncate`
- `innodb_undo_tablespaces`
- `innodb_use_fdatasync`
- `innodb_use_native_aio`
- `innodb_validate_tablespace_paths`
- `innodb_version`
- `innodb_write_io_threads`

The official MySQL 8.4 Reference Manual lists these as InnoDB system variables
for optimizer statistics, InnoDB monitor output, strict InnoDB DDL checks,
mutex/spin behavior, table locks, temporary and undo tablespace paths, undo log
encryption/truncation, asynchronous I/O, tablespace validation, and background
write threads.

Reference:

- <https://dev.mysql.com/doc/refman/8.4/en/innodb-parameters.html>

## Observed MySQL 8.4.9 Behavior

Runtime probes against `mysql:8.4.9` showed these defaults and scopes:

| Variable | Scalar value | SHOW value | Scope | Mutation |
| --- | --- | --- | --- | --- |
| `innodb_stats_auto_recalc` | `1` | `ON` | global | dynamic global |
| `innodb_stats_include_delete_marked` | `0` | `OFF` | global | dynamic global |
| `innodb_stats_method` | `nulls_equal` | `nulls_equal` | global | dynamic global |
| `innodb_stats_on_metadata` | `0` | `OFF` | global | dynamic global |
| `innodb_stats_persistent` | `1` | `ON` | global | dynamic global |
| `innodb_stats_persistent_sample_pages` | `20` | `20` | global | dynamic global |
| `innodb_stats_transient_sample_pages` | `8` | `8` | global | dynamic global |
| `innodb_status_output` | `0` | `OFF` | global | dynamic global |
| `innodb_status_output_locks` | `0` | `OFF` | global | dynamic global |
| `innodb_strict_mode` | `1` | `ON` | global/session | dynamic session and global |
| `innodb_sync_array_size` | `1` | `1` | global | read-only |
| `innodb_sync_spin_loops` | `30` | `30` | global | dynamic global |
| `innodb_table_locks` | `1` | `ON` | global/session | dynamic session and global |
| `innodb_temp_data_file_path` | `ibtmp1:12M:autoextend` | `ibtmp1:12M:autoextend` | global | read-only |
| `innodb_temp_tablespaces_dir` | `./#innodb_temp/` | `./#innodb_temp/` | global | read-only |
| `innodb_thread_concurrency` | `0` | `0` | global | dynamic global |
| `innodb_thread_sleep_delay` | `10000` | `10000` | global | dynamic global |
| `innodb_tmpdir` | `NULL` | empty string | global/session | dynamic nullable session and global |
| `innodb_undo_directory` | `./` | `./` | global | read-only |
| `innodb_undo_log_encrypt` | `0` | `OFF` | global | dynamic global |
| `innodb_undo_log_truncate` | `1` | `ON` | global | dynamic global |
| `innodb_undo_tablespaces` | `2` | `2` | global | dynamic global |
| `innodb_use_fdatasync` | `1` | `ON` | global | dynamic global |
| `innodb_use_native_aio` | `1` | `ON` | global | read-only |
| `innodb_validate_tablespace_paths` | `1` | `ON` | global | read-only |
| `innodb_version` | `8.4.9` | `8.4.9` | global | read-only |
| `innodb_write_io_threads` | `4` | `4` | global | read-only |

All variables appear in default, global, and session `SHOW VARIABLES` output.
For global-only variables, scalar `@@SESSION` and `@@LOCAL` reads fail with
`1238/HY000` and text containing `Variable '<name>' is a GLOBAL variable`.

For dynamic global-only variables, unqualified, `SESSION`, and `LOCAL`
assignments fail with `1229/HY000`. `SET GLOBAL ... = DEFAULT` succeeds.
Numeric variables reject `NULL` and string values with `1232/42000`. Boolean
and enum/text variables reject unsupported values with `1231/42000`.

For read-only variables, every assignment scope fails with `1238/HY000` and
text containing `read only variable`.

`innodb_strict_mode` and `innodb_table_locks` accept unqualified, `SESSION`,
and `LOCAL` assignments as session changes. `SET GLOBAL` changes MySQL's
server-global value, but does not change the current session value.

`innodb_tmpdir` accepts unqualified, `SESSION`, and `LOCAL` string or `NULL`
assignments as session changes. `SET GLOBAL` changes MySQL's server-global
value, but does not change the current session value. The observed valid
absolute path used by the test is `/tmp`; invalid strings fail with
`1231/42000`.

## MyLite Semantics

MyLite exposes fixed placeholder values matching the observed defaults through:

- `SELECT @@variable`
- `SELECT @@GLOBAL.variable`
- `SELECT @@SESSION.variable` and `@@LOCAL.variable` where MySQL supports them
- `SHOW VARIABLES LIKE ...`
- `SHOW GLOBAL VARIABLES LIKE ...`
- `SHOW SESSION VARIABLES LIKE ...`

For global-only variables, `@@SESSION.variable` and `@@LOCAL.variable` return
the MySQL-shaped global-variable diagnostic.

For dynamic global-only variables, MyLite accepts `SET GLOBAL variable =
DEFAULT` and exact fixed-value `SET GLOBAL` assignments as no-ops. Alternate
global values are rejected with a fixed-placeholder unsupported diagnostic
instead of mutating shared server-global state.

For read-only variables, every direct and user-variable assignment path returns
the MySQL-shaped read-only diagnostic before value validation.

For `innodb_strict_mode` and `innodb_table_locks`, MyLite supports handle-local
unqualified, `SESSION`, and `LOCAL` Boolean assignments with scalar and `SHOW`
readback. `SET GLOBAL` accepts only `DEFAULT` and the fixed default value as
no-ops.

For `innodb_tmpdir`, MyLite supports handle-local unqualified, `SESSION`, and
`LOCAL` assignments for `NULL`, `DEFAULT`, and absolute path string values.
Scalar reads expose `NULL` for the default/NULL state; `SHOW VARIABLES` exposes
an empty string for that state. Global assignments accept only `DEFAULT` and
`NULL` as no-ops.

The baseline deliberately does not implement InnoDB statistics recalculation,
InnoDB monitor output, InnoDB strict-mode DDL side effects, mutex array or spin
tuning, physical table-lock changes, temp-file routing, temporary or undo
tablespace path changes, undo encryption/truncation side effects, asynchronous
I/O changes, tablespace validation, physical background thread allocation,
persisted variables, startup option handling, privilege checks, Performance
Schema variable tables, or cross-handle global mutation.

## Parser And Runtime Design

No new grammar is required. Existing system-variable scalar and `SET` syntax
covers the baseline:

```lemon
set_statement ::= SET set_assignment_list.
set_assignment ::= system_variable_target EQ set_value.
system_variable_target ::= GLOBAL ident.
system_variable_target ::= SESSION ident.
system_variable_target ::= LOCAL ident.
scalar_expression ::= system_variable_reference.
system_variable_reference ::= AT_AT ident.
system_variable_reference ::= AT_AT GLOBAL DOT ident.
system_variable_reference ::= AT_AT SESSION DOT ident.
```

Runtime implementation adds descriptors for the variables, fixed scalar/SHOW
value cases, global-only/read-only scope wiring, fixed no-op global assignment
validation, generic Boolean session overrides for `innodb_strict_mode` and
`innodb_table_locks`, and nullable string session override handling for
`innodb_tmpdir`.

This is pure MyLite runtime logic. It does not require a SQLite public
extension API, wrapper translation, file-format change, or targeted SQLite fork
hook.

## Tests

- `packages/libmylite/tests/mysql_baseline_innodb_stats_status_thread_undo_system_variables_expectations.sh`
  verifies the observed MySQL 8.4.9 expectations.
- `packages/libmylite/tests/runtime_innodb_stats_status_thread_undo_system_variables_test.c`
  verifies MyLite scalar reads, `SHOW VARIABLES`, scope diagnostics,
  read-only diagnostics, accepted fixed global no-ops, rejected alternate
  global values, session Boolean overrides, nullable `innodb_tmpdir` overrides,
  user-variable assignment paths, NULL scalar rendering, and independent handle
  state.
