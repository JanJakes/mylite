# Baseline CREATE TABLE SELECT Lifecycle

## Status

This feature specifies a narrow descriptor-driven `CREATE TABLE ... SELECT`
slice for persistent base tables. It builds on `mylite_execute()`, statement
context, parser scaffolding, file-backed `.mylite` opening, durable MyLite
catalog descriptors, baseline table lifecycle, descriptor-backed `SELECT`, and
the limited `INSERT ... SELECT` row-copy path.

This is not full MySQL `CREATE TABLE ... SELECT`. The slice admits one new
persistent base-table target and one existing descriptor-backed source
`SELECT`. It intentionally defers explicit destination column definitions,
indexes, constraints, `IGNORE`, `REPLACE`, `VALUES`, `TABLE`, joins, expression
projection, and general type inference.

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
- Baseline catalog foundation:
  `docs/specs/baseline-catalog-foundation/specs.md`
- Baseline CREATE TABLE LIKE:
  `docs/specs/baseline-create-table-like/specs.md`
- Baseline INSERT SELECT:
  `docs/specs/baseline-insert-select-lifecycle/specs.md`
- Baseline select order/limit lifecycle:
  `docs/specs/baseline-select-order-limit-lifecycle/specs.md`
- MySQL lexer and parser scaffold:
  `docs/specs/mysql-lexer/specs.md`,
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `CREATE TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/create-table.html
- MySQL 8.4 Reference Manual, `CREATE TABLE ... SELECT`:
  https://dev.mysql.com/doc/refman/8.4/en/create-table-select.html
- MySQL 8.4 Reference Manual, `SELECT`:
  https://dev.mysql.com/doc/refman/8.4/en/select.html
- MySQL 8.4 Reference Manual, replication notes for `CREATE TABLE ... SELECT`:
  https://dev.mysql.com/doc/refman/8.4/en/replication-features-create-select.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_create_table_select_lifecycle_expectations.sh`
records the runtime probes for this feature. Observed behavior:

- `CREATE TABLE dst AS SELECT ...` and `CREATE TABLE dst SELECT ...` both
  succeed.
- Successful statements return no result rows, set `ROW_COUNT()` to the number
  of inserted rows, and leave `@@warning_count == 0`.
- A source `SELECT` returning zero rows still creates the table and reports
  `ROW_COUNT() == 0`.
- Schema-qualified target and source names work without a selected default
  database.
- Unqualified targets or sources without a selected default database fail with
  error `1046`, SQLSTATE `3D000`.
- Source resolution happens before target resolution. If both explicit schemas
  are unknown, MySQL reports the source schema. If the source table is unknown
  and the target schema is also unknown, MySQL reports the source table.
- If the target exists and the source exists, `IF NOT EXISTS` is a no-op with
  `ROW_COUNT() == 0`, `@@warning_count == 1`, and no row insertion.
- If the target exists but the source is missing, the missing source error is
  reported before the `IF NOT EXISTS` no-op.
- Selected source column descriptors retain the tested integer type,
  nullability, and default metadata in the created table.
- `SELECT *` excludes invisible source columns. Explicitly selected invisible
  source columns are accepted, retain their default metadata, and become
  visible columns in the destination table.
- Duplicate selected output names fail with error `1060`, SQLSTATE `42S21`.
- Unknown projection, predicate, and ordering columns fail with error `1054`,
  SQLSTATE `42S22`, using MySQL's field/where/order contexts.
- MySQL supports explicit destination column definitions, expression and
  literal projections, `VALUES`, `TABLE`, `IGNORE`, `REPLACE`, indexes,
  constraints, and much wider query expressions. They are out of scope for this
  baseline.

## Scope

The implementation must add:

- parser and AST support for limited `CREATE TABLE [IF NOT EXISTS] table_name
  [AS] SELECT ...`;
- optional `AS`;
- one new persistent base-table target and one existing persistent base-table
  source;
- source `SELECT` limited to the existing descriptor-backed single-table
  subset, including optional table alias, `WHERE`, one-column `ORDER BY`, and
  `LIMIT`;
- source-first resolution and diagnostics, followed by target resolution and
  `IF NOT EXISTS` no-op handling;
- target table descriptor creation from selected source descriptor columns in
  select-list order;
- selected column names taken from the select-item alias when present, or from
  the source descriptor column name otherwise;
- duplicate output-name rejection before mutation;
- destination column descriptors preserving selected source logical type,
  physical type, nullability, and default metadata, while setting destination
  visibility to visible;
- generated SQLite physical `CREATE TABLE` and `INSERT INTO ... SELECT`
  statements built only from descriptors and stable physical table names;
- prepared-statement parameter binding for source predicates and limits;
- statement-atomic catalog, physical table, and row-copy behavior;
- successful result reporting with the existing non-row result shape,
  `affected_rows` equal to inserted rows, and `warning_count == 0`;
