# Baseline Table Maintenance Statements

## Summary

This slice adds a narrow MySQL-compatible maintenance statement surface for
existing persistent or shadowing session temporary base tables:

- `ANALYZE [NO_WRITE_TO_BINLOG | LOCAL] TABLE table_name[, ...]`
- `CHECK TABLE table_name[, ...] [option ...]`
- `OPTIMIZE [NO_WRITE_TO_BINLOG | LOCAL] TABLE table_name[, ...]`
- `REPAIR [NO_WRITE_TO_BINLOG | LOCAL] TABLE table_name[, ...] [option ...]`

The goal is to keep common administration, import, and application-health paths
from failing when they issue table maintenance statements. MyLite returns the
same four-column result shape MySQL uses for these statements, but it does not
claim real physical table optimization, repair, checksum validation, histogram
management, partition maintenance, or storage-engine statistics updates.
`CHECKSUM TABLE` has a different two-column result shape and is specified in
`docs/specs/baseline-checksum-table/specs.md`.

Authoritative references:

- MySQL 8.4 Reference Manual, Table Maintenance Statements:
  <https://dev.mysql.com/doc/refman/8.4/en/table-maintenance-statements.html>
- `ANALYZE TABLE`: <https://dev.mysql.com/doc/refman/8.4/en/analyze-table.html>
- `CHECK TABLE`: <https://dev.mysql.com/doc/refman/8.4/en/check-table.html>
- `OPTIMIZE TABLE`: <https://dev.mysql.com/doc/refman/8.4/en/optimize-table.html>
- `REPAIR TABLE`: <https://dev.mysql.com/doc/refman/8.4/en/repair-table.html>
- MySQL implicit-commit rules:
  <https://dev.mysql.com/doc/refman/8.4/en/implicit-commit.html>

The expected row contents and diagnostics below are verified against MySQL
8.4.9 by `packages/libmylite/tests/mysql_baseline_table_maintenance_expectations.sh`.

## Scope

Included:

- Persistent and shadowing session temporary base tables resolvable through the
  existing selected/default schema policy.
- Unqualified and schema-qualified table names.
- One or more table names in one statement, preserving input order.
- Optional `NO_WRITE_TO_BINLOG` and `LOCAL` for `ANALYZE`, `OPTIMIZE`, and
  `REPAIR`, accepted as no-op replication modifiers.
- Optional `CHECK TABLE` options `FOR UPGRADE`, `QUICK`, `FAST`, `MEDIUM`,
  `EXTENDED`, and `CHANGED`, accepted and ignored for this baseline.
- Optional `REPAIR TABLE` options `QUICK`, `EXTENDED`, and `USE_FRM`, accepted
  and ignored for this baseline.
- MySQL-shaped result metadata: `Table`, `Op`, `Msg_type`, `Msg_text`.
- `ROW_COUNT() = -1` and `warning_count = 0` after successful result-set
  maintenance statements, including result rows whose `Msg_type` is `note` or
  `Error`.
- MySQL-compatible implicit commit before execution, including clearing active
  user-visible savepoints.

Deferred:

- `ANALYZE TABLE ... UPDATE HISTOGRAM` and `DROP HISTOGRAM`.
- `ALTER TABLE ... ANALYZE|CHECK|OPTIMIZE|REPAIR PARTITION`.
- Real statistics collection, optimizer-statistics persistence, histogram
  descriptors, physical table rebuilds, physical repair, and checksum
  calculation.
- Views, stored objects, partitioned tables, privileges, binary logging,
  replication, Performance Schema, and locking/protocol side effects.
- Options beyond the explicitly listed baseline options.
- Maintenance of arbitrary SQLite tables or `_mylite_*` internals.

## Syntax

The MyLite grammar is independently authored for the admitted subset:

```lemon
statement ::= table_maintenance_statement.

table_maintenance_statement ::=
    ANALYZE maintenance_binlog_opt TABLE table_name_list.
table_maintenance_statement ::=
    CHECK TABLE table_name_list check_table_option_list_opt.
table_maintenance_statement ::=
    OPTIMIZE maintenance_binlog_opt TABLE table_name_list.
table_maintenance_statement ::=
    REPAIR maintenance_binlog_opt TABLE table_name_list repair_table_option_list_opt.

maintenance_binlog_opt ::= .
maintenance_binlog_opt ::= NO_WRITE_TO_BINLOG.
maintenance_binlog_opt ::= LOCAL.

check_table_option_list_opt ::= .
check_table_option_list_opt ::= check_table_option_list.
check_table_option_list ::= check_table_option.
check_table_option_list ::= check_table_option_list check_table_option.
check_table_option ::= FOR UPGRADE.
check_table_option ::= QUICK.
check_table_option ::= FAST.
check_table_option ::= MEDIUM.
check_table_option ::= EXTENDED.
check_table_option ::= CHANGED.

repair_table_option_list_opt ::= .
repair_table_option_list_opt ::= repair_table_option_list.
repair_table_option_list ::= repair_table_option.
repair_table_option_list ::= repair_table_option_list repair_table_option.
repair_table_option ::= QUICK.
repair_table_option ::= EXTENDED.
repair_table_option ::= USE_FRM.
```

`table_name_list` reuses the existing `table_name` grammar, so quoted
identifiers and schema-qualified names follow the same identifier handling as
other table statements.

Unsupported forms must fail deterministically at parse time when the grammar can
reject them, or with a MyLite-specific unsupported diagnostic after parsing if
needed. In this slice, histogram clauses, partition maintenance, and unsupported
options are parser rejections. `CHECKSUM TABLE` is handled by the separate
baseline checksum slice.

## Name Resolution

Each table name is resolved independently using the existing MyLite table-name
policy:

- Unqualified table names require an active selected schema.
- Schema-qualified table names use their explicit schema and do not require a
  selected schema.
- Unknown schemas and unknown tables do not fail the statement with a public
  API error. MySQL returns maintenance result rows for these cases, so MyLite
  must also return rows.
- Reserved `_mylite_*` schemas or tables are not exposed to maintenance. They
  are reported as unavailable table targets using the same result-row style as
  unknown user objects.
- Duplicate table names are rejected with MySQL-compatible duplicate alias
  diagnostics when MySQL does so for the admitted syntax.
- Unsupported object kinds must produce deterministic result rows once such
  descriptors exist. For this slice, only base-table descriptors can be
  resolved.

The displayed `Table` value is the schema-qualified logical name
`schema.table`, with decoded identifier text, matching MySQL's visible result
format for ordinary identifiers.

## Statement Semantics

All supported maintenance statements are result-set statements. They return no
affected-row DML count; `ROW_COUNT()` reports `-1`, matching MySQL result-set
semantics.

For a resolved MyLite base table:

- `ANALYZE TABLE` returns one row:
  `schema.table`, `analyze`, `status`, `OK`.
- `CHECK TABLE` returns one row:
  `schema.table`, `check`, `status`, `OK`.
- `OPTIMIZE TABLE` returns two rows:
  `schema.table`, `optimize`, `note`,
  `Table does not support optimize, doing recreate + analyze instead`;
  then `schema.table`, `optimize`, `status`, `OK`.
- `REPAIR TABLE` returns one row:
  `schema.table`, `repair`, `note`,
  `The storage engine for the table doesn't support repair`.

For an unknown table in a known schema, MySQL 8.4.9 returns two rows for
`ANALYZE TABLE missing`: an `Error` row with the unknown-table message and a
`status` row `Operation failed`. MyLite follows that shape for all four
maintenance operations in this baseline, using the operation name appropriate
to the statement.

For an unknown schema, MySQL 8.4.9 returns an `Error` row with
`Unknown database 'schema'` and a second row with `Msg_type = error` and
`Msg_text = Corrupt` for `CHECK TABLE schema.table`. MyLite follows that result
shape for all four operations in this baseline.

No warnings are added for supported maintenance rows, including `note` rows.
`SHOW WARNINGS` remains empty and `@@warning_count` remains zero after the
supported result-set paths.

