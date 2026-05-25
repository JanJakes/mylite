# Baseline SHOW Binary Log Metadata

## Summary

This phase adds a narrow embedded metadata surface for `SHOW BINARY LOG STATUS`
and `SHOW BINARY LOGS`. MyLite does not write binary log files, rotate binary
logs, recover GTIDs, replicate, or expose a server data directory. The supported
behavior is therefore a deterministic placeholder result that matches the MySQL
8.4.9 column shapes and diagnostics while making the embedded no-replication
boundary explicit.

The placeholder is aligned with the existing fixed binary-log system-variable
surface: `@@log_bin` reports enabled compatibility mode, `@@log_bin_basename`
reports `binlog`, `@@log_bin_index` reports `binlog.index`, and GTID set
variables report empty sets.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Server identity and binary-log system variables:
  `docs/specs/baseline-server-identity-binary-log-system-variables/specs.md`
- SQL replication compatibility:
  `docs/compatibility/sql-replication.md`
- MySQL 8.4 Reference Manual, `SHOW BINARY LOG STATUS`:
  https://dev.mysql.com/doc/refman/8.4/en/show-binary-log-status.html
- MySQL 8.4 Reference Manual, `SHOW BINARY LOGS`:
  https://dev.mysql.com/doc/refman/8.4/en/show-binary-logs.html
- MySQL 8.4 Reference Manual, unsupported `SHOW MASTER STATUS` replacement:
  https://dev.mysql.com/doc/refman/8.4/en/show-master-status.html
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- Observed MySQL 8.4.9 runtime behavior recorded by
  `packages/libmylite/tests/mysql_baseline_show_binary_log_metadata_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

Observed against the local `mysql:8.4.9` runtime:

- `SHOW BINARY LOG STATUS` returns columns `File`, `Position`, `Binlog_Do_DB`,
  `Binlog_Ignore_DB`, and `Executed_Gtid_Set`.
- `SHOW BINARY LOGS` returns columns `Log_name`, `File_size`, and `Encrypted`.
- Successful `SHOW BINARY LOG STATUS` and `SHOW BINARY LOGS` leave
  `@@warning_count == 0`, `@@error_count == 0`, and make the next
  `ROW_COUNT()` return `-1`.
- Both statements reject `LIKE`, `WHERE`, `LIMIT`, and `FULL` forms with syntax
  errors.
- `SHOW MASTER STATUS` is no longer supported in MySQL 8.4.9 and is rejected as
  syntax error; MyLite keeps that rejection.

The runtime fixture used by this repository has binary logging enabled and
therefore returns live file names, sizes, and positions. MyLite intentionally
does not preserve those live values because it has no physical binary log.

## Scope

The implementation must add:

- parser and AST support for exactly `SHOW BINARY LOG STATUS` and
  `SHOW BINARY LOGS`;
- a runtime result builder that emits MySQL 8.4.9 column labels;
- one deterministic embedded placeholder row for each statement:
  - `SHOW BINARY LOG STATUS`: `binlog.000001`, position `4`, empty
    `Binlog_Do_DB`, empty `Binlog_Ignore_DB`, and empty `Executed_Gtid_Set`;
  - `SHOW BINARY LOGS`: `binlog.000001`, file size `4`, and `Encrypted = No`;
- result behavior through existing public result conventions: row result set,
  affected rows `0`, warning count `0`, and following `ROW_COUNT() = -1`;
- preservation of `LOGS` as an identifier where MySQL treats it as nonreserved;
- continued rejection of `SHOW MASTER STATUS`;
- fast parser/runtime C tests and a MySQL 8.4.9 expectation artifact for
  statement shapes and unsupported grammar;
- compatibility documentation for the exact partial surface.

The placeholder position and size `4` model a synthetic empty current binary log
at the first usable position. They are compatibility metadata only; no binary
log file is present in the `.mylite` file or host filesystem.

## Non-Goals

This feature must not implement:

- physical binary log files, binary log rotation, purge, replay, or events;
- `SHOW BINLOG EVENTS`, `BINLOG`, `PURGE BINARY LOGS`, or
  `RESET BINARY LOGS AND GTIDS`;
- GTID set mutation, GTID recovery, replication channels, source/replica state,
  Group Replication, or privilege enforcement;
- filters, `LIKE`, `WHERE`, `LIMIT`, `FULL`, schema-qualified forms, ordering,
  or status counter changes such as `Com_show_binary_logs`;
- mutable `@@log_bin`, `@@log_bin_basename`, `@@log_bin_index`, or
  `@@log_bin_trust_function_creators` behavior beyond the existing
  system-variable slice;
- physical SQLite tables, storage-format changes, VFS changes, SQLite
  extension hooks, or SQLite fork patches.

## Ownership Boundaries

- Public API: unchanged. Applications use existing `mylite_execute()` and
  result accessors.
- Statement context: no new session state. Existing diagnostics, warning count,
  previous-row-count, and result lifetime behavior apply.
- Lexer/parser/AST: owns admission of exactly the two supported statements and
  rejection of unsupported extensions.
- Analyzer/planner: no descriptor, schema, table, or privilege resolution is
  needed.
- Catalog module: no catalog rows are read or written. Binary-log metadata is
  not durable catalog state.
- Result builder: emits constant MySQL-shaped text values through
  `mylite_result`.
- Storage/VFS: no `.mylite` preamble, shifted SQLite payload, or VFS behavior
  changes.
- SQLite physical storage: not used. No SQLite SQL is generated.

## Supported Grammar

This phase adds two SHOW statements:

```sql
SHOW BINARY LOG STATUS
SHOW BINARY LOGS
```

MyLite Lemon-syntax snippets:

```lemon
statement(A) ::= show_binary_log_status_statement(B). {
    A = B;
}

