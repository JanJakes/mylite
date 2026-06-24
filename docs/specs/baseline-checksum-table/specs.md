# Baseline CHECKSUM TABLE

## Summary

This slice adds a narrow result-set surface for:

- `CHECKSUM TABLE table_name[, ...]`
- `CHECKSUM TABLE table_name[, ...] QUICK`
- `CHECKSUM TABLE table_name[, ...] EXTENDED`

The goal is to keep applications and maintenance probes from failing when they
issue `CHECKSUM TABLE`. MyLite accepts the statement, resolves targets through
the MyLite catalog, returns MySQL-shaped `Table` and `Checksum` columns, and
preserves MySQL-style diagnostics for unavailable targets. It does not yet claim
MySQL-compatible checksum values for stored rows.

Authoritative references:

- MySQL 8.4 Reference Manual, `CHECKSUM TABLE`:
  <https://dev.mysql.com/doc/refman/8.4/en/checksum-table.html>
- MySQL 8.4 Reference Manual, Table Maintenance Statements:
  <https://dev.mysql.com/doc/refman/8.4/en/table-maintenance-statements.html>
- MySQL implicit-commit rules:
  <https://dev.mysql.com/doc/refman/8.4/en/implicit-commit.html>

The result shape, syntax boundaries, diagnostics, row counts, and warning counts
below were verified against MySQL 8.4.9 using the local `mylite-mysql-849`
runtime and are captured by
`packages/libmylite/tests/mysql_baseline_checksum_table_expectations.sh`.

## Scope

Included:

- Persistent base tables and shadowing session temporary base tables resolvable
  through the existing selected/default schema policy.
- Unqualified and schema-qualified table names.
- One or more target names in one statement, preserving input order.
- Optional `QUICK` or `EXTENDED` after the full target list.
- MySQL-shaped result metadata with two columns: `Table`, `Checksum`.
- `ROW_COUNT() = -1` for successful result-set execution.
- `@@warning_count = 0` for resolved supported targets.
- Error-level warning diagnostics for unknown schemas, unknown tables, reserved
  `_mylite_*` targets, and unsupported object kinds while still returning the
  result set.
- MySQL-compatible implicit commit before execution, including clearing active
  user-visible savepoints.

Deferred:

- MySQL-compatible physical checksum calculation.
- Storage-engine-specific `QUICK` versus `EXTENDED` checksum differences.
- Partition syntax and partition checksums.
- Views and stored object checksums beyond deterministic MySQL-shaped
  unsupported-object diagnostics.
- Privilege checks, binary logging, replication, Performance Schema, and locking
  protocol side effects.
- Maintenance of arbitrary SQLite tables or `_mylite_*` internals.

## Syntax

The MyLite grammar is independently authored for the admitted subset:

```lemon
statement ::= checksum_table_statement.

checksum_table_statement ::=
    CHECKSUM TABLE table_name_list checksum_table_option_opt.

checksum_table_option_opt ::= .
checksum_table_option_opt ::= QUICK.
checksum_table_option_opt ::= EXTENDED.
```

`table_name_list` reuses the existing `table_name` grammar, so quoted
identifiers and schema-qualified names follow the same identifier handling as
other table statements.

Unsupported forms must be deterministic parser rejections when the grammar can
reject them. This slice rejects `CHECKSUM LOCAL TABLE t`, `CHECKSUM TABLE`,
`CHECKSUM TABLE QUICK t`, `CHECKSUM TABLE t FAST`, multiple checksum options,
and options attached before later comma-separated targets such as
`CHECKSUM TABLE t QUICK, other`.

## MySQL 8.4.9 Observations

Runtime probes used a disposable schema under MySQL 8.4.9. Observed behavior:

- `CHECKSUM TABLE a` returns one row with `Table = schema.a` and a numeric
  checksum for an InnoDB base table.
- `CHECKSUM TABLE a QUICK` returns `Checksum = NULL` for the probed InnoDB
  table and no warnings.
- `CHECKSUM TABLE a EXTENDED` returns the same numeric checksum as the no-option
  form for the probed table.
- Result metadata reports `Table` as `VAR_STRING` with collation
  `latin1_swedish_ci`, length `384`, decimals `31`, and no flags. It reports
  `Checksum` as `LONGLONG` with binary collation, length `22`, decimals `0`,
  and `BINARY NUM` flags.
- `CHECKSUM TABLE empty_table` returns `0`.
- Multiple targets preserve input order.
- Schema-qualified targets work without a selected default schema.
- An unqualified target without a selected schema fails with
  `ERROR 1046 (3D000): No database selected`.
- Duplicate target aliases fail with `ERROR 1066 (42000)`.
- Unknown schemas and unknown tables return one result row with `Checksum =
  NULL`, then add one `SHOW WARNINGS` row whose `Level` is `Error`.
- Views return one result row with `Checksum = NULL`, then add one
  `SHOW WARNINGS` row with error code `1347` and text indicating the object is
  not a base table.
- Successful result-set executions report `ROW_COUNT() = -1`.

## Name Resolution

Each target is resolved independently using the existing MyLite table-name
policy:

- Unqualified names require an active selected schema.
- Schema-qualified names use their explicit schema and do not require a selected
  schema.
- Unknown schemas and unknown tables do not fail `mylite_execute()` once the
  statement has parsed and target names have been collected; they return result
  rows and diagnostics.
- Reserved `_mylite_*` schemas or table names are not exposed. They are reported
  as unavailable targets.
- Temporary table descriptors shadow persistent table descriptors in the same
  schema.
- Duplicate target names are rejected with MySQL-compatible duplicate-alias
  diagnostics.