- `IF NOT EXISTS` existing-target no-op with warning count `1` when the source
  is valid;
- tests and MySQL 8.4.9 expectation artifacts for supported behavior and
  deliberately rejected wider MySQL forms.

## Non-Goals

This feature must not implement:

- explicit destination column definitions before the `SELECT`;
- `IGNORE`, `REPLACE`, `TEMPORARY`, `LIKE`, `VALUES`, `TABLE`, CTEs, unions,
  intersections, excepts, joins, grouping, `HAVING`, windows, locking clauses,
  query expression parentheses, or arbitrary SQLite pass-through;
- expression, literal, function, variable, parameter, subquery, string,
  decimal, float, hex, bit, temporal, JSON, or generated-column projection;
- target aliases, table options after `SELECT`, indexes, keys, constraints,
  generated columns, auto-increment, triggers, cascades, foreign keys,
  privileges, metadata locks, implicit commit behavior, binary logging,
  replication semantics, or SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public call
  validation, statement dispatch, result-handle ownership, and failure cleanup.
- Statement context owns diagnostics reset, warning count, affected rows,
  previous `ROW_COUNT()` state, and successful non-row result finalization.
- Lexer/parser/AST own syntax admission and source spans. Parser code remains
  independent of runtime, catalog, storage, and SQLite.
- Analyzer/planner code resolves the source table, source projection,
  predicate, ordering, limit, target table name, output column names, duplicate
  names, and target descriptors against MyLite catalog descriptors before any
  mutation.
- The catalog remains authoritative for schemas, table object kinds, stable
  physical names, descriptor columns, defaults, visibility, and descriptor
  ordering. SQLite schema text and PRAGMA output are not consulted for logical
  metadata.
- Runtime execution owns the atomic sequence: allocate catalog descriptors,
  create the physical table, copy source rows into the new physical table, and
  commit or roll back the catalog mutation.
- SQLite owns physical rowid table creation, source scans, filtering, sorting,
  limiting, and final row insertion. MyLite must not copy the selected row set
  into a C-side buffer.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  Row writes occur only inside the shifted SQLite payload and must not touch
  byte range `[0, 4096)`.

## Supported SQL Grammar

Supported subset:

```sql
CREATE TABLE [IF NOT EXISTS] table_name [AS]
SELECT select_item_list
FROM table_name [AS] alias
[WHERE predicate]
[ORDER BY order_key [ASC | DESC]]
[LIMIT row_count]
```

The source `SELECT` subset is exactly the descriptor-backed single-table
subset currently implemented for ordinary `SELECT`, except that no-source
literal projection and `FROM DUAL` projection are intentionally deferred.

### MyLite Lemon-Syntax Snippet

This snippet describes the intended MyLite grammar extension, not MySQL's full
grammar:

```lemon
statement(A) ::= create_table_select_statement(B). {
    A = B;
}

create_table_select_statement(A) ::=
    CREATE(C) TABLE create_if_not_exists_opt(E) table_name(T)
    create_table_select_as_opt select_statement(S). {
    A = mylite_sql_parser_make_create_table_select_statement(state, C, E, T, S);
}

create_table_select_as_opt ::= .
create_table_select_as_opt ::= AS.
```

The existing `create_table_statement` with parenthesized column definitions
remains separate. Mixed `CREATE TABLE table (column_definition, ...) SELECT`
forms are deliberately not admitted in this slice.

## Resolution Semantics

Resolution follows MySQL's observed source-first behavior for this statement:

1. Resolve the source table through the existing `SELECT` planner.
2. Resolve the target table name through the existing selected/default schema
   policy.
3. If `IF NOT EXISTS` is present and the target already exists, append the
   existing table note and finish without mutation.
4. Otherwise, fail if the target already exists.

Unqualified names require the selected schema at the point they are resolved.
Schema-qualified names use the explicit schema and do not require a selected
schema. Reserved `_mylite_*` schema or table names are rejected before any
SQLite SQL is generated.

The source must be a persistent base-table descriptor. The target must be a
new persistent base table unless the `IF NOT EXISTS` no-op path applies.
Unsupported object kinds must be rejected once descriptors for them exist.

Descriptor catalog identifier matching follows MyLite's current
case-insensitive catalog name policy. Output duplicate checks use the same
policy as existing descriptor column duplicate checks.

## Descriptor Inference

For each selected source descriptor column, in select-list order, the target
table receives one fresh column descriptor:

- name: select-item alias text when present, otherwise the source descriptor
  column name;
- logical type: copied from the source descriptor;
- physical type: copied from the source descriptor;
- nullability: copied from the source descriptor;
- default kind and integer default payload: copied from the source descriptor;
- visibility: always visible in the destination;
- ordinal: selected-column position in the target;
- descriptor identity: fresh catalog row owned by the new target table.

