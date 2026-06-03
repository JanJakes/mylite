# Baseline Unique Index Lifecycle

## Summary

This phase adds the next key/constraint building block for persistent MyLite
base tables: descriptor-owned, single-column unique indexes declared in
`CREATE TABLE`, with physical SQLite unique indexes, MySQL-compatible duplicate
diagnostics for the admitted DML paths, and descriptor-driven metadata through
`SHOW COLUMNS`, `SHOW CREATE TABLE`, `SHOW INDEX`, and limited
`INFORMATION_SCHEMA.STATISTICS`.

The slice is intentionally narrower than MySQL's full unique-index surface. It
does not add standalone `CREATE UNIQUE INDEX`, `ALTER TABLE ADD/DROP/RENAME
INDEX`, composite unique indexes, prefix unique indexes, string-family unique
indexes, named constraints, foreign keys, or `ON DUPLICATE KEY UPDATE`.

## Sources

- MySQL 8.4 Reference Manual, `CREATE TABLE` statement:
  https://dev.mysql.com/doc/refman/8.4/en/create-table.html
- MySQL 8.4 Reference Manual, `SHOW INDEX` statement:
  https://dev.mysql.com/doc/refman/8.4/en/show-index.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.STATISTICS` table:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-statistics-table.html
- MySQL 8.4 Reference Manual, `CREATE INDEX` statement:
  https://dev.mysql.com/doc/refman/8.4/en/create-index.html
- Observed MySQL 8.4.9 runtime behavior recorded by
  `packages/libmylite/tests/mysql_baseline_unique_index_lifecycle_expectations.sh`.

This specification is independently authored from the public documentation and
runtime observations above. It does not copy MySQL grammar or implementation
sources.

## Ownership Boundaries

- Public API: no new public ABI. Applications use existing `mylite_execute()`
  and result/diagnostic accessors.
- Statement context: preserves SQL text, diagnostics, warning counts, affected
  rows, statement atomicity, and cleanup behavior.
- Parser/AST: admits the narrow `CREATE TABLE` unique-index syntax and inline
  unique column attribute syntax. Unsupported unique forms either remain syntax
  errors or reach deterministic runtime unsupported diagnostics.
- Analyzer/planner: resolves unique index names and columns from MyLite
  descriptors, validates supported type/nullability interactions, and rejects
  unsupported key forms before generating physical SQL.
- Catalog module: MyLite descriptors remain authoritative. Unique indexes use
  the existing index descriptor shape: `kind = SECONDARY`, `is_unique = 1`, one
  index-column descriptor, and stable generated physical index names.
- Result builder/metadata: renders MySQL-style metadata from descriptors, not
  SQLite schema text.
- Storage/VFS: no `.mylite` format change and no VFS change. The shifted SQLite
  payload remains behind the MyLite preamble.
- SQLite physical storage: stores rows in generated MyLite rowid tables and
  enforces admitted uniqueness with ordinary SQLite unique indexes created from
  descriptor data.

## Supported SQL Subset

Admitted `CREATE TABLE` unique forms:

- persistent base tables only;
- inline `column_name column_type ... UNIQUE`;
- inline `column_name column_type ... UNIQUE KEY`;
- table-level `UNIQUE (column_name)`;
- table-level `UNIQUE KEY [index_name] (column_name)`;
- table-level `UNIQUE INDEX [index_name] (column_name)`;
- one unqualified key column per unique index;
- optional index name using the existing identifier rules;
- supported unique target descriptors:
  - integer-family columns already admitted by the primary-key and row-value
    phases;
  - exact `DECIMAL` descriptors with canonical MyLite text storage;
  - canonical `DATE` descriptors with canonical MyLite text storage;
- nullable and `NOT NULL` target columns;
- coexistence with the existing single-column primary key and nonunique
  secondary-index subset;
- `CREATE TABLE ... LIKE` descriptor cloning;
- `DROP TABLE`, `RENAME TABLE`, `TRUNCATE`, row DML, and reopen persistence.

Deferred:

- standalone `CREATE UNIQUE INDEX`;
- `ALTER TABLE ... ADD UNIQUE` in this phase; see
  `docs/specs/baseline-alter-table-add-unique-lifecycle/specs.md` for the
  later limited supported subset;
