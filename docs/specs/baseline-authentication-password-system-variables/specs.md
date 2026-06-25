# Baseline Authentication And Password System Variables

## Summary

This slice exposes an embedded-compatible baseline for MySQL authentication
plugin and password-policy system variables that applications may inspect
during startup:

- `authentication_policy`
- `caching_sha2_password_auto_generate_rsa_keys`
- `caching_sha2_password_digest_rounds`
- `caching_sha2_password_private_key_path`
- `caching_sha2_password_public_key_path`
- `default_password_lifetime`
- `disconnect_on_expired_password`
- `mysql_native_password_proxy_users`
- `password_history`
- `password_require_current`
- `password_reuse_interval`
- `sha256_password_auto_generate_rsa_keys`
- `sha256_password_private_key_path`
- `sha256_password_proxy_users`
- `sha256_password_public_key_path`

MyLite supports scalar reads, `SHOW VARIABLES` / `SHOW GLOBAL VARIABLES` /
`SHOW SESSION VARIABLES` rows, global-only scalar scope diagnostics, read-only
assignment diagnostics for startup/read-only values, and fixed global no-op
assignment validation for dynamic global password-policy placeholders whose
server-global effects do not exist in an embedded library. MyLite does not
implement authentication plugin negotiation, RSA key generation/loading,
password expiration or history enforcement, proxy users, account metadata
changes, persisted variables, or Performance Schema variable tables.

## Compatibility Authority

- MySQL 8.4 Reference Manual, server system variables:
  <https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html>
- MySQL 8.4 Reference Manual, caching SHA-2 authentication:
  <https://dev.mysql.com/doc/refman/8.4/en/caching-sha2-pluggable-authentication.html>
- MySQL 8.4 Reference Manual, SHA-256 authentication:
  <https://dev.mysql.com/doc/refman/8.4/en/sha256-pluggable-authentication.html>
- MySQL 8.4.9 runtime observations captured by
  `packages/libmylite/tests/mysql_baseline_authentication_password_system_variables_expectations.sh`.

The official documentation defines variable scope, dynamic/read-only
classification, authentication-plugin context, and display conventions. Runtime
probes establish the exact defaults and diagnostics in the pinned `mysql:8.4.9`
comparison container.

## MySQL 8.4.9 Observations

The target runtime reports these defaults:

| Variable | Scalar value | `SHOW VARIABLES` value |
| --- | --- | --- |
| `authentication_policy` | `*,,` | same |
| `caching_sha2_password_auto_generate_rsa_keys` | `1` | `ON` |
| `caching_sha2_password_digest_rounds` | `5000` | `5000` |
| `caching_sha2_password_private_key_path` | `private_key.pem` | same |
| `caching_sha2_password_public_key_path` | `public_key.pem` | same |
| `default_password_lifetime` | `0` | `0` |
| `disconnect_on_expired_password` | `1` | `ON` |
| `mysql_native_password_proxy_users` | `0` | `OFF` |
| `password_history` | `0` | `0` |
| `password_require_current` | `0` | `OFF` |
| `password_reuse_interval` | `0` | `0` |
| `sha256_password_auto_generate_rsa_keys` | `1` | `ON` |
| `sha256_password_private_key_path` | `private_key.pem` | same |
| `sha256_password_proxy_users` | `0` | `OFF` |
| `sha256_password_public_key_path` | `public_key.pem` | same |

Unscoped and `@@GLOBAL.name` scalar reads succeed. `@@SESSION.name` and
`@@LOCAL.name` scalar reads fail with `1238 / HY000` and a message that the
variable is global. `SHOW VARIABLES`, `SHOW GLOBAL VARIABLES`, and
`SHOW SESSION VARIABLES` all include these global variables.

The caching SHA-2 key/digest variables, `disconnect_on_expired_password`, and
the SHA-256 key variables reject assignment with `1238 / HY000` and a
read-only-variable message. `authentication_policy`,
`default_password_lifetime`, `mysql_native_password_proxy_users`,
`password_history`, `password_require_current`, `password_reuse_interval`, and
`sha256_password_proxy_users` are dynamic global variables in MySQL:
non-global `SET` forms fail with `1229 / HY000`, while `SET GLOBAL` forms can
change server-global state.

`generated_random_password_length` is intentionally excluded from this slice
because MySQL exposes it as mutable session/global state, not as a fixed
global-only placeholder.

## MyLite Scope

MyLite supports:

- scalar default and `GLOBAL` reads for all variables in this slice;
- MySQL-style scalar `SESSION` / `LOCAL` global-variable diagnostics;
- `SHOW VARIABLES`, `SHOW SESSION VARIABLES`, `SHOW LOCAL VARIABLES`, and
  `SHOW GLOBAL VARIABLES` rows with MySQL-style display values;
- read-only assignment diagnostics for startup/read-only variables;
- fixed global no-op assignments for the dynamic global placeholders when the
  value is `DEFAULT` or the fixed MyLite default;
- deterministic rejection for global assignments that would mutate shared
  password or authentication policy state.

MyLite intentionally does not support:

- authentication plugin negotiation, RSA key generation, RSA key file loading,
  password exchange, password hashing, or credential validation;
- password expiration, reuse, history, or current-password enforcement;
- proxy-user behavior;
- account metadata side effects, startup options, option files, privileges,
  persisted variables, or Performance Schema variable tables.

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
SELECT @@authentication_policy, @@GLOBAL.default_password_lifetime;
SELECT @@caching_sha2_password_digest_rounds, @@sha256_password_public_key_path;
SHOW VARIABLES WHERE Variable_name IN ('authentication_policy','password_history');
SHOW GLOBAL VARIABLES LIKE 'password_%';
SET GLOBAL authentication_policy = DEFAULT;
SET GLOBAL default_password_lifetime = 0;
SET GLOBAL password_require_current = OFF;
```

## Diagnostics

- Unknown variables continue to use the existing `1193 / HY000` diagnostic.
- Scalar `@@SESSION` / `@@LOCAL` reads use `1238 / HY000` with
  `Variable '<name>' is a GLOBAL variable`.
- Non-global `SET` forms for dynamic fixed placeholders use `1229 / HY000`
  with the MySQL global-variable message.
- Read-only startup/plugin-key variables use `1238 / HY000` with
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
