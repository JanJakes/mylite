# SHOW STATUS

## Scope

This feature implements a focused first executable slice of `SHOW STATUS`:

- `SHOW STATUS`
- `SHOW GLOBAL STATUS`
- `SHOW SESSION STATUS`
- `SHOW LOCAL STATUS`
- all scope forms with `LIKE 'pattern'`
- all scope forms with `WHERE expr`, parsed but rejected at execution time

The slice exposes a practical catalog of high-value status variables that an
embedded MyLite runtime can answer honestly today. Values backed by current
handle state are returned directly. Command counters that require a broader
statement-accounting design are exposed as documented zero placeholders so
applications can discover the names without receiving invented activity counts.

Deferred surfaces:

- execution of `SHOW STATUS ... WHERE expr`
- full MySQL server status-variable catalog
- complete global/session counter accounting
- `FLUSH STATUS` and resettable counter lifetimes
- Performance Schema status-variable tables
- accurate status counters for networking, SSL, replication, plugins, storage
  engines, table locks, temporary tables, prepared statements, and protocol
  byte counts beyond the documented placeholders below

## Compatibility Sources

- MySQL 8.4 Reference Manual, `SHOW STATUS` Statement:
  https://dev.mysql.com/doc/refman/8.4/en/show-status.html
- MySQL 8.4 Reference Manual, Extensions to `SHOW` Statements:
  https://dev.mysql.com/doc/refman/8.4/en/extended-show.html
- MySQL 8.4 Reference Manual, server status variables:
  https://dev.mysql.com/doc/refman/8.4/en/server-status-variables.html
- MySQL 8.4 Reference Manual, server status-variable reference:
  https://dev.mysql.com/doc/refman/8.4/en/server-status-variable-reference.html
- Runtime observations verified against `mylite-mysql-849`, MySQL `8.4.9`.

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar or
implementation sources.

## MySQL 8.4.9 Runtime Observations

The following behavior was verified against MySQL 8.4.9:

| SQL | Result |
| --- | --- |
| `SHOW STATUS LIKE 'Uptime%'` | Columns `Variable_name`, `Value`; returns `Uptime` and `Uptime_since_flush_status` with numeric string values. |
| `SHOW SESSION STATUS LIKE 'Uptime%'` | Accepted; same row names as the no-scope form. |
| `SHOW GLOBAL STATUS LIKE 'Uptime%'` | Accepted; same row names as the no-scope form. |
| `SHOW LOCAL STATUS LIKE 'Uptime%'` | Accepted; `LOCAL` is a session synonym. |
| `SHOW STATUS LIKE 'Threads%'` | Returns `Threads_cached`, `Threads_connected`, `Threads_created`, and `Threads_running` in name order. |
| `SHOW STATUS LIKE 'Connections'` | Returns one `Connections` row. |
| `SHOW STATUS LIKE 'Questions'` | Returns one `Questions` row. |
| `SHOW STATUS LIKE 'Com_select'` | Returns one `Com_select` row. |
| `SHOW STATUS LIKE 'Com\_%'` | Backslash escapes `_`, so only literal `Com_` names match. |
| `SHOW STATUS LIKE 'threads_%'` | Matches the `Threads_*` rows; status-name pattern matching is case-insensitive on the verified runtime. |
| `SHOW STATUS LIKE 'THREADS\_%'` | Matches the same `Threads_*` rows as the lowercase escaped pattern. |
| `SHOW STATUS LIKE 'no_such_status'` | Returns no rows with stable two-column metadata. |
| `SHOW STATUS WHERE Variable_name = 'Uptime'` | Filters rows in MySQL and returns one `Uptime` row. |
| `SHOW STATUS WHERE Variable_name LIKE 'Threads\_%'` | Filters rows through the general `WHERE` path and returns `Threads_*` rows. |
| `SHOW STATUS WHERE Value = '0'` | Accepted and filters against displayed column names. |
| `SHOW STATUS WHERE Value = '0' AND Variable_name = 'Com_select'` | Accepted; returned no row in the probe session because `Com_select` was nonzero. |
| `SHOW SESSION STATUS WHERE Variable_name IN (...)` | Rows are returned sorted by `Variable_name`, not by the `IN` list. Session counters such as `Com_select`, `Com_show_status`, and `Questions` increase as statements run. |
| `SHOW GLOBAL STATUS WHERE Variable_name IN (...)` | Returns aggregated server counters; global `Com_select` and `Questions` can differ from session values, while connection/thread/uptime rows are server-level values. |
| `SELECT 1`; `SHOW SESSION STATUS WHERE Variable_name IN (...)` | The later status output reflected an incremented `Com_select` and `Questions`; MySQL counts `SHOW STATUS` statements themselves in `Questions`. |
| `SHOW STATUS WHERE No_such_column = 1` | Runtime error `1054`, SQLSTATE `42S22`, unknown column. |
| `SHOW STATUS LIMIT 1` | Syntax error `1064`; `LIMIT` is not part of the statement syntax. |
| `SHOW STATUS LIKE 1` | Syntax error `1064`; the `LIKE` pattern must be a string literal. |
| `SHOW LOCAL GLOBAL STATUS` | Syntax error `1064`; at most one scope modifier is accepted. |
| missing-table error; `SHOW COUNT(*) ERRORS`; `SHOW STATUS LIKE 'Questions'`; `SHOW COUNT(*) ERRORS` | `SHOW STATUS` is nondiagnostic and clears the earlier error before reporting, so the final error count is `0`. |