- `DROP INDEX`, `DROP KEY`, `RENAME INDEX`, or visibility changes in this
  phase;
- named `CONSTRAINT symbol UNIQUE ...`;
- composite indexes;
- prefix lengths;
- descending key parts;
- expression or functional key parts;
- table-qualified key parts;
- unique indexes on `CHAR`, `VARCHAR`, `TEXT`, or future binary/string
  descriptors until collation-aware uniqueness is implemented;
- unique indexes on unsupported column types;
- unique `AUTO_INCREMENT` columns that are not the current primary-key subset;
- `UNIQUE CHECK`, `FULLTEXT`, `SPATIAL`, parser options, comments, visibility,
  storage-engine options, generated columns, foreign keys, and privileges;
- `_rowid` alias behavior for unique non-null integer indexes;
- optimizer/index-use guarantees.

## MyLite Grammar Snippets

The intended MyLite Lemon-style grammar extension is:

```lemon
create_table_item ::= unique_index_definition.

unique_index_definition ::= UNIQUE unique_index_keyword_opt index_name_opt
                            LPAREN unique_index_part_list RPAREN.

unique_index_keyword_opt ::= .
unique_index_keyword_opt ::= KEY.
unique_index_keyword_opt ::= INDEX.

unique_index_part_list ::= unique_index_part.
unique_index_part_list ::= unique_index_part_list COMMA unique_index_part.

unique_index_part ::= identifier.

column_attribute ::= UNIQUE.
column_attribute ::= UNIQUE KEY.
```

The parser may use dedicated AST node kinds or a shared index-definition node as
long as the AST preserves uniqueness independently from physical SQLite text.
The part-list grammar deliberately admits more than one part so the analyzer can
return MyLite's deterministic "one key column" diagnostic instead of depending
on a generic parser error. Prefix lengths and direction tokens are not admitted
by this slice.

## Descriptor and Name Semantics

Unique indexes are stored as index descriptors with `is_unique = 1`. The
existing primary key remains `kind = PRIMARY`, `is_unique = 1`; nonunique
indexes remain `kind = SECONDARY`, `is_unique = 0`. This phase may represent
unique indexes as `kind = SECONDARY`, `is_unique = 1` to avoid a catalog schema
migration because uniqueness is already a descriptor column.

Name rules:

- explicit `PRIMARY` remains reserved for primary-key descriptors and fails
  with MySQL-compatible `1280 / 42000`;
- duplicate index names share a table-local index namespace across primary,
  unique, and nonunique indexes and fail with `1061 / 42000`;
- unnamed unique indexes derive the name from the target column and use MySQL's
  `_2`, `_3`, ... suffix shape to avoid collisions;
- generated physical names remain `_mylite_user_index_<index_id>` and are never
  exposed to users.

Column resolution uses MyLite descriptors. Unknown key columns fail with
MySQL-compatible `1072 / 42000`. Reserved `_mylite_*` logical names are rejected
by the existing table/column policy before any physical SQL is generated.

Current descriptor name comparison stays bytewise/case-insensitive only where
the surrounding catalog code already does so. This phase does not introduce
collation-aware identifier or value comparison.

## Uniqueness Semantics

For admitted target types, values are already canonicalized before binding to
SQLite:

- integers bind as SQLite integers after MyLite range conversion;
- `DECIMAL` values bind as descriptor-canonical fixed-scale text after MyLite
  decimal conversion, rounding notes, and range checks;
- `DATE` values bind as canonical `YYYY-MM-DD` text after MyLite date checks.

The physical SQLite unique index enforces uniqueness over these canonical
stored values. SQLite and MySQL both permit multiple `NULL` values in a unique
index, so no special duplicate-`NULL` emulation is needed for this slice.

String-family unique indexes are deferred because MySQL's default
`utf8mb4_0900_ai_ci` collation makes many text values compare equal that would
not compare equal under SQLite's default binary text uniqueness. MyLite must not
claim string unique-key compatibility until collation-aware storage or index
comparison exists.

## DML Semantics

