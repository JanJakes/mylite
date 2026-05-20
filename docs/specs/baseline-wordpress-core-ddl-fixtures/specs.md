# Baseline WordPress Core DDL Fixtures

## Status

This phase adds focused compatibility coverage for representative WordPress
core table setup DDL. It is a fixture-integration slice over already specified
MyLite features rather than a new broad grammar surface. The goal is to prove
that the common WordPress bootstrap shapes work together:

- `BIGINT(20) UNSIGNED NOT NULL AUTO_INCREMENT` primary keys;
- `VARCHAR` empty-string defaults and integer defaults written as strings;
- `DATETIME NOT NULL DEFAULT '0000-00-00 00:00:00'` under an empty SQL mode;
- `LONGTEXT` nullable and `NOT NULL` columns;
- full and prefix secondary indexes, including `KEY meta_key (meta_key(191))`;
- `utf8mb4_unicode_520_ci` table and column metadata preservation;
- `SHOW COLUMNS`, `SHOW INDEX`, `SHOW CREATE TABLE`, and
  `INFORMATION_SCHEMA` readback for those descriptors.

The feature intentionally does not claim full WordPress compatibility. It only
locks down the DDL and simple row/default behavior needed before higher-level
WordPress query tests can reach their query assertions.

## Sources And Evidence

- MyLite architecture and engineering standards:
  - `README.md`
  - `AGENTS.md`
  - `docs/architecture/engineering-standards.md`
  - `third_party/sqlite/README.md`
- Existing MyLite feature specifications:
  - `docs/specs/baseline-schema-default-charset-collation/specs.md`
  - `docs/specs/baseline-table-charset-collation-surface/specs.md`
  - `docs/specs/baseline-column-charset-collation-attributes/specs.md`
  - `docs/specs/baseline-auto-increment-lifecycle/specs.md`
  - `docs/specs/baseline-zero-temporal-sql-modes/specs.md`
  - `docs/specs/baseline-varchar-type/specs.md`
  - `docs/specs/baseline-text-type/specs.md`
  - `docs/specs/baseline-secondary-index-lifecycle/specs.md`
  - `docs/specs/baseline-index-prefix-key-parts/specs.md`
  - `docs/specs/baseline-show-create-table/specs.md`
- Official MySQL 8.4 Reference Manual:
  - `CREATE TABLE`: <https://dev.mysql.com/doc/refman/8.4/en/create-table.html>
  - `AUTO_INCREMENT`: <https://dev.mysql.com/doc/refman/8.4/en/example-auto-increment.html>
  - `SHOW CREATE TABLE`: <https://dev.mysql.com/doc/refman/8.4/en/show-create-table.html>
  - Character sets and collations:
    <https://dev.mysql.com/doc/refman/8.4/en/charset.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_wordpress_core_ddl_fixtures_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, WordPress implementation code, or
other restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

The expectation script verifies this behavior on MySQL 8.4.9:

- A schema created with `DEFAULT CHARACTER SET utf8mb4 COLLATE
  utf8mb4_unicode_520_ci` selects that default for table and compatible string
  columns unless overridden.
- With `SET sql_mode = ''`, a `DATETIME NOT NULL DEFAULT
  '0000-00-00 00:00:00'` column is accepted and an omitted-column insert stores
  the full zero datetime without warnings.
- `BIGINT(20) UNSIGNED` renders as `bigint unsigned` in `SHOW CREATE TABLE`
  and `SHOW COLUMNS`; the deprecated display width is not preserved in MySQL
  8.4.9 metadata.
- `INT(11) NOT NULL DEFAULT '0'` renders as `int NOT NULL DEFAULT '0'`.
- `VARCHAR` columns inheriting `utf8mb4_unicode_520_ci` render explicit column
  collation clauses in `SHOW CREATE TABLE`.
- `LONGTEXT` columns inheriting the table collation render explicit collation
  clauses in `SHOW CREATE TABLE`.
- `PRIMARY KEY`, `UNIQUE KEY`, full secondary `KEY`, and prefix secondary
  `KEY meta_key (meta_key(191))` definitions are accepted and reflected in
  `SHOW INDEX` and `INFORMATION_SCHEMA.STATISTICS`.
- Simple omitted-default inserts into the covered `wp_users` shape produce one
  inserted row, `ROW_COUNT() = 1`, and `@@warning_count = 0`.
- Simple inserts into the covered `wp_options` and `wp_postmeta` shapes preserve
  generated auto-increment ids, string defaults, nullable long text values, and
  prefix-index metadata.

## Supported Scope

Supported fixture tables:

- `wp_users` with the columns and keys used by the test fixture:
  - unsigned auto-increment `BIGINT` primary key;
  - `VARCHAR` identity/contact columns with empty-string defaults;
  - `DATETIME NOT NULL` full-zero default under empty SQL mode;
  - integer status default written as a quoted string;
  - full-column secondary keys.
- `wp_options` with:
  - unsigned auto-increment `BIGINT` primary key;
  - `VARCHAR(191)` unique option name defaulting to an empty string;
  - `LONGTEXT NOT NULL` option value;
  - `VARCHAR(20)` autoload default;
  - one unique key and one nonunique key.
- `wp_postmeta` with:
  - unsigned auto-increment `BIGINT` primary key;
  - unsigned `BIGINT` post id default written as a quoted string;
  - nullable `VARCHAR(255)` meta key;
  - nullable `LONGTEXT` meta value;
  - full and prefix nonunique secondary keys.

The fixture uses only descriptor-owned persistent base tables in a MyLite
catalog schema. It relies on the existing `SET sql_mode = ''` behavior to admit
WordPress-style zero temporal defaults.

