# Baseline SHOW STATUS Connection Diagnostics

## Summary

This slice expands the `SHOW STATUS` placeholder registry with the MySQL 8.4.9
`Connection_%` connection-control and connection-error diagnostic rows:

```sql
SHOW [GLOBAL | SESSION | LOCAL] STATUS LIKE 'Connection\_%'
```

MyLite exposes the observed row names in MySQL runtime order for
default/session/local and global scopes. Values are deterministic embedded `0`
placeholders. This is status metadata only; it does not implement live
connection-control delay accounting, failed-connection accounting, authentication
state, networking, or protocol-level connection lifecycle tracking.

## Compatibility Authority

- MySQL 8.4 Reference Manual, `SHOW STATUS`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-status.html>
- MySQL 8.4 Reference Manual, server status variables:
  <https://dev.mysql.com/doc/refman/8.4/en/server-status-variable-reference.html>
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_show_status_expectations.sh`.

Runtime probes against the local `mylite-mysql-849` MySQL 8.4.9 container
verified these `Connection_%` rows:

| Variable |
| --- |
| `Connection_control_delay_generated` |
| `Connection_control_exempted_unknown_users` |
| `Connection_errors_accept` |
| `Connection_errors_internal` |
| `Connection_errors_max_connections` |
| `Connection_errors_peer_address` |
| `Connection_errors_select` |
| `Connection_errors_tcpwrap` |

## Supported Behavior

Each row is visible through default, `SESSION`, `LOCAL`, and `GLOBAL`
`SHOW STATUS` scopes and has a fixed MyLite value of `0`. Existing `LIKE` and
limited `WHERE` filtering semantics apply.

`sys.metrics` is derived from the global-visible status descriptor set, so
these rows also appear as lowercase `Global Status` metrics with value `0`.

## Unsupported Behavior

This slice intentionally does not add:

- live connection error counters;
- live connection-control delay or exemption accounting;
- authentication/network/TLS protocol tracking;
- failed connection simulation;
- `FLUSH STATUS` reset behavior;
- Performance Schema status-variable table rows beyond MyLite's current
  synthetic descriptor-derived surfaces.

## Tests

Fast C tests assert exact `SHOW STATUS LIKE 'Connection\_%'` rows for session
and global scopes, deterministic `0` values, and representative `sys.metrics`
readback. The MySQL expectation script verifies the row-name set and scope
visibility against MySQL 8.4.9 without depending on live mutable counter values.