Supported duplicate behavior:

- `INSERT ... VALUES` and `INSERT ... SET` into a table with admitted unique
  descriptors fail atomically on duplicate non-`NULL` unique values with
  MySQL-compatible `1062 / 23000` diagnostics.
- `INSERT IGNORE ... VALUES` and `INSERT IGNORE ... SET` demote duplicate
  unique-key rows to warning `1062`, skip the duplicate row, keep inserting
  nonconflicting rows, and report affected rows for rows actually inserted.
- Single-assignment `UPDATE` fails atomically on duplicate non-`NULL` unique
  values with `1062 / 23000`.
- Updating or inserting `NULL` in a nullable unique column remains allowed even
  when other rows already contain `NULL`.
- Existing row-value conversion, defaults, nullability checks, range checks,
  and warnings run before unique enforcement.

Deferred DML behavior:

- `INSERT ... SELECT` into unique-key targets remains rejected as a key-bearing
  target until duplicate handling for source result streams is specified.
- `REPLACE` into unique-key targets remains rejected until delete-insert
  semantics are implemented for descriptor keys.
- `ON DUPLICATE KEY UPDATE` remains unsupported.
- Unique-key duplicate detection does not change SELECT, WHERE, ORDER BY, GROUP
  BY, or optimizer behavior.

Duplicate diagnostics use the logical table and index name in the MySQL 8.4.9
shape observed by runtime probes, for example:

```text
Duplicate entry '10' for key 'table_name.index_name'
```

## Metadata Semantics

`SHOW CREATE TABLE` renders descriptor columns first, then primary key if
present, unique indexes, and nonunique indexes. MySQL 8.4.9 orders a
single-column `NOT NULL` unique index before nullable unique indexes in the
tested subset; MyLite should match this visible order for admitted indexes
without overclaiming full optimizer ordering semantics.

`SHOW INDEX` and `INFORMATION_SCHEMA.STATISTICS` render one row per admitted
single-column index:

- `Non_unique` / `NON_UNIQUE`: `0` for primary and unique indexes, `1` for
  nonunique indexes;
- `Key_name` / `INDEX_NAME`: descriptor logical name;
- `Seq_in_index`: `1`;
- `Column_name` / `COLUMN_NAME`: descriptor column name;
- `Collation`: `A`;
- `Cardinality`: current deterministic placeholder `0` for `SHOW INDEX`;
- `Sub_part`, `Packed`, and `Expression`: `NULL`;
- `Null` / `NULLABLE`: `YES` for nullable target columns, empty string for
  `NOT NULL` target columns;
- `Index_type`: `BTREE`;
- `Comment`, `Index_comment`: empty string;
- `Visible` / `IS_VISIBLE`: `YES`.

`SHOW COLUMNS` and `INFORMATION_SCHEMA.COLUMNS.COLUMN_KEY` use:

- `PRI` for the primary-key column;
- if no primary key exists, `PRI` for the column of the first admitted
  single-column unique index whose target column is `NOT NULL`, matching
  observed MySQL 8.4.9 behavior for this subset;
- otherwise `UNI` for admitted unique-index columns;
- otherwise `MUL` for admitted nonunique-index columns;
- empty string for non-key columns.

This phase does not add `INFORMATION_SCHEMA.KEY_COLUMN_USAGE` or
`TABLE_CONSTRAINTS`; those are a separate metadata slice.

## Physical SQLite Handling

For each admitted unique index, MyLite generates a standard SQLite unique index:

```sql
CREATE UNIQUE INDEX "_mylite_user_index_<index_id>"
ON "_mylite_user_table_<table_id>" ("physical_column_name");
```

Rules:

- quote every generated identifier;
- generate SQL only from MyLite descriptors and stable physical names;
- create physical unique indexes after the physical table exists and before
  committing the catalog mutation;
- on failure, roll back the catalog mutation and drop any physical artifacts
  already produced by the create path;
- use public SQLite index support; no SQLite fork patch is required;
- preserve the `.mylite` preamble and shifted SQLite payload invariants.

Existing physical table-rebuild `ALTER TABLE` paths already reject secondary
index descriptors after the secondary-index phase; unique indexes use the same
protection until rebuild-time physical index recreation is designed.

