# Baseline SHOW Replica Metadata

## Summary

This phase adds a narrow embedded metadata surface for `SHOW REPLICA STATUS`
and `SHOW REPLICAS`. MyLite does not configure replication channels, connect to
sources, write relay logs, or maintain source/replica topology. The supported
behavior is therefore the MySQL 8.4.9 no-replication shape: both statements are
accepted and return empty row sets with MySQL-shaped column labels, zero
warnings, and `ROW_COUNT() = -1`.

This complements the existing fixed binary-log and GTID placeholder surfaces
without claiming physical replication support.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- SQL replication compatibility:
  `docs/compatibility/sql-replication.md`
- Baseline binary-log SHOW metadata:
  `docs/specs/baseline-show-binary-log-metadata/specs.md`
- MySQL 8.4 Reference Manual, `SHOW REPLICA STATUS`:
  https://dev.mysql.com/doc/refman/8.4/en/show-replica-status.html
- MySQL 8.4 Reference Manual, `SHOW REPLICAS`:
  https://dev.mysql.com/doc/refman/8.4/en/show-replicas.html
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- Observed MySQL 8.4.9 runtime behavior recorded by
  `packages/libmylite/tests/mysql_baseline_show_replica_metadata_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

Observed against the local `mysql:8.4.9` runtime with no configured replica
channel and no registered replicas:

- `SHOW REPLICA STATUS` succeeds and returns zero rows.
- `SHOW REPLICA STATUS` exposes the following column labels, in order:
  `Replica_IO_State`, `Source_Host`, `Source_User`, `Source_Port`,
  `Connect_Retry`, `Source_Log_File`, `Read_Source_Log_Pos`, `Relay_Log_File`,
  `Relay_Log_Pos`, `Relay_Source_Log_File`, `Replica_IO_Running`,
  `Replica_SQL_Running`, `Replicate_Do_DB`, `Replicate_Ignore_DB`,
  `Replicate_Do_Table`, `Replicate_Ignore_Table`,
  `Replicate_Wild_Do_Table`, `Replicate_Wild_Ignore_Table`, `Last_Errno`,
  `Last_Error`, `Skip_Counter`, `Exec_Source_Log_Pos`, `Relay_Log_Space`,
  `Until_Condition`, `Until_Log_File`, `Until_Log_Pos`, `Source_SSL_Allowed`,
  `Source_SSL_CA_File`, `Source_SSL_CA_Path`, `Source_SSL_Cert`,
  `Source_SSL_Cipher`, `Source_SSL_Key`, `Seconds_Behind_Source`,
  `Source_SSL_Verify_Server_Cert`, `Last_IO_Errno`, `Last_IO_Error`,
  `Last_SQL_Errno`, `Last_SQL_Error`, `Replicate_Ignore_Server_Ids`,
  `Source_Server_Id`, `Source_UUID`, `Source_Info_File`, `SQL_Delay`,
  `SQL_Remaining_Delay`, `Replica_SQL_Running_State`, `Source_Retry_Count`,
  `Source_Bind`, `Last_IO_Error_Timestamp`, `Last_SQL_Error_Timestamp`,
  `Source_SSL_Crl`, `Source_SSL_Crlpath`, `Retrieved_Gtid_Set`,
  `Executed_Gtid_Set`, `Auto_Position`, `Replicate_Rewrite_DB`,
  `Channel_Name`, `Source_TLS_Version`, `Source_public_key_path`,
  `Get_Source_public_key`, and `Network_Namespace`.
- `SHOW REPLICAS` succeeds and returns zero rows.
- `SHOW REPLICAS` exposes the column labels `Server_Id`, `Host`, `Port`,
  `Source_Id`, and `Replica_UUID`.
- Successful `SHOW REPLICA STATUS` and `SHOW REPLICAS` leave
  `@@warning_count == 0`, `@@error_count == 0`, and make the next
  `ROW_COUNT()` return `-1`.
- `SHOW REPLICA STATUS FOR CHANNEL 'default'` is accepted by MySQL grammar but
  returns `3074 / HY000` when that channel does not exist.
- `SHOW REPLICA STATUS` rejects `LIKE`, `WHERE`, `LIMIT`, and `FULL` forms with
  syntax errors.
- `SHOW REPLICAS` rejects `LIKE`, `WHERE`, `LIMIT`, and `FULL` forms with
  syntax errors.
- Removed `SHOW SLAVE STATUS` and `SHOW SLAVE HOSTS` forms are syntax errors in
  MySQL 8.4.9.

## Scope

The implementation must add:

- parser and AST support for exactly `SHOW REPLICA STATUS` and `SHOW REPLICAS`;
- runtime result builders that emit the MySQL 8.4.9 column labels listed above;
- empty result sets for both statements;
- result behavior through existing public result conventions: row result set,
  affected rows `0`, warning count `0`, and following `ROW_COUNT() = -1`;
- preservation of `REPLICA` and `REPLICAS` as identifiers where MySQL treats
  them as nonreserved;
- continued rejection of removed `SHOW SLAVE STATUS` and `SHOW SLAVE HOSTS`;
- fast parser/runtime C tests and a MySQL 8.4.9 expectation artifact for
  statement shapes and unsupported grammar;
- compatibility documentation for the exact partial surface.

## Non-Goals

This feature must not implement:

- replica channel descriptors, source connection state, relay-log files,
  applier state, replication lag, or GTID recovery;
- `SHOW REPLICA STATUS FOR CHANNEL ...`;
- `SHOW SLAVE STATUS`, `SHOW SLAVE HOSTS`, or other removed/deprecated slave
  syntax;
- source registration rows for `SHOW REPLICAS`;
- filters, `LIKE`, `WHERE`, `LIMIT`, `FULL`, schema-qualified forms, ordering,
  or status counter changes such as `Com_show_replica_status`;
- privilege enforcement for `REPLICATION CLIENT` or `SUPER`;
- `CHANGE REPLICATION SOURCE TO`, `RESET REPLICA`, `START REPLICA`,
  `STOP REPLICA`, Group Replication, or replication channel diagnostics;
- physical SQLite tables, storage-format changes, VFS changes, SQLite
  extension hooks, or SQLite fork patches.

## Ownership Boundaries

- Public API: unchanged. Applications use existing `mylite_execute()` and
  result accessors.
- Statement context: no new session state. Existing diagnostics, warning count,
  previous-row-count, and result lifetime behavior apply.
- Lexer/parser/AST: owns admission of exactly the two supported statements and
  rejection of unsupported extensions.
- Analyzer/planner: no descriptor, schema, table, channel, source, replica, or
  privilege resolution is needed.
- Catalog module: no catalog rows are read or written. Replica metadata is not
  durable catalog state.
- Result builder: emits constant MySQL-shaped column labels with zero rows
  through `mylite_result`.
- Storage/VFS: no `.mylite` preamble, shifted SQLite payload, or VFS behavior
  changes.
- SQLite physical storage: not used. No SQLite SQL is generated.

## Supported Grammar

This phase adds two SHOW statements:

```sql
SHOW REPLICA STATUS
SHOW REPLICAS
```

MyLite Lemon-syntax snippets:

```lemon
statement(A) ::= show_replica_status_statement(B). {
    A = B;
}

