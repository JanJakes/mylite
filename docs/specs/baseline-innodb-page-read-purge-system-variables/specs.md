# Baseline InnoDB Page, Read, And Purge System Variables

## Scope

This slice adds MySQL 8.4.9-shaped metadata and embedded placeholder handling
for these adjacent InnoDB system variables:

- `innodb_old_blocks_pct`
- `innodb_old_blocks_time`
- `innodb_online_alter_log_max_size`
- `innodb_open_files`
- `innodb_optimize_fulltext_only`
- `innodb_page_cleaners`
- `innodb_page_size`
- `innodb_parallel_read_threads`
- `innodb_print_all_deadlocks`
- `innodb_print_ddl_logs`
- `innodb_purge_batch_size`
- `innodb_purge_rseg_truncate_frequency`
- `innodb_purge_threads`
- `innodb_random_read_ahead`
- `innodb_read_ahead_threshold`
- `innodb_read_io_threads`

The official MySQL 8.4 Reference Manual lists these as InnoDB startup and
system variables controlling buffer-pool page aging, online DDL logging,
file/thread counts, full-text optimization, deadlock/DDL logging, purge work,
read-ahead, and parallel clustered-index reads.

Reference:

- <https://dev.mysql.com/doc/refman/8.4/en/innodb-parameters.html>

## Observed MySQL 8.4.9 Behavior

Runtime probes against `mysql:8.4.9` showed these defaults and scopes:

| Variable | Scalar value | SHOW value | Scope | Mutation |
| --- | --- | --- | --- | --- |
| `innodb_old_blocks_pct` | `37` | `37` | global | dynamic global |
| `innodb_old_blocks_time` | `1000` | `1000` | global | dynamic global |
| `innodb_online_alter_log_max_size` | `134217728` | `134217728` | global | dynamic global |
| `innodb_open_files` | `4000` | `4000` | global | read-only |
| `innodb_optimize_fulltext_only` | `0` | `OFF` | global | dynamic global |
| `innodb_page_cleaners` | `1` | `1` | global | read-only |
| `innodb_page_size` | `16384` | `16384` | global | read-only |
| `innodb_parallel_read_threads` | `4` | `4` | global/session | dynamic session and global |
| `innodb_print_all_deadlocks` | `0` | `OFF` | global | dynamic global |
| `innodb_print_ddl_logs` | `0` | `OFF` | global | dynamic global |
| `innodb_purge_batch_size` | `300` | `300` | global | dynamic global |
| `innodb_purge_rseg_truncate_frequency` | `128` | `128` | global | dynamic global |
| `innodb_purge_threads` | `4` | `4` | global | read-only |

MySQL adjusts the read-only `innodb_purge_threads` startup value from `4` to
`1` on CPU-constrained hosts. MyLite exposes the canonical unconstrained value
`4` because its compatibility placeholder does not allocate purge workers.
| `innodb_random_read_ahead` | `0` | `OFF` | global | dynamic global |
| `innodb_read_ahead_threshold` | `56` | `56` | global | dynamic global |
| `innodb_read_io_threads` | `9` | `9` | global | read-only |

All variables appear in default, global, and session `SHOW VARIABLES` output.
For global-only variables, scalar `@@SESSION` and `@@LOCAL` reads fail with
`1238/HY000` and text containing `Variable '<name>' is a GLOBAL variable`.

For dynamic global-only variables, unqualified, `SESSION`, and `LOCAL`
assignments fail with `1229/HY000`. `SET GLOBAL ... = DEFAULT` succeeds.
Numeric variables reject `NULL` and string values with `1232/42000`. Boolean
variables reject `NULL` and unsupported strings with `1231/42000`.

For read-only variables, every assignment scope fails with `1238/HY000` and
text containing `read only variable`.

`innodb_parallel_read_threads` accepts unqualified, `SESSION`, and `LOCAL`
assignments as session changes. `SET GLOBAL` changes MySQL's server-global
value, but does not change the current session value. Integer session values
are clamped to `1..256` with warning `1292`; non-integer values fail with
`1232/42000`.

## MyLite Semantics

MyLite exposes fixed placeholder values matching the observed defaults through:

- `SELECT @@variable`
- `SELECT @@GLOBAL.variable`
- `SHOW VARIABLES LIKE ...`
- `SHOW GLOBAL VARIABLES LIKE ...`
- `SHOW SESSION VARIABLES LIKE ...`

For global-only variables, `@@SESSION.variable` and `@@LOCAL.variable` return
the MySQL-shaped global-variable diagnostic.

For dynamic global-only variables, MyLite accepts `SET GLOBAL variable =
DEFAULT` and exact fixed-value `SET GLOBAL` assignments as no-ops. Alternate
global values are rejected with a MyLite fixed-placeholder unsupported
diagnostic instead of mutating shared server-global state.

For read-only variables, every direct and user-variable assignment path returns
the MySQL-shaped read-only diagnostic before value validation.

For `innodb_parallel_read_threads`, MyLite supports handle-local unqualified,
`SESSION`, and `LOCAL` integer assignments with scalar and `SHOW` readback.
Session values are clamped to `1..256` with a MySQL-shaped truncation warning.
`SET GLOBAL innodb_parallel_read_threads` accepts only `DEFAULT` and the fixed
default value as no-ops.

The baseline deliberately does not implement InnoDB buffer-pool paging,
physical file/thread allocation, online-DDL log sizing, full-text optimize
side effects, deadlock/DDL logging, purge scheduling, read-ahead behavior,
parallel clustered-index read execution, persisted variables, startup option
handling, privilege checks, Performance Schema variable tables, or
cross-handle global mutation.

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
validation, and a handle-local unsigned integer field for
`innodb_parallel_read_threads`.

This is pure MyLite runtime logic. It does not require a SQLite public
extension API, wrapper translation, file-format change, or targeted SQLite fork
hook.

## Tests

- `packages/libmylite/tests/mysql_baseline_innodb_page_read_purge_system_variables_expectations.sh`
  verifies the observed MySQL 8.4.9 expectations.
- `packages/libmylite/tests/runtime_innodb_page_read_purge_system_variables_test.c`
  verifies MyLite scalar reads, `SHOW VARIABLES`, scope diagnostics,
  read-only diagnostics, accepted fixed global no-ops, rejected alternate
  values, user-variable assignments, and handle-local
  `innodb_parallel_read_threads` assignment/clamping.
