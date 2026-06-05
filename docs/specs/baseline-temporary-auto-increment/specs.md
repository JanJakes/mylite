# Baseline Temporary AUTO_INCREMENT

## Status

This feature specifies session-local `AUTO_INCREMENT` support for the current
temporary-table subset. It builds on descriptor-owned persistent
`AUTO_INCREMENT`, session temporary table descriptors, temporary `CREATE TABLE
... LIKE`, descriptor-driven `INSERT`, `UPDATE`, `SELECT`, and `SHOW`
introspection.

The feature is intentionally not full MySQL temporary-table support. It only
removes the current temporary-table `AUTO_INCREMENT` gap for the same
descriptor column, key, counter, insert, update, and metadata subset already
implemented for persistent base tables.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Baseline temporary table lifecycle:
  `docs/specs/baseline-temporary-table-lifecycle/specs.md`
- Baseline temporary table `LIKE`:
  `docs/specs/baseline-temporary-table-like/specs.md`
- Baseline auto-increment lifecycle:
  `docs/specs/baseline-auto-increment-lifecycle/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `CREATE TEMPORARY TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/create-temporary-table.html
- MySQL 8.4 Reference Manual, `CREATE TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/create-table.html
- MySQL 8.4 Reference Manual, `CREATE TABLE ... LIKE`:
  https://dev.mysql.com/doc/refman/8.4/en/create-table-like.html
- MySQL 8.4 Reference Manual, information functions:
  https://dev.mysql.com/doc/refman/8.4/en/information-functions.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_temporary_auto_increment_expectations.sh`
records runtime probes for this feature. Observed behavior:

- `CREATE TEMPORARY TABLE t (id INT AUTO_INCREMENT PRIMARY KEY, v INT)`
  succeeds and reports ordinary non-row statement success.
- `SHOW COLUMNS FROM t` renders `Extra = auto_increment`, `SHOW INDEX FROM t`
  renders the primary key, and `SHOW CREATE TABLE t` renders `CREATE
  TEMPORARY TABLE` plus the column `AUTO_INCREMENT` attribute.
- `SHOW TABLE STATUS LIKE 't'`, `INFORMATION_SCHEMA.TABLES`, and
  `INFORMATION_SCHEMA.COLUMNS` omit the temporary table.
- `CREATE TEMPORARY TABLE t (...) AUTO_INCREMENT=7` sets the first generated
  value to `7`; after inserting that row, `SHOW CREATE TABLE` renders
  `AUTO_INCREMENT=8`.
- `AUTO_INCREMENT=0` on a temporary table normalizes to the default next value
  `1`.
- Table-level primary keys and the current single-column unique or nonunique
  secondary-key subset can back a temporary auto-increment column.
- Omitted auto-increment columns, explicit `NULL`, explicit `0`, and explicit
  `DEFAULT` generate values under the default SQL mode.
- The admitted one-row row-scalar `INSERT ... SELECT` / `FROM DUAL` paths
  allocate generated temporary values for omitted, `NULL`, and default-mode
  zero auto-increment inputs.
- `SET sql_mode='NO_AUTO_VALUE_ON_ZERO'` makes explicit `0` store zero rather
  than generate a value for temporary tables, matching the persistent table
  rule.
- Explicit positive values are stored as provided and advance the session-local
  temporary counter when they are greater than or equal to the current next
  value. Smaller positive values and negative values do not lower the counter.
- `LAST_INSERT_ID()` is updated from successful generated temporary-table
  inserts and is not changed by explicit non-generated values.
- `UPDATE` assigning a larger positive value to the auto-increment column
  advances the temporary counter; assigning a negative value does not.
- `CREATE TEMPORARY TABLE tmp LIKE persistent_ai`, `CREATE TEMPORARY TABLE tmp
  LIKE temporary_ai`, and `CREATE TABLE persistent_clone LIKE temporary_ai`
  clone the auto-increment column and supported key metadata, but the target
  counter starts at `1` instead of copying the source counter.
