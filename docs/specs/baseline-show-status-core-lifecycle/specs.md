# Baseline SHOW STATUS Core Lifecycle Placeholders

## Summary

This slice verifies and documents MyLite's existing `SHOW STATUS` rows for the
small core lifecycle group commonly inspected by client libraries and framework
diagnostics:

```sql
SHOW [GLOBAL | SESSION | LOCAL] STATUS LIKE 'Bytes\_%';
SHOW [GLOBAL | SESSION | LOCAL] STATUS LIKE 'Connections';
SHOW STATUS LIKE 'Compression';
SHOW [SESSION | LOCAL] STATUS LIKE 'Compression';
SHOW [GLOBAL | SESSION | LOCAL] STATUS LIKE 'Prepared_stmt_count';
SHOW [GLOBAL | SESSION | LOCAL] STATUS LIKE 'Queries';
SHOW [GLOBAL | SESSION | LOCAL] STATUS LIKE 'Questions';
SHOW [GLOBAL | SESSION | LOCAL] STATUS LIKE 'Threads\_%';
SHOW [GLOBAL | SESSION | LOCAL] STATUS LIKE 'Uptime%';
```

MyLite exposes the observed MySQL 8.4.9 row names in the supported scopes.
Values are deterministic embedded placeholders: byte, query, prepared-statement,
and uptime counters are `0`; `Connections`, `Threads_connected`,
`Threads_created`, and `Threads_running` are `1`; `Threads_cached` is `0`; and
session/local `Compression` is `OFF`. This is status metadata only.

## Compatibility Authority

- MySQL 8.4 Reference Manual, `SHOW STATUS`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-status.html>
- MySQL 8.4 Reference Manual, server status variables:
  <https://dev.mysql.com/doc/refman/8.4/en/server-status-variable-reference.html>
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_show_status_expectations.sh`.

Runtime probes against the local `mylite-mysql-849` MySQL 8.4.9 container
verified these row sets:

| Pattern | Variables |
| --- | --- |
| `Bytes\_%` | `Bytes_received`, `Bytes_sent` |
| `Connections` | `Connections` |
| `Compression` | `Compression` in default/session/local scopes only |
| `Prepared_stmt_count` | `Prepared_stmt_count` |
| `Queries` | `Queries` |
| `Questions` | `Questions` |
| `Threads\_%` | `Threads_cached`, `Threads_connected`, `Threads_created`, `Threads_running` |
| `Uptime%` | `Uptime`, `Uptime_since_flush_status` |

The MySQL runtime values are live process/session counters and are intentionally
not pinned as MyLite values.

## Supported Behavior

The core lifecycle rows are visible through the same `SHOW STATUS` registry as
the rest of MyLite's baseline status placeholders. Existing `LIKE` and limited
`WHERE` filtering semantics apply.

`Compression` is omitted from `GLOBAL` scope to match MySQL 8.4.9. The other
rows in this slice are visible through default, `SESSION`, `LOCAL`, and
`GLOBAL` scopes. Global-visible rows also appear in `sys.metrics` as lowercase
`Global Status` metrics derived from the descriptor registry.

## Unsupported Behavior

This slice intentionally does not add:

- live network byte accounting;
- live statement, query, prepared-statement, or uptime counters;
- protocol compression negotiation;
- server thread cache, thread pool, or concurrent connection accounting;
- `FLUSH STATUS` reset behavior;
- Performance Schema status-variable table rows beyond current synthetic
  descriptor-derived surfaces.

## Tests

Fast C tests assert exact MyLite rows and deterministic embedded values for the
core lifecycle patterns and representative scopes. The MySQL expectation script
verifies row names, scope visibility, and numeric value shapes against MySQL
8.4.9 without depending on mutable live counter values.
