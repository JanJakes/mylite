# Baseline Temporary Table LIKE

## Status

This feature specifies a narrow `CREATE TEMPORARY TABLE ... LIKE` slice and a
small source-resolution correction for `CREATE TABLE ... LIKE`:

```sql
CREATE [TEMPORARY] TABLE [IF NOT EXISTS] target_table LIKE source_table
CREATE [TEMPORARY] TABLE [IF NOT EXISTS] target_table (LIKE source_table)
```

The statement creates an empty descriptor clone. When `TEMPORARY` is present,
the destination descriptor and physical table are session-local and omitted
from durable catalog listings. Source tables may be persistent base tables or
session temporary tables visible through MyLite's existing temporary-shadowing
rules. The original slice deliberately kept temporary `AUTO_INCREMENT`,
`FULLTEXT`, and check-constraint cloning deferred because the temporary-table
lifecycle did not own those runtime semantics. The later
`baseline-temporary-auto-increment` slice lifts the auto-increment deferral
with session-local counters; temporary `FULLTEXT` and check-constraint cloning
remain deferred. Foreign-key constraints are not cloned by MySQL `LIKE`; their
supporting indexes may be cloned when they fit the existing index subset.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Baseline CREATE TABLE LIKE:
  `docs/specs/baseline-create-table-like/specs.md`
- Baseline temporary table lifecycle:
  `docs/specs/baseline-temporary-table-lifecycle/specs.md`
- Baseline auto-increment lifecycle:
  `docs/specs/baseline-auto-increment-lifecycle/specs.md`
- MySQL lexer and parser scaffold specs:
  `docs/specs/mysql-lexer/specs.md`,
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `CREATE TABLE ... LIKE`:
  https://dev.mysql.com/doc/refman/8.4/en/create-table-like.html
- MySQL 8.4 Reference Manual, `CREATE TEMPORARY TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/create-temporary-table.html
- MySQL 8.4 Reference Manual, implicit commits:
  https://dev.mysql.com/doc/refman/8.4/en/implicit-commit.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_temporary_table_like_expectations.sh`
records runtime probes for this feature. Observed behavior:

- `CREATE TEMPORARY TABLE dst LIKE src` succeeds for a persistent InnoDB base
  table, reports `ROW_COUNT() == 0`, `@@warning_count == 0`, and
  `@@error_count == 0`, and creates no destination rows.
- `CREATE TEMPORARY TABLE dst (LIKE src)` is accepted.
- The destination is a temporary table: `SHOW CREATE TABLE` renders
  `CREATE TEMPORARY TABLE`, `SHOW TABLE STATUS LIKE 'dst'` returns no rows,
  and `INFORMATION_SCHEMA.TABLES` has no row for the destination.
- A temporary source table is a valid source. If a temporary source shadows a
  persistent table with the same effective name, the temporary descriptor is
  cloned.
- `CREATE TABLE persistent_dst LIKE temporary_src` creates a persistent table;
  the source's temporary status is not copied.
- `CREATE TEMPORARY TABLE existing_persistent LIKE src` creates a temporary
  table that shadows the existing persistent table and produces no warning.
- `CREATE TEMPORARY TABLE IF NOT EXISTS existing_temp LIKE src` no-ops with
  one warning after the source has resolved successfully.
- Source resolution happens before target-schema checks and before
  `IF NOT EXISTS` target no-op handling. Missing source schemas and source
  tables therefore win over target errors and existing-target no-ops.
- `information_schema` write targets are denied before source resolution,
  matching MySQL's protected-system-schema precedence.
- Unqualified target and source names independently require a selected default
  schema. Schema-qualified names do not require a selected default schema.
- `CREATE TEMPORARY TABLE ... LIKE view_name` fails with MySQL error `1347`
  because the source is not a base table.
- MySQL clones `AUTO_INCREMENT` into temporary `LIKE` tables and resets the
  temporary counter to `1`. The later `baseline-temporary-auto-increment`
  feature adopts that behavior for the current indexed auto-increment subset.

## Scope

The implementation must add:

- parser and AST support for `CREATE TEMPORARY TABLE [IF NOT EXISTS]
  target_table LIKE source_table`;
- parser and AST support for `CREATE TEMPORARY TABLE [IF NOT EXISTS]
  target_table (LIKE source_table)`;
- source resolution for both persistent and temporary `CREATE TABLE ... LIKE`
  paths through the existing visible-table precedence rules;
- descriptor cloning from persistent or temporary base-table sources using the
  existing `planned_create_table` shape;
- temporary-target creation through the existing temporary catalog and
  generated SQLite `TEMPORARY` table path;
- `IF NOT EXISTS` behavior where a valid existing temporary target no-ops with
  one warning, while an existing persistent table does not block temporary
  creation;
- durable-target `CREATE TABLE ... LIKE temporary_source` support for current
  source descriptor families;
- compatibility documentation for the exact limited surface.

## Non-Goals

This feature must not implement:

