# Baseline Temporary CREATE TABLE SELECT

## Status

This feature specifies a narrow descriptor-driven
`CREATE TEMPORARY TABLE ... SELECT` slice:

```sql
CREATE TEMPORARY TABLE [IF NOT EXISTS] target_table [AS] SELECT ...
```

It extends the existing persistent `CREATE TABLE ... SELECT` and session-local
temporary table lifecycle. The destination descriptor is session-local, the
source query uses the current descriptor-backed single-table `SELECT` subset,
and the row copy stays SQLite-side through generated `INSERT INTO ... SELECT`.

This is not full MySQL temporary CTAS. It intentionally defers explicit
destination column definitions, `IGNORE`, `REPLACE`, `VALUES`, `TABLE`, joins,
expression projection, indexes, constraints, table options after the query,
privileges, binary logging, replication behavior, and temporary DDL inside
active MyLite user transactions.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- SQLite connection bootstrap policy:
  `docs/specs/sqlite-connection-bootstrap-policy/specs.md`
- File-backed MyLite opening VFS:
  `docs/specs/file-backed-mylite-opening-vfs/specs.md`
- MyLite file-format preamble:
  `docs/specs/mylite-file-format/specs.md`
- Baseline CREATE TABLE SELECT:
  `docs/specs/baseline-create-table-select-lifecycle/specs.md`
- Baseline temporary table lifecycle:
  `docs/specs/baseline-temporary-table-lifecycle/specs.md`
- Baseline temporary table LIKE:
  `docs/specs/baseline-temporary-table-like/specs.md`
- MySQL lexer and parser scaffold:
  `docs/specs/mysql-lexer/specs.md`,
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `CREATE TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/create-table.html
- MySQL 8.4 Reference Manual, `CREATE TEMPORARY TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/create-temporary-table.html
- MySQL 8.4 Reference Manual, `CREATE TABLE ... SELECT`:
  https://dev.mysql.com/doc/refman/8.4/en/create-table-select.html
- MySQL 8.4 Reference Manual, temporary table limitations:
  https://dev.mysql.com/doc/refman/8.4/en/temporary-table-problems.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_temporary_create_table_select_expectations.sh`
records the runtime probes for this feature. Observed behavior:

- `CREATE TEMPORARY TABLE tmp AS SELECT ...` and
  `CREATE TEMPORARY TABLE tmp SELECT ...` both succeed.
- Successful statements return no result rows, set `ROW_COUNT()` to the number
  of copied rows, and leave `@@warning_count == 0`.
- A source `SELECT` returning zero rows still creates the temporary table and
  reports `ROW_COUNT() == 0`.
- `SHOW CREATE TABLE` renders `CREATE TEMPORARY TABLE` for the target.
- `SHOW TABLES`, `SHOW TABLE STATUS`, and `INFORMATION_SCHEMA.TABLES` omit the
  temporary target.
- Creating a temporary table with the same effective name as a persistent table
  succeeds and shadows the persistent table until the temporary table is
  dropped.
- `CREATE TEMPORARY TABLE IF NOT EXISTS existing_temp AS SELECT ...` resolves a
  valid source, no-ops, reports `ROW_COUNT() == 0`, and emits one note with
  code `1050`.
- If an existing temporary target is protected by `IF NOT EXISTS`, a missing
  source table still fails before the no-op. If the source table exists, MySQL
  no-ops before duplicate selected output-name validation.
- Schema-qualified target and source names work without a selected default
  database. Unqualified target or source names require a selected default
  database at the point they are resolved.
- Protected `information_schema` targets fail with error `1044 / 42000` before
  source resolution.
- If both explicit source and target schemas are unknown, the source-schema
  error wins.
- A temporary source table shadows a persistent source with the same effective
  name.
- MySQL permits `CREATE TEMPORARY TABLE ... SELECT` inside a transaction.
  Rollback preserves the created temporary table definition and rolls back the
  copied rows.

## Scope

The implementation must add:

- parser and AST support for
  `CREATE TEMPORARY TABLE [IF NOT EXISTS] table_name [AS] select_statement`;
- optional `AS`;
- one session-local temporary target and one descriptor-backed source;
- source `SELECT` limited to the current persistent CTAS source subset:
  descriptor columns from one visible source table, optional table alias,
  `WHERE`, one-column `ORDER BY`, and `LIMIT`;
- source resolution through the visible-table rules, where session temporary
  source descriptors shadow persistent descriptors;
- target resolution through the selected/default schema policy;
- target collision handling against the session temporary catalog only;
- persistent target shadowing for temporary targets;
- descriptor inference from selected source descriptor columns, including
  aliases and selected invisible columns, using the same supported descriptor
  families as persistent CTAS;
- generated temporary SQLite table creation and descriptor-built
  `INSERT INTO ... SELECT` copy;
- failure cleanup that leaves no temporary descriptor or physical table when
  create or copy fails;
- successful result reporting with the existing non-row result shape,
  `affected_rows` equal to copied rows, and `warning_count == 0`;
- compatibility documentation for the exact limited surface.

## Non-Goals

This feature must not implement:

- explicit destination column definitions before the query;
- `IGNORE`, `REPLACE`, `LIKE`, `VALUES`, `TABLE`, CTEs, unions, intersections,
  excepts, joins, grouping, `HAVING`, windows, locking clauses, query
  expression parentheses, or arbitrary SQLite pass-through beyond the current
  source `SELECT` subset;
- expression, literal, function, variable, parameter, subquery, or general
  projection inference;
- destination aliases, table options after `SELECT`, indexes, keys,
  constraints, generated columns, auto-increment attributes copied from the
  source, triggers, cascades, foreign keys, privileges, metadata locks,
  binary logging, replication semantics, or SQLite fork patches;
- MySQL's transaction-warning behavior for rollback of temporary CTAS.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public call
  validation, statement dispatch, result-handle ownership, and failure cleanup.
- Statement context owns diagnostics, warning count, affected rows,
  `ROW_COUNT()` state, and successful non-row result finalization.
- Lexer/parser/AST own syntax admission and source spans. Parser code does not
  inspect runtime state.
- Analyzer/planner code resolves the source query, target name, output column
  names, duplicate output names, descriptor families, and target descriptor
  shape before generated SQLite row-copy SQL exists.
- The durable catalog remains authoritative for persistent source schemas and
  descriptors. Temporary descriptors remain connection-local and never mutate
  durable catalog rows.
- The temporary catalog owns session-local target descriptors, generated
  negative ids, generated physical table names, and close-time cleanup.
- Runtime execution owns the sequence: infer descriptors, create the temporary
  physical table, copy rows, append the temporary descriptor, and clean up any
  physical table or descriptor object on failure.
- SQLite owns physical temp table creation, source scans, filtering, sorting,
  limiting, and final row insertion. MyLite must not buffer the selected row
  set in C memory.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload. Temporary
  CTAS must not touch byte range `[0, 4096)`.

## Supported SQL Grammar

Supported subset:

```sql
CREATE TEMPORARY TABLE [IF NOT EXISTS] table_name [AS]
SELECT select_item_list
FROM table_name [AS] alias
[WHERE predicate]
[ORDER BY order_key [ASC | DESC]]
[LIMIT row_count]
```

The source `SELECT` subset is exactly the descriptor-backed single-table subset
currently implemented for persistent CTAS. No-source scalar selects, `FROM
DUAL`, row-scalar projection, joins, grouping, unions, and expression
projection remain outside this slice.

### MyLite Lemon-Syntax Snippet

This snippet describes MyLite's intended grammar extension, not MySQL's full
grammar:

```lemon
statement(A) ::= create_temporary_table_select_statement(B). {
    A = B;
}

create_temporary_table_select_statement(A) ::=
    CREATE(C) TEMPORARY TABLE create_if_not_exists_opt(E) table_name(T)
    create_table_select_as_opt select_statement(S). {
    A = mylite_sql_parser_make_create_temporary_table_select_statement(
        state, C, E, T, S);
}
```

