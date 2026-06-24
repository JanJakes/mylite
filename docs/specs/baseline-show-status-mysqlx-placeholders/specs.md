# Baseline SHOW STATUS Mysqlx Placeholders

## Scope

This slice adds MySQL 8.4.9-shaped `SHOW STATUS LIKE 'Mysqlx%'` rows exposed
by the pinned comparison runtime. MyLite does not implement MySQL X Plugin,
X Protocol, Document Store, or an X Plugin listener, so these rows are
deterministic embedded placeholders rather than live protocol instrumentation.

All 78 rows observed in the pinned MySQL 8.4.9 runtime are visible in default,
`SESSION`, `LOCAL`, and `GLOBAL` scopes. Counter rows return `0`. String
session-state rows return the empty string. Listener state uses deterministic
disabled placeholders:

| Variable | MyLite value |
| --- | --- |
| `Mysqlx_address` | `UNDEFINED` |
| `Mysqlx_port` | `UNDEFINED` |
| `Mysqlx_socket` | empty string |
| `Mysqlx_worker_threads` | `0` |

`Mysqlx_ssl_ctx_verify_depth` and `Mysqlx_ssl_ctx_verify_mode` return `0`;
certificate date rows return the empty string.

## Sources

- MySQL 8.4 Reference Manual, `SHOW STATUS`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-status.html>
- MySQL 8.4 Reference Manual, X Plugin status variables:
  <https://dev.mysql.com/doc/refman/8.4/en/x-plugin-status-variables.html>
- MySQL 8.4.9 runtime probes in
  `packages/libmylite/tests/mysql_baseline_show_status_expectations.sh`.

## Semantics

`SHOW STATUS LIKE 'Mysqlx%'` exposes the row names and scope visibility
observed from MySQL 8.4.9. `sys.metrics` exposes the same rows as lowercase
`Global Status` metrics because they are global-visible.

The values are placeholders. They do not imply that MyLite accepts X Protocol
connections, has Document Store collections, has X Plugin worker threads, or
maintains X Plugin SSL and message counters.

## Unsupported Behavior

This slice does not implement X Plugin startup, X Protocol connections,
Document Store APIs, X Plugin SQL execution, listener sockets, compression
state, SSL certificates, `FLUSH STATUS` counter reset behavior, or live
Performance Schema instrumentation.
