# Baseline SHOW STATUS SSL Placeholders

## Summary

MyLite exposes the MySQL 8.4.9 `Ssl_%` status row-name surface as deterministic
embedded no-TLS placeholders. This gives applications a stable introspection
shape without implying that the embedded runtime has a MySQL server TLS layer,
certificate state, or SSL session cache.

## MySQL 8.4.9 Observations

Observed against the local `mysql:8.4.9` runtime:

```sql
SHOW STATUS LIKE 'Ssl\_%';
```

returns these row names in order for default, `SESSION`, `LOCAL`, and `GLOBAL`
scopes:

| Variable_name |
| --- |
| `Ssl_accept_renegotiates` |
| `Ssl_accepts` |
| `Ssl_callback_cache_hits` |
| `Ssl_cipher` |
| `Ssl_cipher_list` |
| `Ssl_client_connects` |
| `Ssl_connect_renegotiates` |
| `Ssl_ctx_verify_depth` |
| `Ssl_ctx_verify_mode` |
| `Ssl_default_timeout` |
| `Ssl_finished_accepts` |
| `Ssl_finished_connects` |
| `Ssl_server_not_after` |
| `Ssl_server_not_before` |
| `Ssl_session_cache_hits` |
| `Ssl_session_cache_misses` |
| `Ssl_session_cache_mode` |
| `Ssl_session_cache_overflows` |
| `Ssl_session_cache_size` |
| `Ssl_session_cache_timeout` |
| `Ssl_session_cache_timeouts` |
| `Ssl_sessions_reused` |
| `Ssl_used_session_cache_entries` |
| `Ssl_verify_depth` |
| `Ssl_verify_mode` |
| `Ssl_version` |

Values vary with server configuration, current connection TLS mode, and server
uptime. MyLite therefore verifies MySQL row names and order, while MyLite
runtime tests pin deterministic embedded values.

## MyLite Behavior

All 26 `Ssl_%` rows are visible in default, `SESSION`, `LOCAL`, and `GLOBAL`
`SHOW STATUS` scopes. Numeric counters and limits return `0`. TLS text fields
return an empty string:

- `Ssl_cipher`
- `Ssl_cipher_list`
- `Ssl_server_not_after`
- `Ssl_server_not_before`
- `Ssl_session_cache_mode`
- `Ssl_version`

The rows are also exposed through `sys.metrics` as lowercase `Global Status`
metrics with `Enabled = 'YES'`.

## Non-Goals

- No TLS handshake, certificate, cipher, verification, or session-cache state.
- No MySQL protocol TLS negotiation.
- No live Performance Schema SSL instrumentation.

## Verification

- `packages/libmylite/tests/mysql_baseline_show_status_expectations.sh`
  verifies MySQL 8.4.9 row names and order.
- `packages/libmylite/tests/runtime_show_status_test.c` verifies MyLite result
  rows and values for default, local, and global scopes.
- `packages/libmylite/tests/runtime_sys_metrics_view_test.c` verifies a
  representative `sys.metrics` row and the global metric count.
