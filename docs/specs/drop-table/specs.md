# DROP TABLE base execution

## Scope

This feature makes `DROP TABLE` executable for base tables created by the
supported `CREATE TABLE` subset. It builds on the schema lifecycle, core
metadata catalog, and `CREATE TABLE` base execution features.

In scope:

- `DROP TABLE table_name [, table_name ...]`
- `DROP TABLE IF EXISTS table_name [, table_name ...]`
- schema-qualified and default-schema table resolution
- physical SQLite user-table removal
- cleanup of `__mylite_table_catalog`, `__mylite_column_catalog`, and
  `__mylite_index_catalog`
- deterministic duplicate-name validation
- all-or-nothing behavior for non-`IF EXISTS` multi-table drops
- `RESTRICT` and `CASCADE` as accepted no-op tail modifiers
- `DROP TABLE` temporary-table shadow resolution
- `DROP TEMPORARY TABLE` for supported temporary tables

Out of scope:

- warning/note records for missing `IF EXISTS` targets
- trigger, privilege, foreign-key, view, routine, and prepared-statement
  interactions
- `DROP TEMPORARY TABLE` transaction-warning and rollback exception fidelity
- crash recovery beyond SQLite transaction atomicity

## Sources

- MySQL 8.4 Reference Manual, `DROP TABLE` statement:
  https://dev.mysql.com/doc/refman/8.4/en/drop-table.html
- MySQL 8.4 Reference Manual, Identifier Case Sensitivity:
  https://dev.mysql.com/doc/refman/8.4/en/identifier-case-sensitivity.html
- Existing MyLite specs:
  - `docs/specs/core-metadata-catalog/specs.md`
  - `docs/specs/create-table-base-execution/specs.md`
  - `docs/specs/schema-lifecycle/specs.md`
- Observed MySQL 8.4.9 runtime behavior from Docker container
  `mylite-mysql-849`, including Jan's independent Task 12 probe set and
  additional live checks for multi-table atomicity, duplicate targets,
  `IF EXISTS`, missing schemas, and `TEMPORARY`.

This specification is independently authored from official documentation and
observed runtime behavior. It does not copy MySQL grammar, documentation prose,
or implementation sources.

## MySQL 8.4.9 behavior summary

Syntax:

```sql
DROP [TEMPORARY] TABLE [IF EXISTS] tbl_name [, tbl_name ...]
    [RESTRICT | CASCADE]
```

The statement removes one or more tables. `RESTRICT` and `CASCADE` are accepted
for portability and have no effect for ordinary base-table drops.

Schema resolution and missing objects:

- Unqualified names require a selected default schema, even with `IF EXISTS`.
  Without one, MySQL returns error 1046 `No database selected`.
- Qualified names target the written schema.
- A qualified missing schema is reported as unknown table 1051, not as unknown
  database. With `IF EXISTS`, the statement succeeds and records a 1051 note.
- A missing unqualified table in a selected schema returns error 1051 unless
  `IF EXISTS` is present.
- With `IF EXISTS`, existing tables in the same statement are dropped and each
  missing target records a note. Warning storage is deferred in MyLite.
- System schemas such as `information_schema` reject base and temporary drops
  before mutation.

Multi-table and duplicates:

- If any non-`IF EXISTS` listed target is missing, the statement fails and no
  listed existing table is dropped.
- Repeating the same table name in one `DROP TABLE` statement returns error
  1066 `Not unique table/alias` and does not drop the table. `IF EXISTS` does
  not bypass this validation.
- On the verified Linux runtime, `lower_case_table_names=0`, so database and
  table name lookup is case-sensitive. MyLite follows its existing
  byte-preserving, binary catalog lookup for schema and table names.

Temporary tables:

- `DROP TEMPORARY TABLE` only targets temporary tables and does not drop base
  tables.
- `DROP TEMPORARY TABLE base_table` returns missing-table error 1051 and leaves
  the base table intact.
- `DROP TEMPORARY TABLE IF EXISTS base_table` succeeds as a temporary-table
  no-op and records a 1051 note.
- `DROP TABLE base_table` removes a shadowing temporary table first, then
  exposes the persistent table again.

Metadata and side effects:

- Dropping a base table removes its rows from `INFORMATION_SCHEMA.TABLES`,
  `COLUMNS`, and `STATISTICS`.