## Diagnostics

MySQL-compatible diagnostics for the admitted subset:

- duplicate key name: `1061 / 42000`;
- duplicate unique value: `1062 / 23000`;
- unknown key column: `1072 / 42000`;
- BLOB-family key without prefix: `1170 / 42000`; full-column `TEXT` family
  unique keys are accepted by the documented WordPress bridge and enforce
  full-value uniqueness;
- explicit `PRIMARY` nonprimary index name: `1280 / 42000`.

MyLite-specific deterministic unsupported diagnostics:

- composite unique indexes;
- qualified unique key parts;
- string-family unique indexes until collation-aware uniqueness exists;
- unique key on unsupported descriptor types;
- inline/table unique `AUTO_INCREMENT` outside the current primary-key
  auto-increment subset;
- named constraints, prefix lengths, descending parts, expressions, standalone
  create/drop/alter unique index statements, and other deferred syntax when the
  parser admits enough structure to report a feature-specific error.

Physical SQLite failures, catalog failures, allocation failures, and public API
misuse use existing runtime paths.

## Tests

Add or extend fast C tests under `packages/libmylite/tests/`, preferably with a
new `runtime_unique_index_lifecycle` binary and dotted CTest name
`libmylite.runtime.unique_index_lifecycle`.

Coverage:

- parser acceptance for inline `UNIQUE`, inline `UNIQUE KEY`, table-level
  `UNIQUE`, `UNIQUE KEY`, and `UNIQUE INDEX`;
- parser/runtime rejection for composite, prefix, descending, expression,
  qualified, named constraint, string-family, and unsupported-type forms;
- successful unique indexes on integer-family, exact `DECIMAL`, and canonical
  `DATE` columns;
- nullable unique columns permitting multiple `NULL` values;
- duplicate insert and update errors with `1062 / 23000`;
- `INSERT IGNORE` warning demotion and affected rows;
- coexistence with primary keys and nonunique indexes;
- default unique names and suffixes;
- `SHOW COLUMNS`, `SHOW CREATE TABLE`, `SHOW INDEX`, and
  `INFORMATION_SCHEMA.STATISTICS` metadata;
- `CREATE TABLE ... LIKE` cloning and `CREATE TABLE ... SELECT` omission;
- key-bearing target rejection for `INSERT ... SELECT` and `REPLACE`;
- reopen persistence, rename, drop, truncate, independent handles, and preamble
  preservation;
- rebuild `ALTER TABLE` rejection for unique-index tables;
- zero-initialized cleanup for new AST/planner objects;
- existing parser/runtime/catalog/DDL/DML/introspection tests still pass.

The MySQL 8.4.9 expected behavior for this phase is recorded in
`packages/libmylite/tests/mysql_baseline_unique_index_lifecycle_expectations.sh`.

## Compatibility Documentation

When implemented, update:

- `COMPATIBILITY.md` rows for `CREATE TABLE`, unique indexes, nonunique
  indexes if wording changes, `INSERT`, `INSERT IGNORE`, `UPDATE`, `SHOW
  COLUMNS`, `SHOW CREATE TABLE`, `SHOW INDEX`, and
  `INFORMATION_SCHEMA.STATISTICS`;
- `docs/compatibility/sql-indexes-constraints.md` for limited unique indexes;
- `docs/compatibility/sql-table-ddl.md` for admitted unique DDL and continued
  standalone/ALTER gaps;
- `docs/compatibility/sql-table-dml.md` for duplicate unique-key DML effects;
- `docs/compatibility/sql-show-statements.md` and
  `docs/compatibility/metadata-information-schema.md` for metadata rendering;
- `docs/compatibility/type-system-literals-conversion.md` only to state exact
  integer/decimal/date unique-key support and string-family deferral.

Do not claim full unique constraints, string collation-aware uniqueness,
composite keys, prefix lengths, expression keys, standalone index DDL, ALTER
index DDL, foreign keys, `ON DUPLICATE KEY UPDATE`, `REPLACE` duplicate-key
semantics, optimizer guarantees, or complete information-schema constraint
metadata.
