# Baseline Server Environment System Variables

## Summary

This phase adds a limited embedded-compatible subset of MySQL server
environment system variables. The supported user-visible surface is scalar
system-variable reads, `SHOW VARIABLES` rows, scope diagnostics, and read-only
assignment diagnostics for fixed placeholders.

The slice covers:

- `@@basedir`
- `@@datadir`
- `@@hostname`
- `@@license`
- `@@pid_file`
- `@@plugin_dir`
- `@@port`
- `@@socket`

MyLite does not add host installation discovery, data-directory layout,
network listening sockets, PID files, loadable plugins, startup options, option
files, or mutable server-global state in this phase.

## Compatibility Authority

Authoritative inputs:

- MySQL 8.4 Reference Manual, "Server System Variables":
  https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html
- MySQL 8.4 Reference Manual, "Server System Variable Reference":
  https://dev.mysql.com/doc/refman/8.4/en/server-system-variable-reference.html
- Observed MySQL 8.4.9 runtime behavior from the local `mysql:8.4.9`
  comparison container.

Runtime probes verified:

```sql
SELECT VERSION();
SELECT @@basedir, @@GLOBAL.basedir,
       @@datadir, @@hostname, @@pid_file, @@plugin_dir,
       @@port, @@GLOBAL.port, HEX(@@port),
       @@socket, @@license, @@GLOBAL.license;
SHOW VARIABLES WHERE Variable_name IN
  ('basedir','datadir','hostname','license','pid_file',
   'plugin_dir','port','socket');
SHOW GLOBAL VARIABLES WHERE Variable_name IN
  ('basedir','datadir','hostname','license','pid_file',
   'plugin_dir','port','socket');
SHOW SESSION VARIABLES WHERE Variable_name IN
  ('basedir','datadir','hostname','license','pid_file',
   'plugin_dir','port','socket');
SELECT @@SESSION.basedir;
SET GLOBAL basedir = DEFAULT;
SET @@SESSION.port = 3306;
```

Observed MySQL 8.4.9 behavior:

- The variables are global variables. Unscoped scalar reads and
  `@@GLOBAL.name` reads succeed.
- `@@SESSION.name` and `@@LOCAL.name` scalar reads fail with
  `1238 / HY000` and message text of the form
  `Variable 'name' is a GLOBAL variable`.
- `SHOW VARIABLES`, `SHOW GLOBAL VARIABLES`, and `SHOW SESSION VARIABLES`
  include rows for these global variables.
- Every tested assignment form, including unscoped, `SESSION`, direct
  `@@SESSION`, `GLOBAL`, and direct `@@GLOBAL`, fails with `1238 / HY000` and
  message text of the form `Variable 'name' is a read only variable`.
- Successful scalar and SHOW reads emit no warnings. `port` behaves as an
  integer for scalar numeric contexts such as `HEX(@@port)`.

## Ownership Boundary

- Public API: no new public ABI. Existing `mylite_execute()` and result APIs
  expose scalar result sets, non-row `SET` errors, diagnostics, affected rows,
  warning counts, and result values.
- Parser/AST: no new grammar is required. Existing system-variable and
  `SHOW VARIABLES` syntax already represents the target forms.
- Analyzer/runtime: resolves variable names case-insensitively, applies scope
  rules, formats scalar/show values, exposes `port` to existing numeric scalar
  contexts, and rejects assignments as read-only.
- Catalog/storage/VFS: no catalog descriptors, file format fields, SQLite
  schema objects, preamble bytes, or VFS behavior change. These variables are
  fixed runtime placeholders.
- SQLite physical execution: no SQLite SQL or fork hook is required. The
  values are produced in MyLite's scalar/select and SHOW paths.

## Supported Syntax

No new Lemon productions are needed. The existing system-variable grammar must
continue to admit these forms:

```lemon
expr ::= SYSTEM_VARIABLE.
set_target ::= system_variable_name.
set_target ::= GLOBAL system_variable_name.
set_target ::= SYSTEM_VARIABLE.
show_stmt ::= SHOW show_scope_opt VARIABLES show_filter_opt.
```

Admitted examples:

