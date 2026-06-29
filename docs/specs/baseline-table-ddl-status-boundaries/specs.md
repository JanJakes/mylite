# Baseline Table DDL Status Boundaries

## Summary

This slice closes three broad table-DDL compatibility rows whose previous red
status no longer matched the implemented baseline:

- limited silent column-specification normalization;
- generated invisible primary keys in MyLite's fixed-disabled baseline;
- NDB-shaped comment option strings preserved as ordinary table and column
  comments.

The slice does not add new storage-engine behavior. It documents the current
boundary precisely and adds focused MySQL/runtime evidence for NDB-shaped
comment strings.

## Compatibility Authority

- MySQL 8.4 Reference Manual, silent column specification changes:
  <https://dev.mysql.com/doc/refman/8.4/en/silent-column-changes.html>
- MySQL 8.4 Reference Manual, generated invisible primary keys:
  <https://dev.mysql.com/doc/refman/8.4/en/create-table-gipks.html>
- MySQL 8.4 Reference Manual, NDB comment options:
  <https://dev.mysql.com/doc/refman/8.4/en/create-table-ndb-comment-options.html>
- MySQL 8.4 Reference Manual, `ALTER TABLE`:
  <https://dev.mysql.com/doc/refman/8.4/en/alter-table.html>
- Existing MyLite specs for integer display width, `SERIAL`, primary-key
  lifecycle, table comments, column comments, and
  `@@sql_generate_invisible_primary_key`.
- Observed MySQL 8.4.9 behavior recorded in:
  - `packages/libmylite/tests/mysql_baseline_alter_table_comment_expectations.sh`
  - `packages/libmylite/tests/mysql_baseline_column_comments_expectations.sh`

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

MySQL performs many automatic descriptor rewrites and metadata normalizations in
table DDL. Existing MyLite slices already verify representative subsets,
including deprecated integer display-width normalization, `YEAR UNSIGNED`
normalization, `SERIAL` expansion, primary-key `NOT NULL` effects, repeated
attribute handling where implemented, and descriptor replacement behavior for
the supported `MODIFY` / `CHANGE` subset. Broad automatic rewrite parity is not
complete.

With `@@sql_generate_invisible_primary_key = OFF`, MySQL creates a table
without a user-declared primary key exactly as declared. With the variable
enabled, MySQL can add an invisible `my_row_id` primary key to qualifying InnoDB
tables. MyLite exposes a verified fixed-disabled variable baseline and does not
create hidden columns.

NDB comment options are carried inside ordinary table comments using
`NDB_TABLE=...` and ordinary column comments using `NDB_COLUMN=...`. On the
verified non-NDB MySQL 8.4.9 runtime, InnoDB tables preserve these strings in
`SHOW CREATE TABLE`, `SHOW TABLE STATUS`, `INFORMATION_SCHEMA.TABLES`, and
`INFORMATION_SCHEMA.COLUMNS`; no NDB behavior is applied.

## Supported Surface

### Silent Column Specification Changes

MyLite supports documented normalization only where covered by existing feature
specs and MySQL-runtime expectations:

- deprecated integer display widths are accepted, warned, and normalized except
  for visible signed `TINYINT(1)` / `INT1(1)` metadata;
- `YEAR UNSIGNED` aliases normalize to `YEAR`;
- `SERIAL` expands to the existing unsigned auto-increment descriptor plus its
  generated unique-key metadata;
- primary-key participation drives the existing descriptor nullability rules;
- repeated `DEFAULT` and supported nullability attributes follow the current
  implemented last-effective-value behavior;
- supported `MODIFY` / `CHANGE` definitions replace descriptor metadata in the
  current MyLite subset.

This remains a partial row. MyLite does not claim the full MySQL automatic
rewrite matrix, non-admitted column attributes, generated-column rewrites,
engine-specific rewrites, or complete protocol metadata parity.

### Generated Invisible Primary Keys

MyLite supports only the fixed-disabled generated-invisible-primary-key
baseline:

- `@@sql_generate_invisible_primary_key` reads return the verified disabled
  value through the documented scalar and `SHOW VARIABLES` surfaces;
- disabled assignments covered by the fixed-system-variable baseline are
  accepted as no-effect assignments where documented;
- `CREATE TABLE` output remains user-declared and no hidden `my_row_id` column
  is generated.

Mutable global/session state, hidden primary-key descriptors, generated
auto-increment columns, GIPK visibility controls, replication behavior, and
Performance Schema variable metadata remain unsupported.

### NDB Comment Option Strings

MyLite preserves NDB-shaped comment strings as ordinary descriptor metadata
where table and column comments are already supported:

```sql
CREATE TABLE t (
    id INT,
    body TEXT COMMENT 'NDB_COLUMN=BLOB_INLINE_SIZE=4096,MAX_BLOB_PART_SIZE'
) COMMENT='NDB_TABLE=READ_BACKUP=1,PARTITION_BALANCE=FOR_RA_BY_LDM';
```

The strings are decoded, length-checked, stored, cloned, persisted, and rendered
by the same table-comment and column-comment paths as any other comment text.
MyLite does not implement the NDB storage engine, NDB-specific validation,
partition balancing, read-backup behavior, blob part sizing, or NDB metadata
tools.

## Architecture

No new SQLite extension point or fork patch is needed. The affected behavior is
descriptor-owned metadata:

- parser and AST support already admits table `COMMENT [=] string` and column
  `COMMENT string` in supported DDL positions;
- runtime planning decodes and validates comment text before catalog mutation;
- durable and temporary table descriptors own the comment strings;
- metadata renderers read descriptor comments instead of SQLite schema text;
- the fixed-disabled GIPK variable remains handle/runtime state with no catalog
  side effects.

## Tests

This slice extends existing MySQL expectation scripts and fast C runtime tests
instead of adding duplicate executables:

- table NDB-shaped comment string preservation through `SHOW CREATE TABLE`,
  `SHOW TABLE STATUS`, and `INFORMATION_SCHEMA.TABLES`;
- column NDB-shaped comment string preservation through `SHOW CREATE TABLE`,
  `SHOW FULL COLUMNS`, and `INFORMATION_SCHEMA.COLUMNS`;
- existing table/column comment diagnostics, persistence, cloning, and SQL-mode
  tests continue to cover the shared comment infrastructure.

## Deferred

- Full MySQL silent column-specification rewrite parity.
- Mutable `sql_generate_invisible_primary_key` state and hidden `my_row_id`
  primary key generation.
- NDB engine support and semantic interpretation of `NDB_TABLE` /
  `NDB_COLUMN` comment options.
