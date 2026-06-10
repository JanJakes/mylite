# Parser Corpus SHOW Metadata Filter Surfaces

This slice admits supported MySQL 8.4.9 `SHOW` metadata forms that remain in
the MySQL server-test parser corpus:

- `SHOW STATUS ... WHERE expr`
- `SHOW EVENTS ... WHERE expr`
- `SHOW OPEN TABLES ... WHERE expr`
- `SHOW BINLOG EVENTS`

It also records that legacy removed spellings such as `SHOW SLAVE STATUS`,
`SHOW SLAVE HOSTS`, and `SHOW MASTER STATUS` remain syntax errors because
MySQL 8.4.9 rejects them.

Primary MySQL references:

- https://dev.mysql.com/doc/refman/8.4/en/show-status.html
- https://dev.mysql.com/doc/refman/8.4/en/show-events.html
- https://dev.mysql.com/doc/refman/8.4/en/show-open-tables.html
- https://dev.mysql.com/doc/refman/8.4/en/show-binlog-events.html
- https://dev.mysql.com/doc/refman/8.4/en/show-master-status.html
- https://dev.mysql.com/doc/refman/8.4/en/show-replica-status.html

## MySQL 8.4.9 Observations

Runtime probes were executed against the `mysql:8.4.9` container
`mylite-mysql-849`.

```sql
DROP DATABASE IF EXISTS mylite_parser_show_probe;
CREATE DATABASE mylite_parser_show_probe;
USE mylite_parser_show_probe;
SHOW STATUS WHERE Variable_name = 'Threads_connected';
SELECT @@warning_count, ROW_COUNT();
SHOW EVENTS WHERE Name='ev1';
SELECT @@warning_count, ROW_COUNT();
SHOW OPEN TABLES WHERE `Table` LIKE 'user';
SELECT @@warning_count, ROW_COUNT();
SHOW BINLOG EVENTS LIMIT 1;
SHOW BINARY LOG STATUS;
SHOW BINARY LOGS;
DROP DATABASE mylite_parser_show_probe;
```

Observed behavior:

- `SHOW STATUS WHERE Variable_name = 'Threads_connected'` returns the
  `Threads_connected` row, leaves `@@warning_count = 0`, and makes following
  `ROW_COUNT()` return `-1`.
- `SHOW EVENTS WHERE Name='ev1'` succeeds with the MySQL event metadata columns
  and zero rows when no events exist.
- ``SHOW OPEN TABLES WHERE `Table` LIKE 'user'`` succeeds and filters the
  currently open table-cache rows. On the probe container, this returned
  `mysql.user` with `In_use = 0` and `Name_locked = 0`.
- `SHOW BINLOG EVENTS LIMIT 1` succeeds and exposes columns `Log_name`, `Pos`,
  `Event_type`, `Server_id`, `End_log_pos`, and `Info`.
- `SHOW BINARY LOG STATUS` and `SHOW BINARY LOGS` remain the current supported
  binary-log status forms.

The same MySQL 8.4.9 runtime rejected these legacy spellings with
`1064 / 42000` parse errors:

```sql
SHOW SLAVE STATUS;
SHOW SLAVE HOSTS;
SHOW MASTER STATUS;
```

## Scope

### SHOW STATUS WHERE

MyLite already exposes a limited in-memory status registry through `SHOW
STATUS` and implements the same output-column predicate evaluator for `SHOW
VARIABLES WHERE`. This slice extends `SHOW STATUS` to accept `WHERE` predicates
over the output columns `Variable_name` and `Value`, using the current limited
predicate subset:

- comparisons with string and `NULL` literals;
- `LIKE`;
- `IN` lists of string and `NULL` literals;
- `IS [NOT] NULL`;
- `AND`, `OR`, and `NOT`.

`LIKE` and `WHERE` remain mutually exclusive, matching the general MySQL `SHOW`
filter form. Unsupported predicate expressions return deterministic MyLite
diagnostics rather than silently evaluating arbitrary SQL expressions.

### SHOW EVENTS WHERE

MyLite currently has no event descriptors and returns empty MySQL-shaped event
metadata rows. This slice admits `WHERE` predicates syntactically for `SHOW
EVENTS`, including optional `FROM` / `IN` schema selection. Since the result is
empty, no row predicate evaluation is required yet. Future event descriptor
support must evaluate `WHERE` predicates over the displayed event columns
before returning rows.

