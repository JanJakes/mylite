# Baseline SQL Require Primary Key DDL Enforcement

## Status

This feature extends the existing `@@sql_require_primary_key` scalar baseline
with a limited session-local DDL enforcement slice.

MySQL 8.4 exposes `sql_require_primary_key` as a dynamic global and session
boolean variable. When the effective session value is enabled, MySQL rejects
table creation and table-structure changes that would leave a table without a
primary key. MyLite implements only handle-local session/local/unqualified
assignment and fixed global readback in this slice. That is enough to exercise
the common application setup path without introducing embedded server-wide
mutable state.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing scalar variable baseline:
  `docs/specs/baseline-sql-require-primary-key-system-variable/specs.md`
- Primary key, temporary table, `CREATE TABLE LIKE`, and CTAS baselines
- MySQL 8.4 Reference Manual, server system variables:
  https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html
- Observed MySQL 8.4.9 runtime behavior in container `mylite-mysql-849`

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_sql_require_primary_key_ddl_expectations.sh`
records the probes for this feature.

Observed behavior:

- `SET [SESSION|LOCAL] sql_require_primary_key = ON|1|TRUE` enables the
  session value; `OFF|0|FALSE|DEFAULT` disables it in the default runtime.
- `SET @@sql_require_primary_key = ...`,
  `SET @@session.sql_require_primary_key = ...`, and
  `SET @@local.sql_require_primary_key = ...` behave like session assignment.
- Unsupported values such as `2`, `-1`, `NULL`, or arbitrary strings fail with
  MySQL error `1231`, SQLSTATE `42000`.
- With the session value enabled, `CREATE TABLE t (id INT)` fails with error
  `3750`, SQLSTATE `HY000`, and a message containing
  `without a primary key`.
- A nonprimary `UNIQUE` key is not sufficient.
- Inline and table-level primary keys satisfy enforcement.
- `CREATE TABLE IF NOT EXISTS existing ...` succeeds when the target already
  exists, even if that target has no primary key; MySQL emits only the existing
  table warning.
- `CREATE TABLE ... LIKE` fails when the source descriptor has no primary key
  and succeeds when the cloned descriptor includes one.
- `CREATE TABLE ... SELECT` fails unless the target definition declares a
  primary key. MyLite's current CTAS subset does not admit explicit target
  primary-key definitions, so every supported CTAS target is rejected while the
  session value is enabled.
- Temporary table creation is not exempt.
- `ALTER TABLE ... DROP PRIMARY KEY` and `ALTER TABLE ... DROP CONSTRAINT`
  for the quoted `PRIMARY` constraint fail when they would leave the table
  without a primary key, even if a secondary or unique key remains.
- Multi-action `ALTER TABLE ... DROP PRIMARY KEY, ADD PRIMARY KEY(...)`
  succeeds; replacing the primary key with only a secondary or unique key
  fails and rolls back.
- Supported single-action table-changing `ALTER TABLE` and `CREATE INDEX`
  forms against an existing table that currently has no primary key fail with
  `3750` while the session value is enabled, unless the statement adds a
  primary key. MySQL accepts `ALTER TABLE ... RENAME` for such a table; MyLite
  follows that observed rename exception.

## Scope

The implementation must add:

- handle-local session state for `sql_require_primary_key`;
- supported `SET` forms for no-scope, `SESSION`, `LOCAL`, direct `@@variable`,
  `@@session.variable`, and `@@local.variable`;
- fixed global reads returning `0` and fixed no-op global assignments only for
  the disabled value;
- scalar `SELECT`, `SHOW VARIABLES`, and mixed-variable readback using the
  effective session value for non-global scopes;
- error `3750 / HY000` for supported DDL paths that would create or leave a
  table without a primary key while the session value is enabled;
- fast C tests and a MySQL 8.4.9 expectation artifact.

The enforced DDL paths are:

- `CREATE TABLE (...)` for persistent and temporary tables;
- `CREATE TABLE ... LIKE` and `CREATE TEMPORARY TABLE ... LIKE`;
- supported `CREATE TABLE ... SELECT` and `CREATE TEMPORARY TABLE ... SELECT`;
- `CREATE INDEX` on existing supported tables;
- supported single-action `ALTER TABLE` forms that change a table and would
  leave it without a primary key, excluding the observed rename exception and
  key-maintenance placeholders;
- single-action `ALTER TABLE ... DROP PRIMARY KEY`;
- single-action `ALTER TABLE ... DROP CONSTRAINT` for the quoted `PRIMARY`
  constraint;
- current supported multi-action `ALTER TABLE` final states that include
  primary-key drop/add combinations.

Supported assignment examples:

```sql
SET sql_require_primary_key = ON
SET SESSION sql_require_primary_key = 1
SET LOCAL sql_require_primary_key = FALSE
SET @@sql_require_primary_key = DEFAULT
SET @@session.sql_require_primary_key = TRUE
SET @@local.sql_require_primary_key = 0
```

Supported enforced DDL examples:

```sql
CREATE TABLE ok (id INT PRIMARY KEY)
CREATE TEMPORARY TABLE ok_tmp (id INT, PRIMARY KEY(id))
CREATE TABLE clone LIKE source_with_primary_key
ALTER TABLE t DROP PRIMARY KEY, ADD PRIMARY KEY(other_id)
```

## Non-Goals

This feature must not implement:

- mutable shared global state, startup options, persisted variables,
  `SET_VAR` hints, Performance Schema variable tables, or privilege checks;
- generated invisible primary keys;
- explicit primary-key definitions in CTAS beyond the existing parser subset;
- a complete ALTER TABLE enforcement matrix for every future action;
- import tablespace behavior, replication applier policy, or server-wide
  primary-key governance;
- SQLite fork patches or SQLite metadata authority.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns statement
  orchestration, diagnostics, result ownership, and row-count state.
- Statement context continues to manage diagnostics snapshots. DDL enforcement
  errors are ordinary statement errors.
- Parser/AST use existing `SET`, system-variable, `CREATE TABLE`, and
  `ALTER TABLE` grammar. No MySQL grammar text is copied.
- Runtime session state owns the handle-local `sql_require_primary_key` value.
  Global scope remains a fixed MyLite server placeholder.
- Analyzer/planner paths own primary-key detection from MyLite descriptors and
  planned primary-key metadata.
- Catalog descriptors remain authoritative. Enforcement checks descriptor
  primary-key state before physical SQLite SQL is generated or before an ALTER
  mutation is committed.
- Storage/VFS/SQLite physical row storage are unchanged. The feature does not
  alter `.mylite` preamble bytes or require SQLite fork changes.

## Runtime Semantics

The default effective session value is `0`. When enabled:

- planned explicit `CREATE TABLE` targets must include an inline or
  table-level primary key;
- cloned `LIKE` targets must preserve a source primary key;
- supported CTAS targets are rejected because they infer ordinary visible
  columns and no indexes;
- supported temporary creation follows the same rule;
- dropping a primary key is rejected unless the supported multi-action final
  state still has a primary key;
- supported single-action table-changing ALTER/CREATE INDEX forms are rejected
  on existing no-primary-key targets unless they add the missing primary key;
  `ALTER TABLE ... RENAME` remains accepted for current MySQL compatibility.

The checks run after `IF NOT EXISTS` no-op detection, matching MySQL's
existing-target behavior, and before catalog/physical mutations for failing
paths.

Global reads continue to return `0`. `SET GLOBAL sql_require_primary_key = 0`,
`OFF`, `FALSE`, or `DEFAULT` is accepted as a fixed no-op. Enabling global
state remains unsupported.

Successful supported assignments and DDL emit no warnings. Successful
non-query statements follow the existing MyLite result API conventions and do
not return rows.

## Diagnostics

- Primary-key requirement violation: error `3750`, SQLSTATE `HY000`, message
  containing `Unable to create or change a table without a primary key`.
- Unsupported boolean value: existing MySQL-shaped system-variable diagnostics
  from the SET path, including `1231 / 42000` for invalid values.
- Unsupported global enablement: deterministic MyLite unsupported diagnostic,
  because mutable global variable state is outside this embedded slice.
- Missing schemas, unknown tables, reserved names, duplicate table names,
  duplicate primary keys, and unsupported object kinds continue to use existing
  descriptor-driven diagnostics.
- Allocation, physical SQLite, and public API misuse diagnostics are
  unchanged.

## Compatibility Documentation

Update:

- `COMPATIBILITY.md`;
- `docs/compatibility/runtime-system-variables.md`;
- `docs/compatibility/sql-table-ddl.md`;
- `docs/compatibility/sql-replication.md`.

Docs must state that this is a limited session-local DDL enforcement slice and
must not claim generated invisible primary keys, mutable shared global state,
replication policy, import behavior, privileges, or complete future ALTER
coverage.

## Verification

Before committing, run:

1. `cmake --build --preset dev`
2. `packages/libmylite/tests/mysql_baseline_sql_require_primary_key_ddl_expectations.sh`
3. `ctest --preset dev -R 'libmylite\\.runtime\\.sql_require_primary_key' --output-on-failure`
4. Related CREATE TABLE, temporary table, CTAS, primary-key, index, and ALTER
   CTest entries.
5. `cmake --workflow --preset check`
