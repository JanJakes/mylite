# Baseline SHOW STATUS Command Counters

## Summary

This slice expands the existing `SHOW STATUS` placeholder registry with the
MySQL 8.4.9 `Com_%` command-counter name surface:

```sql
SHOW [GLOBAL | SESSION | LOCAL] STATUS LIKE 'Com\_%'
```

MyLite exposes the MySQL 8.4.9 command-counter names in runtime-observed order
for default, session, local, and global scopes. The row values are deterministic
embedded `0` placeholders. This is status metadata only; it does not implement
live command accounting or `FLUSH STATUS` counter lifecycle.

## Compatibility Authority

- MySQL 8.4 Reference Manual, `SHOW STATUS`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-status.html>
- MySQL 8.4 Reference Manual, server status variables:
  <https://dev.mysql.com/doc/refman/8.4/en/server-status-variable-reference.html>
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_show_status_expectations.sh`.

Runtime probes against the local `mylite-mysql-849` MySQL 8.4.9 container
verified that `SHOW STATUS LIKE 'Com\_%'`,
`SHOW SESSION STATUS LIKE 'Com\_%'`, and
`SHOW GLOBAL STATUS LIKE 'Com\_%'` all return the same 168 command-counter
names. Live MySQL counter values are intentionally not pinned because they vary
with server startup and the probe statements themselves.

## Supported Behavior

MyLite adds the full observed MySQL 8.4.9 `Com_%` row-name set to the static
`SHOW STATUS` descriptor registry. The exact names and order are enumerated in
`docs/specs/baseline-show-status/specs.md` and pinned by the MySQL expectation
script. Each row is visible in session/local/default and global scopes.

The existing `SHOW STATUS` shape applies:

- columns are `Variable_name` and `Value`;
- optional `GLOBAL`, `SESSION`, and `LOCAL` scope is honored;
- optional `LIKE` filtering is applied to `Variable_name`;
- optional limited `WHERE` filtering is evaluated against `Variable_name` and
  `Value`;
- successful statements report no warnings and make the next `ROW_COUNT()`
  return `-1`.

## Unsupported Behavior

This slice intentionally does not add:

- live per-command counters;
- counter increments for executed MyLite statements;
- `FLUSH STATUS` reset behavior;
- Performance Schema status-variable tables;
- status rows beyond MyLite's currently supported descriptor registry;
- privilege filtering or protocol-level status accounting.

## Architecture

The change is a MyLite runtime descriptor-table expansion. It does not add
public ABI, catalog storage, SQLite SQL generation, user data writes, file
format changes, VFS changes, or SQLite fork hooks.

`sys.metrics` is derived from the global-visible `SHOW STATUS` descriptor set,
so it gains the same command-counter placeholder rows as `Global Status`
metrics, still with deterministic placeholder values.

## Tests

Fast C tests assert:

- the expanded `SHOW STATUS` row order and deterministic values;
- default/session/local/global scope behavior;
- `LIKE 'Com\_%'` and representative `WHERE` filters;
- `sys.metrics` row-count adjustment from the expanded global descriptor set.

The MySQL expectation script verifies the exact 168 `Com_%` row names and
scope visibility against MySQL 8.4.9, while deliberately ignoring volatile live
counter values.

## Compatibility Documentation

Update:

- `COMPATIBILITY.md` with a narrow green baseline row for this verified
  placeholder surface;
- `docs/compatibility/runtime-status-variables.md` to mark the observed
  command-counter rows as supported placeholder values;
- `docs/specs/baseline-show-status/specs.md` and
  `docs/specs/baseline-sys-metrics-view/specs.md` for the expanded registry.
