# Baseline InnoDB Core System Variables

## Scope

This slice covers MySQL 8.4.9-shaped metadata for these global InnoDB system
variables:

- `innodb_adaptive_flushing`
- `innodb_adaptive_flushing_lwm`
- `innodb_adaptive_hash_index`
- `innodb_adaptive_hash_index_parts`
- `innodb_adaptive_max_sleep_delay`
- `innodb_autoextend_increment`
- `innodb_autoinc_lock_mode`
- `innodb_buffer_pool_chunk_size`
- `innodb_buffer_pool_dump_at_shutdown`
- `innodb_buffer_pool_dump_now`
- `innodb_buffer_pool_dump_pct`
- `innodb_buffer_pool_filename`
- `innodb_buffer_pool_in_core_file`
- `innodb_buffer_pool_instances`
- `innodb_buffer_pool_load_abort`
- `innodb_buffer_pool_load_at_startup`
- `innodb_buffer_pool_load_now`
- `innodb_buffer_pool_size`
- `innodb_change_buffer_max_size`
- `innodb_change_buffering`

MySQL documents these variables as InnoDB startup/runtime options in the MySQL
8.4 Reference Manual, "InnoDB Startup Options and System Variables". Runtime
expectations below were independently verified against MySQL 8.4.9.

## MySQL 8.4.9 Behavior

All variables in this slice are global variables. Unqualified and `GLOBAL`
scalar reads return the global value. `SESSION` and `LOCAL` scalar reads fail
with error `1238/HY000` and a message containing
`Variable '<name>' is a GLOBAL variable`.

All variables appear in `SHOW VARIABLES`, `SHOW GLOBAL VARIABLES`, and
`SHOW SESSION VARIABLES`.

| Variable | Scalar value | `SHOW VARIABLES` value | SET behavior |
| --- | --- | --- | --- |
| `innodb_adaptive_flushing` | `1` | `ON` | Global dynamic |
| `innodb_adaptive_flushing_lwm` | `10` | `10` | Global dynamic |
| `innodb_adaptive_hash_index` | `0` | `OFF` | Global dynamic |
| `innodb_adaptive_hash_index_parts` | `8` | `8` | Read-only |
| `innodb_adaptive_max_sleep_delay` | `150000` | `150000` | Global dynamic |
| `innodb_autoextend_increment` | `64` | `64` | Global dynamic |
| `innodb_autoinc_lock_mode` | `2` | `2` | Read-only |
| `innodb_buffer_pool_chunk_size` | `134217728` | `134217728` | Read-only |
| `innodb_buffer_pool_dump_at_shutdown` | `1` | `ON` | Global dynamic |
| `innodb_buffer_pool_dump_now` | `0` | `OFF` | Global dynamic |
| `innodb_buffer_pool_dump_pct` | `25` | `25` | Global dynamic |
| `innodb_buffer_pool_filename` | `ib_buffer_pool` | `ib_buffer_pool` | Global dynamic |
| `innodb_buffer_pool_in_core_file` | `0` | `OFF` | Global dynamic |
| `innodb_buffer_pool_instances` | `1` | `1` | Read-only |
| `innodb_buffer_pool_load_abort` | `0` | `OFF` | Global dynamic |
| `innodb_buffer_pool_load_at_startup` | `1` | `ON` | Read-only |
| `innodb_buffer_pool_load_now` | `0` | `OFF` | Global dynamic |
| `innodb_buffer_pool_size` | `134217728` | `134217728` | Global dynamic |
| `innodb_change_buffer_max_size` | `25` | `25` | Global dynamic |
| `innodb_change_buffering` | `none` | `none` | Global dynamic |

For global dynamic variables, unqualified and `SESSION`/`LOCAL` assignment fail
with error `1229/HY000` and a message containing
`Variable '<name>' is a GLOBAL variable and should be set with SET GLOBAL`.
`SET GLOBAL <name> = DEFAULT` succeeds.

For read-only variables, assignment at any scope fails with error `1238/HY000`
and a message containing `Variable '<name>' is a read only variable`.

## MyLite Behavior

MyLite exposes fixed placeholders for the variables above:

- scalar reads match the MySQL 8.4.9 values in the table;
- `SHOW VARIABLES`, `SHOW GLOBAL VARIABLES`, and `SHOW SESSION VARIABLES`
  report the MySQL 8.4.9 display values;
- global-only scalar diagnostics match MySQL for `SESSION` and `LOCAL` reads;
- read-only assignment diagnostics match MySQL;
- dynamic global variables accept `SET GLOBAL <name> = DEFAULT` and exact
  assignments to the fixed MyLite value as no-ops;
- dynamic global variables reject other assignments with MyLite's standard
  fixed no-op unsupported diagnostic.

MyLite does not implement InnoDB buffer-pool, change-buffer, adaptive flushing,
adaptive hash, autoextend, or auto-increment lock-mode side effects. The values
are metadata compatibility placeholders for embedded applications and test
suites that inspect server configuration.

## Tests

- `packages/libmylite/tests/mysql_baseline_innodb_core_system_variables_expectations.sh`
  records MySQL 8.4.9 defaults, `SHOW VARIABLES`, scope diagnostics, read-only
  diagnostics, and global default assignment behavior.
- `packages/libmylite/tests/runtime_innodb_core_system_variables_test.c`
  verifies MyLite scalar reads, `SHOW VARIABLES`, global-only diagnostics,
  read-only diagnostics, fixed global no-op assignments, and rejection of
  unsupported non-default mutations.
