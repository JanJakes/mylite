# Baseline Server Capability System Variables

## Summary

This slice exposes fixed MySQL 8.4.9-shaped server capability readbacks for:

- `have_compress`
- `have_dynamic_loading`
- `have_geometry`
- `have_profiling`
- `have_query_cache`
- `have_rtree_keys`
- `have_statement_timeout`
- `have_symlink`

MyLite supports scalar default and `GLOBAL` reads, `SHOW VARIABLES` /
`SHOW GLOBAL VARIABLES` / `SHOW SESSION VARIABLES` rows, MySQL-style
global-only scalar scope diagnostics, and read-only assignment diagnostics.
MyLite does not make these variables control compression functions, plugin
loading, spatial storage, profiling, query cache behavior, MyISAM RTREE
indexes, statement timeout worker threads, or symbolic-link table options.

## Compatibility Authority

- MySQL 8.4 Reference Manual, server system variables:
  <https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html>
- MySQL 8.4.9 runtime observations captured by
  `packages/libmylite/tests/mysql_baseline_server_capability_system_variables_expectations.sh`.

The official documentation classifies these variables as server capability
readbacks, including global-only and read-only behavior for
`have_statement_timeout`. Runtime probes establish the exact defaults and
diagnostics in the pinned `mysql:8.4.9` comparison container.

## MySQL 8.4.9 Observations

The target runtime reports these values:

| Variable | Scalar value | `SHOW VARIABLES` value |
| --- | --- | --- |
| `have_compress` | `YES` | `YES` |
| `have_dynamic_loading` | `YES` | `YES` |
| `have_geometry` | `YES` | `YES` |
| `have_profiling` | `YES` | `YES` |
| `have_query_cache` | `NO` | `NO` |
| `have_rtree_keys` | `YES` | `YES` |
| `have_statement_timeout` | `YES` | `YES` |
| `have_symlink` | `DISABLED` | `DISABLED` |

Unscoped and `@@GLOBAL.name` scalar reads succeed. `@@SESSION.name` and
`@@LOCAL.name` scalar reads fail with `1238 / HY000` and a message that the
variable is global. `SHOW VARIABLES`, `SHOW GLOBAL VARIABLES`, and
`SHOW SESSION VARIABLES` all include these variables.

`SET GLOBAL name = DEFAULT` and unscoped `SET name = DEFAULT` fail with
`1238 / HY000` and a read-only-variable message for every variable in this
slice.

## MyLite Scope

MyLite supports:

- scalar default and `GLOBAL` reads for all variables in this slice;
- MySQL-style scalar `SESSION` / `LOCAL` global-variable diagnostics;
- `SHOW VARIABLES`, `SHOW SESSION VARIABLES`, `SHOW LOCAL VARIABLES`, and
  `SHOW GLOBAL VARIABLES` rows with MySQL-style display values;
- read-only assignment diagnostics for direct and user-variable-backed `SET`
  forms.

MyLite intentionally does not support:

- changing the availability of compression, plugin loading, geometry, profiling,
  query cache, RTREE, statement timeout, or symbolic-link capabilities;
- loading or unloading plugins as a side effect of `have_dynamic_loading`;
- MyISAM-specific RTREE behavior;
- server-global mutable state, startup options, privileges, persisted
  variables, or Performance Schema variable tables for these readbacks.

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
SELECT @@have_compress, @@GLOBAL.have_statement_timeout;
SELECT @@have_query_cache, @@GLOBAL.have_symlink;
SHOW VARIABLES WHERE Variable_name IN ('have_compress','have_symlink');
SHOW GLOBAL VARIABLES LIKE 'have\_%';
SET GLOBAL have_compress = DEFAULT;
```

## Diagnostics

- Unknown variables continue to use the existing `1193 / HY000` diagnostic.
- Scalar `@@SESSION` / `@@LOCAL` reads use `1238 / HY000` with
  `Variable '<name>' is a GLOBAL variable`.
- All `SET` forms for these variables use `1238 / HY000` with
  `Variable '<name>' is a read only variable`.

## Runtime And Storage

This slice is entirely in MyLite's system-variable registry and `SET`
validation path. It does not add public ABI, SQLite SQL, a SQLite extension
hook, a fork patch, catalog descriptors, file-format state, VFS behavior, or
persistent mutable state. Values are constants and are independent of handles,
schemas, transactions, and database files.

## Tests

Coverage includes:

- a MySQL 8.4.9 expectation script for default values, `SHOW` visibility,
  scope diagnostics, and read-only assignment diagnostics;
- a runtime C test for scalar reads, `SHOW` rows, global-only diagnostics,
  direct read-only assignment diagnostics, and user-variable-backed read-only
  assignment diagnostics;
- focused `SHOW VARIABLES` regression coverage through the existing registry
  test.