## Non-Goals

This phase does not add:

- a WordPress-specific parser mode or compatibility switch;
- generic application fixture loading;
- full WordPress schema coverage beyond the named representative tables;
- `TEXT`/`BLOB` full-column index support;
- full Unicode collation comparison or ordering;
- full SQL mode semantics beyond existing zero-temporal/default behavior;
- `dbDelta()` quirks, comments, engine-specific options beyond current
  `InnoDB`, or all MySQL table options;
- optimizer use of the declared secondary indexes;
- new public APIs;
- SQLite fork patches.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` remains the statement boundary and
  owns result-handle creation for fixture DDL, DML, and metadata queries.
- Statement context: owns diagnostics, warning count, affected rows, insert id,
  and row-count state. Successful fixture statements must leave MySQL-compatible
  row counts and zero warnings where MySQL 8.4.9 does.
- Lexer/parser/AST: no new grammar should be needed. The fixture is a composed
  acceptance test for existing `CREATE DATABASE`, `SET sql_mode`,
  `CREATE TABLE`, `INSERT`, `SHOW`, and `SELECT INFORMATION_SCHEMA` syntax.
- Analyzer/planner: continues to resolve schema/table/column/index names
  through MyLite descriptors and validates defaults, types, keys, and table
  options before any SQLite SQL is generated.
- Catalog module: remains authoritative for table, column, default,
  auto-increment, charset/collation, and key descriptors. Fixture execution must
  not derive behavior from SQLite schema text.
- SQLite physical storage: receives descriptor-generated physical table and
  index statements only. User-visible MySQL identifiers are mapped to stable
  MyLite physical names and quoted by the existing generation paths.
- Storage/VFS/file format: `.mylite` preamble bytes and shifted SQLite payload
  invariants must remain unchanged by fixture table creation and row writes.

## Grammar

No grammar expansion is intended. The fixture is admitted through the existing
independently authored grammar slices:

```sql
CREATE DATABASE schema_name DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_520_ci
USE schema_name
SET sql_mode = ''
CREATE TABLE table_name (column_definition_or_key_definition[, ...])
    DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_520_ci
INSERT INTO table_name [(column[, ...])] VALUES (value[, ...])
SHOW COLUMNS FROM table_name
SHOW INDEX FROM table_name
SHOW CREATE TABLE table_name
SELECT ... FROM INFORMATION_SCHEMA.COLUMNS|STATISTICS WHERE ...
```

The MyLite Lemon syntax snippets are inherited from the individual feature
specifications listed above. This feature does not introduce a new snippet.

## Semantics

- Schema resolution follows the existing selected-schema policy. The fixture
  selects the schema with `USE` before unqualified table creation.
- Table definitions are persistent base-table descriptors.
- Display-width integer attributes are accepted through the existing deprecated
  display-width path. The descriptor and rendered metadata follow the current
  MySQL 8.4.9 no-display-width shape.
- String columns inherit the table default `utf8mb4_unicode_520_ci` metadata
  and expose it through `SHOW CREATE TABLE` and `INFORMATION_SCHEMA.COLUMNS`.
  MyLite's existing ASCII comparison subset remains unchanged.
- Full-zero temporal defaults are accepted only under an SQL mode combination
  where the current zero-temporal feature admits them. This fixture explicitly
  sets `sql_mode = ''`.
- Auto-increment ids are generated by the existing descriptor-owned counter and
  are independent per table and per database file.
- Full and prefix keys are descriptor metadata plus generated SQLite physical
  indexes where current index slices provide them. Metadata comes from catalog
  descriptors, not SQLite introspection.
- Successful supported fixture setup statements return no row result set unless
  the statement is a metadata/query statement, preserve `warning_count == 0`
  for the verified paths, and report MySQL-compatible affected rows.

## Diagnostics

This feature does not add new diagnostics. It relies on the existing
feature-level diagnostics for:

- syntax errors and unsupported table-definition forms;
- unknown or missing schema/table/column/index names;
- unknown character sets or collations;
- invalid default values under the current SQL mode;
- unsupported key parts and unsupported text/blob full-column indexes;
- duplicate primary/unique keys;
- allocation and physical SQLite failures;
- public API misuse.

The runtime test should assert success for the covered fixture paths and should
avoid broadening the documented diagnostics surface without a separate feature
spec.

## Test Plan

Add one fast C runtime test and one MySQL expectation script.

The C test must cover:

- creating a file-backed database and schema with `utf8mb4_unicode_520_ci`;
- `SET sql_mode = ''`;
- creating `wp_users`, `wp_options`, and `wp_postmeta`;
- `SHOW COLUMNS`, `SHOW CREATE TABLE`, `SHOW INDEX`, and selected
  `INFORMATION_SCHEMA.COLUMNS` / `STATISTICS` rows for fixture descriptors;
- inserts that prove default values, auto-increment counters, nullable text,
  and zero datetime storage;
- close/reopen persistence;
- independent file-backed handles with independent auto-increment and row
  state;
- `.mylite` preamble preservation.

The MySQL expectation script must verify the same user-visible SQL behavior
against MySQL 8.4.9.

## Performance And SQLite Integration

The fixture should stay on the existing descriptor-driven optimal paths:

- DDL writes descriptor rows and generated physical SQLite table/index objects.
- Inserts use the existing prepared SQLite insert paths and descriptor-owned
  conversion/default handling.
- Metadata queries read MyLite descriptors and do not scan user rows except
  where MySQL-visible cardinality fields already do so in existing slices.

No row materialization beyond the existing test result comparisons is required.
No SQLite fork patch or new extension point is needed.