## Syntax

MyLite owns the grammar below; it is intentionally authored for MyLite's Lemon
parser rather than copied from MySQL sources:

```lemon
statement ::= show_status_statement.

show_status_statement ::= SHOW opt_show_status_scope STATUS
                          opt_show_status_filter.

opt_show_status_scope ::= .
opt_show_status_scope ::= GLOBAL.
opt_show_status_scope ::= SESSION.
opt_show_status_scope ::= LOCAL.

opt_show_status_filter ::= .
opt_show_status_filter ::= LIKE STRING.
opt_show_status_filter ::= where_clause.
```

`GLOBAL`, `SESSION`, `LOCAL`, and `STATUS` must remain available as
nonreserved identifiers outside this production.

## AST

Add a `show_status_statement` AST node with:

- a scope marker:
  - omitted, `SESSION`, and `LOCAL` normalize to session scope
  - `GLOBAL` uses global/default scope
- an optional string-literal `LIKE` pattern child or `WHERE` clause child

The statement must preserve the source span from `SHOW` through the last token.

## Runtime Semantics

Rows:

- The result set always has columns `Variable_name` and `Value`.
- Rows are ordered by variable name case-insensitively, then by binary name for
  deterministic tie-breaking, matching the existing `SHOW VARIABLES` ordering
  policy.
- Successful `SHOW STATUS` produces no warnings.
- `mylite_affected_rows()` remains `-1` for the read-only SQLite-backed result.
- `SHOW STATUS` is a nondiagnostic statement. Like MySQL, it clears prior
  diagnostics before producing rows.

Result metadata:

| Scope | Column | Schema | Table | Type | Collation | Length | Flags |
| --- | --- | --- | --- | --- | --- | ---: | --- |
| session/local/default | `Variable_name` | `performance_schema` | `session_status` | `VAR_STRING` | `latin1_swedish_ci` | 64 | `NOT_NULL NO_DEFAULT_VALUE` |
| session/local/default | `Value` | `performance_schema` | `session_status` | `VAR_STRING` | `latin1_swedish_ci` | 1024 | none |
| global | `Variable_name` | `performance_schema` | `global_status` | `VAR_STRING` | `latin1_swedish_ci` | 64 | `NOT_NULL NO_DEFAULT_VALUE` |
| global | `Value` | `performance_schema` | `global_status` | `VAR_STRING` | `latin1_swedish_ci` | 1024 | none |

Scope:

- Omitted scope, `SESSION`, and `LOCAL` expose session values.
- `GLOBAL` exposes process/default values.
- MyLite has no separate mutable global status state in this slice. For values
  where a single embedded handle is the only meaningful runtime scope, session
  and global intentionally report the same value.
- Variables that MySQL classifies as global-only may still appear for
  session/default `SHOW STATUS`, matching MySQL's documented fallback behavior.

Catalog for this slice:

