# Baseline Temporary Table Lifecycle

## Status

This feature specifies the first user-visible temporary-table slice for
file-backed and in-memory MyLite handles. It adds session-scoped
`CREATE TEMPORARY TABLE` and `DROP TEMPORARY TABLE` for the existing explicit
table-definition subset, plus descriptor-driven read/write and metadata
resolution for the temporary tables through the already supported single-table
DML and introspection paths.

The feature is intentionally not full MySQL temporary-table support. It does
not implement temporary `LIKE`, temporary `CREATE TABLE ... SELECT`, temporary
ALTER/RENAME/TRUNCATE, temporary DDL inside an active user transaction, or
privilege behavior.

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
- Existing baseline create/drop/rename/DML/introspection feature specs
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `CREATE TEMPORARY TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/create-temporary-table.html
- MySQL 8.4 Reference Manual, `DROP TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/drop-table.html
- MySQL 8.4 Reference Manual, implicit commits:
  https://dev.mysql.com/doc/refman/8.4/en/implicit-commit.html
- MySQL 8.4 Reference Manual, temporary table limitations:
  https://dev.mysql.com/doc/refman/8.4/en/temporary-table-problems.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## Scope

The implementation must add:

- parser and AST support for `CREATE TEMPORARY TABLE [IF NOT EXISTS]
  table_name (...) table_options` using the existing explicit `CREATE TABLE`
  column, key, default, charset/collation, and engine-option subset;
- parser and AST support for `DROP TEMPORARY TABLE [IF EXISTS] table_name
  [, table_name ...]`;
- session-owned temporary table descriptors stored outside the durable
  `_mylite_catalog_*` tables;
- physical SQLite `TEMPORARY` tables and temporary indexes with MyLite-owned
  generated physical names;
- MySQL-style name precedence where a temporary table in the selected or
  qualified schema hides a persistent base table with the same name for
  descriptor-driven `SELECT`, `INSERT`, `UPDATE`, and `DELETE`;
- `SHOW COLUMNS`, `DESCRIBE`, `SHOW INDEX`, and `SHOW CREATE TABLE` over
  temporary descriptors;
- `SHOW TABLES`, `SHOW TABLE STATUS`, and `INFORMATION_SCHEMA` table/statistic
  views continuing to list only durable descriptors;
- `DROP TABLE` selecting a temporary table first when one shadows a persistent
  table, while `DROP TEMPORARY TABLE` selects only temporary tables;
- close-time cleanup of all session temporary descriptors, with no durable
  catalog mutation and no `.mylite` file-format change;
- deterministic unsupported diagnostics for temporary DDL in an active MyLite
  user transaction.

## Non-Goals

This feature must not implement:

- `CREATE TEMPORARY TABLE ... LIKE`;
- `CREATE TEMPORARY TABLE ... SELECT`;
- temporary-table `ALTER TABLE`, `RENAME TABLE`, `TRUNCATE TABLE`,
  standalone `CREATE INDEX`, or standalone `DROP INDEX`;
- temporary DDL inside an active user transaction;
- temporary views, stored routines, triggers, foreign keys, cascades,
  generated columns, check constraints, privilege checks, metadata locks,
  binary logging, replication behavior, or optimizer statistics;
- memory-only C row storage, new third-party dependencies, or SQLite fork
  patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns call validation,
  result ownership, public misuse behavior, and failure cleanup.
- Statement context owns statement diagnostics, affected rows, warning count,
  and transaction status. Successful temporary-table DDL returns through the
  existing non-row result conventions.
- Lexer/parser/AST own syntax admission for the `TEMPORARY` keyword and keep
  syntax independent from catalog/runtime state.
- Analyzer/planner code resolves schemas, temporary-vs-durable object
  precedence, column descriptors, key descriptors, and unsupported shapes.
- The durable catalog module remains authoritative only for persistent schemas,
  tables, columns, and indexes. Temporary descriptors live in session state and
  must not update catalog rows, catalog generation, descriptor caches, or the
  persistent schema version.
- The temporary catalog owns session-local descriptor arrays, generated negative
  descriptor IDs, generated physical names, and close-time cleanup.
- SQLite owns physical row storage for generated temporary tables and indexes.
  MyLite owns identifier generation, quoting, descriptor metadata, and name
  resolution.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  Temporary tables are SQLite connection-local objects and must not touch the
  MyLite preamble.