- temporary `FULLTEXT` cloning;
- temporary check-constraint cloning;
- temporary `CREATE TABLE ... SELECT`;
- temporary `ALTER TABLE`, `RENAME TABLE`, `TRUNCATE TABLE`, standalone
  `CREATE INDEX`, or standalone `DROP INDEX`;
- views, generated columns, triggers, stored routines, partitions, tablespaces,
  compressed temporary tables, privilege checks, metadata locks, binary
  logging, or replication behavior;
- SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns statement entry,
  result ownership, and public misuse behavior.
- Statement context owns diagnostics, affected rows, warning count, and
  `ROW_COUNT()` state. Supported statements return through existing non-row
  result conventions.
- Lexer/parser/AST own syntax admission for the `TEMPORARY` `LIKE` forms and
  do not inspect runtime state.
- Analyzer/planner code resolves target and source names before generated
  SQLite SQL exists. It applies source-before-target precedence verified
  against MySQL 8.4.9.
- The durable catalog remains authoritative for persistent schemas, tables,
  columns, indexes, and supported constraints. Temporary descriptors remain
  connection-local and do not mutate durable catalog rows.
- The temporary catalog owns session-local destination descriptors, generated
  negative ids, generated physical names, and close-time cleanup.
- Runtime execution generates SQLite DDL only from MyLite descriptors and
  stable generated physical names. SQLite schema text is never logical
  metadata authority.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload.
  Temporary `LIKE` creation uses SQLite connection-local temp storage and must
  not touch the preamble.

## Supported SQL Grammar

Supported subset:

```sql
CREATE TABLE [IF NOT EXISTS] table_name LIKE table_name
CREATE TABLE [IF NOT EXISTS] table_name (LIKE table_name)
CREATE TEMPORARY TABLE [IF NOT EXISTS] table_name LIKE table_name
CREATE TEMPORARY TABLE [IF NOT EXISTS] table_name (LIKE table_name)
```

Both target and source names may be unqualified or schema-qualified.

MyLite Lemon-syntax snippet:

```lemon
statement(A) ::= create_table_like_statement(B). {
    A = B;
}

create_table_like_statement(A) ::=
    CREATE(C) TABLE create_if_not_exists_opt(E) table_name(T) LIKE table_name(S). {
    A = mylite_sql_parser_make_create_table_like_statement(state, C, E, T, S);
}

create_table_like_statement(A) ::=
    CREATE(C) TABLE create_if_not_exists_opt(E) table_name(T)
    LPAREN LIKE table_name(S) RPAREN. {
    A = mylite_sql_parser_make_create_table_like_statement(state, C, E, T, S);
}

create_temporary_table_like_statement(A) ::=
    CREATE(C) TEMPORARY TABLE create_if_not_exists_opt(E) table_name(T)
    LIKE table_name(S). {
    A = mylite_sql_parser_make_create_temporary_table_like_statement(
        state, C, E, T, S);
}

create_temporary_table_like_statement(A) ::=
    CREATE(C) TEMPORARY TABLE create_if_not_exists_opt(E) table_name(T)
    LPAREN LIKE table_name(S) RPAREN. {
    A = mylite_sql_parser_make_create_temporary_table_like_statement(
        state, C, E, T, S);
}
```

Mixed forms such as `CREATE TEMPORARY TABLE t (LIKE s, c INT)`, table options
after `LIKE`, `CREATE TEMPORARY TABLE ... SELECT`, and query modifiers remain
syntax errors or deterministic unsupported forms according to the existing
parser surface.

## Resolution Semantics

The existing `information_schema` write-target guard is applied before source
resolution, matching MySQL's protected-system-schema precedence. For ordinary
targets, source resolution is performed before target resolution and before
`IF NOT EXISTS` target handling:

1. Resolve the source effective schema from a selected schema for unqualified
   names or from the explicit qualifier for qualified names.
2. Reject reserved `_mylite_*` source names.
3. Resolve a visible source table: session temporary descriptor first, then
   persistent descriptor.
4. Require the source descriptor kind to be persistent base table or temporary
   table.
5. Copy source default charset/collation and clone supported descriptors into
   a `planned_create_table`.
6. Resolve the target effective schema.
7. Reject reserved `_mylite_*` target names.
8. Apply target creation or `IF NOT EXISTS` handling.

An unqualified source or target without a selected default schema reports the
current no-database-selected diagnostic. Unknown explicit source and target
schemas report the current unknown-database diagnostic. Unknown source tables
report the current table-does-not-exist diagnostic.

For temporary targets, an existing temporary table with the same effective
name blocks creation or no-ops under `IF NOT EXISTS`; an existing persistent
table with the same name does not block creation because the temporary table
shadows it. For persistent targets, existing persistent targets keep the
existing persistent `CREATE TABLE ... LIKE` behavior. This slice does not
introduce a persistent target collision check against temporary tables because
MySQL allows persistent and temporary tables with the same name to coexist.

Descriptor catalog identifier matching follows MyLite's current
case-insensitive catalog-name policy. This feature does not add collation-aware
identifier comparison.

## Descriptor Cloning Semantics

The clone reuses the existing persistent `CREATE TABLE ... LIKE` descriptor
clone policy for supported source descriptors:

- logical column name, logical type, physical type, nullability, default
  metadata, visibility, character set, collation, and ordinal order;
- current supported primary-key metadata;
- current supported unique and nonunique secondary-index metadata, including
  prefix and direction metadata;
- current supported check constraints only when the destination is persistent;
- supporting indexes for foreign-key source constraints when they fit the
  existing index subset, without cloning foreign-key constraints themselves;
- table default charset and collation;
- target auto-increment counter reset to `1` for supported persistent and
  temporary targets.

Temporary targets reject cloned descriptors that the current temporary table
runtime cannot execute safely:

- any `FULLTEXT` source index;
- any check-constraint source descriptor.

The source rows are never copied. The source descriptors and physical table
are unchanged.

## Runtime Semantics

Persistent targets continue through `create_table_from_plan()`. Temporary
targets continue through `create_temporary_table_from_plan()`.

Generated SQLite DDL uses the same descriptor-built shape as explicit
temporary table creation:

```sql
CREATE TEMPORARY TABLE "<generated_temp_table>" (...);
CREATE [UNIQUE] INDEX "<generated_temp_index>" ON "<generated_temp_table>" (...);
```

Every generated SQLite identifier is MyLite-generated and quoted. User names
are catalog metadata only and are never interpolated into physical SQL.

This is MyLite wrapper/translation over public SQLite APIs. No SQLite fork
patch is needed.

## Transaction Semantics

The existing temporary-table lifecycle rejects temporary DDL inside active
MyLite user transactions because SQLite temporary DDL rollback semantics do
not match MySQL's nonrollbackable temporary-table definition behavior. This
feature preserves that policy for `CREATE TEMPORARY TABLE ... LIKE`.

Persistent `CREATE TABLE ... LIKE` keeps the existing persistent DDL implicit
commit behavior.

## Result and Metadata Behavior

Successful temporary `LIKE` creation returns the existing non-row result with
`affected_rows == 0` and `warning_count == 0`.

`CREATE TEMPORARY TABLE IF NOT EXISTS existing_temp LIKE source` resolves the
source first, then no-ops with one warning if the target temporary table
already exists. If only a persistent target table exists, the statement creates
the temporary table and reports no warning.

The temporary target is visible through descriptor-driven `SELECT`, `INSERT`,
`UPDATE`, `DELETE`, `SHOW COLUMNS`, `DESCRIBE`, `SHOW INDEX`, and
`SHOW CREATE TABLE`. It is omitted from `SHOW TABLES`, `SHOW TABLE STATUS`,
and synthetic `INFORMATION_SCHEMA` table/statistic rows, preserving the
existing temporary-table lifecycle policy.

## Diagnostics

The implementation must provide deterministic diagnostics for:

- syntax errors and deferred grammar forms;
- missing default schema;
- unknown source or target schema;
- unknown source table;
- reserved `_mylite_*` schema or table names;
- existing target temporary table without `IF NOT EXISTS`;
- unsupported source object kind such as a view once view descriptors exist;
- unsupported source descriptor families;
- temporary target with `FULLTEXT` or check-constraint source descriptors;
- temporary DDL inside an active user transaction;
- physical SQLite failures;
- allocation failures.

Existing diagnostics remain authoritative where this feature reuses existing
planner helpers. MyLite-specific unsupported diagnostics are acceptable for
deferred temporary-runtime gaps when MySQL supports a wider behavior but the
current MyLite architecture lacks the required prerequisite.

## Tests

Add a fast C runtime test under `packages/libmylite/tests/` and register it
with CTest. The test must cover:

- parser acceptance for both temporary `LIKE` forms and existing persistent
  forms;
- successful temporary clone from a persistent table;
- successful temporary clone from a temporary source;
- successful persistent clone from a temporary source;
- temporary source shadowing of a persistent source;
- `IF NOT EXISTS` behavior for existing temporary and persistent targets;
- schema-qualified and unqualified name resolution, including missing default
  schema, unknown source schema, unknown target schema, and source-before-target
  precedence;
- unknown source table;
- reserved target/source names;
- rejected temporary clone from `FULLTEXT` and check constraint sources;
- temporary clone from a foreign-key source, verifying supporting index cloning
  without cloning the foreign-key constraint;
- temporary DDL inside an active transaction;
- DML, `SHOW CREATE TABLE`, `SHOW INDEX`, durable metadata omission, close
  cleanup, independent handles, and `.mylite` preamble preservation.

Run:

1. `packages/libmylite/tests/mysql_baseline_temporary_table_like_expectations.sh`
2. `cmake --build --preset dev`
3. Focused CTest entries for parser, temporary table lifecycle, persistent
   create-table-like lifecycle, and the new temporary-table-like lifecycle.
4. `cmake --workflow --preset check`

## Compatibility Documentation

Update `COMPATIBILITY.md` and `docs/compatibility/sql-table-ddl.md` to claim
only the limited `CREATE TEMPORARY TABLE ... LIKE` and temporary-source
cloning surface. Do not claim temporary fulltext, temporary checks,
temporary CTAS, temporary ALTER, views, privileges, or full MySQL metadata
parity.
