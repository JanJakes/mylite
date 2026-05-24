# Baseline Server Identity And Binary Log System Variables

## Summary

This phase adds a limited embedded-compatible subset of MySQL server identity
and binary-log system variables. The supported user-visible surface is scalar
system-variable reads, `SHOW VARIABLES` rows, scope diagnostics, and fixed
no-op global assignments for values that MyLite intentionally keeps immutable.

The slice covers:

- `@@server_id`
- `@@server_id_bits`
- `@@server_uuid`
- `@@log_bin`
- `@@log_bin_basename`
- `@@log_bin_index`
- `@@log_bin_trust_function_creators`

MyLite does not add binary log files, GTID execution, stored program privilege
checks, replication channels, or mutable server-global state in this phase.

## Compatibility Authority

Authoritative inputs:

- MySQL 8.4 Reference Manual, "Server System Variables":
  https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html
- MySQL 8.4 Reference Manual, "Server System Variable Reference":
  https://dev.mysql.com/doc/refman/8.4/en/server-system-variable-reference.html
- MySQL 8.4 Reference Manual, "Binary Logging Options and Variables":
  https://dev.mysql.com/doc/refman/8.4/en/replication-options-binary-log.html
- Observed MySQL 8.4.9 runtime behavior from the local `mysql:8.4.9`
  comparison container.

Runtime probes verified:

```sql
SELECT VERSION();
SELECT @@server_id, @@global.server_id,
       @@server_id_bits, @@global.server_id_bits,
       @@server_uuid, @@global.server_uuid,
       @@log_bin, @@global.log_bin,
       @@log_bin_basename, @@global.log_bin_basename,
       @@log_bin_index, @@global.log_bin_index,
       @@log_bin_trust_function_creators,
       @@global.log_bin_trust_function_creators;
SHOW VARIABLES WHERE Variable_name IN
  ('server_id','server_id_bits','server_uuid','log_bin',
   'log_bin_basename','log_bin_index','log_bin_trust_function_creators');
SELECT @@session.server_id;
SET GLOBAL server_id = 1;
SET GLOBAL server_id = DEFAULT;
SET GLOBAL log_bin_trust_function_creators = 0;
```

Observed MySQL 8.4.9 behavior:

- The variables are global variables. Unscoped scalar reads and
  `@@GLOBAL.name` reads succeed.
- `@@SESSION.name` and `@@LOCAL.name` scalar reads fail with
  `1238 / HY000` and message text of the form
  `Variable 'name' is a GLOBAL variable`.
- `SHOW VARIABLES`, `SHOW GLOBAL VARIABLES`, and `SHOW SESSION VARIABLES`
  include rows for these global variables.
- `log_bin`, `log_bin_basename`, `log_bin_index`, and `server_uuid` reject
  assignment as read-only variables.
- `server_id`, `server_id_bits`, and `log_bin_trust_function_creators` are
  mutable global variables in MySQL. MyLite admits only fixed no-op global
  assignments for this baseline.
- MySQL emits deprecation warning `1287` when
  `log_bin_trust_function_creators` is read or set. MyLite mirrors that warning
  for admitted scalar reads and successful no-op assignments.

## Ownership Boundary

- Public API: no new public ABI. Existing `mylite_execute()` and result APIs
  expose scalar result sets, non-row `SET` results, diagnostics, affected rows,
  and warning counts.
- Parser/AST: no new grammar is required. Existing system-variable and
  `SHOW VARIABLES` syntax already represents the target forms.
- Analyzer/runtime: resolves variable names case-insensitively, applies scope
  rules, formats scalar/show values, and rejects unsupported assignment forms.
- Catalog/storage/VFS: no catalog descriptors, file format fields, SQLite schema
  objects, or VFS behavior change. These variables are fixed runtime
  placeholders.
