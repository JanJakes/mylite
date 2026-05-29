# Baseline INFORMATION_SCHEMA InnoDB Virtual

## Status

This phase adds a limited descriptor-backed
`INFORMATION_SCHEMA.INNODB_VIRTUAL` system view. The view exposes
MySQL 8.4.9-shaped table and column metadata and emits virtual generated-column
dependency rows for MyLite persistent base-table descriptors that already use
the baseline generated-column expression subset.

The slice is metadata-only. It does not add physical InnoDB dictionary storage,
privilege filtering, rows for generated columns outside MyLite's current
generated-expression descriptor subset, generated-column indexes, or broader
generated-column expression support.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing generated-column lifecycle specification:
  `docs/specs/baseline-generated-column-lifecycle/specs.md`
- Existing descriptor-backed `INFORMATION_SCHEMA.INNODB_COLUMNS` and
  `INFORMATION_SCHEMA.INNODB_TABLES` code paths in
  `packages/libmylite/src/runtime/mylite_execution.c`
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.INNODB_VIRTUAL`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-innodb-virtual-table.html
- MySQL 8.4 Reference Manual, generated columns:
  https://dev.mysql.com/doc/refman/8.4/en/create-table-generated-columns.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, and
existing MyLite source code. It does not copy MySQL, MariaDB, Percona, SQLite
implementation internals, or other restrictively licensed implementation
sources.

## MySQL 8.4.9 Observations

Runtime probes were run against the local `mysql:8.4.9` Docker runtime named
`mylite-mysql-849`.

Observed behavior shaping this slice:

- `INFORMATION_SCHEMA.INNODB_VIRTUAL` exists as an `information_schema`
  `SYSTEM VIEW`.
- `INFORMATION_SCHEMA.TABLES` reports `ENGINE = NULL`, `VERSION = 10`,
  `ROW_FORMAT = NULL`, `TABLE_ROWS = 0`, `DATA_LENGTH = 0`, and
  `AUTO_INCREMENT = NULL`.
- `INFORMATION_SCHEMA.COLUMNS` reports three non-null columns in order:
  `TABLE_ID` as `bigint unsigned`, `POS` as `int unsigned`, and `BASE_POS` as
  `int unsigned`. The columns report empty-string defaults and SQL `NULL` for
  character, numeric precision, numeric scale, and datetime precision metadata.
- A virtual generated column with base-column references emits one row per
  distinct referenced base column. Stored generated columns do not emit rows.
- A virtual generated column whose expression is constant or `NULL` does not
  emit a row, although it remains visible through `INNODB_COLUMNS`.
- `POS` is `((n + 1) << 16) + ordinal`, where `n` is the zero-based ordinal of
  the virtual generated column among virtual generated columns in that table,
  including constant virtual columns that emit no dependency rows. `ordinal` is
  the zero-based column position of the virtual generated column.
- The observed MySQL 8.4.9 runtime resets `n` per table. This matches the
  runtime probes, even though the reference-manual wording describes the
  sequence more broadly.
- `BASE_POS` is the zero-based position of each distinct base column referenced
  by the virtual generated expression. Duplicate references are collapsed, and
  rows are returned in ascending `BASE_POS` order for a given `POS`.
- Successful reads leave `@@warning_count = 0`, and `ROW_COUNT()` reports
  `-1`.

Representative probe:

```sh
docker exec -i mylite-mysql-849 mysql --protocol=TCP -h127.0.0.1 -uroot \
  --batch --raw --skip-column-names <<'SQL'
DROP DATABASE IF EXISTS mylite_innodb_virtual_probe;
CREATE DATABASE mylite_innodb_virtual_probe;
USE mylite_innodb_virtual_probe;
CREATE TABLE generated_values (
  id INT PRIMARY KEY,
  a INT,
  b INT,
  c INT GENERATED ALWAYS AS (a + b) VIRTUAL,
  d INT GENERATED ALWAYS AS (a * 2) STORED,
  e INT GENERATED ALWAYS AS (5) VIRTUAL,
  f INT GENERATED ALWAYS AS (a) VIRTUAL,
  g INT GENERATED ALWAYS AS (NULL) VIRTUAL,
  h INT GENERATED ALWAYS AS (-a + b) VIRTUAL
) ENGINE=InnoDB;
SELECT v.TABLE_ID, t.NAME, v.POS, v.BASE_POS
  FROM INFORMATION_SCHEMA.INNODB_VIRTUAL AS v
  JOIN INFORMATION_SCHEMA.INNODB_TABLES AS t ON t.TABLE_ID = v.TABLE_ID
 WHERE t.NAME = 'mylite_innodb_virtual_probe/generated_values'
 ORDER BY v.POS, v.BASE_POS;
