# INFORMATION_SCHEMA Write Access Baseline

## Summary

MySQL exposes `INFORMATION_SCHEMA` as a metadata database. The database can be
selected as the current default database, and its tables can be read, but
mutating statements that target it fail with database access denied diagnostics.
This slice teaches MyLite to preserve that distinction for the currently
implemented schema, table, index, and single-table DML write targets.

Official reference: MySQL 8.4 Reference Manual,
`INFORMATION_SCHEMA` introduction:
https://dev.mysql.com/doc/refman/8.4/en/information-schema-introduction.html

Runtime authority: local MySQL 8.4.9 probes in
`packages/libmylite/tests/mysql_information_schema_write_access_expectations.sh`.

## MySQL 8.4.9 Observations

The following statement families were verified against MySQL 8.4.9 as
`root` over the local comparison runtime:

- `CREATE DATABASE information_schema`
- `CREATE DATABASE IF NOT EXISTS information_schema`
- `DROP DATABASE information_schema`
- `DROP DATABASE IF EXISTS information_schema`
- `CREATE TABLE information_schema.t (id INT)`
- `CREATE TEMPORARY TABLE information_schema.t (id INT)`
- `CREATE TABLE IF NOT EXISTS information_schema.t (id INT)`
- `DROP TABLE information_schema.t`
- `DROP TABLE IF EXISTS information_schema.t`
- `CREATE INDEX idx ON information_schema.TABLES (TABLE_NAME)`
- `DROP INDEX idx ON information_schema.TABLES`
- `ALTER TABLE information_schema.TABLES ADD COLUMN x INT`
- `TRUNCATE TABLE information_schema.TABLES`
- `INSERT INTO information_schema.SCHEMATA ...`
- `REPLACE INTO information_schema.SCHEMATA ...`
- `UPDATE information_schema.SCHEMATA ...`
- `DELETE FROM information_schema.SCHEMATA`
- `RENAME TABLE app.t TO information_schema.t`
- `RENAME TABLE information_schema.TABLES TO app.t`
- `ALTER TABLE app.t RENAME TO information_schema.t`
- `USE information_schema; CREATE TABLE t (id INT)`
- `USE information_schema; INSERT INTO SCHEMATA ...`

All mutating statements above fail before ordinary unknown-schema handling with:

```text
ERROR 1044 (42000): Access denied for user 'root'@'%' to database 'information_schema'
```

`USE information_schema` itself succeeds and makes `DATABASE()` return
`information_schema`.

## Scope

Supported in this slice:

- `USE information_schema` as a synthetic selected schema.
- Unqualified `INFORMATION_SCHEMA` reads through the selected synthetic schema
  for the existing limited metadata `SELECT` path.
- MySQL-compatible access-denied diagnostics for currently implemented mutating
  statements whose write target is explicitly schema-qualified with
  `information_schema`.
- The same diagnostic for unqualified mutating statements when
  `information_schema` is the selected schema.
- Existing MyLite catalog schemas, table descriptors, information-schema rows,
  and physical SQLite tables remain unchanged after denied statements.

Rejected with this slice's diagnostic:

- `CREATE DATABASE` / `CREATE SCHEMA` and `DROP DATABASE` / `DROP SCHEMA`
  targeting `information_schema`.
- `CREATE TABLE`, `CREATE TEMPORARY TABLE`, `CREATE TABLE ... LIKE`, and
  `CREATE TABLE ... SELECT` when the created table target is in
  `information_schema`.
- `DROP TABLE` and `DROP TEMPORARY TABLE` targets in `information_schema`,
  including `IF EXISTS` forms.
- `CREATE INDEX`, `DROP INDEX`, and supported `ALTER TABLE` actions whose table
  write target is in `information_schema`.
- `TRUNCATE TABLE` targets in `information_schema`.
- `RENAME TABLE` pairs where either the source or destination target is in
  `information_schema`, and supported `ALTER TABLE ... RENAME` forms where
  either side is in `information_schema`.
- Current single-table `INSERT`, `REPLACE`, `UPDATE`, and `DELETE` write targets
  in `information_schema`.

Out of scope:

- Privilege tables, accounts, grants, roles, authentication, or configurable
  privileges.