`SELECT *` expands only visible source columns through the existing select
planner. Explicit source column references may name invisible source columns;
the destination column is still visible.

Duplicate output names fail before mutation with the duplicate-column
diagnostic. Unsupported selected expressions fail through the existing source
`SELECT` unsupported diagnostics before mutation.

## Generated SQLite Handling

The implementation uses public SQLite prepared statements only. No SQLite fork
patch is required.

Planning creates:

- a descriptor-backed `CREATE TABLE <target_physical>(...)` statement using a
  newly allocated stable physical table name;
- a descriptor-backed `INSERT INTO <target_physical>(target columns)
  SELECT source columns FROM <source_physical> ...` statement using the same
  `WHERE`, `ORDER BY`, and `LIMIT` as the planned source `SELECT`.

Every SQLite identifier is quoted. Predicate values and limits are bound
parameters. User SQL literals are never interpolated into generated SQLite
SQL.

The statement executes inside one catalog mutation transaction. Catalog rows,
the physical table, copied rows, catalog generation, and
`sqlite_schema_generation` advance only after the complete statement succeeds.
If table creation or row copy fails, the mutation rolls back and no target
descriptor or physical table remains.

## Result Semantics

Successful statements return the existing non-row statement result shape:

- zero result columns;
- zero result rows;
- `affected_rows` equals the number of copied rows;
- `warning_count == 0`;
- `ROW_COUNT()` observes the copied row count.

`CREATE TABLE IF NOT EXISTS existing AS SELECT ...` with a valid source returns
the existing CREATE no-op shape:

- zero result columns;
- zero result rows;
- `affected_rows == 0`;
- `warning_count == 1`;
- `ROW_COUNT() == 0`;
- no target row insertion.

## Diagnostics

The implementation must produce deterministic diagnostics for:

- syntax errors and unsupported grammar;
- missing default schema for unqualified source or target names;
- unknown source or target schemas;
- unknown source table;
- existing target table with and without `IF NOT EXISTS`;
- reserved `_mylite_*` source or target schema/table names;
- unsupported source or target object kind;
- unknown projection, predicate, and ordering columns;
- duplicate selected output names;
- unsupported selected expression, literal, function, parameter, or subquery;
- unsupported `IGNORE`, `REPLACE`, `TEMPORARY`, `VALUES`, `TABLE`, joins, CTEs,
  grouping, windows, locking clauses, or explicit destination column
  definitions;
- unsupported limit forms inherited from the existing `SELECT` subset;
- allocation failures;
- physical SQLite failures during create/copy/drop rollback;
- public API misuse only if existing public execution/result misuse behavior
  changes.

When MySQL-runtime-verified diagnostics already exist for the same source
`SELECT` behavior, this feature reuses them. Any MyLite-specific unsupported
diagnostic must be stable and covered by tests.

## Storage and File-Format Safety

The feature writes only descriptor catalog rows and generated physical SQLite
rows inside the shifted SQLite payload. It must preserve the `.mylite`
preamble, VFS offset invariants, file-backed reopen behavior, and independent
handle isolation.

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
implemented source `SELECT` subset changes. Do not overclaim full CTAS,
explicit destination definitions, expression inference, indexes, constraints,
temporary tables, privileges, replication, implicit commits, or general query
expressions.

## Test Plan

Add a focused runtime C test, preferably
`libmylite.runtime.create_table_select_lifecycle`, covering:

- `CREATE TABLE dst AS SELECT ...` and `CREATE TABLE dst SELECT ...`;
- affected rows, warning count, no result rows, and `ROW_COUNT()`;
- zero-row source table creation;
- schema-qualified and unqualified target/source resolution;
- source-first diagnostics for unknown schemas/tables and `IF NOT EXISTS`;
- selected integer families, `NULL`, defaults, explicit invisible source
  columns becoming visible, and `SELECT *` excluding invisible source columns;
- aliases as destination column names and duplicate output-name rejection;
- `WHERE`, one-column `ORDER BY`, and `LIMIT` reuse;
- unsupported expression/literal projection, explicit destination definitions,
  `IGNORE`, `REPLACE`, `TEMPORARY`, `VALUES`, `TABLE`, joins, CTEs, grouping,
  windows, and locking clauses;
- persistence after close/reopen;
- behavior after source table rename/drop where applicable;
- `.mylite` preamble preservation;
- independent file-backed handles;
- catalog generation and SQLite schema generation changes only on successful
  creation;
- zero-initialized cleanup for new planner objects;
- no public API changes.

Also run the MySQL expectation script, focused parser/runtime tests, and full
`cmake --workflow --preset check`.