- SQLite physical execution: no SQLite SQL or fork hook is required. The values
  are produced in MyLite's scalar/select and SHOW paths.

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
SELECT @@server_id, @@GLOBAL.server_uuid, @@log_bin;
SELECT @@`server_id`, @@global.`log_bin_index`;
SHOW VARIABLES LIKE 'server\_%';
SHOW GLOBAL VARIABLES WHERE Variable_name IN ('server_id','log_bin');
SHOW SESSION VARIABLES WHERE Variable_name = 'server_uuid';
SET GLOBAL server_id = 1;
SET @@GLOBAL.server_id_bits = DEFAULT;
SET GLOBAL log_bin_trust_function_creators = OFF;
```

## Values And Scope

MyLite exposes fixed embedded placeholder values:

| Variable | Scalar value | SHOW value | Scope |
| --- | ---: | --- | --- |
| `server_id` | `1` | `1` | Global only |
| `server_id_bits` | `32` | `32` | Global only |
| `server_uuid` | `4d796c69-7465-4000-8000-000000000001` | same | Global only |
| `log_bin` | `1` | `ON` | Global only |
| `log_bin_basename` | `binlog` | `binlog` | Global only |
| `log_bin_index` | `binlog.index` | `binlog.index` | Global only |
| `log_bin_trust_function_creators` | `0` | `OFF` | Global only |

The values are intentionally stable and are not read from host configuration,
environment variables, file paths, or the `.mylite` file. `server_uuid` is a
synthetic MyLite UUID-shaped identifier, not a persisted server instance UUID.
`log_bin_basename` and `log_bin_index` are names only; MyLite does not create
binary log files.

Name resolution is case-insensitive for unquoted and quoted variable names,
matching the existing MyLite system-variable policy.

## SET Semantics

Assignments do not create mutable global state in this phase.

Supported exact no-op global assignments:

```sql
SET GLOBAL server_id = 1;
SET @@GLOBAL.server_id = 1;
SET GLOBAL server_id = DEFAULT;
SET GLOBAL server_id_bits = 32;
SET @@GLOBAL.server_id_bits = DEFAULT;
SET GLOBAL log_bin_trust_function_creators = OFF;
SET GLOBAL log_bin_trust_function_creators = 0;
SET GLOBAL log_bin_trust_function_creators = FALSE;
SET @@GLOBAL.log_bin_trust_function_creators = DEFAULT;
```

Unsupported assignments:

- `SET server_id = ...`, `SET SESSION server_id = ...`, and `SET LOCAL
  server_id = ...` return MySQL-style global-variable diagnostics.
- Non-no-op global assignments for `server_id`, `server_id_bits`, and
  `log_bin_trust_function_creators` return deterministic MyLite unsupported
  diagnostics.
- Any assignment to `server_uuid`, `log_bin`, `log_bin_basename`, or
  `log_bin_index` returns MySQL-style read-only-variable diagnostics.
- Assignments from user variables, expressions, strings, functions, parameters,
  or arithmetic values are outside the admitted no-op subset unless they already
  reduce through an existing supported fixed-value path.

`SET` success reports through the existing non-row statement result conventions:
no row result set, affected rows `0`, and row count `0`.
`log_bin_trust_function_creators` successful no-op assignments report one
deprecation warning; the other admitted no-op assignments report warning count
`0`.

## Diagnostics

Supported diagnostics:

- Unknown variable: existing MyLite/MySQL-shaped unknown system-variable error.
- Session/local scalar read of global-only variables:
  `1238 / HY000`, `Variable 'name' is a GLOBAL variable`.
- Non-global `SET` target for global-only mutable placeholders:
  `1229 / HY000`, `Variable 'name' is a GLOBAL variable and should be set with SET GLOBAL`.
- Read-only assignment for `server_uuid`, `log_bin`, `log_bin_basename`, and
  `log_bin_index`: `1238 / HY000`,
  `Variable 'name' is a read only variable`.
- Non-no-op assignment for fixed placeholders: deterministic MyLite unsupported
  error text documenting that only fixed no-op assignments are accepted.
- Scalar reads and successful no-op assignments for
  `log_bin_trust_function_creators`: warning `1287 / HY000`, matching MySQL's
  deprecation warning text for the variable.
- Allocation failures continue to use existing runtime allocation diagnostics.

## Performance And Storage

The implementation is constant-time table lookup plus scalar formatting. It
does not query SQLite, scan catalog descriptors, allocate persistent state, or
touch storage. `SHOW VARIABLES` continues to enumerate the existing descriptor
array and filter rows in MyLite.

## Tests

Tests must cover:

- Scalar reads with no scope and global scope.
- Case-insensitive and quoted variable names.
- `SHOW VARIABLES`, `SHOW GLOBAL VARIABLES`, `SHOW SESSION VARIABLES`, `LIKE`,
  and `WHERE` filters.
- Session/local scope diagnostics for scalar reads.
- Exact no-op global assignments for `server_id`, `server_id_bits`, and
  `log_bin_trust_function_creators`.
- Unsupported non-no-op assignments and read-only assignment diagnostics.
- No catalog generation, SQLite schema generation, or `.mylite` preamble
  mutation.
- Reopen persistence: values remain fixed because they are runtime constants.
- Independent handles: no assignment leaks mutable state between handles.
- MySQL 8.4.9 expectation script for the observed upstream values, scopes,
  assignment behavior, and diagnostics.

## Compatibility Gaps

MyLite does not yet support:

- Mutable server-global state.
- Persisted server UUIDs or configured server IDs.
- Binary log files, binary-log rotation, binary-log index files, GTID recovery,
  or replication side effects.
- Stored program or trigger privilege behavior tied to
  `log_bin_trust_function_creators`.
- Performance Schema variable tables for these variables.
- `SET PERSIST`, startup options, option files, or privilege checks.