## Supported SQL Grammar

The feature admits one top-level statement per `mylite_execute()` call.

Supported subset:

```sql
create_temporary_table_statement:
    CREATE TEMPORARY TABLE create_if_not_exists_opt table_name
    '(' create_table_item_list ')' table_option_list_opt

drop_temporary_table_statement:
    DROP TEMPORARY TABLE drop_if_exists_opt table_name_list
```

The `table_name`, `create_table_item_list`, `table_option_list_opt`, and
`table_name_list` nonterminals reuse the existing independently authored MyLite
grammar for persistent table lifecycle. `TEMPORARY` is a keyword token only in
the statement positions above; it remains a nonreserved identifier elsewhere
when the grammar context does not consume it.

Deferred MySQL forms such as `CREATE TEMPORARY TABLE ... LIKE`, temporary
CTAS, `DROP TEMPORARY TABLE ... RESTRICT`, and `DROP TEMPORARY TABLE ...
CASCADE` are rejected deterministically in this slice.

## Schema and Name Resolution

Unqualified temporary table creation requires a selected schema. Without one,
MyLite returns the existing MySQL-compatible no-database-selected diagnostic.

Schema-qualified temporary table creation requires that the named durable schema
exists at creation time. MyLite stores the schema name on the temporary
descriptor because MySQL keeps a temporary table alive even if its original
schema is later dropped.

For ordinary table references in the current single-table read/write subset:

1. Resolve the effective schema name from the selected schema for unqualified
   names or from the explicit qualifier for qualified names.
2. Look for a session temporary descriptor with the effective schema name and
   table name using the current descriptor name comparison policy.
3. If found, use the temporary descriptor and its generated SQLite `TEMP`
   physical table.
4. If not found, resolve the durable schema and persistent base table exactly
   as the existing persistent path does.

`DROP TEMPORARY TABLE` uses only step 2. It never drops a persistent table.
`DROP TABLE` uses step 2 first and falls back to the existing persistent drop
path when no matching temporary table exists.

Reserved `_mylite_*` schema and table names remain rejected before generated
SQLite SQL is built.

## Temporary Descriptors

Temporary table descriptors use the same public C structures as durable table,
column, index, and index-column descriptors so the existing descriptor-driven
DML and SHOW code can operate without SQLite metadata authority. Temporary IDs
are negative and allocated from the connection-local temporary catalog. Durable
catalog IDs remain positive and unchanged.

Temporary descriptors store:

- schema name and user-visible table name;
- generated physical SQLite temp table name;
- logical and physical column descriptors;
- primary, unique, and nonunique secondary index descriptors supported by the
  current explicit `CREATE TABLE` subset;
- default charset and collation metadata;
- descriptor versions fixed to zero for this baseline because temporary
  descriptors have no persistent catalog generation.

The initial slice rejects temporary `AUTO_INCREMENT` definitions because the
current auto-increment counter is persistent-catalog backed. Adding
session-local temporary auto-increment counters is a later feature.

## Physical SQLite Handling

MyLite generates SQLite SQL from descriptors only:

```sql
CREATE TEMPORARY TABLE "<physical_temp_table>" (...);
CREATE [UNIQUE] INDEX "<physical_temp_index>" ON "<physical_temp_table>" (...);
DROP TABLE "<physical_temp_table>";
```

Every SQLite identifier is generated by MyLite and quoted. User-provided names
are never interpolated into SQLite SQL. Row DML reuses existing prepared
statement paths and binds values rather than interpolating literals.

This is MyLite wrapper/translation over public SQLite APIs, not a SQLite fork
change. SQLite's `TEMP` schema is used only for physical row/index storage; it
is not metadata authority.

## Transaction Semantics

MySQL 8.4.9 verifies that `CREATE TEMPORARY TABLE` and `DROP TEMPORARY TABLE`
do not implicitly commit an active transaction, but their table-definition
effects are not rollbackable. SQLite `CREATE TEMPORARY TABLE` and `DROP TABLE`
inside a transaction are rollbackable.

To avoid silently exposing wrong semantics, this slice rejects
`CREATE TEMPORARY TABLE` and temporary-table drops while
`session.user_transaction_active` is true. DML against an already-created
temporary table follows SQLite transactional row semantics and is supported by
the current read/write paths.