- With `foreign_key_checks=1`, dropping a parent table referenced by a
  surviving child table fails with error 3730 and no table is dropped. A
  multi-table `DROP TABLE` that includes the referenced parent and every
  referencing child succeeds.
- With `foreign_key_checks=0`, dropping the referenced parent succeeds and
  preserves the surviving child table's foreign-key metadata; future checked
  child writes still fail until a matching parent is restored or the foreign
  key is dropped.
- MySQL performs an implicit commit before ordinary `DROP TABLE`, including
  failed statements such as missing-table errors. Later `ROLLBACK` does not
  undo work that preceded the drop attempt or later autocommit work.
- `DROP TEMPORARY TABLE` does not implicitly commit the active transaction.
  MySQL still does not roll back temporary-table drops and can emit warning
  1752 when rollback cannot undo them; exact warning fidelity remains deferred
  in MyLite.

## MyLite behavior

### Parser and AST

MyLite adds a `drop_table_statement` AST node. Children are appended in stable
order:

1. table-name list
2. optional `IF EXISTS`

The statement node carries booleans for `TEMPORARY`, `RESTRICT`, and `CASCADE`.
The table-name list contains each parsed table name as an identifier or
qualified identifier. Qualified identifiers with more than two parts are
rejected during runtime copy with an execution diagnostic, matching the current
`CREATE TABLE` boundary.

Supported syntax:

```sql
DROP [TEMPORARY] TABLE [IF EXISTS] table_name_list opt_drop_table_tail

table_name_list ::= table_name
table_name_list ::= table_name_list ',' table_name
opt_drop_table_tail ::= /* empty */
opt_drop_table_tail ::= RESTRICT
opt_drop_table_tail ::= CASCADE
```

Malformed comma lists remain syntax errors, including `DROP TABLE IF EXISTS t,`.

### Runtime execution

Normal base-table drops:

- Preparing a parsed `DROP TABLE` creates a custom statement handle.
- The first `mylite_step()` resolves and validates every listed target before
  mutating storage.
- Ordinary non-`TEMPORARY` `DROP TABLE` commits any active explicit
  transaction before target validation. This follows the MySQL boundary even
  when validation later fails.
- Unqualified targets use the selected default schema. If none is selected,
  execution fails with `MYLITE_EXEC_ERROR` and diagnostic `No database
  selected`.
- Qualified targets use their explicit schema.
- System schemas are rejected with MySQL-compatible access-denied diagnostics.
- Duplicate targets are detected before existence handling. Duplicates are
  compared by effective schema and table name using bytewise comparison.
- Missing targets fail the statement unless `IF EXISTS` is present. With
  `IF EXISTS`, they are skipped and warning/note records are deferred.
- `RESTRICT` and `CASCADE` are accepted and ignored.
- Foreign-key parent-table dependencies are validated before storage mutation
  when `foreign_key_checks` is enabled. Referenced parents can be dropped only
  when all referencing child tables are also existing persistent targets in the
  same statement.

Temporary drops:

- Because MyLite does not support temporary tables yet, `DROP TEMPORARY TABLE`
  never drops base-table metadata or physical tables.
- `DROP TEMPORARY TABLE name` fails with `MYLITE_EXEC_ERROR` and an unknown
  table diagnostic.
- `DROP TEMPORARY TABLE IF EXISTS name` succeeds as a no-op after duplicate,
  default-schema, and system-schema validation.
- Qualified missing schemas under `TEMPORARY IF EXISTS` follow the same
  temporary-table no-op behavior as MySQL's observed base-table shadow boundary.

Storage cleanup:

- The physical SQLite table name is the same hex-encoded internal name created
  by `CREATE TABLE`: `__mylite_user_<schema_hex>__<table_hex>`.
- Cleanup executes inside a SQLite transaction:
  1. drop every physical SQLite table for the resolved targets
  2. delete matching rows from `__mylite_index_catalog`
  3. delete matching rows from `__mylite_column_catalog`
  4. delete matching rows from `__mylite_table_catalog`
- If any step fails, the transaction is rolled back and no listed existing
  table is partially dropped.

### Diagnostics

MyLite does not yet expose MySQL numeric error codes or SQLSTATEs through the
public API. Diagnostics should still include stable MySQL-like text:

- `No database selected`
- `Unknown table '<schema>.<table>'`
- `Not unique table/alias: '<table>'`
- `Access denied for user 'root'@'localhost' to database '<schema>'`

