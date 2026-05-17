# Baseline Composite Unique Indexes

## Summary

This phase expands MyLite's descriptor-owned unique-index support from
one-part unique keys to full-column composite unique keys on persistent base
tables. The goal is to support common MySQL schema definitions that use
`UNIQUE KEY (a, b)` or `CREATE UNIQUE INDEX ... (a, b)` while preserving the
current MyLite ownership model: descriptors define logical schema, generated
SQLite indexes enforce physical uniqueness, and MyLite owns compatibility
diagnostics, metadata, and value conversion.

The slice is deliberately narrower than MySQL. It does not add composite
unique prefix keys, primary prefix keys, descending key parts, functional key
parts, general named unique constraints, optimizer promises, or composite-key
`INSERT ... ON DUPLICATE KEY UPDATE`.

## Sources

- MySQL 8.4 Reference Manual, `CREATE TABLE` statement:
  https://dev.mysql.com/doc/refman/8.4/en/create-table.html
- MySQL 8.4 Reference Manual, `CREATE INDEX` statement:
  https://dev.mysql.com/doc/refman/8.4/en/create-index.html
- MySQL 8.4 Reference Manual, `SHOW INDEX` statement:
  https://dev.mysql.com/doc/refman/8.4/en/show-index.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.STATISTICS` table:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-statistics-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.TABLE_CONSTRAINTS` table:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-table-constraints-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.KEY_COLUMN_USAGE` table:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-key-column-usage-table.html
- Observed MySQL 8.4.9 runtime behavior recorded by
  `packages/libmylite/tests/mysql_baseline_composite_unique_indexes_expectations.sh`.

This specification is independently authored from the public documentation and
runtime observations above. It does not copy MySQL grammar, MySQL
implementation sources, MariaDB sources, Percona sources, or SQLite internals.

## MySQL 8.4.9 Observations

The expectation script for this phase records the behavior that shapes the
implementation. Relevant observed behavior:

- `UNIQUE KEY u_ab (a, b)` renders one `SHOW INDEX` and
  `INFORMATION_SCHEMA.STATISTICS` row per key part with `Seq_in_index` /
  `SEQ_IN_INDEX` starting at 1.
- A composite unique key permits duplicate rows when any key part is `NULL`.
  Duplicate fully populated tuples fail with `1062 / 23000`.
- Duplicate messages join key-part values with `-`, for example
  `Duplicate entry '1-2' for key 't.u_ab'`.
- `INSERT IGNORE` skips duplicate composite-key rows, reports only actually
  inserted rows in `ROW_COUNT()`, and stores warning `1062`.
- `CREATE UNIQUE INDEX` and `ALTER TABLE ... ADD UNIQUE` validate existing
  rows and fail with duplicate-entry diagnostics when non-`NULL` key tuples
  already repeat.
- `SHOW COLUMNS.Key` reports `PRI` for all parts of a single `NOT NULL`
  composite unique key when no primary key exists; nullable composite unique
  parts report `MUL` only when MySQL's key classification treats the column as
  a nonunique lookup candidate. MyLite keeps its existing limited
  descriptor-driven `SHOW COLUMNS` policy and documents that it is not full
  optimizer metadata parity.
- MySQL accepts composite unique prefix keys such as
  `UNIQUE KEY u_ab (a(3), b(4))`. MyLite defers that because prefix tuple
  uniqueness needs separate descriptor validation, duplicate-entry formatting,
  and DML conflict handling.
- MySQL accepts composite-unique `INSERT ... ON DUPLICATE KEY UPDATE`.
  MyLite defers that because the existing ODKU slice intentionally chooses at
  most one single-column conflict key.

## Ownership Boundaries

- Public API: no new public ABI. Applications continue to use
  `mylite_execute()` and the existing result and diagnostic accessors.
- Statement context: owns diagnostics, warnings, affected rows, and statement
  atomicity. Successful supported DDL reports zero affected rows and warning
  count through current non-row result conventions.
- Parser/AST: already admits ordered key-part lists for unique index
  definitions. The parser preserves syntax and source spans only; it does not
  inspect descriptors or SQLite schema.
- Analyzer/planner: resolves schema, table, index names, and key parts against
  MyLite descriptors. It rejects unsupported key shapes before generated
  SQLite SQL is produced.
