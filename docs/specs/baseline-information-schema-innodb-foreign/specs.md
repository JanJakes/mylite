# Baseline INFORMATION_SCHEMA InnoDB Foreign-Key Tables

## Status

This phase adds descriptor-backed `INFORMATION_SCHEMA.INNODB_FOREIGN` and
`INFORMATION_SCHEMA.INNODB_FOREIGN_COLS` system views. The views expose MySQL
8.4.9-shaped table and column metadata and return rows for MyLite's current
persistent-table foreign-key descriptors.

The slice does not add new foreign-key syntax, actions, enforcement behavior,
cross-schema references, physical InnoDB dictionary tables, buffer-pool state,
or privilege filtering.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing foreign-key descriptor and information-schema implementation in
  `packages/libmylite/src/runtime/mylite_execution.c`
- MySQL 8.4 Reference Manual, `INNODB_FOREIGN`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-innodb-foreign-table.html
- MySQL 8.4 Reference Manual, `INNODB_FOREIGN_COLS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-innodb-foreign-cols-table.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, and
existing MyLite source code. It does not copy MySQL, MariaDB, Percona, SQLite
implementation internals, or other restrictively licensed implementation
sources.

## MySQL 8.4.9 Observations

Runtime probes were run against the local `mysql:8.4.9` Docker runtime named
`mylite-mysql-849`.

Observed behavior shaping this slice:

- `INFORMATION_SCHEMA.INNODB_FOREIGN` and
  `INFORMATION_SCHEMA.INNODB_FOREIGN_COLS` exist as `SYSTEM VIEW` tables.
- `INFORMATION_SCHEMA.TABLES` reports `ENGINE = NULL`, `VERSION = 10`,
  `ROW_FORMAT = NULL`, `TABLE_ROWS = 0`, `DATA_LENGTH = 0`, and
  `AUTO_INCREMENT = NULL` for both views.
- `INNODB_FOREIGN` has columns `ID`, `FOR_NAME`, `REF_NAME`, `N_COLS`, and
  `TYPE`. The name columns are nullable `varchar(129)` with
  `utf8mb3_bin`; `N_COLS` is `bigint`; `TYPE` is `bigint unsigned`.
- `INNODB_FOREIGN_COLS` has columns `ID`, `FOR_COL_NAME`, `REF_COL_NAME`,
  and `POS`. `ID` is nullable `varchar(129)` with `utf8mb3_bin`; column names
  are non-null `varchar(64)` with `utf8mb3_tolower_ci`; `POS` is
  `int unsigned`.
- Foreign-key IDs and table names are formatted as `schema/object`, for
  example `app/fk_name` and `app/child_table`.
- The observed `TYPE` bit flags are: delete cascade `1`, delete set null `2`,
  update cascade `4`, update set null `8`, delete no action `16`, and update
  no action `32`. `RESTRICT` contributes no bits. A default foreign key
  reports `48`, matching default `NO ACTION` metadata in MySQL 8.4.9.
- Mixed-action probes returned `1` for delete cascade/update restrict, `4`
  for delete restrict/update cascade, `24` for delete no action/update set
  null, and `34` for delete set null/update no action.
- `INNODB_FOREIGN.N_COLS` is the number of child-reference columns.
- `INNODB_FOREIGN_COLS.POS` returned `1..n` for the probed MySQL 8.4.9
  runtime. MyLite mirrors the observed runtime value.
- Successful reads leave `@@warning_count = 0`, and `ROW_COUNT()` reports
  `-1` after the `SELECT`.

Representative probes:

```sh
docker exec mylite-mysql-849 mysql --protocol=TCP -h127.0.0.1 -uroot \
  --batch --raw --skip-column-names -e \
  "CREATE DATABASE mylite_innodb_foreign_probe; \
   USE mylite_innodb_foreign_probe; \
   CREATE TABLE parent(id INT NOT NULL PRIMARY KEY, b INT NOT NULL,
     UNIQUE KEY ub(id,b)) ENGINE=InnoDB; \
   CREATE TABLE child_setnull(id INT NOT NULL PRIMARY KEY, pid INT, pb INT,
     CONSTRAINT fk_setnull FOREIGN KEY(pid,pb) REFERENCES parent(id,b)
       ON DELETE SET NULL ON UPDATE SET NULL) ENGINE=InnoDB; \
   SELECT ID,FOR_NAME,REF_NAME,N_COLS,TYPE
     FROM INFORMATION_SCHEMA.INNODB_FOREIGN
    WHERE ID LIKE 'mylite_innodb_foreign_probe/%' ORDER BY ID; \
   SELECT ID,FOR_COL_NAME,REF_COL_NAME,POS
     FROM INFORMATION_SCHEMA.INNODB_FOREIGN_COLS
    WHERE ID LIKE 'mylite_innodb_foreign_probe/%' ORDER BY ID,POS; \
   SELECT @@warning_count, ROW_COUNT(); \
   DROP DATABASE mylite_innodb_foreign_probe;"