## Transactions

MySQL documents table maintenance statements as administrative statements that
implicitly commit an active transaction. MyLite therefore commits an active
user transaction before executing a supported maintenance statement and clears
user-visible savepoints. The maintenance result generation itself does not
start a new user transaction and does not mutate catalog descriptors, descriptor
versions, catalog generation, `sqlite_schema_generation`, or stored row data.

If the implicit commit fails, the statement fails with the existing physical
SQLite/control diagnostic path before any maintenance result rows are produced.

## Physical SQLite Handling

This baseline is MyLite-owned result synthesis. It does not generate SQLite
`ANALYZE`, `REINDEX`, `VACUUM`, `PRAGMA integrity_check`, or any SQLite table
rewrite/repair SQL. That is intentional: MySQL's maintenance statements expose
MySQL storage-engine semantics, while this slice only admits MyLite's current
InnoDB-like embedded base-table surface.

The implementation reads MyLite descriptors to decide whether a logical
maintenance target is a supported base table. It does not inspect SQLite schema
text as the source of truth and it does not touch MyLite's file preamble or VFS
offset invariants.

## Ownership Boundaries

- Public API: unchanged. The existing `mylite_execute()` result conventions
  expose row-result maintenance statements.
- Parser/AST: owns recognition of the narrow maintenance grammar and table-name
  list structure.
- Runtime/analyzer: owns selected-schema resolution, descriptor lookup,
  implicit transaction commit, result-row synthesis, diagnostics, and warning
  counts.
- Catalog: remains authoritative for schemas, tables, object kind, and logical
  table names. It is read-only for this feature.
- Result builder: owns the four-column result metadata and text row storage.
- Storage/VFS/SQLite: unchanged. SQLite remains physical storage, but this
  feature does not ask SQLite to perform maintenance operations.

## Diagnostics

Supported statement-level result rows should not set public execution errors.
The public `mylite_execute()` call returns `MYLITE_OK` when it successfully
builds the maintenance result set, even when the result rows report an unknown
table or schema.

Diagnostics to cover:

- Syntax errors and unsupported grammar forms: parser syntax error.
- Missing default schema for an unqualified table: MySQL-compatible
  `1046 / 3D000 / No database selected`.
- Duplicate table aliases/names in the same maintenance statement:
  MySQL-compatible duplicate-table/alias diagnostic.
- Unknown schema: successful result set with `Error` and `error` rows.
- Unknown table: successful result set with `Error` and `status` rows.
- Reserved `_mylite_*` names: successful unavailable-target rows rather than
  physical SQLite access.
- Unsupported object kind: successful unavailable-target rows.
- Allocation failure: `MYLITE_NOMEM`, no leaked partial result.
- Implicit-commit SQLite failure: existing normalized runtime error.
- Public API misuse: unchanged.

## Tests

Add a focused C runtime test binary or extend an existing runtime statement
test, whichever keeps CTest names clear. Tests must cover:

- Parser acceptance for all four statements, optional `LOCAL` and
  `NO_WRITE_TO_BINLOG`, `CHECK TABLE` options, `REPAIR TABLE` options,
  schema-qualified and comma-separated table lists.
- Parser rejection for histogram clauses, partition forms, unsupported options,
  and missing table list.
- Successful result metadata and rows for `ANALYZE`, `CHECK`, `OPTIMIZE`, and
  `REPAIR`.
- Multiple table rows preserve input order.
- Unqualified and schema-qualified resolution.
- Missing default schema public error.
- Unknown table and unknown schema result-row shapes.
- Duplicate table names.
- Implicit commit and savepoint cleanup before maintenance, including the row
  persisting after a later `ROLLBACK`.
- No catalog mutation, descriptor version churn, or row-data mutation.
- Reopen persistence and MyLite preamble preservation.
- Warning count remains zero and no result rows are added to `SHOW WARNINGS`.
- Existing parser, transaction, file-format, catalog, DDL/DML, and result tests
  still pass.
