# Baseline Admin Listener System Variables

## Summary

This slice exposes an embedded-compatible baseline for MySQL admin listener and
admin TLS system variables:

- `admin_address`
- `admin_port`
- `admin_ssl_ca`
- `admin_ssl_capath`
- `admin_ssl_cert`
- `admin_ssl_cipher`
- `admin_ssl_crl`
- `admin_ssl_crlpath`
- `admin_ssl_key`
- `admin_tls_ciphersuites`
- `admin_tls_version`
- `create_admin_listener_thread`

MyLite supports scalar default and `GLOBAL` reads, `SHOW VARIABLES` /
`SHOW GLOBAL VARIABLES` / `SHOW SESSION VARIABLES` rows, MySQL-style
global-only scalar scope diagnostics, read-only diagnostics for startup-only
values, and fixed global no-op assignment validation for dynamic admin TLS
placeholders. MyLite does not create a separate admin listener, bind sockets,
load TLS files, validate TLS material, or mutate process-global TLS state.

## Compatibility Authority

- MySQL 8.4 Reference Manual, server system variables:
  <https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html>
- MySQL 8.4.9 runtime observations captured by
  `packages/libmylite/tests/mysql_baseline_admin_listener_system_variables_expectations.sh`.

The official documentation defines the admin listener variable surface.
Runtime probes establish the exact target defaults, scalar `NULL` behavior,
`SHOW VARIABLES` display strings, scope diagnostics, and assignment behavior
in the pinned `mysql:8.4.9` comparison container.

## MySQL 8.4.9 Observations

The target runtime reports these defaults:

| Variable | Scalar value | `SHOW VARIABLES` value |
| --- | --- | --- |
| `admin_address` | `NULL` | empty string |
| `admin_port` | `33062` | `33062` |
| `admin_ssl_ca` | `NULL` | empty string |
| `admin_ssl_capath` | `NULL` | empty string |
| `admin_ssl_cert` | `NULL` | empty string |
| `admin_ssl_cipher` | `NULL` | empty string |
| `admin_ssl_crl` | `NULL` | empty string |
| `admin_ssl_crlpath` | `NULL` | empty string |
| `admin_ssl_key` | `NULL` | empty string |
| `admin_tls_ciphersuites` | `NULL` | empty string |
| `admin_tls_version` | `TLSv1.2,TLSv1.3` | same |
| `create_admin_listener_thread` | `0` | `OFF` |

Unscoped and `@@GLOBAL.name` scalar reads succeed. `@@SESSION.name` and
`@@LOCAL.name` scalar reads fail with `1238 / HY000` and a message that the
variable is global. `SHOW VARIABLES`, `SHOW GLOBAL VARIABLES`, and
`SHOW SESSION VARIABLES` all include these variables.

`admin_address`, `admin_port`, and `create_admin_listener_thread` are read-only
in the target runtime. The admin SSL/TLS path and version variables are dynamic
global variables in MySQL: non-global `SET` forms fail with `1229 / HY000`,
while `SET GLOBAL name = DEFAULT` succeeds and resets server-global listener
configuration.

## MyLite Scope

MyLite supports:

- scalar default and `GLOBAL` reads for all variables in this slice;
- SQL `NULL` scalar values for the admin address, SSL path, and ciphersuite
  defaults that are `NULL` in MySQL;
- blank `SHOW VARIABLES` display values for those same `NULL` defaults;
- MySQL-style scalar `SESSION` / `LOCAL` global-variable diagnostics;
- read-only assignment diagnostics for `admin_address`, `admin_port`, and
  `create_admin_listener_thread`;
- fixed global no-op assignments for admin SSL/TLS placeholders when the value
  is `DEFAULT`; `admin_tls_version` also admits the exact fixed default text.

MyLite intentionally does not support:

- separate admin TCP listener creation;
- binding `admin_address` or `admin_port`;
- TLS file loading, TLS ciphersuite changes, TLS version negotiation changes,
  certificate validation, or OpenSSL context mutation;
- persisted variables, option-file/startup changes, privileges, Performance
  Schema variable tables, or shared process-global state.

State-changing admin SSL/TLS assignments that MySQL accepts are rejected with
MyLite's deterministic fixed-no-op unsupported diagnostic.

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
SELECT @@admin_address, @@GLOBAL.admin_port;
SELECT @@admin_tls_version, @@GLOBAL.create_admin_listener_thread;
SHOW VARIABLES WHERE Variable_name IN ('admin_address','admin_tls_version');
SET GLOBAL admin_ssl_ca = DEFAULT;
SET GLOBAL admin_tls_version = 'TLSv1.2,TLSv1.3';
```

## Diagnostics

- Unknown variables continue to use the existing `1193 / HY000` diagnostic.
- Scalar `@@SESSION` / `@@LOCAL` reads use `1238 / HY000` with
  `Variable '<name>' is a GLOBAL variable`.
- Non-global `SET` forms for dynamic fixed placeholders use `1229 / HY000`
  with the MySQL global-variable message.
- Read-only startup variables use `1238 / HY000` with
  `Variable '<name>' is a read only variable`.
- Global assignments for dynamic placeholders admit only `DEFAULT`, plus the
  exact `admin_tls_version` default text. Other values return MyLite's
  deterministic unsupported fixed-no-op diagnostic.

## Runtime And Storage

This slice is entirely in MyLite's system-variable registry and `SET`
validation path. It does not add public ABI, SQLite SQL, a SQLite extension
hook, a fork patch, catalog descriptors, file-format state, VFS behavior, or
persistent mutable state. Values are constants and are independent of handles,
schemas, transactions, and database files.

## Tests

Coverage includes:

- a MySQL 8.4.9 expectation script for default values, `SHOW` visibility,
  scope diagnostics, read-only assignment diagnostics, and global default
  no-op behavior;
- a runtime C test for scalar reads, scalar `NULL`s, `SHOW` rows, global-only
  diagnostics, read-only assignment diagnostics, fixed global no-op
  assignments, unsupported state-changing assignments, and user-variable
  assignment diagnostics;
- focused `SHOW VARIABLES` regression coverage through the existing registry
  test.