statement(A) ::= show_replicas_statement(B). {
    A = B;
}

show_replica_status_statement(A) ::= SHOW(S) REPLICA STATUS(T). {
    A = mylite_sql_parser_make_show_replica_status_statement(state, S, T);
}

show_replicas_statement(A) ::= SHOW(S) REPLICAS(R). {
    A = mylite_sql_parser_make_show_replicas_statement(state, S, R);
}
```

Unsupported `SHOW REPLICA STATUS FOR CHANNEL ...`,
`SHOW REPLICA STATUS LIKE ...`, `SHOW REPLICA STATUS WHERE ...`,
`SHOW REPLICA STATUS LIMIT ...`, `SHOW FULL REPLICA STATUS`,
`SHOW REPLICAS LIKE ...`, `SHOW REPLICAS WHERE ...`, `SHOW REPLICAS LIMIT ...`,
`SHOW FULL REPLICAS`, `SHOW SLAVE STATUS`, and `SHOW SLAVE HOSTS` remain
syntax errors in this MyLite slice.

## Result Shapes

`SHOW REPLICA STATUS` returns the MySQL 8.4.9 column labels listed in
[MySQL 8.4.9 Observations](#mysql-849-observations), with no rows.

`SHOW REPLICAS` returns these column labels, with no rows:

| Column |
| --- |
| `Server_Id` |
| `Host` |
| `Port` |
| `Source_Id` |
| `Replica_UUID` |

The empty row sets are fixed compatibility placeholders. They are independent
of the selected schema, user-created tables, file-backed reopen, handle
identity, transaction state, `@@sql_log_bin`, GTID variables, and existing
binary-log placeholder reads.

## Diagnostics

- Unsupported grammar forms are syntax errors with the existing parser
  diagnostic shape.
- `FOR CHANNEL` is intentionally unsupported for this slice and is rejected at
  parse time rather than modeling MySQL's missing-channel runtime diagnostic.
- Allocation failures use existing `MYLITE_NOMEM` and handle diagnostics.
- Public API misuse is unchanged.

No supported statement in this phase emits warnings.

## Performance and Storage

The runtime builds only column metadata directly in memory. It does not scan
user tables, read or write catalog rows, query SQLite metadata, inspect the
filesystem, or generate SQLite SQL. The cost is bounded by allocating and
copying static column-label strings.

## Test Plan

Fast C tests must cover:

- parser acceptance for `SHOW REPLICA STATUS` and `SHOW REPLICAS`;
- `REPLICA` and `REPLICAS` as table identifiers;
- parser rejection for unsupported `FOR CHANNEL`, `LIKE`, `WHERE`, `LIMIT`,
  `FULL`, `SHOW SLAVE STATUS`, and `SHOW SLAVE HOSTS` forms;
- successful runtime column labels, empty row count, warning count, affected
  rows, and following `ROW_COUNT()`;
- case-insensitive statement spelling;
- behavior with and without a selected schema;
- file-backed reopen and `.mylite` preamble preservation;
- independent handles returning identical empty metadata result sets.

The MySQL expectation script must verify MySQL 8.4.9 version, statement column
metadata, zero-row behavior on the no-replication fixture, row-count and
diagnostic behavior, unsupported syntax diagnostics, and the observed
missing-channel diagnostic for the deferred `FOR CHANNEL` form.

## Compatibility Documentation

Update only the exact partial surface:

- `COMPATIBILITY.md` replica SHOW metadata row;
- `docs/compatibility/sql-show-statements.md`;
- `docs/compatibility/sql-replication.md`.

Do not document replication channels, relay logs, source/replica topology, GTID
state mutation, privilege enforcement, status counters, or replication command
execution as supported.
