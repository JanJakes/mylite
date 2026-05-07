# WP Priority Server Placeholders

## Scope

This slice expands MyLite's server-variable and server-status compatibility for
high-value application probes and import scripts. It covers:

- `SHOW VARIABLES`, `SHOW GLOBAL VARIABLES`, `SHOW SESSION VARIABLES`, and
  `SELECT @@...` reads for:
  - `default_storage_engine`
  - `foreign_key_checks`
  - `last_insert_id`
  - `lower_case_table_names`
  - `max_allowed_packet`
  - `max_connections`
  - `time_zone`
  - `unique_checks`
  - `wait_timeout`
- session `SET` for `default_storage_engine`, `foreign_key_checks`,
  `time_zone`, `unique_checks`, and `wait_timeout`
- `SHOW STATUS` placeholders for:
  - `Bytes_received`
  - `Bytes_sent`
  - `Created_tmp_disk_tables`
  - `Created_tmp_tables`
  - `Last_query_cost`
  - `Queries`
  - `Slow_queries`

The goal is compatibility with common WordPress-oriented connection probes,
schema imports, and administrative checks without pretending that MyLite has a
network server, replication layer, or process-wide mutable server state.

## Sources

The behavior is independently specified from MySQL 8.4 server-variable and
status-variable documentation plus MySQL 8.4.9 runtime probes.

- <https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html>
- <https://dev.mysql.com/doc/refman/8.4/en/server-status-variables.html>
- <https://dev.mysql.com/doc/refman/8.4/en/show-variables.html>
- <https://dev.mysql.com/doc/refman/8.4/en/show-status.html>

Runtime probes verified default values, displayed `SHOW` values, and
global/session scope errors for this slice.

## Runtime Observations

MySQL 8.4.9 returned these default `SHOW VARIABLES` rows in the verification
container:

| Variable | Value | Scope notes |
| --- | --- | --- |
| `default_storage_engine` | `InnoDB` | global and session |
| `foreign_key_checks` | `ON` | global and session |
| `last_insert_id` | `0` | session-only |
| `lower_case_table_names` | `0` | global-only |
| `max_allowed_packet` | `67108864` | global and session, session read-only |
| `max_connections` | `151` | global-only |
| `sql_log_bin` | `ON` | session-only |
| `time_zone` | `SYSTEM` | global and session |
| `unique_checks` | `ON` | global and session |
| `wait_timeout` | `28800` | global and session |

`SELECT @@...` returns integers for boolean and integer variables, and text for
string variables. Explicit wrong-scope reads return error `1238`, such as
`@@SESSION.lower_case_table_names`, `@@SESSION.max_connections`, and
`@@GLOBAL.last_insert_id`.

`SET foreign_key_checks = 0`, `SET unique_checks = 0`, `SET time_zone = '+00:00'`,
`SET wait_timeout = 123`, `SET default_storage_engine = 'MEMORY'`, and
`SET SQL_LOG_BIN = 0` update the session value. `SET wait_timeout = 0` stores
`1`. `SET ... = DEFAULT` restores the session default. MyLite implements the
same visible session state for these forms, while deeper storage-engine side
effects remain deferred.

The verified status rows are ordinary `SHOW STATUS` string values. MySQL's byte,
query, and temporary-table counters are server/session counters. MyLite exposes
documented placeholders for them until exact statement and protocol accounting
exist.

## Syntax

This slice reuses the existing independently authored MyLite grammar for
system-variable assignment:

```lemon
set_system_variable_statement ::= SET opt_set_system_variable_scope
                                  set_system_variable_name EQ
                                  set_system_variable_value.

set_system_variable_value ::= literal.
set_system_variable_value ::= PLUS numeric_literal.
set_system_variable_value ::= MINUS numeric_literal.
set_system_variable_value ::= DEFAULT.
```

No new grammar is needed for `SHOW VARIABLES`, `SHOW STATUS`, or `@@...`
expression reads.

## Semantics

### System Variables

`SHOW VARIABLES` displays boolean values as `ON` or `OFF`; `SELECT @@...`
returns `1` or `0`. Integer variables return unsigned integer values in
expressions and decimal text in `SHOW VARIABLES`.

| Variable | Session value | Global value | Assignment |
| --- | --- | --- | --- |
| `default_storage_engine` | session string, default `InnoDB` | `InnoDB` | session string or `DEFAULT`; this controls visible state only for now |
| `foreign_key_checks` | session boolean, default `ON` | `ON` | session `0`, `1`, or `DEFAULT`; enforcement effects are deferred |
| `last_insert_id` | current handle last insert id | omitted from global reads | direct assignment is deferred; `LAST_INSERT_ID(expr)` remains the mutation path |
| `lower_case_table_names` | omitted from explicit session reads | `0` | read-only |
| `max_allowed_packet` | `67108864` | `67108864` | read-only in this slice |
| `max_connections` | omitted from explicit session reads | `151` | read-only |
| `sql_log_bin` | session boolean, default `ON` | omitted from global reads | session `0`, `1`, boolean keywords, or `DEFAULT`; binary-log side effects are not applicable to MyLite |
| `time_zone` | session string, default `SYSTEM` | `SYSTEM` | session string or `DEFAULT`; temporal conversion effects are deferred |
| `unique_checks` | session boolean, default `ON` | `ON` | session `0`, `1`, or `DEFAULT`; enforcement effects are deferred |
| `wait_timeout` | session integer, default `28800` | `28800` | session integer or `DEFAULT`; values below `1` clamp to `1` |

Unsupported global assignment returns `MYLITE_UNSUPPORTED` with a variable
specific message. Unsupported value shapes return `MYLITE_EXEC_ERROR` and a
diagnostic attached to the handle.

### Status Variables

`SHOW STATUS` exposes these additional rows:

| Variable | Value | Notes |
| --- | --- | --- |
| `Bytes_received` | `0` | No MySQL network protocol byte counter exists yet |
| `Bytes_sent` | `0` | No MySQL network protocol byte counter exists yet |
| `Created_tmp_disk_tables` | `0` | Exact temporary table accounting is deferred |
| `Created_tmp_tables` | `0` | Exact temporary table accounting is deferred |
| `Last_query_cost` | `0.000000` | Optimizer cost accounting is deferred |
| `Queries` | `0` | Exact statement accounting is deferred |
| `Slow_queries` | `0` | Slow-query accounting is deferred |

## Storage And Performance

Mutable session values are stored on `mylite_db`. They do not change the
`.mylite` file format and introduce no process-global state. The additional
catalog rows are materialized through the existing small SQLite-backed `SHOW`
query builders.

## Tests

Runtime tests cover:

- default `SHOW VARIABLES` rows for each new variable
- session and global `@@...` expression reads
- wrong-scope diagnostics for global-only and session-only variables
- session `SET` and `DEFAULT` behavior for mutable variables
- `SHOW STATUS` catalog rows and focused `LIKE` filtering for the new status
  placeholders

## Deferred Work

- General mutable system-variable infrastructure and privilege-sensitive global
  assignment.
- Applying `foreign_key_checks`, `unique_checks`, and `default_storage_engine`
  to all relevant storage/runtime paths.
- Applying `time_zone` to temporal conversion and named time-zone tables.
- Enforcing `wait_timeout`.
- Exact protocol, statement, temporary-table, optimizer-cost, and slow-query
  status counters.