- Catalog: `_mylite_catalog_indexes` and `_mylite_catalog_index_columns`
  remain authoritative for logical unique-key descriptors. Composite unique
  keys use one secondary index descriptor with `is_unique = 1` and one ordered
  index-column descriptor per key part.
- Result builders: `SHOW CREATE TABLE`, `SHOW INDEX`, `SHOW COLUMNS`, and
  limited `INFORMATION_SCHEMA` rows are rendered from descriptors, not from
  SQLite schema text or `PRAGMA` output.
- Storage/VFS: no `.mylite` preamble, file-format, or shifted-payload change.
- SQLite physical storage: generated ordinary SQLite unique indexes enforce
  admitted uniqueness over generated rowid tables. MyLite keeps MySQL-specific
  diagnostics and compatibility decisions outside the SQLite fork.

## Supported SQL Subset

Admitted persistent-base-table forms:

```sql
CREATE TABLE table_name (
  column_definition,
  ...,
  UNIQUE [KEY|INDEX] [index_name] (column_name, column_name[, ...])
)

CREATE UNIQUE INDEX index_name
  ON table_name (column_name, column_name[, ...])

ALTER TABLE table_name
  ADD UNIQUE [INDEX|KEY] [index_name] (column_name, column_name[, ...])
```

The supported key parts are unqualified full descriptor columns only. Supported
descriptor families match the existing full unique-key subset:

- integer-family descriptors, including unsigned forms;
- exact `DECIMAL` descriptors;
- canonical `YEAR`, `DATE`, `TIME`, `DATETIME`, and `TIMESTAMP` descriptors;
- ASCII-valued `CHAR(1..255)` and `VARCHAR(1..255)` descriptors using the
  current MyLite ASCII subset of `utf8mb4_0900_ai_ci` key comparison.

Existing one-part unique prefix indexes remain supported. Composite unique
prefix indexes are not part of this phase.

Deferred:

- multi-action `ALTER TABLE`;
- composite unique prefix keys;
- primary prefix keys;
- table-qualified key parts;
- descending key parts;
- expression or functional key parts;
- ordinal, invisible, parser-attribute, optioned, commented, fulltext,
  spatial, or multi-valued key parts;
- binary string, `BIT`, `ENUM`, `SET`, `JSON`, approximate numeric, and
  unsupported future descriptors as unique key parts;
- non-ASCII string key values and full Unicode collation parity;
- composite unique keys as parent keys for foreign keys;
- composite unique conflict handling in `INSERT ... ON DUPLICATE KEY UPDATE`;
- optimizer/index-use guarantees.

## MyLite Grammar Snippets

The parser already has the required key-list shape. The intended MyLite
Lemon-style grammar is:

```lemon
create_table_item ::= unique_index_definition.

unique_index_definition ::= UNIQUE unique_index_keyword_opt index_name_opt
                            LPAREN secondary_index_part_list RPAREN.

create_index_statement ::= CREATE UNIQUE INDEX identifier ON table_name
                           LPAREN secondary_index_part_list RPAREN.

alter_table_action ::= ADD UNIQUE unique_index_keyword_opt index_name_opt
                       LPAREN secondary_index_part_list RPAREN.

secondary_index_part_list ::= secondary_index_part.
secondary_index_part_list ::= secondary_index_part_list COMMA secondary_index_part.

secondary_index_part ::= identifier.
secondary_index_part ::= identifier LPAREN integer_literal RPAREN.
```

The semantic analyzer admits multi-part full-column unique keys and continues
to reject multi-part unique prefix keys. Prefix syntax stays in the grammar so
MyLite can produce deterministic unsupported or key-length diagnostics instead
of relying on generic parse failures.

## Name and Schema Resolution

Target table resolution follows existing policy:

- unqualified target names use the selected/default schema;
- schema-qualified target names use the explicit schema and do not require a
  selected default schema;
- missing default schema fails with `1046 / 3D000`;
- unknown schema fails with `1049 / 42000`;
- unknown table fails with `1146 / 42S02`;
- reserved logical `_mylite_*` schema, table, column, and index names are
  rejected before generated SQLite SQL is built.