```sql
SELECT @@basedir, @@GLOBAL.datadir, @@hostname, @@port;
SELECT @@`license`, @@global.`plugin_dir`, HEX(@@port);
SHOW VARIABLES LIKE '%dir';
SHOW GLOBAL VARIABLES WHERE Variable_name IN ('basedir','socket');
SHOW SESSION VARIABLES WHERE Variable_name = 'port';
```

Assignments are parsed by existing `SET` support but are rejected as read-only.

## Values And Scope

MyLite exposes fixed embedded placeholder values:

| Variable | Scalar value | SHOW value | Scope |
| --- | --- | --- | --- |
| `basedir` | `/usr/` | `/usr/` | Global only |
| `datadir` | `/var/lib/mysql/` | `/var/lib/mysql/` | Global only |
| `hostname` | `mylite` | `mylite` | Global only |
| `license` | `GPL` | `GPL` | Global only |
| `pid_file` | `/var/run/mysqld/mysqld.pid` | same | Global only |
| `plugin_dir` | `/usr/lib64/mysql/plugin/` | same | Global only |
| `port` | `3306` | `3306` | Global only |
| `socket` | `/var/run/mysqld/mysqld.sock` | same | Global only |

The values are intentionally stable and are not read from host configuration,
environment variables, file paths, network state, or the `.mylite` file.
`license` is a MySQL-compatible server-variable placeholder value for
application compatibility; it is not a project license notice. The path values
are compatibility strings only and do not imply that MyLite reads or writes
those locations.

Name resolution is case-insensitive for unquoted and quoted variable names,
matching the existing MyLite system-variable policy.

## SET Semantics

Assignments do not create mutable global state in this phase.

All assignments to this batch return MySQL-style read-only diagnostics:

```sql
SET basedir = DEFAULT;
SET SESSION datadir = DEFAULT;
SET @@SESSION.port = 3306;
SET GLOBAL socket = DEFAULT;
SET @@GLOBAL.license = 'GPL';
```

`SET` does not alter catalog generation, SQLite schema generation, session
snapshots, diagnostics outside the current error, or file bytes.

## Diagnostics

Supported diagnostics:

- Unknown variable: existing MyLite/MySQL-shaped unknown system-variable error.
- Session/local scalar read of global-only variables:
  `1238 / HY000`, `Variable 'name' is a GLOBAL variable`.
- Any assignment target for these read-only variables:
  `1238 / HY000`, `Variable 'name' is a read only variable`.
- Unsupported expression contexts continue to use the existing system-variable
  expression diagnostics.
- Allocation failures continue to use existing runtime allocation diagnostics.

Successful scalar and SHOW reads produce `warning_count == 0`.

## Performance And Storage

The implementation is constant-time table lookup plus scalar formatting. It
does not query SQLite, scan catalog descriptors, allocate persistent state, or
touch storage. `SHOW VARIABLES` continues to enumerate the existing descriptor
array and filter rows in MyLite.

## Tests

Tests must cover:

- Scalar reads with no scope and global scope.
- Numeric scalar behavior for `port`, including `HEX(@@port)`.
- Case-insensitive and quoted variable names.
- `SHOW VARIABLES`, `SHOW GLOBAL VARIABLES`, `SHOW SESSION VARIABLES`, `LIKE`,
  and `WHERE` filters.
- Session/local scope diagnostics for scalar reads.
- Read-only assignment diagnostics for unscoped, `SESSION`, direct
  `@@SESSION`, `GLOBAL`, and direct `@@GLOBAL` targets.
- Warning count, error count, and `ROW_COUNT()` after successful reads.
- No catalog generation, SQLite schema generation, or `.mylite` preamble
  mutation.
- Reopen persistence: values remain fixed because they are runtime constants.
- Independent handles: no assignment or diagnostic state leaks between handles.
- MySQL 8.4.9 expectation script for the observed upstream values, scopes,
  assignment behavior, and diagnostics.

## Compatibility Gaps

MyLite does not yet support:

- Host installation discovery or path derivation.
- Real MySQL data directories, plugin directories, socket files, PID files, or
  network listening state.
- Mutable server-global state.
- Startup options, option files, `SET PERSIST`, or privilege checks.
- Performance Schema variable tables for these variables.
