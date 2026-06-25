# Baseline Server TLS System Variables

## Summary

This slice exposes an embedded-compatible baseline for MySQL server TLS
certificate and ciphersuite system variables:

- `ssl_ca`
- `ssl_capath`
- `ssl_cert`
- `ssl_cipher`
- `ssl_crl`
- `ssl_crlpath`
- `ssl_key`
- `tls_ciphersuites`

MyLite does not implement a server listener, TLS handshakes, certificate-file
loading, ciphersuite negotiation, option-file startup state, persisted
variables, or `ALTER INSTANCE RELOAD TLS`. The supported baseline is the MySQL
8.4.9 default-state shape: scalar reads return SQL `NULL`, `SHOW VARIABLES`
prints an empty value, `SESSION` / `LOCAL` scalar reads use MySQL's global-only
diagnostic, and `SET GLOBAL name = DEFAULT` / `SET GLOBAL name = NULL` are
accepted no-ops.

## Compatibility Authority

- MySQL 8.4 Reference Manual, server system variables:
  <https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html>
- MySQL 8.4 Reference Manual, `SHOW VARIABLES`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-variables.html>
- MySQL 8.4.9 runtime observations captured by
  `packages/libmylite/tests/mysql_baseline_server_tls_system_variables_expectations.sh`.

The official manual identifies these variables as global, dynamic server TLS
configuration variables with default value `NULL`. Runtime probes establish the
exact pinned-container diagnostics and the distinction between SQL `NULL`
scalar readback and blank `SHOW VARIABLES` display.

## MySQL 8.4.9 Observations

The comparison container may start with certificate filenames such as `ca.pem`,
`server-cert.pem`, and `server-key.pem`. After `SET GLOBAL name = DEFAULT`, the
target variables return to MySQL's default-state value:

| Variable | Scalar value | `SHOW VARIABLES` value |
| --- | --- | --- |
| `ssl_ca` | `NULL` | empty string |
| `ssl_capath` | `NULL` | empty string |
| `ssl_cert` | `NULL` | empty string |
| `ssl_cipher` | `NULL` | empty string |
| `ssl_crl` | `NULL` | empty string |
| `ssl_crlpath` | `NULL` | empty string |
| `ssl_key` | `NULL` | empty string |
| `tls_ciphersuites` | `NULL` | empty string |

Unscoped and `@@GLOBAL.name` scalar reads succeed. `@@SESSION.name` and
`@@LOCAL.name` fail with `1238 / HY000` and a message that the variable is
global. `SHOW VARIABLES`, `SHOW GLOBAL VARIABLES`, and `SHOW SESSION
VARIABLES` all include rows for these global variables.

`SET name = DEFAULT` fails with `1229 / HY000` because the variables are
global-only. `SET GLOBAL name = DEFAULT` and `SET GLOBAL name = NULL` succeed
and leave the default scalar value as `NULL`. MySQL also accepts some
state-changing path or ciphersuite assignments, including empty-string
assignments for several variables, but those assignments mutate server-global
TLS state and are outside MyLite's embedded baseline.

## MyLite Scope

MyLite supports:

- scalar unscoped and `GLOBAL` reads returning SQL `NULL`;
- `SHOW VARIABLES`, `SHOW GLOBAL VARIABLES`, and `SHOW SESSION VARIABLES` rows
  with blank display values;
- MySQL-style global-only `SESSION` / `LOCAL` scalar diagnostics;
- MySQL-style non-global `SET` diagnostics;
- no-op `SET GLOBAL name = DEFAULT`;
- no-op `SET GLOBAL name = NULL`;
- deterministic unsupported diagnostics for assignments that would mutate TLS
  path or ciphersuite state, including values supplied through user variables.

MyLite intentionally does not support:

- TLS listener or wire-protocol encryption behavior;
- certificate, key, CA, CRL, directory, or ciphersuite validation;
- `ALTER INSTANCE RELOAD TLS` effects;
- persisted system variables, option-file startup state, or privileges;
- mutable process-global TLS configuration.

## Syntax

No new grammar is required. Existing system-variable expressions, `SET`, and
`SHOW VARIABLES` productions admit the required forms:

```sql
SELECT @@ssl_ca, @@GLOBAL.tls_ciphersuites;
SHOW VARIABLES LIKE 'ssl_%';
SET GLOBAL ssl_ca = DEFAULT;
SET GLOBAL tls_ciphersuites = NULL;
```

## Diagnostics

- Unknown variables continue to use the existing `1193 / HY000` diagnostic.
- `@@SESSION` / `@@LOCAL` reads use `1238 / HY000` with
  `Variable '<name>' is a GLOBAL variable`.
- Non-global `SET` forms use `1229 / HY000` with the MySQL global-variable
  message.
- Global assignments other than `DEFAULT` and literal `NULL` use MyLite's
  deterministic fixed-no-op unsupported diagnostic.
- User-variable-backed global assignments use a more specific unsupported
  diagnostic for server TLS system variables.

## Runtime And Storage

This slice is entirely in MyLite's system-variable registry and `SET`
validation path. It does not add public ABI, SQLite SQL, SQLite extension API
use, a SQLite fork hook, catalog state, file-format state, VFS behavior,
persistent state, or process-global mutable state.

## Tests

Coverage includes:

- a MySQL 8.4.9 expectation script for default-state values, blank `SHOW`
  display, scope diagnostics, non-global `SET` diagnostics, and no-op
  `DEFAULT` / `NULL` global assignments;
- a runtime C test for scalar SQL `NULL`s, `SHOW` rows, scope diagnostics,
  fixed global no-op assignment forms, unsupported state-changing assignments,
  and user-variable assignment diagnostics;
- full `SHOW VARIABLES` registry regression coverage.