## Lemon grammar snippets

These snippets describe MyLite's intended grammar for this feature:

```lemon
drop_table_statement ::= DROP opt_temporary TABLE opt_if_exists
                         drop_table_name_list opt_drop_table_mode.

opt_temporary ::= .
opt_temporary ::= TEMPORARY.

drop_table_name_list ::= table_name.
drop_table_name_list ::= drop_table_name_list COMMA table_name.

opt_drop_table_mode ::= .
opt_drop_table_mode ::= RESTRICT.
opt_drop_table_mode ::= CASCADE.
```

## MySQL-runtime-verified expectations

Implementation tests should cover these MySQL 8.4.9 expectations:

| SQL or behavior | Expected MyLite-compatible outcome |
| --- | --- |
| `DROP TABLE no_default_table` with no selected schema | Execution error containing `No database selected`. |
| `DROP TABLE IF EXISTS no_default_table` with no selected schema | Execution error containing `No database selected`. |
| `DROP TABLE missing_schema.t` | Execution error containing `Unknown table 'missing_schema.t'`. |
| `DROP TABLE IF EXISTS missing_schema.t` | Success no-op. |
| `DROP TABLE information_schema.tables` | Execution error; system schema remains unmodified. |
| `DROP TEMPORARY TABLE IF EXISTS information_schema.tables` | Execution error; system schema remains unmodified. |
| `DROP TABLE existing` | Removes physical table and table/column/index catalog rows. |
| `DROP TABLE missing` in a selected schema | Execution error containing `Unknown table`. |
| `DROP TABLE IF EXISTS missing` | Success no-op. |
| `DROP TABLE IF EXISTS existing, missing, existing2` | Drops existing targets and succeeds. |
| `DROP TABLE d, missing, e` | Fails and leaves `d` and `e` intact. |
| `DROP TABLE dup, dup` | Fails as duplicate target and leaves `dup` intact. |
| `DROP TABLE IF EXISTS dup, dup` | Fails as duplicate target and leaves `dup` intact. |
| `DROP TABLE parent` when surviving child table references it | Error 3730; parent and child remain. |
| `DROP TABLE parent, child` for a referenced parent and its child | Succeeds; both table rows and the child FK metadata are removed. |
| `SET foreign_key_checks=0; DROP TABLE parent` for a referenced parent | Succeeds; child table and FK metadata remain. |
| `START TRANSACTION; INSERT; DROP TABLE existing; INSERT; ROLLBACK` | Both inserts and the drop survive because ordinary `DROP TABLE` commits before execution. |
| `START TRANSACTION; INSERT; DROP TABLE missing; INSERT; ROLLBACK` | Both inserts survive because ordinary `DROP TABLE` commits before validation and leaves the session outside the explicit transaction after the error. |
| `DROP TEMPORARY TABLE base_table` | Does not drop base table; deterministic execution error. |
| `DROP TEMPORARY TABLE IF EXISTS base_table` | Succeeds without dropping base table. |
| `DROP TABLE IF EXISTS x, y RESTRICT` | Accepts modifier, drops existing targets. |
| `DROP TABLE IF EXISTS x, y CASCADE` | Accepts modifier, drops existing targets. |
| `DROP TABLE IF EXISTS trailing,` | Syntax error. |

## Test plan

- Parser tests:
  - single and multi-table drops
  - schema-qualified and unqualified names
  - `IF EXISTS`
  - `TEMPORARY`
  - `RESTRICT` and `CASCADE`
  - malformed trailing comma and empty name list
  - reserved identifier boundaries, including quoted reserved words
- Runtime tests:
  - successful drop removes `TABLES`, `COLUMNS`, `STATISTICS`, and physical
    SQLite table
  - mixed `IF EXISTS` existing/missing drop succeeds and drops existing tables
  - missing default schema fails even with `IF EXISTS`
  - missing qualified schema/table behavior follows the MySQL 1051 shape
  - missing table without `IF EXISTS` fails and preserves existing tables
  - multi-table non-`IF EXISTS` failure is all-or-nothing
  - duplicate listed names fail and preserve the table
  - system schema access denied, including `TEMPORARY IF EXISTS`
  - `DROP TEMPORARY TABLE` never drops base tables
  - `RESTRICT` and `CASCADE` are no-op accepted modifiers