Index names are table-local across primary, unique, and nonunique descriptors.
Explicit `PRIMARY` remains reserved for the primary key and fails with
`1280 / 42000`. Duplicate index names fail with `1061 / 42000`. Omitted
unique-index names derive from the first key part and use `_2`, `_3`, and later
suffixes through the current descriptor name-generation policy.

Column names resolve against MyLite descriptors in key-part order. Unknown
columns fail with `1072 / 42000`. Duplicate key-part columns fail with the
MySQL-compatible duplicate-column diagnostic already used by primary and
secondary index validation. Identifier comparison keeps the current catalog
case-sensitivity and collation policy; this phase does not add a new
identifier collation layer.

## Value and Uniqueness Semantics

Row values are converted by existing MyLite descriptor-owned conversion before
they reach SQLite:

- integer-family values bind as SQLite integers inside the currently supported
  physical ranges;
- exact decimals store descriptor-canonical text;
- temporal and `YEAR` values store canonical text;
- supported string key values store MyLite-canonical text and require the
  current ASCII-safe unique-key subset.

Composite uniqueness applies to the ordered key tuple. Multiple rows may share
the same key values when at least one key part is SQL `NULL`; no duplicate
probe or duplicate-entry diagnostic is produced for those rows. Fully populated
tuples must be distinct.

Generated SQLite unique indexes enforce the physical tuple. MyLite still maps
constraint failures and preflight validation failures to MySQL-shaped
diagnostics and must not expose SQLite table, column, or index names.

## Physical SQLite Handling

For an admitted composite unique index, MyLite generates one ordinary SQLite
unique index from descriptor data:

```sql
CREATE UNIQUE INDEX "_mylite_user_index_<index_id>"
ON "_mylite_user_table_<table_id>" (
  "physical_column_1",
  "physical_column_2"
)
```

For supported string key parts, the generated expression or column reference
uses the same MyLite collation annotation as existing single-column unique
keys and composite primary keys. Every generated identifier is quoted. Values
used for duplicate validation are bound through prepared statements or read
from descriptor-built physical expressions; user SQL literals are not
interpolated into generated SQLite SQL.

Existing-row validation for `CREATE UNIQUE INDEX` and
`ALTER TABLE ... ADD UNIQUE` must group by the full descriptor-built key tuple
and ignore rows where any key part is `NULL`. If a duplicate tuple exists,
MyLite returns `1062 / 23000` before mutating descriptors or physical indexes.

This phase uses public SQLite DDL and prepared-statement APIs only. It does not
require a SQLite fork patch.

## DML Semantics

Supported DML enforcement:

- `INSERT ... VALUES` and `INSERT ... SET` fail atomically on duplicate
  populated composite unique tuples.
- `INSERT IGNORE ... VALUES` and `INSERT IGNORE ... SET` demote duplicate
  composite unique tuple failures to warning `1062`, skip only the duplicate
  rows, and continue inserting nonconflicting rows.
- Single-table `UPDATE` fails atomically when the post-update row set would
  contain duplicate populated composite unique tuples. Updates that produce or
  preserve `NULL` in any key part remain allowed.
- Existing one-part unique and primary-key duplicate behavior remains
  unchanged.

Deferred DML behavior:

- `INSERT ... ON DUPLICATE KEY UPDATE` remains limited to the existing
  single-column conflict-key subset. If a statement would need a composite
  unique key for conflict detection or key selection, MyLite returns a
  deterministic unsupported diagnostic.
- Key-aware `REPLACE`, `LOAD DATA`, multi-table DML, generated columns,
  triggers, cascades, and privilege semantics remain outside this phase.

Duplicate-entry messages use logical table and index names and the observed
MySQL tuple rendering for supported values:

```text
Duplicate entry '1-2' for key 'table_name.index_name'
```

## Metadata Semantics

`SHOW CREATE TABLE` renders composite unique descriptors after primary-key
descriptors and with existing supported unique/nonunique index ordering:

```sql
UNIQUE KEY `u_ab` (`a`,`b`)
```

`SHOW INDEX` and `INFORMATION_SCHEMA.STATISTICS` render one row per key part.
`Non_unique` / `NON_UNIQUE` is `0`, `Seq_in_index` / `SEQ_IN_INDEX` starts at
1, `Sub_part` / `SUB_PART` is `NULL` for this full-column composite slice, and
the fixed baseline BTREE/statistics placeholders remain unchanged.