| Variable | Session value | Global value | Notes |
| --- | --- | --- | --- |
| `Aborted_clients` | `0` | `0` | Placeholder connection-abort counter; MyLite has no server-side network connection lifecycle yet. |
| `Aborted_connects` | `0` | `0` | Placeholder failed-connect counter. |
| `Bytes_received` | `0` | `0` | Placeholder protocol byte counter. |
| `Bytes_sent` | `0` | `0` | Placeholder protocol byte counter. |
| `Com_alter_table` | `0` | `0` | Placeholder command counter. |
| `Com_begin` | `0` | `0` | Placeholder command counter; exact statement accounting is deferred. |
| `Com_commit` | `0` | `0` | Placeholder command counter. |
| `Com_create_db` | `0` | `0` | Placeholder command counter. |
| `Com_create_index` | `0` | `0` | Placeholder command counter. |
| `Com_create_table` | `0` | `0` | Placeholder command counter. |
| `Com_delete` | `0` | `0` | Placeholder command counter. |
| `Com_drop_db` | `0` | `0` | Placeholder command counter. |
| `Com_drop_index` | `0` | `0` | Placeholder command counter. |
| `Com_drop_table` | `0` | `0` | Placeholder command counter. |
| `Com_insert` | `0` | `0` | Placeholder command counter. |
| `Com_release_savepoint` | `0` | `0` | Placeholder command counter. |
| `Com_rename_table` | `0` | `0` | Placeholder command counter. |
| `Com_replace` | `0` | `0` | Placeholder command counter. |
| `Com_rollback` | `0` | `0` | Placeholder command counter. |
| `Com_rollback_to_savepoint` | `0` | `0` | Placeholder command counter. |
| `Com_savepoint` | `0` | `0` | Placeholder command counter. |
| `Com_select` | `0` | `0` | Placeholder command counter. |
| `Com_set_option` | `0` | `0` | Placeholder for `SET`-family statements currently implemented by charset features. |
| `Com_show_errors` | `0` | `0` | Placeholder command counter. |
| `Com_show_fields` | `0` | `0` | Placeholder for `SHOW COLUMNS` / `SHOW FIELDS`. |
| `Com_show_keys` | `0` | `0` | Placeholder for `SHOW INDEX` / `SHOW KEYS`. |
| `Com_show_status` | `0` | `0` | Placeholder command counter; the reporting statement itself is not counted yet. |
| `Com_show_tables` | `0` | `0` | Placeholder command counter. |
| `Com_show_variables` | `0` | `0` | Placeholder command counter. |
| `Com_show_warnings` | `0` | `0` | Placeholder command counter. |
| `Com_stmt_close` | `0` | `0` | Placeholder prepared-statement command counter. |
| `Com_stmt_execute` | `0` | `0` | Placeholder prepared-statement command counter. |
| `Com_stmt_fetch` | `0` | `0` | Placeholder prepared-statement command counter. |
| `Com_stmt_prepare` | `0` | `0` | Placeholder prepared-statement command counter. |
| `Com_stmt_reprepare` | `0` | `0` | Placeholder prepared-statement command counter. |
| `Com_stmt_reset` | `0` | `0` | Placeholder prepared-statement command counter. |
| `Com_stmt_send_long_data` | `0` | `0` | Placeholder prepared-statement command counter. |
| `Com_truncate` | `0` | `0` | Placeholder command counter. |
| `Com_update` | `0` | `0` | Placeholder command counter. |
| `Connections` | `1` | `1` | A MyLite handle represents one embedded connection. Broader process-wide connection counting is deferred. |
| `Created_tmp_disk_tables` | `0` | `0` | Placeholder temporary-table counter. |
| `Created_tmp_tables` | `0` | `0` | Placeholder temporary-table counter. |
| `Handler_commit` | `0` | `0` | Placeholder handler operation counter. |
| `Handler_delete` | `0` | `0` | Placeholder handler operation counter. |
| `Handler_external_lock` | `0` | `0` | Placeholder handler operation counter. |
| `Handler_mrr_init` | `0` | `0` | Placeholder handler operation counter. |
| `Handler_prepare` | `0` | `0` | Placeholder handler operation counter. |
| `Handler_read_first` | `0` | `0` | Placeholder handler operation counter. |
| `Handler_read_key` | `0` | `0` | Placeholder handler operation counter. |
| `Handler_read_last` | `0` | `0` | Placeholder handler operation counter. |
| `Handler_read_next` | `0` | `0` | Placeholder handler operation counter. |
| `Handler_read_prev` | `0` | `0` | Placeholder handler operation counter. |
| `Handler_read_rnd` | `0` | `0` | Placeholder handler operation counter. |
| `Handler_read_rnd_next` | `0` | `0` | Placeholder handler operation counter. |
| `Handler_rollback` | `0` | `0` | Placeholder handler operation counter. |
| `Handler_savepoint` | `0` | `0` | Placeholder handler operation counter. |
| `Handler_savepoint_rollback` | `0` | `0` | Placeholder handler operation counter. |
| `Handler_update` | `0` | `0` | Placeholder handler operation counter. |
| `Handler_write` | `0` | `0` | Placeholder handler operation counter. |
| `Last_query_cost` | `0.000000` | `0.000000` | Placeholder optimizer-cost value. |
| `Open_tables` | `0` | `0` | Placeholder table-cache counter; MyLite does not expose a MySQL server table cache. |
| `Opened_tables` | `0` | `0` | Placeholder table-cache counter. |
| `Queries` | `0` | `0` | Placeholder statement counter; exact prepare/execute/protocol accounting is deferred. |
| `Questions` | `0` | `0` | Placeholder statement counter; exact prepare/execute/protocol accounting is deferred. |
| `Select_full_join` | `0` | `0` | Placeholder SELECT optimizer counter. |
| `Select_full_range_join` | `0` | `0` | Placeholder SELECT optimizer counter. |
| `Select_range` | `0` | `0` | Placeholder SELECT optimizer counter. |
| `Select_range_check` | `0` | `0` | Placeholder SELECT optimizer counter. |
| `Select_scan` | `0` | `0` | Placeholder SELECT optimizer counter. |
| `Slow_queries` | `0` | `0` | Placeholder slow-query counter. |
| `Sort_merge_passes` | `0` | `0` | Placeholder sort counter. |
| `Sort_range` | `0` | `0` | Placeholder sort counter. |
| `Sort_rows` | `0` | `0` | Placeholder sort counter. |
| `Sort_scan` | `0` | `0` | Placeholder sort counter. |
| `Table_locks_immediate` | `0` | `0` | Placeholder table-lock counter. |
| `Table_locks_waited` | `0` | `0` | Placeholder table-lock counter. |
| `Threads_cached` | `0` | `0` | MyLite does not maintain a server thread cache. |
| `Threads_connected` | `1` | `1` | A live MyLite handle represents one embedded connection. |
| `Threads_created` | `1` | `1` | Placeholder embedded-thread value for the current handle. |
| `Threads_running` | `1` | `1` | The current statement is running on the caller's thread. |
| `Uptime` | seconds since handle open | same as session | Process-wide server uptime is mapped to handle lifetime in this embedded slice. |
| `Uptime_since_flush_status` | same as `Uptime` | same as session | `FLUSH STATUS` is not implemented, so no later reset point exists. |