statement(A) ::= show_binary_logs_statement(B). {
    A = B;
}

show_binary_log_status_statement(A) ::= SHOW(S) BINARY LOG STATUS(T). {
    A = mylite_sql_parser_make_show_binary_log_status_statement(state, S, T);
}

show_binary_logs_statement(A) ::= SHOW(S) BINARY LOGS(L). {
    A = mylite_sql_parser_make_show_binary_logs_statement(state, S, L);
}
```

Unsupported `SHOW BINARY LOG STATUS LIKE ...`,
`SHOW BINARY LOG STATUS WHERE ...`, `SHOW BINARY LOG STATUS LIMIT ...`,
`SHOW BINARY LOGS LIKE ...`, `SHOW BINARY LOGS WHERE ...`,
`SHOW BINARY LOGS LIMIT ...`, `SHOW FULL BINARY LOGS`, and
`SHOW MASTER STATUS` remain syntax errors.

## Result Shapes

`SHOW BINARY LOG STATUS` returns one row:

| Column | MyLite value |
| --- | --- |
| `File` | `binlog.000001` |
| `Position` | `4` |
| `Binlog_Do_DB` | empty string |
| `Binlog_Ignore_DB` | empty string |
| `Executed_Gtid_Set` | empty string |

`SHOW BINARY LOGS` returns one row:

| Column | MyLite value |
| --- | --- |
| `Log_name` | `binlog.000001` |
| `File_size` | `4` |
| `Encrypted` | `No` |

These rows are fixed compatibility placeholders. They are independent of the
selected schema, user-created tables, file-backed reopen, handle identity,
transaction state, `@@sql_log_bin`, and existing `@@log_bin*` placeholder reads.

## Diagnostics

- Unsupported grammar forms are syntax errors with the existing parser
  diagnostic shape.
- Allocation failures use existing `MYLITE_NOMEM` and handle diagnostics.
- Public API misuse is unchanged.

No supported statement in this phase emits warnings.

## Performance and Storage

The runtime builds at most one constant row directly in memory. It does not scan
user tables, read or write catalog rows, query SQLite metadata, inspect the
filesystem, or generate SQLite SQL. The cost is bounded by allocating and
copying the static result strings.

## Test Plan

Fast C tests must cover:

- parser acceptance for `SHOW BINARY LOG STATUS` and `SHOW BINARY LOGS`;
- `LOGS` as a table identifier;
- parser rejection for unsupported `LIKE`, `WHERE`, `LIMIT`, `FULL`, and
  `SHOW MASTER STATUS` forms;
- successful runtime column labels, exact placeholder row values, row count,
  warning count, affected rows, and following `ROW_COUNT()`;
- case-insensitive statement spelling;
- behavior with and without a selected schema;
- file-backed reopen and `.mylite` preamble preservation;
- independent handles returning identical static rows.

The MySQL expectation script must verify statement column shapes, row-count and
diagnostic behavior, unsupported syntax diagnostics, and MySQL 8.4.9 version.
It must not pin live MySQL file names, positions, or file sizes because those
depend on the runtime fixture state.

## Compatibility Documentation

Update only the exact partial surface:

- `COMPATIBILITY.md` binary-log SHOW metadata row;
- `docs/compatibility/sql-show-statements.md`;
- `docs/compatibility/sql-replication.md`.

Do not document binary log files, binary log event reading, replication, GTID
state mutation, privilege enforcement, status counters, or mutable log
configuration as supported.
