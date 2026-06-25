# Baseline Bootstrap System Variables

## Summary

This slice exposes MySQL 8.4.9-shaped metadata placeholders for eight server
bootstrap and compatibility system variables:

- `activate_all_roles_on_login`
- `auto_generate_certs`
- `automatic_sp_privileges`
- `block_encryption_mode`
- `build_id`
- `bulk_insert_buffer_size`
- `character_sets_dir`
- `check_proxy_users`

MyLite supports default scalar reads, `SHOW VARIABLES` rows, scalar scope
diagnostics, read-only diagnostics, and exact/default no-op `SET` forms for
the dynamic variables. It does not implement role activation, automatic stored
routine privilege grants, AES encryption-mode effects, MySQL build discovery,
bulk-insert buffering, character-set file loading, proxy-user checks, startup
options, or mutable server-global state.

## Compatibility Authority

- MySQL 8.4 Reference Manual, server system variables:
  <https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html>
- MySQL 8.4 Reference Manual, `SHOW VARIABLES`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-variables.html>
- MySQL 8.4.9 runtime observations captured by
  `packages/libmylite/tests/mysql_baseline_bootstrap_system_variables_expectations.sh`.

The manual defines variable names, scope, dynamic/read-only classification,
and documented feature interactions. Runtime probes establish pinned 8.4.9
defaults, `SHOW` display values, assignment diagnostics, and session-scope
behavior.

## MySQL 8.4.9 Observations

| Variable | Scalar value | `SHOW VARIABLES` value | Scalar scope |
| --- | --- | --- | --- |
| `activate_all_roles_on_login` | `0` | `OFF` | global |
| `auto_generate_certs` | `1` | `ON` | global |
| `automatic_sp_privileges` | `1` | `ON` | global |
| `block_encryption_mode` | `aes-128-ecb` | `aes-128-ecb` | global/session |
| `build_id` | `66e221b3840955d27f740799b5b2c6eb0baf3283` | same | global |
| `bulk_insert_buffer_size` | `8388608` | `8388608` | global/session |
| `character_sets_dir` | `/usr/share/mysql-8.4/charsets/` | same | global |
| `check_proxy_users` | `0` | `OFF` | global |

All variables appear in `SHOW VARIABLES`, `SHOW GLOBAL VARIABLES`, and
`SHOW SESSION VARIABLES`. Global-only variables reject explicit
`@@SESSION.name` reads with `1238 / HY000`. `auto_generate_certs`,
`build_id`, and `character_sets_dir` reject all `SET` forms with read-only
diagnostics. Dynamic global-only variables reject non-global `SET` forms with
`1229 / HY000`. MySQL can mutate `block_encryption_mode`,
`bulk_insert_buffer_size`, `activate_all_roles_on_login`,
`automatic_sp_privileges`, and `check_proxy_users`.

## MyLite Scope

MyLite supports:

- unscoped and `GLOBAL` scalar reads for all variables;
- `SESSION` / `LOCAL` scalar reads for `block_encryption_mode` and
  `bulk_insert_buffer_size`;
- MySQL-style scalar `SESSION` / `LOCAL` diagnostics for the global-only
  variables;
- `SHOW VARIABLES`, `SHOW GLOBAL VARIABLES`, and `SHOW SESSION VARIABLES`
  rows with MySQL-shaped display values;
- read-only diagnostics for `auto_generate_certs`, `build_id`, and
  `character_sets_dir`;
- exact/default no-op assignment forms for `activate_all_roles_on_login`,
  `automatic_sp_privileges`, `block_encryption_mode`,
  `bulk_insert_buffer_size`, and `check_proxy_users`;
- deterministic unsupported diagnostics for state-changing assignments and
  user-variable-backed assignments.

MyLite intentionally does not support:

- mutable global or session state for this batch;
- role activation on login or privilege graph side effects;
- automatic stored routine privilege grants;
- AES function behavior controlled by `block_encryption_mode`;
- bulk-insert buffering, MyISAM tuning, or loader performance changes;
- character-set directory probing or character-set definition loading;
- proxy-user authentication checks;
- startup options, option files, persisted variables, privilege checks, or
  Performance Schema variable tables.

## Syntax

No new grammar is required. Existing system-variable, `SHOW VARIABLES`, and
`SET` productions already admit the supported forms:

```sql
SELECT @@block_encryption_mode, @@GLOBAL.bulk_insert_buffer_size;
SHOW VARIABLES LIKE 'build_id';
SET GLOBAL activate_all_roles_on_login = DEFAULT;
SET SESSION block_encryption_mode = 'aes-128-ecb';
SET bulk_insert_buffer_size = 8388608;
```

## Diagnostics

- Unknown variables continue to use `1193 / HY000`.
- `@@SESSION` / `@@LOCAL` reads of global-only variables use
  `1238 / HY000`, `Variable '<name>' is a GLOBAL variable`.
- Non-global `SET` forms for dynamic global-only variables use
  `1229 / HY000`, `Variable '<name>' is a GLOBAL variable and should be set
  with SET GLOBAL`.
- Startup/read-only variables use `1238 / HY000`,
  `Variable '<name>' is a read only variable`.
- State-changing values and user-variable-backed assignments use MyLite's
  deterministic unsupported fixed-no-op diagnostics.

## Runtime And Storage

This slice is implemented in MyLite's system-variable registry, scalar
readback, `SHOW VARIABLES` display path, and `SET` validation. It does not add
public ABI, catalog rows, SQLite SQL, SQLite extension API use, fork patches,
file-format state, VFS behavior, persistent state, or mutable process-global
state.

## Tests

Coverage includes:

- a MySQL 8.4.9 expectation script for defaults, `SHOW` rows, scope
  diagnostics, read-only diagnostics, no-op assignment forms, and upstream
  mutable behavior;
- a runtime C test for MyLite scalar values, `SHOW` rows, diagnostics,
  no-op fixed assignments, unsupported state-changing assignments, and
  user-variable assignment rejection;
- full `SHOW VARIABLES` registry regression coverage.