LIKE filtering:

- `%` matches any byte sequence.
- `_` matches one byte.
- Backslash escapes the following byte for SHOW-pattern purposes.
- Matching is case-insensitive for status names, matching verified MySQL
  behavior for this statement.

WHERE filtering:

- `WHERE expr` is evaluated over the displayed `Variable_name` and `Value`
  columns.
- The shared SHOW filter supports displayed-column identifiers, literals,
  comparison operators, `AND`/`OR`/`NOT`, `LIKE`, `IN`, unary signs,
  `IS NULL`, `IS NOT NULL`, and parentheses.
- Unknown displayed-column identifiers return MySQL error `1054`.
- Broader SHOW `WHERE` expressions remain deferred.

## Storage And Performance

This feature is read-only and requires no file format change. Runtime execution
materializes a small deterministic in-memory status catalog into a SQLite read
statement. `Uptime` values are derived from handle-owned open time; no process
global mutable state is introduced.

## Tests

Parser coverage:

- `SHOW STATUS`
- `SHOW GLOBAL STATUS`
- `SHOW SESSION STATUS`
- `SHOW LOCAL STATUS`
- scope forms with `LIKE`
- `SHOW STATUS WHERE Variable_name = 'Uptime'`
- `SHOW STATUS WHERE Value = '0'`
- `GLOBAL`, `SESSION`, `LOCAL`, and `STATUS` as unquoted identifiers where the
  grammar permits them
- syntax rejection for non-string `LIKE`, combined `LIKE` plus `WHERE`,
  `SHOW STATUS LIMIT 1`, repeated scope modifiers, and trailing scope
  modifiers

Runtime coverage:

- exact result column names
- MySQL 8.4.9-derived result-column descriptors for session and global scopes
- unfiltered catalog contains expected high-value rows
- deterministic row ordering
- `LIKE` exact, wildcard, escaped underscore, case-insensitive, and empty
  filtering
- `SESSION`, omitted scope, and `LOCAL` return session values
- `GLOBAL` returns the same embedded process/default values for this slice
- `WHERE` filters displayed columns and reports unknown-column diagnostics
- `LIMIT` remains a syntax error
- `Uptime` and `Uptime_since_flush_status` are numeric strings
- `SHOW STATUS` clears prior diagnostics before reporting

## Deferred Work

- Full MySQL status-variable catalog.
- Broader SHOW `WHERE` expressions beyond the shared filter subset.
- Accurate `Questions`, `Queries`, and `Com_*` command counters.
- Global status aggregation across multiple MyLite handles.
- `FLUSH STATUS` and resettable session/global counter lifetimes.
- Protocol byte counters, prepared-statement counters, server-thread counters,
  table-lock counters, temporary-table counters, and Performance Schema status
  tables.