SELECT @@warning_count, ROW_COUNT();
DROP DATABASE mylite_innodb_virtual_probe;
SQL
```

The row probe returned `POS` values `65539`, `196614`, and `327688` for
zero-based generated-column positions `3`, `6`, and `8`, with base positions
`1`, `2`, `1`, `1`, and `2` respectively.

## Scope

Supported:

- `SELECT` from `INFORMATION_SCHEMA.INNODB_VIRTUAL` using the existing
  information-schema query subset;
- case-insensitive information-schema table name lookup;
- table aliases, predicates, ordering, and `COUNT(*)` through the existing
  metadata query path;
- unqualified `INNODB_VIRTUAL` reads while `information_schema` is the
  selected schema;
- one row per distinct base-column dependency for each virtual generated column
  in persistent MyLite base-table descriptors;
- `TABLE_ID` derived from the MyLite table descriptor id;
- `POS` derived from the MySQL-observed virtual generated-column ordinal and
  zero-based column position encoding;
- `BASE_POS` derived from each referenced base column's zero-based
  descriptor position;
- descriptor changes from supported DDL and close/reopen persistence are
  reflected in subsequent reads;
- system metadata through `INFORMATION_SCHEMA.TABLES`,
  `INFORMATION_SCHEMA.COLUMNS`, `SHOW TABLES`, `SHOW FULL TABLES`, and
  `SHOW TABLE STATUS` via the existing built-in table directory.

Out of scope:

- exact MySQL physical `TABLE_ID` values beyond MyLite descriptor ids;
- hidden InnoDB system columns, temporary tables, views, built-in-schema rows,
  privilege checks, and account-specific filtering;
- rows for stored generated columns or constant virtual generated columns;
- generated-column expressions beyond MyLite's current canonical descriptor
  expression subset;
- generated-column dependency rows involving generated-column references,
  functions, JSON paths, spatial expressions, collations, SQL-mode-sensitive
  expressions, or virtual generated columns in indexes;
- SQLite storage, VFS, extension, or fork changes.

## Ownership Boundary

- Public API: unchanged. Applications continue through `mylite_execute()` and
  current result accessors.
- Parser/AST: unchanged. The existing information-schema `SELECT` path already
  resolves table names, aliases, identifiers, predicates, and ordering.
- Analyzer/runtime: recognizes `INNODB_VIRTUAL` as a supported
  information-schema system view and emits rows from loaded catalog
  descriptors.
- Catalog metadata: unchanged. Existing table and column descriptors are
  authoritative. Dependency discovery uses MyLite's canonical persisted
  generated-expression text, which is rendered from validated AST nodes during
  DDL and quotes every base-column reference as a MySQL identifier.
- Storage/SQLite: unchanged. No physical SQLite table, view, extension, or
  fork patch is required.

## Syntax

No new SQL grammar is added. The feature uses the existing admitted
information-schema `SELECT` grammar.

Examples in scope:

```sql
SELECT * FROM INFORMATION_SCHEMA.INNODB_VIRTUAL;
SELECT TABLE_ID, POS, BASE_POS
  FROM INFORMATION_SCHEMA.INNODB_VIRTUAL
 WHERE TABLE_ID = 1
 ORDER BY POS, BASE_POS;
SELECT COUNT(*) FROM INNODB_VIRTUAL WHERE BASE_POS = 1;
```

## Runtime Semantics

`INNODB_VIRTUAL` is registered in the static information-schema table registry.
Row production is descriptor-backed:

- system rows for `TABLES` and `COLUMNS` are generated from static
  descriptors;
- direct reads iterate persistent catalog schemas and base tables;
- non-virtual and stored generated columns are skipped;
- a per-table virtual-generated-column counter starts at zero and increments
  for every virtual generated column descriptor, including constant virtual
  columns that emit no dependency rows;
- `POS` is encoded as `((virtual_counter + 1) << 16) + column_position`, using
  zero-based descriptor positions;
- generated-expression dependencies are discovered from the canonical
  descriptor expression text by collecting distinct MySQL-quoted identifiers
  and matching them to table columns by case-insensitive descriptor name;
- only non-generated base-column matches emit dependency rows;
- dependency rows for a virtual generated column are emitted once per base
  column in ascending zero-based base-column position;
- successful reads introduce no warnings;
- `ROW_COUNT()` after a successful `SELECT` remains the existing query value
  `-1`.

## Diagnostics

The feature relies on existing information-schema diagnostics:

- unknown selected columns fail with the current unknown-column diagnostic;
- unsupported expressions, joins, grouping, predicates, and limits retain the
  current information-schema query subset behavior;
- invalid catalog descriptors, invalid generated-expression descriptor text, or
  arithmetic overflow in `POS` encoding fail with runtime diagnostics;
- allocation failures use existing MyLite runtime diagnostics.

Successful reads introduce no warnings.

## Tests

Add a focused C runtime test and a MySQL expectation script. Coverage must
include:

- `SHOW FULL TABLES` and `INFORMATION_SCHEMA.TABLES` metadata for the system
  view;
- `INFORMATION_SCHEMA.COLUMNS` metadata for all three columns;
- virtual generated-column dependency rows for expressions with one base
  column, two base columns, duplicate base references, and reverse expression
  order;
- omission of stored generated columns, constant virtual generated columns, and
  `NULL` virtual generated columns;
- per-table reset of the virtual-generated-column ordinal;
- `COUNT(*)`, case-insensitive table lookup, aliases, predicates, and
  unqualified reads after `USE information_schema`;
- close/reopen persistence;
- `@@warning_count` and `ROW_COUNT()` status after a successful read.

Verification before commit:

```sh
cmake --build --preset dev --target mylite_runtime_information_schema_innodb_virtual_test
ctest --preset dev -R '^libmylite\.runtime\.(information_schema_innodb_virtual|information_schema_innodb_columns|generated_column_lifecycle|information_schema_static_catalogs|builtin_schema_table_directory)$' --output-on-failure
packages/libmylite/tests/mysql_baseline_information_schema_innodb_virtual_expectations.sh
git diff --check
cmake --workflow --preset check
```
