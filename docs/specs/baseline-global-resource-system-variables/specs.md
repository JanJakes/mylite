# Baseline Global Resource System Variables

## Summary

This slice exposes an embedded-compatible baseline for MySQL global system
variables that describe server resource/cache sizing, persisted-variable policy,
protocol compression policy, and temporary-table/thread settings:

- `ngram_token_size`
- `offline_mode`
- `persist_only_admin_x509_subject`
- `persist_sensitive_variables_in_plaintext`
- `persisted_globals_load`
- `protocol_compression_algorithms`
- `schema_definition_cache`
- `stored_program_cache`
- `stored_program_definition_cache`
- `sync_binlog`
- `table_definition_cache`
- `table_encryption_privilege_check`
- `table_open_cache`
- `table_open_cache_instances`
- `tablespace_definition_cache`
- `temptable_max_mmap`
- `temptable_use_mmap`
- `thread_cache_size`
- `thread_stack`

MyLite supports scalar reads, `SHOW VARIABLES` / `SHOW GLOBAL VARIABLES` /
`SHOW SESSION VARIABLES` rows, global-only scalar diagnostics, read-only
assignment diagnostics for startup/read-only variables, and fixed global no-op
assignment validation for dynamic variables whose server-global effects do not
exist in an embedded library. MyLite does not implement global resource cache
resizing, server offline mode, persisted-variable file handling, protocol
compression negotiation, temporary-table memory policy, binary-log fsync
policy, thread-cache management, startup option loading, `SET PERSIST`, or
Performance Schema variable tables.

`open_files_limit` and `temptable_max_ram` are intentionally excluded from this
slice because their displayed MySQL values are host/resource dependent in the
comparison container. They need a separate limited-placeholder decision rather
than pretending a fixed value is fully equivalent.

## Compatibility Authority

- MySQL 8.4 Reference Manual, server system variables:
  <https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html>
- MySQL 8.4 Reference Manual, `SHOW VARIABLES`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-variables.html>
- MySQL 8.4.9 runtime observations captured by
  `packages/libmylite/tests/mysql_baseline_global_resource_system_variables_expectations.sh`.

The official documentation defines scope, dynamic/read-only classification,
boolean display conventions, and `SHOW VARIABLES` behavior. Runtime probes
establish the exact defaults, warnings, and diagnostics in the pinned
`mysql:8.4.9` comparison container.

## MySQL 8.4.9 Observations

The target runtime reports these defaults:

| Variable | Scalar value | `SHOW VARIABLES` value |
| --- | --- | --- |
| `ngram_token_size` | `2` | `2` |
| `offline_mode` | `0` | `OFF` |
| `persist_only_admin_x509_subject` | empty string | empty string |
| `persist_sensitive_variables_in_plaintext` | `1` | `ON` |
| `persisted_globals_load` | `1` | `ON` |
| `protocol_compression_algorithms` | `zlib,zstd,uncompressed` | same |
| `schema_definition_cache` | `256` | `256` |
| `stored_program_cache` | `256` | `256` |
| `stored_program_definition_cache` | `256` | `256` |
| `sync_binlog` | `1` | `1` |
| `table_definition_cache` | `2000` | `2000` |
| `table_encryption_privilege_check` | `0` | `OFF` |
| `table_open_cache` | `4000` | `4000` |
| `table_open_cache_instances` | `16` | `16` |
| `tablespace_definition_cache` | `256` | `256` |
| `temptable_max_mmap` | `0` | `0` |
| `temptable_use_mmap` | `0` | `OFF` |
| `thread_cache_size` | `9` | `9` |
| `thread_stack` | `1048576` | `1048576` |

Unscoped and `@@GLOBAL.name` scalar reads succeed. `@@SESSION.name` and
`@@LOCAL.name` scalar reads fail with `1238 / HY000` and a message that the
variable is global. `SHOW VARIABLES`, `SHOW GLOBAL VARIABLES`, and
`SHOW SESSION VARIABLES` all include these global variables.

`ngram_token_size`, `persist_only_admin_x509_subject`,
`persist_sensitive_variables_in_plaintext`, `persisted_globals_load`,
`table_open_cache_instances`, and `thread_stack` reject `SET GLOBAL ... =
DEFAULT` with `1238 / HY000` and a read-only-variable message. The remaining
variables in this slice accept `SET GLOBAL ... = DEFAULT`. MySQL emits
deprecation warning `1287` when `temptable_use_mmap` is assigned.

## MyLite Scope

MyLite supports:

- scalar default and `GLOBAL` reads for all variables in this slice;
- MySQL-style scalar `SESSION` / `LOCAL` global-variable diagnostics;
- `SHOW VARIABLES`, `SHOW SESSION VARIABLES`, `SHOW LOCAL VARIABLES`, and
  `SHOW GLOBAL VARIABLES` rows with MySQL-style display values;
- read-only assignment diagnostics for startup/read-only variables;
- fixed global no-op assignments for dynamic variables when the value is
  `DEFAULT` or the fixed MyLite default;
- the MySQL deprecation warning on successful `temptable_use_mmap` assignments.

MyLite intentionally does not support:

- real resource/cache resizing or cache eviction behavior;
- server offline mode, client disconnects, or connection gating;
- persisted-variable file loading/writing, encryption, or privilege policy;
- protocol-compression negotiation or packet compression;
- TempTable memory/mmap policy;
- binary-log fsync behavior;
- table-definition/table-open cache management;
- thread-cache creation or thread-stack sizing;
- mutable shared server-global state, startup options, option files,
  `SET PERSIST`, privileges, or Performance Schema variable tables.

## Syntax

No new grammar is required. Existing MyLite productions already admit the
required forms:

```lemon
expr ::= SYSTEM_VARIABLE.
set_statement ::= SET set_assignment_list.
set_assignment ::= set_system_variable_target EQ set_value.
show_statement ::= SHOW show_scope_opt VARIABLES show_filter_opt.
```

## Runtime Design

The variables are descriptor-backed system variables. Read-only variables use
the existing read-only server-environment classification so assignment
diagnostics are raised before generic fixed-placeholder validation. Dynamic
variables use the existing compatibility-placeholder path, which accepts only
fixed no-op global assignments and rejects mutating assignments deterministically.
Boolean variables are formatted as numeric scalar values and `ON`/`OFF` SHOW
values, matching MySQL.

SQLite changes are not required. The implementation is entirely in MyLite's
runtime wrapper/translation layer and does not need a targeted SQLite fork hook.

## Tests

The MySQL expectation script verifies against MySQL 8.4.9:

- scalar default/global values;
- `SHOW VARIABLES`, `SHOW GLOBAL VARIABLES`, and `SHOW SESSION VARIABLES` rows;
- global-only scalar diagnostics for `SESSION` and `LOCAL` scope;
- read-only `SET GLOBAL` diagnostics;
- dynamic global `SET ... = DEFAULT` acceptance;
- `temptable_use_mmap` deprecation warning on assignment.

The runtime test mirrors those expectations in MyLite and also checks that
fixed no-op assignments do not change exposed readback values.