`INFORMATION_SCHEMA.TABLE_CONSTRAINTS` renders one `UNIQUE` row per supported
unique descriptor. `INFORMATION_SCHEMA.KEY_COLUMN_USAGE` renders one row per
unique key part with `ORDINAL_POSITION` matching the descriptor key-part order
and `POSITION_IN_UNIQUE_CONSTRAINT = NULL`.

`SHOW COLUMNS.Key` remains a limited descriptor-driven compatibility surface.
The implementation must not overclaim optimizer metadata parity. The docs must
state that composite unique parts are exposed according to the current
supported key-class policy and that full MySQL `PRI`/`UNI`/`MUL` precedence for
all overlapping index combinations remains future work.

## Diagnostics

The implementation must produce deterministic diagnostics for:

- syntax accepted by MySQL but outside this slice;
- missing default schema, unknown schema, unknown table, and reserved
  `_mylite_*` names;
- duplicate table-local index names;
- `PRIMARY` as a nonprimary index name;
- unknown key columns;
- duplicate key-part columns;
- unsupported descriptor type;
- `CHAR(0)` / `VARCHAR(0)` unique key parts and non-ASCII string key values;
- composite unique prefix key parts;
- descending, functional, table-qualified, or expression key parts;
- duplicate existing tuples during `CREATE UNIQUE INDEX` and
  `ALTER TABLE ... ADD UNIQUE`;
- duplicate tuples during `INSERT`, `INSERT IGNORE`, and `UPDATE`;
- allocation failures and physical SQLite failures.

Successful supported composite unique DDL and DML must report
`warning_count == 0` unless the statement already has a separately specified
warning behavior such as `INSERT IGNORE` duplicate demotion.

## Storage, File Format, and Performance

No catalog schema migration is expected: the existing index descriptor model
already stores ordered key parts. Successful DDL increments the same descriptor
generations and catalog cache state as existing index DDL. Failed DDL must
leave no catalog rows and no generated physical index.

The `.mylite` preamble remains untouched. All table rows, catalog rows, and
generated physical indexes live in the shifted SQLite payload.

Performance should stay close to SQLite's native path:

- normal reads and writes continue to use generated SQLite tables and indexes;
- uniqueness enforcement is performed by SQLite unique indexes on supported
  physical tuples;
- existing-row validation for add/create unique indexes uses one grouped SQL
  scan instead of materializing all rows in MyLite memory;
- duplicate-entry text may read the first duplicate tuple for diagnostics, but
  must not require loading the full table into memory.

## Test Plan

Add MySQL-runtime expectation coverage and fast C tests for:

- `CREATE TABLE` composite unique keys over supported full key parts;
- standalone `CREATE UNIQUE INDEX` over multiple full key parts;
- `ALTER TABLE ... ADD UNIQUE` over multiple full key parts;
- schema-qualified and unqualified target resolution;
- omitted and explicit unique-index names;
- `SHOW CREATE TABLE`, `SHOW COLUMNS`, `SHOW INDEX`,
  `INFORMATION_SCHEMA.STATISTICS`, `TABLE_CONSTRAINTS`, and
  `KEY_COLUMN_USAGE`;
- `INSERT`, `INSERT IGNORE`, and single-table `UPDATE` duplicate enforcement;
- duplicate `NULL` tuple behavior;
- existing-row duplicate validation for add/create unique indexes;
- tuple duplicate diagnostics and warning storage;
- reopen persistence, `CREATE TABLE ... LIKE`, index drop, table rename/drop,
  and independent file-backed handles;
- unsupported composite unique prefix keys, duplicate key parts, unsupported
  descriptor types, table-qualified key parts, descending key parts, expression
  key parts, multi-action `ALTER`, and composite-unique ODKU limitations;
- `.mylite` preamble preservation and zero-initialized cleanup for any new
  planner/runtime helpers.

## Compatibility Documentation

Update `COMPATIBILITY.md` and detailed compatibility docs only for the
implemented subset. The docs must explicitly avoid claiming full unique-index
support, composite unique prefix indexes, named unique constraints, optimizer
behavior, composite unique foreign-key parents, or composite-key ODKU support.