```

## Scope

Supported:

- `SELECT` from `INFORMATION_SCHEMA.INNODB_FOREIGN` and
  `INFORMATION_SCHEMA.INNODB_FOREIGN_COLS` using the existing
  information-schema query subset;
- case-insensitive information-schema table name lookup;
- table aliases, predicates, ordering, and `COUNT(*)` through the existing
  metadata query path;
- rows for MyLite persistent base-table foreign-key descriptors in child-table
  order;
- `ID`, `FOR_NAME`, and `REF_NAME` values formatted as `schema/object`;
- `N_COLS`, `TYPE`, and per-column `POS` values derived from descriptor
  foreign-key parts and action rules;
- table metadata through `INFORMATION_SCHEMA.TABLES`;
- column metadata through `INFORMATION_SCHEMA.COLUMNS`;
- descriptor changes from supported `ALTER TABLE ... DROP FOREIGN KEY` are
  reflected in subsequent reads.

Out of scope:

- new foreign-key syntax, action rules, or enforcement semantics;
- cross-schema foreign keys;
- temporary tables, views, generated columns outside the existing FK subset,
  or physical InnoDB dictionary rows;
- built-in-schema foreign-key rows;
- `PROCESS` privilege checks or account-specific filtering;
- storage-engine statistics, InnoDB table ids, or physical `.ibd` metadata.

## Ownership Boundary

- Public API: unchanged. Applications continue through `mylite_execute()` and
  current result accessors.
- Parser/AST: unchanged. The existing information-schema `SELECT` path already
  resolves table names, aliases, identifiers, predicates, and ordering.
- Analyzer/runtime: recognizes both InnoDB foreign-key views as supported
  information-schema system views and emits rows from loaded catalog
  descriptors.
- Catalog metadata: unchanged. The existing `_mylite_catalog_foreign_keys` and
  `_mylite_catalog_foreign_key_columns` descriptors are authoritative.
- Storage/SQLite: unchanged. No physical SQLite table, view, extension, or
  fork patch is required.

## Syntax

No new SQL grammar is added. The feature uses the existing admitted
information-schema `SELECT` grammar.

Examples in scope:

```sql
SELECT * FROM INFORMATION_SCHEMA.INNODB_FOREIGN;
SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_FOREIGN_COLS
 WHERE ID LIKE 'app/%';
SELECT f.ID, f.TYPE
  FROM INFORMATION_SCHEMA.INNODB_FOREIGN AS f
 WHERE f.TYPE IN (0, 10, 48)
 ORDER BY f.ID;
```

## Runtime Semantics

`INNODB_FOREIGN` and `INNODB_FOREIGN_COLS` are registered in the static
information-schema table registry. Row production is descriptor-backed:

- system rows for `TABLES` and `COLUMNS` are generated from static
  descriptors;
- direct reads iterate persistent catalog schemas and base tables;
- each child-table foreign-key descriptor produces one `INNODB_FOREIGN` row;
- each child/parent column pair produces one `INNODB_FOREIGN_COLS` row;
- `ID` is `child_schema/constraint_name`;
- `FOR_NAME` is `child_schema/child_table`;
- `REF_NAME` is `referenced_schema/referenced_table`. The current MyLite
  foreign-key subset admits same-schema references only, but the formatter uses
  the referenced table's schema descriptor if cross-schema descriptors appear in
  a future slice;
- `N_COLS` is the loaded foreign-key part count;
- `POS` is the descriptor ordinal position, matching the observed MySQL 8.4.9
  `1..n` values;
- `TYPE` is composed from stored `delete_rule` and `update_rule` text:
  `CASCADE`, `SET NULL`, `NO ACTION`, and `RESTRICT` map to the observed bit
  pattern above;
- successful reads introduce no warnings;
- `ROW_COUNT()` after a successful `SELECT` remains the existing query value
  `-1`.

## Diagnostics

The feature relies on existing information-schema diagnostics:

- unknown selected columns fail with the current unknown-column diagnostic;
- unsupported expressions, joins, grouping, predicates, and limits retain the
  current information-schema query subset behavior;
- stale or invalid catalog descriptors fail with existing runtime diagnostics;
- allocation failures use existing MyLite runtime diagnostics.

Successful reads introduce no warnings.

## Tests

Add a focused C runtime test and a MySQL expectation script. Coverage must
include:

- table kind and `INFORMATION_SCHEMA.TABLES` metadata for both views;
- `INFORMATION_SCHEMA.COLUMNS` metadata for all columns in both views;
- named, generated-name, composite, `CASCADE`, `SET NULL`, `NO ACTION`, and
  `RESTRICT` foreign-key rows, including mixed delete/update action
  combinations;
- `INNODB_FOREIGN.TYPE` bit mapping and `N_COLS`;
- `INNODB_FOREIGN_COLS` child and parent columns with `POS = 1..n`;
- case-insensitive table-name lookup, aliases, predicates, ordering, and
  unqualified selected-schema reads;
- `warning_count == 0` and `ROW_COUNT() == -1` after successful reads;
- descriptor updates after supported `ALTER TABLE ... DROP FOREIGN KEY`;
- file-backed reopen behavior.

Verification before commit:

```sh
cmake --build --preset dev --target mylite_runtime_information_schema_innodb_foreign_test
ctest --preset dev -R '^libmylite\.runtime\.(information_schema_innodb_foreign|information_schema_static_catalogs|builtin_schema_table_directory|foreign_key_constraints)$' --output-on-failure
packages/libmylite/tests/mysql_baseline_information_schema_innodb_foreign_expectations.sh
git diff --check
cmake --workflow --preset check
```