- Row changes to temporary tables are transactional. A generated insert rolled
  back by `ROLLBACK` removes the row, but the session counter and
  `LAST_INSERT_ID()` remain advanced for the connection.
- Temporary tables with small integer auto-increment columns show the same
  verified maximum/exhaustion behavior as the persistent baseline for the
  admitted current physical range.

## Scope

The implementation must add:

- `AUTO_INCREMENT` support to `CREATE TEMPORARY TABLE` for the same limited
  column definitions and key backing already admitted by persistent `CREATE
  TABLE`;
- temporary table-option `AUTO_INCREMENT=N` using the existing nonnegative
  integer literal subset;
- session-local temporary table descriptor counters stored in the temporary
  catalog's `mylite_catalog_table_descriptor.auto_increment_next`;
- preservation of `mylite_catalog_column_descriptor.is_auto_increment` for
  temporary descriptors;
- generated-value allocation for temporary `INSERT ... VALUES`, `INSERT ...
  SET`, and current one-row row-scalar `INSERT ... SELECT` paths that already
  use the shared insert planner;
- counter advancement for successful explicit positive values and successful
  explicit `UPDATE` assignments to the auto-increment column, using a
  session-local temporary-catalog update when the target table id is negative;
- `LAST_INSERT_ID()` updates for successful generated temporary inserts through
  the existing session state;
- `NO_AUTO_VALUE_ON_ZERO` behavior for temporary auto-increment inserts through
  the existing session SQL-mode flag;
- temporary `CREATE TABLE ... LIKE` auto-increment cloning with the target
  counter reset to `1`;
- persistent `CREATE TABLE ... LIKE temporary_source` auto-increment cloning
  with the persistent target counter reset to `1`;
- descriptor-backed `SHOW COLUMNS`, `SHOW INDEX`, and `SHOW CREATE TABLE`
  rendering for temporary auto-increment tables;
- omission of temporary tables from durable metadata listings, unchanged from
  the temporary-table lifecycle baseline;
- statement-level cleanup on failed temporary table creation, failed inserts,
  failed updates, and allocation failures;
- compatibility documentation for the exact supported subset.

## Non-Goals

This feature must not implement:

- temporary `FULLTEXT`, `FOREIGN KEY`, or `CHECK` runtime support;
- temporary `ALTER TABLE`, `RENAME TABLE`, `TRUNCATE TABLE`, standalone
  `CREATE INDEX`, or standalone `DROP INDEX`;
- `AUTO_INCREMENT` support beyond the current persistent subset, including
  generated invisible primary keys, unsupported types, multiple columns,
  unsupported indexes, `LAST_INSERT_ID(expr)`, full mixed-mode multi-row gap
  emulation, `auto_increment_increment`, `auto_increment_offset`, lock modes,
  replication, privileges, protocol insert-id metadata, or full C API state;
- `SHOW TABLE STATUS` or `INFORMATION_SCHEMA` rows for temporary tables;
- SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns statement entry,
  result ownership, public misuse behavior, and failure cleanup.
- Statement context owns diagnostics, affected rows, warning count, and
  statement transaction boundaries.
- Session state owns `LAST_INSERT_ID()` and `@@sql_mode`.
- Lexer/parser/AST already admit `AUTO_INCREMENT` attributes and table options
  through the persistent `CREATE TABLE` grammar. This feature does not add new
  grammar; it changes the temporary-table semantic admission.
- Analyzer/planner code validates temporary auto-increment definitions with the
  same descriptor rules as persistent `CREATE TABLE`.
- The durable catalog remains authoritative for persistent descriptors and
  counters only.
- The temporary catalog owns session-local table descriptors, negative ids,
  physical names, column/index descriptors, and temporary
  `auto_increment_next` updates.
