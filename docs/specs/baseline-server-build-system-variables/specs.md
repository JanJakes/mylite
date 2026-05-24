# Baseline Server Build System Variables

## Summary

This phase adds a limited embedded-compatible subset of MySQL server protocol
and build-identity system variables. The supported user-visible surface is
scalar system-variable reads, `SHOW VARIABLES` rows, scope diagnostics, and
read-only assignment diagnostics for fixed placeholders.

The slice covers:

- `@@protocol_version`
- `@@version_compile_machine`
- `@@version_compile_os`
- `@@version_compile_zlib`

MyLite does not add MySQL wire-protocol handshakes, host build introspection,
dynamic server-global state, or MySQL build impersonation in this phase.

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
SELECT @@protocol_version, @@GLOBAL.protocol_version,
       HEX(@@protocol_version),
       @@version_compile_machine,
       @@version_compile_os,
       @@version_compile_zlib;
SHOW VARIABLES WHERE Variable_name IN
  ('protocol_version','version_compile_machine',
   'version_compile_os','version_compile_zlib');
SHOW GLOBAL VARIABLES WHERE Variable_name IN
  ('protocol_version','version_compile_machine',
   'version_compile_os','version_compile_zlib');
SHOW SESSION VARIABLES WHERE Variable_name IN
  ('protocol_version','version_compile_machine',
   'version_compile_os','version_compile_zlib');
SELECT @@SESSION.protocol_version;
SET protocol_version = DEFAULT;
SET @@GLOBAL.version_compile_os = 'Linux';
```

Observed MySQL 8.4.9 behavior:

- The variables are global variables. Unscoped scalar reads and
  `@@GLOBAL.name` reads succeed.
- `@@SESSION.name` and `@@LOCAL.name` scalar reads fail with
  `1238 / HY000` and message text of the form
  `Variable 'name' is a GLOBAL variable`.
- `SHOW VARIABLES`, `SHOW GLOBAL VARIABLES`, and `SHOW SESSION VARIABLES`
  include rows for these global variables.
- Assignment forms, including unscoped, `SESSION`, direct `@@SESSION`,
  `GLOBAL`, and direct `@@GLOBAL`, fail with `1238 / HY000` and message text
  of the form `Variable 'name' is a read only variable`.
- `protocol_version` is integer-valued and defaulted to `10`. It behaves as an
  integer for scalar numeric contexts such as `HEX(@@protocol_version)`.
- Compile variable values are build-environment strings in MySQL; the local
  comparison container reported `aarch64`, `Linux`, and `1.3.2`.

## Ownership Boundary

- Public API: no new public ABI. Existing `mylite_execute()` and result APIs
  expose scalar result sets, non-row `SET` errors, diagnostics, affected rows,
  warning counts, and result values.
- Parser/AST: no new grammar is required. Existing system-variable and
  `SHOW VARIABLES` syntax already represents the target forms.
- Analyzer/runtime: resolves variable names case-insensitively, applies scope
  rules, formats scalar/show values, exposes `protocol_version` to existing
  numeric scalar contexts, and rejects assignments as read-only.
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
SELECT @@protocol_version, @@GLOBAL.protocol_version;
SELECT @@version_compile_machine, @@version_compile_os;
SELECT @@`version_compile_zlib`, HEX(@@protocol_version);
SHOW VARIABLES LIKE 'version_compile_%';
SHOW GLOBAL VARIABLES WHERE Variable_name = 'protocol_version';
SHOW SESSION VARIABLES WHERE Variable_name IN
  ('version_compile_machine','version_compile_os');
```

Assignments are parsed by existing `SET` support but are rejected as read-only.

## Values And Scope

MyLite exposes fixed embedded placeholder values:

| Variable | Scalar value | SHOW value | Scope |
| --- | --- | --- | --- |
| `protocol_version` | `10` | `10` | Global only |
| `version_compile_machine` | `aarch64` | `aarch64` | Global only |
| `version_compile_os` | `Linux` | `Linux` | Global only |
| `version_compile_zlib` | `1.3.2` | `1.3.2` | Global only |

The values are intentionally stable and are not read from host configuration,
compiler macros, dynamic library versions, network state, or the `.mylite`
file. The compile values are compatibility strings only; they do not assert the
actual machine, operating system, or zlib version used to build MyLite.

Name resolution is case-insensitive for unquoted and quoted variable names,
matching the existing MyLite system-variable policy.

## SET Semantics

Assignments do not create mutable global state in this phase.

All assignments to this batch return MySQL-style read-only diagnostics:

```sql
SET protocol_version = DEFAULT;
SET SESSION version_compile_machine = DEFAULT;
SET @@SESSION.version_compile_os = 'Linux';
SET GLOBAL version_compile_zlib = DEFAULT;
SET @@GLOBAL.protocol_version = 10;
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
- Numeric scalar behavior for `protocol_version`, including
  `HEX(@@protocol_version)`.
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

- MySQL wire-protocol handshake metadata changes.
- Runtime or build-time host discovery for compile variables.
- Mutable server-global state.
- Startup options, option files, `SET PERSIST`, or privilege checks.
- Performance Schema variable tables for these variables.