### SHOW OPEN TABLES WHERE

MyLite currently returns empty MySQL-shaped open-table metadata rows. This
slice admits `WHERE` predicates syntactically for `SHOW OPEN TABLES`, including
optional `FROM` / `IN` schema selection. Since MyLite has no table-cache row
surface yet, no row predicate evaluation is required in this slice.

### SHOW BINLOG EVENTS

MyLite has no physical binary log, event stream, rotation, GTID recovery, or
replication source. This slice adds a limited placeholder `SHOW BINLOG EVENTS`
surface with MySQL-shaped columns and a single synthetic `Format_desc` row,
consistent with the existing `SHOW BINARY LOG STATUS` and `SHOW BINARY LOGS`
placeholder model:

| Column | Placeholder |
| --- | --- |
| `Log_name` | `binlog.000001` |
| `Pos` | `4` |
| `Event_type` | `Format_desc` |
| `Server_id` | `1` |
| `End_log_pos` | `127` |
| `Info` | `Server ver: 8.4.9, Binlog ver: 4` |

`SHOW BINLOG EVENTS` accepts no `IN`, `FROM`, or `LIMIT` modifiers in this
slice. Unsupported modifiers stay syntax errors until their result semantics
are specified. `SHOW RELAYLOG EVENTS` remains unsupported.

### Removed Legacy Aliases

`SHOW MASTER STATUS`, `SHOW SLAVE STATUS`, and `SHOW SLAVE HOSTS` remain
syntax errors. The corpus contains those historical spellings, but accepting
them would diverge from MySQL 8.4.9.

## MyLite Grammar Snippets

These snippets describe MyLite's intended grammar shape for this slice and are
independently authored from documentation and runtime behavior.

```lemon
show_status_statement ::=
    SHOW show_status_scope_opt STATUS show_status_filter_opt.

show_status_filter_opt ::= .
show_status_filter_opt ::= LIKE STRING.
show_status_filter_opt ::= WHERE predicate.

show_events_statement ::= SHOW EVENTS show_event_filter_opt.
show_events_statement ::= SHOW EVENTS FROM identifier show_event_filter_opt.
show_events_statement ::= SHOW EVENTS IN identifier show_event_filter_opt.

show_open_tables_statement ::= SHOW OPEN TABLES show_schema_object_filter_opt.
show_open_tables_statement ::=
    SHOW OPEN TABLES FROM identifier show_schema_object_filter_opt.
show_open_tables_statement ::=
    SHOW OPEN TABLES IN identifier show_schema_object_filter_opt.

show_schema_object_filter_opt ::= .
show_schema_object_filter_opt ::= LIKE STRING.
show_schema_object_filter_opt ::= WHERE predicate.

show_binlog_events_statement ::= SHOW BINLOG EVENTS.
```

## Runtime Behavior

No SQLite fork hook is needed. All behavior stays in MyLite parser/runtime
code.

- `SHOW STATUS WHERE` filters the existing status descriptors after scope and
  optional `LIKE` handling.
- `SHOW EVENTS WHERE` and `SHOW OPEN TABLES WHERE` return the same empty
  metadata-shaped result as the existing `LIKE` forms.
- `SHOW BINLOG EVENTS` returns one stable placeholder row with warning count
  `0`, affected rows `0`, and following `ROW_COUNT() = -1`.
- None of these statements mutate the catalog, SQLite schema, file preamble, or
  session transaction state.

## Tests

Focused tests cover:

- parser acceptance for the new `SHOW` forms and parser rejection of removed
  legacy aliases;
- runtime `SHOW STATUS WHERE` filtering, result shape, diagnostics state, and
  no-mutation behavior;
- runtime empty `SHOW EVENTS WHERE` and `SHOW OPEN TABLES WHERE` result shapes;
- runtime `SHOW BINLOG EVENTS` result shape and row-count/warning behavior;
- MySQL 8.4.9 expectation script evidence for accepted and rejected forms;
- parser corpus movement over
  `build/perf-data/mysql-server-tests-queries.csv`.

## Compatibility Status

This slice improves limited `SHOW` metadata compatibility. It does not add live
status counters, event descriptors, event DDL, table-cache tracking, binary log
files, binlog event streaming, privileges, `SHOW BINLOG EVENTS` modifiers, or
legacy removed `SHOW MASTER` / `SHOW SLAVE` aliases.