- SQLite owns physical temporary row and index storage. MyLite binds generated
  integer values as ordinary prepared-statement parameters and does not use
  SQLite rowid or SQLite `AUTOINCREMENT` as MySQL metadata authority.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  Temporary auto-increment state is connection-local memory plus SQLite TEMP
  row storage and must not mutate durable catalog rows or the preamble.

## Supported SQL Grammar

No new tokens or grammar productions are introduced. The existing MyLite
grammar for the supported persistent `AUTO_INCREMENT` subset becomes admitted
for temporary table creation:

```sql
CREATE TEMPORARY TABLE [IF NOT EXISTS] table_name (
    column_definition [, create_table_item ...]
) [AUTO_INCREMENT = unsigned_decimal_integer_literal] [table_option ...]

column_definition:
    column_name integer_type [NULL | NOT NULL] AUTO_INCREMENT [PRIMARY KEY]
  | column_name integer_type [NULL | NOT NULL] [PRIMARY KEY] AUTO_INCREMENT
```

The effective column/key subset is exactly the current persistent
auto-increment subset: one integer-family auto-increment column, backed by the
current single-column primary, unique, or nonunique secondary-key subset.

MyLite Lemon-syntax snippet, unchanged from the existing create-table grammar:

```lemon
column_attribute(A) ::= AUTO_INCREMENT(T). {
    A = mylite_sql_parser_make_column_auto_increment(state, T);
}

table_option(A) ::= AUTO_INCREMENT EQ unsigned_integer_literal(V). {
    A = mylite_sql_parser_make_table_auto_increment_option(state, V);
}
```

Deferred forms remain syntax errors or deterministic unsupported diagnostics
through existing grammar and planner rules: expression table options, signed
negative options, non-integer auto-increment columns, missing key backing,
multiple auto-increment columns, auto-increment defaults, temporary fulltext,
temporary foreign keys, temporary checks, and temporary alter/rename/truncate.

## Schema and Name Resolution

Temporary auto-increment creation uses the existing temporary-table schema
policy:

- unqualified creation requires a selected default schema;
- schema-qualified creation requires the named durable schema to exist;
- reserved `_mylite_*` schema and table names are rejected before SQLite SQL is
  generated;
- a session temporary table shadows a persistent table with the same effective
  schema/table name for descriptor-driven DML and supported `SHOW` statements;
- durable metadata listings continue to ignore temporary descriptors.

`CREATE TEMPORARY TABLE ... LIKE` keeps the source-before-target resolution
specified by `baseline-temporary-table-like`. Source descriptors may be
persistent or temporary visible tables. The target descriptor is always empty
and starts with `auto_increment_next = 1`.

## Counter Semantics

Temporary counters use the same value conversion and range envelope as the
persistent auto-increment baseline:

- the default next value is `1`;
- `AUTO_INCREMENT=0` normalizes to `1`;
- a positive `AUTO_INCREMENT=N` table option sets the temporary next value to
  `N` if the literal fits the current signed-64 physical range;
- generated inserts consume the current next value, fail with the existing
  auto-increment diagnostic if no supported value can be generated, and advance
  the next value according to the column range;
- explicit positive inserted values advance the next value when needed;
- explicit zero generates unless `NO_AUTO_VALUE_ON_ZERO` is enabled;
- explicit negative values are stored through the ordinary integer path and do
  not advance the counter;
- `UPDATE` of the auto-increment column to a positive value advances the
  counter when the update changes at least one row and the assigned value is
  greater than or equal to the current next value;
- successful generated inserts update `LAST_INSERT_ID()` after statement
  success. Explicit non-generated values do not replace it.

Temporary counter state is session-local and is not durable. Closing the handle
destroys the descriptor and counter. Independent handles have independent
temporary counters even when connected to the same `.mylite` file.

When a generated temporary-table insert succeeds inside an active user
transaction and the transaction is later rolled back, SQLite removes the row,
but the MyLite temporary counter and `LAST_INSERT_ID()` remain advanced. This
matches observed MySQL 8.4.9 temporary-table behavior for the admitted subset.
Failed statements must not advance temporary counters.

