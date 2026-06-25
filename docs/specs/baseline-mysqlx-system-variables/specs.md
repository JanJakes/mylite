# Baseline X Plugin System Variables

## Summary

This slice exposes MySQL 8.4.9-shaped metadata and diagnostics for the X Plugin
system-variable surface whose names begin with `mysqlx_`. MyLite does not
implement the X Protocol, Document Store, the X Plugin listener, network
compression, or X Plugin TLS state. Values are embedded placeholders chosen to
match the pinned MySQL 8.4.9 default runtime.

## Compatibility Authority

- MySQL 8.4 Reference Manual, server system variables:
  <https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html>
- MySQL 8.4.9 runtime observations captured by
  `packages/libmylite/tests/mysql_baseline_mysqlx_system_variables_expectations.sh`.

Runtime probes establish the default scalar values, `SHOW VARIABLES` display
values, scope diagnostics, read-only diagnostics, and `DEFAULT` assignment
behavior for the pinned `mysql:8.4.9` comparison container.

## MyLite Scope

MyLite supports:

- scalar default and `GLOBAL` reads for all `mysqlx_*` variables in the target
  runtime;
- SQL `NULL` scalar values for the X Plugin SSL path variables that are `NULL`
  in MySQL, with blank `SHOW VARIABLES` display values;
- `SHOW VARIABLES`, `SHOW GLOBAL VARIABLES`, and `SHOW SESSION VARIABLES` rows;
- MySQL-style global-only `SESSION` / `LOCAL` scalar diagnostics for variables
  that are global-only in MySQL;
- read-only assignment diagnostics for startup-only X Plugin variables;
- fixed global no-op `SET GLOBAL name = DEFAULT` for dynamic global X Plugin
  placeholders;
- handle-local session `SET` / `SET DEFAULT` for `mysqlx_read_timeout`,
  `mysqlx_wait_timeout`, and `mysqlx_write_timeout`.

MyLite intentionally does not support:

- X Protocol handshakes or packets;
- X Plugin listener socket binding;
- X Plugin compression or TLS runtime behavior;
- live X Plugin worker, timeout, or connection accounting;
- process-global mutable X Plugin state, persisted variables, option files, or
  privilege enforcement.

State-changing global assignments that MySQL accepts are rejected with MyLite's
deterministic fixed-no-op unsupported diagnostic, except for `DEFAULT` resets.

## Variables

| Variable | Scalar default | `SHOW VARIABLES` value | Scope | Assignment |
| --- | --- | --- | --- | --- |
| `mysqlx_bind_address` | `*` | `*` | global | read-only |
| `mysqlx_compression_algorithms` | `DEFLATE_STREAM,LZ4_MESSAGE,ZSTD_STREAM` | same | global | fixed global `DEFAULT` no-op |
| `mysqlx_connect_timeout` | `30` | `30` | global | fixed global `DEFAULT` no-op |
| `mysqlx_deflate_default_compression_level` | `3` | `3` | global | fixed global `DEFAULT` no-op |
| `mysqlx_deflate_max_client_compression_level` | `5` | `5` | global | fixed global `DEFAULT` no-op |
| `mysqlx_document_id_unique_prefix` | `0` | `0` | global | fixed global `DEFAULT` no-op |
| `mysqlx_enable_hello_notice` | `1` | `ON` | global | fixed global `DEFAULT` no-op |
| `mysqlx_idle_worker_thread_timeout` | `60` | `60` | global | fixed global `DEFAULT` no-op |
| `mysqlx_interactive_timeout` | `28800` | `28800` | global | fixed global `DEFAULT` no-op |
| `mysqlx_lz4_default_compression_level` | `2` | `2` | global | fixed global `DEFAULT` no-op |
| `mysqlx_lz4_max_client_compression_level` | `8` | `8` | global | fixed global `DEFAULT` no-op |
| `mysqlx_max_allowed_packet` | `67108864` | `67108864` | global | fixed global `DEFAULT` no-op |
| `mysqlx_max_connections` | `100` | `100` | global | fixed global `DEFAULT` no-op |
| `mysqlx_min_worker_threads` | `2` | `2` | global | fixed global `DEFAULT` no-op |
| `mysqlx_port` | `33060` | `33060` | global | read-only |
| `mysqlx_port_open_timeout` | `0` | `0` | global | read-only |
| `mysqlx_read_timeout` | `30` | `30` | global/session | handle-local session subset |
| `mysqlx_socket` | `/var/run/mysqld/mysqlx.sock` | same | global | read-only |
| `mysqlx_ssl_ca` | `NULL` | empty string | global | read-only |
| `mysqlx_ssl_capath` | `NULL` | empty string | global | read-only |
| `mysqlx_ssl_cert` | `NULL` | empty string | global | read-only |
| `mysqlx_ssl_cipher` | `NULL` | empty string | global | read-only |
| `mysqlx_ssl_crl` | `NULL` | empty string | global | read-only |
| `mysqlx_ssl_crlpath` | `NULL` | empty string | global | read-only |
| `mysqlx_ssl_key` | `NULL` | empty string | global | read-only |
| `mysqlx_wait_timeout` | `28800` | `28800` | global/session | handle-local session subset |
| `mysqlx_write_timeout` | `60` | `60` | global/session | handle-local session subset |
| `mysqlx_zstd_default_compression_level` | `3` | `3` | global | fixed global `DEFAULT` no-op |
| `mysqlx_zstd_max_client_compression_level` | `11` | `11` | global | fixed global `DEFAULT` no-op |

## Syntax

No new grammar is required. Existing system-variable expressions, `SET`, and
`SHOW VARIABLES` productions admit the required forms:

```sql
SELECT @@mysqlx_port, @@GLOBAL.mysqlx_ssl_ca;
SHOW VARIABLES LIKE 'mysqlx_%';
SET GLOBAL mysqlx_connect_timeout = DEFAULT;
SET mysqlx_read_timeout = 31;
```

## Diagnostics

- Unknown variables continue to use the existing `1193 / HY000` diagnostic.
- Global-only scalar `@@SESSION` / `@@LOCAL` reads use `1238 / HY000` with
  `Variable '<name>' is a GLOBAL variable`.
- Non-global `SET` forms for fixed global placeholders use `1229 / HY000`.
- Startup-only variables use `1238 / HY000` read-only diagnostics for both
  non-global and global `SET`.
- Unsupported state-changing global assignments use MyLite's fixed-no-op
  unsupported diagnostic.

## Runtime And Storage

This slice is entirely in the system-variable registry and `SET` validation
path. It adds no public ABI, SQLite SQL, SQLite fork patch, persistent
file-format state, VFS behavior, X Protocol runtime, or process-global mutable
state.

## Tests

Coverage includes:

- a MySQL 8.4.9 expectation script for defaults, `SHOW` rows, scope diagnostics,
  read-only diagnostics, global `DEFAULT` no-op behavior, and session timeout
  assignment behavior;
- a runtime C test for scalar reads, SQL `NULL`s, `SHOW` rows, scope
  diagnostics, read-only diagnostics, fixed global no-op assignments, session
  timeout overrides, and unsupported state-changing assignments;
- focused `SHOW VARIABLES` regression coverage through the existing full-row
  registry test.