- Descriptor table kind is authoritative. Base and temporary table descriptors
  are supported; other object kinds return unsupported-object diagnostics once
  such descriptors are resolvable.

The displayed `Table` value is the schema-qualified logical name
`schema.table`, with decoded identifier text, matching MySQL's visible result
format for ordinary identifiers.

Current descriptor lookup uses the same case-sensitivity and comparison policy
as the rest of the catalog. Duplicate target detection remains ASCII
case-insensitive, matching the existing table-maintenance implementation.

## Result Semantics

`CHECKSUM TABLE` is a row-result statement. It does not report an affected-row
DML count; `ROW_COUNT()` reports `-1`.

For every admitted target, MyLite returns exactly one row:

- `Table`: the schema-qualified logical display name.
- `Checksum`: a deterministic unsigned decimal MyLite checksum for normal and
  `EXTENDED` resolved base-table targets, `0` for empty base tables, and `NULL`
  for `QUICK`, unavailable targets, and unsupported object kinds.

The result metadata follows the observed MySQL 8.4.9 metadata surface:

- `Table`: `VAR_STRING`, `latin1_swedish_ci`, display length `384`, decimals
  `31`, no flags, nullable.
- `Checksum`: `LONGLONG`, binary collation, display length `22`, decimals `0`,
  `BINARY NUM` flags, nullable.

The MyLite checksum is deliberately not a MySQL storage-engine checksum.
MySQL checksum values depend on storage-engine and row-format details that
MyLite does not reproduce on SQLite physical storage. This slice implements a
stable MyLite-owned row scan so applications get useful non-`NULL` base-table
metadata while exact MySQL checksum parity remains deferred.

For unknown schemas, unknown tables, reserved targets, and unsupported object
kinds, MyLite also returns one row with `Checksum = NULL` and appends one
diagnostic warning row whose visible `Level` is `Error`.

## Diagnostics

Statement-level failures:

- Syntax errors use the existing parse error path, currently code `1064` and
  SQLSTATE `42000`.
- Missing default schema for an unqualified target uses code `1046` and
  SQLSTATE `3D000`.
- Duplicate target names use code `1066` and SQLSTATE `42000`.
- Allocation failures use the existing `MYLITE_NOMEM`/`HY001` path.

Successful result-set execution with target diagnostics:

- Unknown schema: one result row with `Checksum = NULL`, plus `SHOW WARNINGS`
  row `Error 1049 Unknown database 'schema'`.
- Unknown table and reserved table names: one result row with `Checksum = NULL`,
  plus `SHOW WARNINGS` row `Error 1146 Table 'schema.table' doesn't exist`.
- Unsupported object kind: one result row with `Checksum = NULL`, plus
  `SHOW WARNINGS` row `Error 1347 'schema.table' is not BASE TABLE`.

The public `mylite_execute()` call returns `MYLITE_OK` when it successfully
builds the checksum result set, even when warning diagnostics are present.
`mylite_result_warning_count()` and `@@warning_count` report the count of
diagnostic rows appended by the statement.

## Transactions

MySQL classifies table maintenance statements as administrative statements that
implicitly commit an active transaction. MyLite therefore commits an active
user transaction before executing supported `CHECKSUM TABLE` statements and
clears user-visible savepoints. The result generation itself does not start a
new user transaction.

If the implicit commit fails, the statement fails through the existing physical
SQLite/control diagnostic path before any checksum result rows are produced.

## Physical SQLite Handling

This baseline is MyLite-owned result synthesis. It does not generate SQLite
`ANALYZE`, `REINDEX`, `VACUUM`, checksum SQL, table scans, or table rewrites.

The implementation reads MyLite descriptors to decide whether a logical target
is a supported base table. SQLite schema text is not the source of truth. The
feature does not mutate catalog rows, descriptor versions, catalog generation,
`sqlite_schema_generation`, stored rows, the `.mylite` preamble, or VFS offset
invariants.

## Ownership Boundaries

- Public API: unchanged. Existing `mylite_execute()` and result APIs expose the
  row-result statement.
- Parser/AST: owns recognition of the narrow `CHECKSUM TABLE` grammar and target
  list.
- Runtime/analyzer: owns selected-schema resolution, descriptor lookup,
  duplicate detection, implicit transaction commit, row synthesis, diagnostics,
  warning counts, and `ROW_COUNT()` behavior.
- Catalog: remains authoritative for schemas, table descriptors, object kind,
  and logical names. It is read-only for this feature.
- Result builder: owns the two-column result metadata and text/NULL row storage.
- Storage/VFS/SQLite: physical table rows are scanned read-only for normal and
  `EXTENDED` checksum values; the `.mylite` file format and SQLite fork remain
  unchanged.

## Tests

Fast C tests must cover:

- Parser acceptance for no option, `QUICK`, `EXTENDED`, schema-qualified names,
  and multiple targets.
- Parser rejection for misplaced modifiers, unsupported options, missing target
  lists, multiple options, and options before additional comma-separated names.
- Runtime result rows for base tables, temporary table shadowing, multiple
  targets, `QUICK`, and `EXTENDED`.
- `ROW_COUNT() = -1`, result warning counts, `@@warning_count`, and absence of
  affected rows for successful resolved targets.
- Missing default schema, duplicate aliases, unknown schema, unknown table,
  reserved names, rename/drop interactions, and warning snapshots.
- Implicit commit, savepoint cleanup, reopen persistence, independent file-backed
  handles, and `.mylite` preamble preservation.

The MySQL expectation artifact must verify the MySQL 8.4.9 behavior used by this
spec, including result shapes, warning rows, syntax errors, duplicate errors,
missing-default-schema errors, and the storage-engine-specific observation that
`QUICK` returns `NULL` for the probed InnoDB table.
