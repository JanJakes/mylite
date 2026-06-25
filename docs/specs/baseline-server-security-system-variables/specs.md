# Baseline Server Security System Variables

## Summary

This slice exposes an embedded-compatible baseline for stable MySQL server
path, network, TLS, and thread-policy system variables that applications often
inspect during bootstrap:

- `require_secure_transport`
- `secure_file_priv`
- `skip_external_locking`
- `skip_name_resolve`
- `skip_networking`
- `skip_show_database`
- `ssl_fips_mode`
- `ssl_session_cache_mode`
- `ssl_session_cache_timeout`
- `thread_handling`
- `tls_certificates_enforced_validation`
- `tls_version`
- `tmpdir`

MyLite supports scalar reads, `SHOW VARIABLES` / `SHOW GLOBAL VARIABLES` /
`SHOW SESSION VARIABLES` rows, global-only scalar scope diagnostics, read-only
assignment diagnostics for startup-only values, and fixed global no-op
assignment validation for the few dynamic variables whose server-global effects
do not exist in an embedded library. MyLite does not implement TLS negotiation,
secure-transport enforcement, host DNS policy, TCP/socket listener behavior,
temporary-directory placement, thread scheduling, startup option loading,
`SET PERSIST`, or Performance Schema variable tables.

## Compatibility Authority

- MySQL 8.4 Reference Manual, server system variables:
  <https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html>
- MySQL 8.4 Reference Manual, `SHOW VARIABLES`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-variables.html>
- MySQL 8.4.9 runtime observations captured by
  `packages/libmylite/tests/mysql_baseline_server_security_system_variables_expectations.sh`.

The official documentation defines server-variable scope, dynamic/read-only
classification, boolean display conventions, and `SHOW VARIABLES` behavior.
Runtime probes establish the exact defaults and diagnostics in the pinned
`mysql:8.4.9` comparison container. TLS certificate file variables such as
`ssl_ca`, `ssl_cert`, and `ssl_key` are intentionally excluded from this slice
because their runtime values depend on startup configuration and `SET GLOBAL
... = DEFAULT` changes them away from the container's configured values.

## MySQL 8.4.9 Observations

The target runtime reports these defaults:

| Variable | Scalar value | `SHOW VARIABLES` value |
| --- | --- | --- |
| `require_secure_transport` | `0` | `OFF` |
| `secure_file_priv` | `/var/lib/mysql-files/` | `/var/lib/mysql-files/` |
| `skip_external_locking` | `1` | `ON` |
| `skip_name_resolve` | `1` | `ON` |
| `skip_networking` | `0` | `OFF` |
| `skip_show_database` | `0` | `OFF` |
| `ssl_fips_mode` | `OFF` | `OFF` |
| `ssl_session_cache_mode` | `1` | `ON` |
| `ssl_session_cache_timeout` | `300` | `300` |
| `thread_handling` | `one-thread-per-connection` | same |
| `tls_certificates_enforced_validation` | `0` | `OFF` |
| `tls_version` | `TLSv1.2,TLSv1.3` | same |
| `tmpdir` | `/tmp` | `/tmp` |

Unscoped and `@@GLOBAL.name` scalar reads succeed. `@@SESSION.name` and
`@@LOCAL.name` scalar reads fail with `1238 / HY000` and a message that the
variable is global. `SHOW VARIABLES`, `SHOW GLOBAL VARIABLES`, and
`SHOW SESSION VARIABLES` all include these global variables.

Read-only startup variables reject all assignment forms with `1238 / HY000`
and a read-only-variable message. `require_secure_transport`,
`ssl_session_cache_mode`, `ssl_session_cache_timeout`, and `tls_version` are
dynamic global variables in MySQL: non-global `SET` forms fail with
`1229 / HY000`, while `SET GLOBAL` forms can change server-global state.

## MyLite Scope

MyLite supports:

- scalar default and `GLOBAL` reads for all variables in this slice;
- MySQL-style scalar `SESSION` / `LOCAL` global-variable diagnostics;
- `SHOW VARIABLES`, `SHOW SESSION VARIABLES`, `SHOW LOCAL VARIABLES`, and
  `SHOW GLOBAL VARIABLES` rows with MySQL-style display values;
- read-only assignment diagnostics for startup/read-only variables;
- fixed global no-op assignments for `require_secure_transport`,
  `ssl_session_cache_mode`, `ssl_session_cache_timeout`, and `tls_version`
  when the value is `DEFAULT` or the fixed MyLite default;
- deterministic rejection for global assignments that would mutate shared
  server-global networking or TLS state.

MyLite intentionally does not support:

- secure transport enforcement, TLS handshakes, certificate validation, TLS
  ciphersuite negotiation, FIPS mode, or SSL session cache behavior;
- network listener changes, DNS lookup policy, external locking, or
  `SHOW DATABASES` privilege filtering driven by these variables;
- host temporary-directory routing or MySQL file import/export enforcement;
- thread-pool or scheduler behavior;
- startup options, option files, persisted variables, privileges, or
  Performance Schema variable tables.

## Syntax

No new grammar is required. Existing MyLite productions already admit the
required forms:

```lemon
expr ::= SYSTEM_VARIABLE.
set_statement ::= SET set_assignment_list.
set_assignment ::= set_system_variable_target EQ set_value.
show_statement ::= SHOW show_scope_opt VARIABLES show_filter_opt.
```

Examples:

```sql
SELECT @@secure_file_priv, @@GLOBAL.tls_version;
SELECT @@skip_name_resolve, @@ssl_session_cache_timeout;
SHOW VARIABLES WHERE Variable_name IN ('require_secure_transport','tmpdir');
SHOW GLOBAL VARIABLES LIKE 'skip_%';
SET GLOBAL require_secure_transport = DEFAULT;
SET GLOBAL ssl_session_cache_mode = ON;
SET GLOBAL ssl_session_cache_timeout = 300;
SET GLOBAL tls_version = 'TLSv1.2,TLSv1.3';
```

## Diagnostics

- Unknown variables continue to use the existing `1193 / HY000` diagnostic.
- Scalar `@@SESSION` / `@@LOCAL` reads use `1238 / HY000` with
  `Variable '<name>' is a GLOBAL variable`.
- Non-global `SET` forms for the dynamic fixed placeholders use
  `1229 / HY000` with the MySQL global-variable message.
- Read-only startup variables use `1238 / HY000` with
  `Variable '<name>' is a read only variable`.
- Global assignments for dynamic placeholders admit only `DEFAULT` or the fixed
  default value. Other values return MyLite's deterministic unsupported
  fixed-no-op diagnostic.

## Runtime And Storage

This slice is entirely in MyLite's system-variable registry and `SET`
validation path. It does not add public ABI, SQLite SQL, a SQLite extension
hook, a fork patch, catalog descriptors, file-format state, VFS behavior, or
persistent mutable state. Values are constants and are independent of handles,
schemas, transactions, and database files.

## Tests

Coverage includes:

- a MySQL 8.4.9 expectation script for default values, `SHOW` visibility,
  scope diagnostics, read-only assignment diagnostics, and no-op global
  assignment behavior;
- a runtime C test for scalar reads, `SHOW` rows, global-only diagnostics,
  read-only assignment diagnostics, fixed global no-op assignments, and
  unsupported state-changing assignments;
- focused `SHOW VARIABLES` regression coverage through the existing registry
  test.