The existing persistent CTAS grammar remains unchanged. Mixed
`CREATE TEMPORARY TABLE table (column_definition, ...) SELECT` forms are
deliberately not admitted.

## Resolution Semantics

The `information_schema` write-target guard is applied before source
resolution, matching observed MySQL protected-system-schema precedence.
Ordinary target handling follows the current MyLite CTAS source-first model:

1. Resolve the source query through the existing `SELECT` planner.
2. Resolve the target effective schema from a selected schema for unqualified
   names or from the explicit qualifier for qualified names.
3. Reject reserved `_mylite_*` target names.
4. If `IF NOT EXISTS` is present and a same-schema temporary target already
   exists, append the existing-table note and finish without mutation.
5. Otherwise, fail if a same-schema temporary target already exists.
6. Infer destination columns and reject duplicate output names.
7. Create the temporary physical table, copy rows, and append the temporary
   descriptor.

Temporary targets do not check for persistent target name collisions because
MySQL allows a temporary table to shadow a persistent table. Persistent
descriptor lookups remain visible after `DROP TEMPORARY TABLE`.

Unqualified names require the selected schema at the point they are resolved.
Schema-qualified names use the explicit schema and do not require a selected
schema. Reserved `_mylite_*` schema or table names are rejected before any
SQLite SQL is generated.

Source descriptors may be persistent base tables or session temporary tables.
Unsupported object kinds must be rejected once descriptors for them exist.
Descriptor catalog identifier matching follows MyLite's current
case-insensitive catalog-name policy.

## Descriptor Inference

For each selected source descriptor column, in select-list order, the temporary
target receives one fresh session-local column descriptor:

- name: select-item alias text when present, otherwise the source descriptor
  column name;
- logical type: copied from the source descriptor;
- physical type: copied from the source descriptor;
- nullability: copied from the source descriptor;
- default metadata: copied from the source descriptor when the descriptor
  family supports current CTAS default preservation;
- visibility: always visible in the destination;
- ordinal: selected-column position in the target;
- descriptor identity: fresh temporary catalog row owned by the new temporary
  target.

`SELECT *` expands only visible source columns through the existing select
planner. Explicit source column references may name invisible source columns;
the destination column is visible.

Duplicate output names fail before mutation when the target will be created.
Unsupported selected expressions fail through the existing source `SELECT`
unsupported diagnostics.

## Generated SQLite Handling

The implementation uses public SQLite prepared statements only. No SQLite fork
patch is required.

Execution creates:

```sql
CREATE TEMPORARY TABLE "<generated_temp_table>" (...);
INSERT INTO "<generated_temp_table>"("<target_col>", ...)
SELECT "<source_col>", ...
FROM "<source_physical>" ...
```

The `WHERE`, `ORDER BY`, and `LIMIT` clauses are the descriptor-built source
clauses from the planned `SELECT`. Every SQLite identifier is quoted.
Predicate values and limits are bound parameters. User SQL literals are never
interpolated into generated SQLite SQL.

The temporary descriptor is appended only after the physical table exists and
row copy succeeds. If physical creation or row copy fails, the physical table
is dropped and the temporary descriptor object is deinitialized without
becoming visible through the session catalog.

## Result Semantics

Successful statements return the existing non-row statement result shape:

- zero result columns;
- zero result rows;
- `affected_rows` equals the number of copied rows;
- `warning_count == 0`;
- `ROW_COUNT()` observes the copied row count.

`CREATE TEMPORARY TABLE IF NOT EXISTS existing_temp AS SELECT ...` with a valid
source returns the existing create-noop shape:

- zero result columns;
- zero result rows;
- `affected_rows == 0`;
- `warning_count == 1`;
- `ROW_COUNT() == 0`;
- no target row insertion.