Future exact support can add a MyLite-side transaction-exempt temporary DDL
mechanism or a targeted SQLite extension point if public APIs are insufficient.

## Result and Metadata Behavior

Successful `CREATE TEMPORARY TABLE` returns a non-row result with
`affected_rows == 0` and `warning_count == 0`.

`CREATE TEMPORARY TABLE IF NOT EXISTS`:

- creates a temporary table when only a persistent table of the same name
  exists, hiding the persistent table and producing no warning;
- no-ops with a MySQL-compatible note when a temporary table of the same name
  already exists in the effective schema.

Successful `DROP TEMPORARY TABLE` and `DROP TABLE` of a temporary table return
`affected_rows == 0`. `DROP TEMPORARY TABLE IF EXISTS` reports one note for
each missing temporary table, including the case where only a persistent table
exists.

`SHOW CREATE TABLE` over a temporary table renders `CREATE TEMPORARY TABLE`
and the descriptor-owned column/index/table-option metadata. `SHOW TABLES`,
`SHOW TABLE STATUS`, `INFORMATION_SCHEMA.TABLES`, and
`INFORMATION_SCHEMA.STATISTICS` do not expose temporary descriptors.

## Diagnostics

The implementation must provide deterministic diagnostics for:

- syntax errors and deferred grammar forms;
- missing default schema;
- unknown schema at temporary creation time;
- unknown table;
- duplicate temporary table;
- reserved `_mylite_*` schema or table names;
- unsupported temporary auto-increment definitions;
- unsupported temporary DDL in an active user transaction;
- unsupported object kinds once non-base-table durable descriptors exist;
- unsupported temporary `LIKE`, temporary CTAS, ALTER, RENAME, TRUNCATE,
  standalone index DDL, triggers, cascades, foreign keys, privileges, and
  metadata locks;
- physical SQLite failures;
- allocation failures;
- public API misuse through existing public conventions.

Where MySQL-compatible diagnostics are already available in MyLite, use them.
Where MyLite intentionally defers a wider MySQL behavior, use explicit
MyLite-specific unsupported diagnostics.

## MySQL 8.4.9 Runtime Evidence

`packages/libmylite/tests/mysql_baseline_temporary_table_lifecycle_expectations.sh`
records the runtime behavior used by this spec. Observed MySQL 8.4.9 behavior
includes:

- unqualified `CREATE TEMPORARY TABLE` without a selected schema fails with
  error 1046 / SQLSTATE `3D000`;
- schema-qualified temporary creation works without a selected schema when the
  schema exists and fails with error 1049 / SQLSTATE `42000` when it does not;
- a temporary table hides a persistent table with the same name until the
  temporary table is dropped;
- `DROP TABLE` drops a visible temporary table before the hidden persistent
  table, while `DROP TEMPORARY TABLE` never drops a persistent table;
- temporary tables are omitted from `SHOW TABLES`, `SHOW TABLE STATUS`, and
  `INFORMATION_SCHEMA.TABLES` / `STATISTICS`, but `SHOW COLUMNS`,
  `SHOW INDEX`, and `SHOW CREATE TABLE` can inspect a visible temporary table;
- a temporary table survives dropping its original schema for qualified
  references;
- temporary-table row DML participates in user transaction rollback;
- temporary create/drop definition effects are not rollbackable in MySQL even
  though they do not implicitly commit.

## Performance Notes

Temporary tables are not materialized in C. MyLite stores only descriptors in
session memory and delegates row storage, predicates, ordering, limits, updates,
and deletes to SQLite-generated SQL over a SQLite `TEMP` b-tree. This keeps the
baseline close to SQLite's optimal execution path for admitted single-table
queries while preserving MyLite-owned MySQL metadata and name semantics.

## Compatibility Documentation

Update `COMPATIBILITY.md` and `docs/compatibility/sql-table-ddl.md` to mark
`CREATE TEMPORARY TABLE` as limited, and to remove the blanket “no temporary
tables” wording only for the exact supported subset. Update `DROP TABLE`,
`SHOW COLUMNS`, `SHOW INDEX`, `SHOW CREATE TABLE`, and metadata docs only where
their temporary-table behavior changes. Do not overclaim temporary `LIKE`,
temporary CTAS, temporary ALTER/RENAME/TRUNCATE, privileges, metadata locks,
full information schema visibility, or exact temporary DDL inside user
transactions.