## Physical SQLite Handling

This feature is MyLite wrapper/translation over public SQLite APIs. It does not
require a SQLite fork patch.

Temporary table creation keeps the existing generated physical names:

```sql
CREATE TEMPORARY TABLE "_mylite_temp_table_<n>" (...);
CREATE [UNIQUE] INDEX "_mylite_temp_index_<n>" ON "_mylite_temp_table_<n>" (...);
```

The physical columns are ordinary SQLite columns. Generated auto-increment
values are bound into `INSERT` or `UPDATE` prepared statements exactly like
explicit values. SQLite rowid state, `sqlite_sequence`, `PRAGMA`, and
`sqlite_schema` are not MySQL metadata authority.

## Result and Metadata Behavior

Successful temporary auto-increment DDL returns a non-row result with
`affected_rows == 0` and `warning_count == 0`.

Supported inserts and updates return through existing result conventions:
affected rows follow the current MyLite/MySQL-compatible DML rules and
`warning_count == 0` for supported in-range statements.

`SHOW COLUMNS` and `DESCRIBE` render `auto_increment` in `Extra`.
`SHOW INDEX` renders the backing key descriptor.
`SHOW CREATE TABLE` renders `CREATE TEMPORARY TABLE`, the column
`AUTO_INCREMENT` attribute, and table-level `AUTO_INCREMENT=N` when the
session-local counter is greater than the default rendered state. `SHOW TABLE
STATUS`, `SHOW TABLES`, and `INFORMATION_SCHEMA` remain durable-only listings
and do not expose temporary auto-increment targets.

## Diagnostics

The implementation must provide deterministic diagnostics for:

- syntax errors and deferred grammar forms;
- missing default schema;
- unknown qualified schema;
- duplicate temporary table names;
- reserved `_mylite_*` schema or table names;
- unsupported temporary fulltext, foreign-key, and check-constraint tables;
- unsupported auto-increment types, missing key backing, multiple
  auto-increment columns, explicit defaults, and nullable primary-key parts via
  the existing persistent auto-increment diagnostics;
- out-of-range table-option values;
- generated counter exhaustion and duplicate-key exhaustion paths;
- duplicate-key writes through the current key enforcement subset;
- allocation failures;
- physical SQLite failures after descriptor-built SQL execution;
- public API misuse through the existing public API rules.

## Tests

Add MySQL-runtime-verified expectations and fast C tests for:

- direct temporary create with inline and table-level primary-key
  auto-increment definitions;
- unique-key and nonunique secondary-key backed temporary auto-increment
  definitions;
- `AUTO_INCREMENT=N` table options and `AUTO_INCREMENT=0`;
- `SHOW COLUMNS`, `SHOW INDEX`, and `SHOW CREATE TABLE`;
- omission from `SHOW TABLE STATUS`, `SHOW TABLES`, and `INFORMATION_SCHEMA`;
- generated values for omitted, `NULL`, `0`, and `DEFAULT`;
- generated values through the current one-row row-scalar `INSERT ... SELECT`
  / `FROM DUAL` paths;
- `NO_AUTO_VALUE_ON_ZERO`;
- explicit positive, smaller positive, and negative values;
- `INSERT ... SET`;
- explicit `UPDATE` of the auto-increment column;
- transaction rollback advancing only the temporary counter and
  `LAST_INSERT_ID()` while removing the row;
- `CREATE TEMPORARY TABLE ... LIKE` from persistent and temporary
  auto-increment sources;
- `CREATE TABLE ... LIKE temporary_source`;
- temporary shadowing of a persistent auto-increment table with independent
  counters;
- close/reopen cleanup and independent handles;
- supported integer-family boundary and exhaustion behavior;
- deterministic diagnostics for unsupported temporary auto-increment and
  temporary-table forms that remain outside this slice;
- existing temporary table, temporary `LIKE`, persistent auto-increment,
  parser, DML, metadata, storage, and full workflow checks.