- Writes to other MySQL system schemas such as `mysql`, `performance_schema`,
  and `sys`.
- Complete `INFORMATION_SCHEMA` query support beyond the existing metadata
  `SELECT` subset.
- `SHOW TABLES` / `SHOW COLUMNS` behavior for selected `information_schema`
  beyond already supported explicit metadata queries.
- `CREATE TABLE ... LIKE information_schema.table` source-table compatibility.
- MySQL table/view updatability diagnostics inside `INFORMATION_SCHEMA`; this
  slice returns the database access denied diagnostic before resolving table
  names or object kinds.

## Ownership Boundary

- Public API: no ABI changes. `mylite_execute()` still returns `MYLITE_ERROR`
  and exposes the diagnostic through the existing error accessors.
- Statement context: unchanged. Access-denied checks happen during statement
  planning before any physical SQL is built.
- Parser/AST: unchanged. Existing schema-qualified and unqualified identifiers
  already represent all admitted statements.
- Analyzer/planner/runtime: owns detecting synthetic `information_schema` write
  targets and producing MySQL-shaped diagnostics before catalog lookup.
- Catalog module: remains authoritative for user schemas and base-table
  descriptors. It does not store an `information_schema` schema descriptor.
- Result builder: successful `USE information_schema` returns the normal
  non-row result object; denied writes return no result rows.
- Storage/VFS/SQLite: unchanged. Denied writes do not generate SQLite SQL,
  mutate descriptor rows, change catalog generations, or touch the `.mylite`
  preamble or shifted SQLite payload.

## Resolution Rules

For a mutating statement write target:

1. Copy identifier parts using MyLite's existing identifier normalization.
2. If the target is schema-qualified and the first part matches
   `information_schema` case-insensitively, fail with `1044 / 42000`.
3. If the target is unqualified and the selected schema is
   `information_schema`, fail with `1044 / 42000`.
4. Otherwise, continue through the existing schema and table resolution path.

The guard runs before:

- reserved `_mylite_*` name checks,
- unknown-database diagnostics,
- unknown-table diagnostics,
- `IF EXISTS` / `IF NOT EXISTS` no-op handling,
- table kind checks,
- catalog mutations, and
- generated SQLite SQL construction.

For `USE information_schema`, MyLite stores the selected schema string in the
session without creating a catalog descriptor. Later unqualified write targets
use the write-target guard above. Later unqualified metadata `SELECT` sources
are resolved through the existing synthetic `INFORMATION_SCHEMA` query engine.

## Grammar

No grammar changes are required. The existing grammar already admits the needed
schema-qualified and unqualified forms:

```lemon
use_statement ::= USE identifier.
schema_statement ::= CREATE DATABASE identifier.
schema_statement ::= DROP DATABASE identifier.
table_name ::= identifier.
table_name ::= identifier DOT identifier.
create_table_statement ::= CREATE table_kind_opt TABLE table_name create_table_body table_options.
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

Denied writes return:

- code: `1044`
- SQLSTATE: `42000`
- message: `Access denied for user 'root'@'%' to database 'information_schema'`

The message uses MyLite's current embedded identity `root@%`, matching the
current `USER()` / `CURRENT_USER()` baseline. The diagnostic is deterministic
and independent of the named table, column, index, or action.

Ordinary user-schema diagnostics remain unchanged:

- missing default schema still returns `1046 / 3D000`;
- unknown user schemas still return the existing MyLite/MySQL-compatible
  unknown schema/table diagnostics for that statement family;
- `_mylite_*` reserved names still use the existing reserved-name diagnostics.

## Performance and Storage

The feature is an early planner check over at most two identifier components.
It avoids catalog reads for denied `information_schema` writes and never
materializes table rows. No SQLite fork change is needed; this is MyLite wrapper
behavior around synthetic metadata schema handling.

## Tests

The implementation must add:

- a MySQL expectation script that verifies the access-denied diagnostics and
  `USE information_schema` behavior against MySQL 8.4.9;
- a fast C runtime test covering schema DDL, table DDL, index DDL, rename,
  truncate, insert, replace, update, delete, selected-schema unqualified writes,
  unqualified metadata reads after `USE information_schema`, and unchanged
  ordinary user-schema diagnostics;
- CTest registration with a dotted runtime test name.