The temporary target is visible through descriptor-driven `SELECT`, `INSERT`,
`UPDATE`, `DELETE`, `SHOW COLUMNS`, `DESCRIBE`, `SHOW INDEX`, and
`SHOW CREATE TABLE`. It is omitted from `SHOW TABLES`, `SHOW TABLE STATUS`, and
synthetic `INFORMATION_SCHEMA` table/statistic rows.

## Diagnostics

The implementation must provide deterministic diagnostics for:

- syntax errors and unsupported grammar;
- missing default schema for unqualified source or target names;
- unknown source or target schemas;
- unknown source table;
- protected `information_schema` targets;
- reserved `_mylite_*` source or target schema/table names;
- existing temporary target with and without `IF NOT EXISTS`;
- unsupported source object kind;
- unknown projection, predicate, and ordering columns;
- duplicate selected output names;
- unsupported selected expression, literal, function, parameter, or subquery;
- unsupported `IGNORE`, `REPLACE`, `VALUES`, `TABLE`, joins, CTEs, grouping,
  windows, locking clauses, or explicit destination column definitions;
- unsupported limit forms inherited from the existing `SELECT` subset;
- temporary CTAS inside an active MyLite user transaction;
- allocation failures;
- physical SQLite failures during create, copy, or cleanup;
- public API misuse only if existing public execution/result misuse behavior
  changes.

When MySQL-runtime-verified diagnostics already exist for the same source
`SELECT` behavior, this feature reuses them. MyLite-specific unsupported
diagnostics are acceptable for deliberately deferred temporary-runtime gaps.

## Storage and File-Format Safety

The feature writes temporary physical rows through SQLite temporary storage and
does not add durable catalog rows. Persistent source reads and persistent
target shadowing must preserve the `.mylite` preamble, VFS offset invariants,
file-backed reopen behavior, and independent handle isolation.

## Performance

The row-copy path must stay SQLite-side. MyLite may materialize descriptors,
output names, and generated SQL strings in memory, but it must not fetch the
source row set into C memory for insertion. The physical copy should use one
prepared `INSERT INTO ... SELECT ...` statement after descriptor planning.

## Compatibility Documentation

Update:

- `COMPATIBILITY.md`;
- `docs/compatibility/sql-table-ddl.md`.

Do not update query-expression, operator, or literal conversion docs unless the
implemented source `SELECT` subset changes. Do not overclaim full temporary
CTAS, explicit destination definitions, expression inference, indexes,
constraints, privileges, replication, implicit commits, or general query
expressions.

## Test Plan

Add a focused runtime C test, preferably
`libmylite.runtime.temporary_create_table_select`, covering:

- parser acceptance for `CREATE TEMPORARY TABLE ... AS SELECT` and bare
  `SELECT`;
- copied-row affected count, warning count, no result rows, and `ROW_COUNT()`;
- zero-row source table creation;
- schema-qualified and unqualified target/source resolution;
- source-first diagnostics for unknown schemas/tables and `IF NOT EXISTS`;
- protected `information_schema` targets;
- persistent target shadowing and `DROP TEMPORARY TABLE` revealing the
  persistent table again;
- temporary source shadowing;
- selected descriptor families and aliases through the current CTAS support;
- duplicate output-name rejection when the target will be created;
- `WHERE`, one-column `ORDER BY`, and `LIMIT` reuse;
- unsupported expression/literal projection, explicit destination definitions,
  `IGNORE`, `REPLACE`, `VALUES`, `TABLE`, joins, CTEs, grouping, windows, and
  locking clauses;
- close/reopen cleanup of temporary target descriptors while persistent source
  rows remain durable;
- `.mylite` preamble preservation;
- independent file-backed handles;
- catalog generation staying unchanged and SQLite schema generation changing
  only on successful temporary creation;
- active user transaction support where rollback preserves the temporary target
  descriptor and rolls copied rows back;
- zero-initialized cleanup for new planner objects;
- no public API changes.

Also run the MySQL expectation script, focused parser/runtime tests for
persistent CTAS and temporary table lifecycle, and full
`cmake --workflow --preset check`.
