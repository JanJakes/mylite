# Baseline SHOW STATUS Optional Absence

## Summary

This slice documents and tests the status variables that are documented or
historically known for optional MySQL components, plugins, storage engines, or
server features, but are absent from the standard MySQL 8.4.9 runtime used by
MyLite's compatibility suite.

MyLite intentionally omits these names from its fixed `SHOW STATUS` registry.
It should not expose invented `0` or empty-string rows for optional status
variables that the target runtime itself does not expose.

## Compatibility Authority

- MySQL 8.4 Reference Manual, `SHOW STATUS`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-status.html>
- MySQL 8.4 Reference Manual, server status variable reference:
  <https://dev.mysql.com/doc/refman/8.4/en/server-status-variable-reference.html>
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_show_status_optional_absence_expectations.sh`.

The MySQL 8.4.9 target runtime returns no rows for the covered names in default,
session, local, or global `SHOW STATUS` scope. The names are associated with
surfaces such as Enterprise Audit, Enterprise Firewall, Group Replication,
NDB Cluster, Rewriter, semi-sync replication, MeCab, validate-password
dictionary state, and optional X Plugin SSL counters that are not present in
the target runtime profile.

## Supported Behavior

For each covered name, MyLite returns an empty `SHOW STATUS` result with the
normal `Variable_name` and `Value` columns:

```sql
SHOW STATUS WHERE Variable_name = 'Audit_log_events';
SHOW SESSION STATUS WHERE Variable_name = 'Ndb_cluster_node_id';
SHOW GLOBAL STATUS WHERE Variable_name = 'Rpl_semi_sync_source_status';
```

`SHOW STATUS WHERE Variable_name IN (...)` over the covered names also returns
zero rows in default/session/local/global scope.

Successful statements use the normal `SHOW STATUS` result-state contract:

- affected rows `0`;
- warning count `0`;
- error count `0`;
- `ROW_COUNT()` becomes `-1`.

## Syntax

No grammar change is required. This slice relies on the existing `SHOW STATUS`
syntax:

```lemon
show_status_statement ::=
    SHOW show_status_scope_opt STATUS show_status_filter_opt.

show_status_filter_opt ::= WHERE predicate.
```

The covered absence checks use the existing limited output-row predicate subset
over `Variable_name`.

## Semantics

The covered names are not added to the `SHOW STATUS` descriptor registry. They
are therefore absent from:

- `SHOW STATUS`;
- `SHOW SESSION STATUS`;
- `SHOW LOCAL STATUS`;
- `SHOW GLOBAL STATUS`;
- `sys.metrics`, which is derived from global-visible status descriptors.

If a future MyLite target runtime profile enables one of these optional MySQL
components, that component's status rows should be implemented as a separate
feature with MySQL-runtime-verified row names, scope visibility, and values.

## Diagnostics And Limits

- Absence is represented as an empty result set, not an error.
- MyLite does not emulate optional component lifecycle state for these rows.
- MyLite does not expose NDB, Enterprise Audit, Enterprise Firewall, Rewriter,
  semi-sync replication, MeCab, validate-password dictionary, or optional
  Group Replication counters through `SHOW STATUS`.
- This slice does not change scalar system variables or Performance Schema
  status-variable tables.

## Ownership Boundary

- Public API: unchanged.
- Parser/AST: unchanged.
- Runtime: no registry expansion; tests assert the existing absence behavior.
- Catalog/storage/SQLite: unchanged.
- Docs: detailed status-variable rows move from unverified red to explicit
  target-runtime absence.

## MySQL Runtime Evidence

The recorded MySQL 8.4.9 probe verifies that each covered name returns no row
from:

```sql
SHOW STATUS WHERE Variable_name IN (...);
SHOW SESSION STATUS WHERE Variable_name IN (...);
SHOW LOCAL STATUS WHERE Variable_name IN (...);
SHOW GLOBAL STATUS WHERE Variable_name IN (...);
```

The probe also verifies the server version is `8.4.9`.

## Test Plan

- Add a MySQL expectation script that verifies zero rows for the full covered
  name set in default, session, local, and global scope.
- Add focused C runtime coverage for the same MyLite behavior.
- Update the detailed server status-variable compatibility table.
- Run:
  - `sh -n packages/libmylite/tests/mysql_baseline_show_status_optional_absence_expectations.sh`
  - `packages/libmylite/tests/mysql_baseline_show_status_optional_absence_expectations.sh`
  - `cmake --build --preset dev --target mylite_runtime_show_status_optional_absence_test`
  - `ctest --preset dev -R '^libmylite\.runtime\.show_status_optional_absence$' --output-on-failure`
  - `git diff --check`
  - `git diff --cached --check`
  - `cmake --workflow --preset check`
