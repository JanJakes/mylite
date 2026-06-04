# Built-in Schema Write Protection Baseline

## Summary

MySQL exposes `information_schema`, `mysql`, `performance_schema`, and `sys` as
special built-in schemas. MyLite exposes those names as synthetic schema
catalog entries and metadata-only table-directory rows. Because MyLite does not
store or execute real server data-dictionary, privilege, Performance Schema, or
sys objects, writes that target any built-in schema are rejected before catalog
lookup or physical SQLite planning.

Official references:

- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA` introduction:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-introduction.html
- MySQL 8.4 Reference Manual, `mysql` system schema:
  https://dev.mysql.com/doc/refman/8.4/en/system-schema.html
- MySQL 8.4 Reference Manual, Performance Schema:
  https://dev.mysql.com/doc/refman/8.4/en/performance-schema.html
- MySQL 8.4 Reference Manual, sys schema:
  https://dev.mysql.com/doc/refman/8.4/en/sys-schema.html

Runtime authority: local MySQL 8.4.9 probes in
`packages/libmylite/tests/mysql_baseline_builtin_schema_write_protection_expectations.sh`.

## MySQL 8.4.9 Observations

The MySQL 8.4.9 comparison runtime has nuanced system-schema write behavior for
the embedded `root@%` identity used by MyLite tests:

- `CREATE TEMPORARY TABLE information_schema.t ...` fails with
  `1044 / 42000` access denied for `information_schema`.
- `DROP TEMPORARY TABLE information_schema.t` also fails with
  `1044 / 42000` access denied for `information_schema`.
- `CREATE TEMPORARY TABLE performance_schema.t ...` fails with
  `1044 / 42000` access denied for `performance_schema`.
- `DROP TEMPORARY TABLE performance_schema.t` for a missing temporary table
  returns `1051 / 42S02` unknown table.
- `CREATE TEMPORARY TABLE mysql.t ...` succeeds for `root`.
- `USE mysql; CREATE TEMPORARY TABLE t ...` succeeds for `root`.
- `CREATE TEMPORARY TABLE sys.t ...` succeeds for `root`.
- `USE sys; CREATE TEMPORARY TABLE t ...` succeeds for `root`.
- `CREATE SCHEMA` and `DROP SCHEMA` aliases follow the same diagnostics as the
  corresponding `CREATE DATABASE` and `DROP DATABASE` statements for
  `information_schema`, `mysql`, and `performance_schema`.
- `CREATE DATABASE mysql`, `CREATE SCHEMA mysql`, `DROP DATABASE mysql`, and
  `DROP SCHEMA mysql` fail with
  `3552 / HY000` and the `Access to system schema 'mysql' is rejected.`
  diagnostic.
- `ALTER DATABASE information_schema`, `ALTER SCHEMA information_schema`,
  `ALTER DATABASE performance_schema`, and `ALTER SCHEMA performance_schema`
  fail with `1044 / 42000` access denied.
- `ALTER DATABASE mysql` and `ALTER SCHEMA mysql` fail with
  `3552 / HY000` system-schema diagnostics.
- `ALTER DATABASE sys` and `ALTER SCHEMA sys` with idempotent default
  charset/collation options succeed for `root`.
- `CREATE DATABASE IF NOT EXISTS mysql` succeeds as an existing-database no-op
  with `Note 1007`.
- `CREATE DATABASE sys` fails with the ordinary existing-database diagnostic
  when `sys` is installed, and `CREATE DATABASE IF NOT EXISTS sys` succeeds as
  an existing-database no-op with `Note 1007`.
- The comparison script intentionally avoids destructive `DROP DATABASE sys`
  probes. MyLite still protects `sys` from all schema-level and table-level
  writes because its `sys` rows are metadata-only.

## MyLite Compatibility Decision

MyLite deliberately uses a stricter embedded baseline than MySQL for `mysql` and
`sys` table writes. All four built-in schema names are reserved synthetic
schemas, not user catalog schemas:

- `information_schema`
- `mysql`
- `performance_schema`
- `sys`

`USE` can select any of those schemas, and supported metadata listing surfaces
can show their metadata-only table-directory rows. Mutating statements that
target them fail deterministically before object lookup.

This avoids creating user objects inside metadata-only namespaces, avoids
persisting accidental objects that would look like real MySQL server internals,
and keeps the `.mylite` file format free of fake data-dictionary storage.

## Scope

Supported in this slice:

- `USE information_schema`, `USE mysql`, `USE performance_schema`, and `USE sys`
  remain accepted selected-schema operations.
- Qualified write targets in all four built-in schemas fail before catalog
  lookup.
- Unqualified write targets fail when the selected schema is any built-in
  schema.
- Denied writes do not mutate MyLite catalog descriptors, user base tables,
  metadata rows, or physical SQLite storage.
- Diagnostics are stable and MySQL-shaped:
  - `information_schema` and `performance_schema` writes return
    `1044 / 42000` access denied for the named database.
  - `mysql` and `sys` writes return `3552 / HY000`
    `Access to system schema '<schema>' is rejected.`

Rejected with the built-in schema diagnostic:

- `CREATE DATABASE` / `CREATE SCHEMA`, `ALTER DATABASE` / `ALTER SCHEMA`, and
  `DROP DATABASE` / `DROP SCHEMA` targeting a built-in schema, including
  `IF [NOT] EXISTS` forms where applicable.
- `CREATE TABLE`, `CREATE TEMPORARY TABLE`, `CREATE TABLE ... LIKE`, and
  `CREATE TABLE ... SELECT` whose created table target is in a built-in schema.
- `DROP TABLE` and `DROP TEMPORARY TABLE` targets in a built-in schema,
  including `IF EXISTS` forms.
- `CREATE INDEX`, `DROP INDEX`, supported `ALTER TABLE` actions,
  `TRUNCATE TABLE`, and `LOCK TABLES` whose table target is in a built-in
  schema.
- `ANALYZE TABLE`, `CHECK TABLE`, `OPTIMIZE TABLE`, and `REPAIR TABLE` whose
  table maintenance target is in a built-in schema.
- `RENAME TABLE` pairs where either source or destination is in a built-in
  schema, and supported `ALTER TABLE ... RENAME` forms where either side is in
  a built-in schema.
- Current single-table `INSERT`, `REPLACE`, `UPDATE`, and `DELETE` write
  targets in a built-in schema, plus supported joined `DELETE` sources where
  the deleted alias resolves to a built-in schema table.

Out of scope:

- MySQL privilege-table mutation semantics for `mysql`.
- Writable `sys` schema user objects or `sys_config` updates.
- Performance Schema setup-table writes or instrumentation state changes.
- Physical system-schema table storage in SQLite.
- Privilege engines, users, roles, grants, definers, or account-specific
  filtering.
- Stored procedure, function, event, trigger, or view execution inside built-in
  schemas.

## Ownership Boundary

- Public API: no ABI changes. `mylite_execute()` still returns `MYLITE_ERROR`
  and exposes diagnostics through the existing error accessors.
- Parser/AST: unchanged. Existing schema-qualified and unqualified identifiers
  already represent the admitted statements.
- Analyzer/planner/runtime: owns detecting built-in schema write targets before
  user-schema catalog lookup and before generated SQLite SQL is built.
- Catalog metadata: built-in schemas remain synthetic descriptors. User schema
  descriptors and table descriptors are not created for built-in schema names.
- Result builder: successful `USE` returns the normal no-row result object;
  denied writes return no result rows.
- Storage/VFS/SQLite: unchanged. Denied writes do not create SQLite tables,
  indexes, views, triggers, or data rows.

## Resolution Rules

For mutating schema statements:

1. Normalize the target schema identifier through MyLite's existing identifier
   path.
2. If the target matches a built-in schema name, fail with that schema's
   built-in write diagnostic.
3. Otherwise continue through existing user-schema creation, alteration, or
   drop handling.

For mutating table statements:

1. Copy table-name identifier parts using MyLite's existing table-name
   collection path.
2. If the target is schema-qualified and the schema part names a built-in
   schema, fail with that schema's built-in write diagnostic.
3. If the target is unqualified and the selected schema is a built-in schema,
   fail with that selected schema's built-in write diagnostic.
4. Otherwise continue through existing user-schema and table resolution.

The guard runs before:

- reserved `_mylite_*` name checks,
- unknown-database diagnostics,
- unknown-table diagnostics,
- `IF EXISTS` / `IF NOT EXISTS` no-op handling,
- table kind checks,
- table/view/index/constraint descriptor mutation, and
- generated SQLite SQL construction.

## Grammar

No grammar changes are required. The existing grammar already admits the needed
schema-qualified and unqualified forms:

```lemon
use_statement ::= USE identifier.
schema_statement ::= CREATE DATABASE if_not_exists_opt identifier schema_options_opt.
schema_statement ::= CREATE SCHEMA if_not_exists_opt identifier schema_options_opt.
schema_statement ::= ALTER DATABASE identifier_opt schema_options.
schema_statement ::= ALTER SCHEMA identifier_opt schema_options.
schema_statement ::= DROP DATABASE if_exists_opt identifier.
schema_statement ::= DROP SCHEMA if_exists_opt identifier.
table_name ::= identifier.
table_name ::= identifier DOT identifier.
create_table_statement ::= CREATE table_kind_opt TABLE if_not_exists_opt table_name create_table_body table_options.
create_table_statement ::= CREATE table_kind_opt TABLE table_name LIKE table_name.
create_table_statement ::= CREATE TABLE table_name AS select_statement.
drop_table_statement ::= DROP table_kind_opt TABLE if_exists_opt table_name_list.
rename_table_statement ::= RENAME TABLE rename_pair_list.
alter_table_statement ::= ALTER TABLE table_name alter_action.
truncate_statement ::= TRUNCATE table_opt table_name.
insert_statement ::= INSERT insert_modifier_opt INTO table_name insert_payload.
replace_statement ::= REPLACE replace_modifier_opt INTO table_name replace_payload.
update_statement ::= UPDATE table_name SET assignment_list update_tail.
delete_statement ::= DELETE FROM table_name delete_tail.
```

The snippets above are MyLite-owned descriptions of the active grammar surface,
not copied MySQL grammar.

## Diagnostics

Denied `information_schema` writes return:

- code: `1044`
- SQLSTATE: `42000`
- message: `Access denied for user 'root'@'%' to database 'information_schema'`

Denied `performance_schema` writes return:

- code: `1044`
- SQLSTATE: `42000`
- message:
  `Access denied for user 'root'@'%' to database 'performance_schema'`

Denied `mysql` writes return:

- code: `3552`
- SQLSTATE: `HY000`
- message: `Access to system schema 'mysql' is rejected.`

Denied `sys` writes return:

- code: `3552`
- SQLSTATE: `HY000`
- message: `Access to system schema 'sys' is rejected.`

The `1044` messages use MyLite's current embedded identity `root@%`, matching
the current `USER()` / `CURRENT_USER()` baseline. The diagnostic is independent
of the named table, column, index, or action.

## Performance and Storage

The guard is an early planner check over at most two identifier components and a
four-entry built-in schema descriptor table. It avoids catalog reads and
generated SQLite SQL for denied built-in schema writes. No SQLite fork change or
new dependency is needed.

## Tests

The implementation must add:

- a safe MySQL expectation script that verifies MySQL 8.4.9 diagnostics and the
  observed writable temporary-table behavior for `mysql` and `sys` without
  destructive `sys` schema operations;
- focused C runtime coverage for schema DDL, table DDL, index DDL, rename,
  truncate, insert, replace, update, delete, selected-schema unqualified writes,
  and unchanged user table contents after denied writes;
- CTest registration with a dotted runtime test name.
