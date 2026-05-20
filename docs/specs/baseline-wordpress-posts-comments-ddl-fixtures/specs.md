# Baseline WordPress Posts And Comments DDL Fixtures

## Status

This phase extends the existing WordPress core DDL fixture coverage with
representative `wp_posts` and `wp_comments` table setup shapes. It is a
fixture-integration slice over existing MyLite DDL, DML, metadata, default,
charset/collation, auto-increment, temporal, text, and index behavior. It does
not introduce a new SQL grammar feature.

The goal is to prove that common WordPress bootstrap tables beyond users,
options, and postmeta are accepted together:

- unsigned `BIGINT` auto-increment primary keys;
- quoted integer defaults on integer-family columns;
- zero `DATETIME` defaults under an empty SQL mode;
- `TINYTEXT`, `TEXT`, and `LONGTEXT` `NOT NULL` columns;
- inherited `utf8mb4_unicode_520_ci` metadata on text and varchar columns;
- prefix indexes such as `post_name(191)` and `comment_author_email(10)`;
- composite secondary indexes mixing string, temporal, and integer columns;
- descriptor-driven `SHOW COLUMNS`, `SHOW INDEX`, `SHOW CREATE TABLE`, and
  `INFORMATION_SCHEMA` readback.

This remains fixture coverage, not full WordPress compatibility. It locks down
the common table-creation and simple row/default behavior needed before broader
WordPress query-suite failures become meaningful query failures.

## Sources And Evidence

- MyLite architecture and standards:
  - `README.md`
  - `AGENTS.md`
  - `docs/architecture/engineering-standards.md`
  - `third_party/sqlite/README.md`
- Existing MyLite feature specifications:
  - `docs/specs/baseline-wordpress-core-ddl-fixtures/specs.md`
  - `docs/specs/baseline-zero-temporal-sql-modes/specs.md`
  - `docs/specs/baseline-auto-increment-lifecycle/specs.md`
  - `docs/specs/baseline-text-type/specs.md`
  - `docs/specs/baseline-index-prefix-key-parts/specs.md`
  - `docs/specs/baseline-show-create-table/specs.md`
- Official MySQL 8.4 Reference Manual:
  - `CREATE TABLE`: <https://dev.mysql.com/doc/refman/8.4/en/create-table.html>
  - `SHOW CREATE TABLE`:
    <https://dev.mysql.com/doc/refman/8.4/en/show-create-table.html>
  - `SHOW INDEX`: <https://dev.mysql.com/doc/refman/8.4/en/show-index.html>
  - `INFORMATION_SCHEMA.STATISTICS`:
    <https://dev.mysql.com/doc/refman/8.4/en/information-schema-statistics-table.html>
- MySQL runtime probes are captured in the updated
  `packages/libmylite/tests/mysql_baseline_wordpress_core_ddl_fixtures_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, WordPress implementation code, or
other restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

The expectation script verifies this behavior on MySQL 8.4.9:

- With `SET sql_mode = ''`, the representative `wp_posts` and `wp_comments`
  definitions succeed.
- Each fixture table emits five deprecated integer display-width warnings for
  the `BIGINT(20)` and `INT(11)` declarations.
- MySQL 8.4.9 metadata normalizes those declarations to `bigint`,
  `bigint unsigned`, or `int` without preserving display widths.
- `TEXT` family `NOT NULL` columns without explicit defaults display
  `Default = NULL` in `SHOW COLUMNS` and `COLUMN_DEFAULT = NULL` in
  `INFORMATION_SCHEMA.COLUMNS`.
- String and text columns inheriting `utf8mb4_unicode_520_ci` render explicit
  column `COLLATE` clauses in `SHOW CREATE TABLE` and expose the collation in
  `INFORMATION_SCHEMA.COLUMNS`.
- Prefix and composite secondary indexes are accepted and reported through
  `SHOW INDEX` and `INFORMATION_SCHEMA.STATISTICS`.
- `SHOW COLUMNS.Key` and `INFORMATION_SCHEMA.COLUMNS.COLUMN_KEY` mark only the
  leftmost column of a composite secondary key unless another key makes a later
  column independently visible, matching the probed `wp_posts` shape.
- Simple inserts that provide the required text values and omit covered default
  columns generate auto-increment ids, store zero datetimes, materialize string
  and integer defaults, and report `ROW_COUNT() = 1` with
  `@@warning_count = 0`.

## Supported Scope

Supported fixture additions:

- `wp_posts` with:
  - unsigned auto-increment `BIGINT` primary key;
  - unsigned integer ownership/parent defaults written as quoted strings;
  - zero `DATETIME` defaults for creation and modification timestamps;
  - `LONGTEXT` and `TEXT` `NOT NULL` content columns;
  - common status, name, password, GUID, MIME, type, order, and count columns;
  - `post_name(191)`, `type_status_date`, `post_parent`, and `post_author`
    secondary keys.
- `wp_comments` with:
  - unsigned auto-increment `BIGINT` primary key;
  - unsigned integer post, parent, and user defaults written as quoted strings;
  - `TINYTEXT` and `TEXT` `NOT NULL` value columns;
  - zero `DATETIME` defaults for local and GMT timestamps;
  - common author, approval, agent, and type defaults;
  - `comment_post_ID`, `comment_approved_date_gmt`, `comment_date_gmt`,
    `comment_parent`, and `comment_author_email(10)` secondary keys.

The fixture uses persistent base tables in a MyLite catalog schema selected
with `USE`. It relies on the existing `SET sql_mode = ''` behavior to admit
WordPress-style zero temporal defaults.

## Non-Goals

This phase does not add:

- full WordPress schema coverage or `dbDelta()` behavior;
- generated columns, functional indexes, invisible columns, or unsupported
  table options;
- full text search behavior for the text columns;
- optimizer use of the secondary indexes;
- broad non-strict implicit-default DML behavior beyond simple verified rows;
- full Unicode collation comparison or ordering;
- new public APIs;
- SQLite fork patches.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` remains the statement boundary.
- Statement context: owns affected rows, warning counts, insert ids, row-count
  state, and diagnostics for fixture DDL, DML, and metadata queries.
- Lexer/parser/AST: no new grammar is introduced. Existing `CREATE TABLE`,
  `INSERT`, `SHOW`, `SET`, and `INFORMATION_SCHEMA` `SELECT` grammar admits
  the fixture statements.
- Analyzer/planner: continues to validate types, defaults, key parts,
  collations, and table options from MyLite descriptors before SQLite SQL is
  generated.
- Catalog module: remains authoritative for column, key, charset/collation,
  auto-increment, and table metadata.
- SQLite physical storage: receives descriptor-generated physical tables and
  indexes only. User identifiers are mapped to stable quoted physical names by
  existing generation paths.
- Storage/VFS/file format: fixture setup and row writes must preserve the
  `.mylite` preamble and shifted SQLite payload invariants.

## Grammar

No grammar expansion is intended. This fixture is admitted through existing
independently authored grammar slices:

```sql
CREATE TABLE table_name (column_definition_or_key_definition[, ...])
    DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_520_ci
INSERT INTO table_name (column[, ...]) VALUES (value[, ...])
SHOW COLUMNS FROM table_name
SHOW INDEX FROM table_name
SHOW CREATE TABLE table_name
SELECT ... FROM INFORMATION_SCHEMA.COLUMNS|STATISTICS WHERE ...
```

The MyLite Lemon syntax snippets are inherited from the existing feature
specifications listed above.

## Semantics

- Schema resolution follows the existing selected-schema policy.
- The tables are persistent base-table descriptors.
- Display-width attributes are accepted through the existing deprecated
  display-width path and produce the MySQL-verified warning count.
- Quoted integer defaults are converted through the existing descriptor-owned
  exact decimal string conversion rules.
- Zero temporal defaults are accepted only because the fixture explicitly uses
  an empty SQL mode.
- `TEXT` family columns without explicit defaults retain no explicit default;
  metadata displays SQL `NULL` default values like MySQL.
- Secondary indexes are descriptor metadata plus existing generated SQLite
  physical indexes. Prefix key parts use the existing prefix-index descriptor
  and physical expression-index paths.
- Successful fixture setup statements return through existing non-row result
  conventions. Simple inserts report one affected row and zero warnings.

## Diagnostics

This feature does not add new diagnostics. It relies on existing diagnostics
for unsupported grammar, invalid defaults, unsupported key parts, unknown
charsets/collations, duplicate keys, allocation failures, physical SQLite
failures, and public API misuse.

## Test Plan

Extend the existing WordPress fixture coverage with:

- MySQL 8.4.9 expectation output for `wp_posts` and `wp_comments`;
- runtime creation of both tables with MySQL-compatible warning counts;
- `SHOW COLUMNS`, `SHOW CREATE TABLE`, `SHOW INDEX`, and selected
  `INFORMATION_SCHEMA.COLUMNS` / `STATISTICS` assertions;
- simple inserted rows proving defaults, zero datetimes, text values, and
  auto-increment ids;
- close/reopen persistence through the existing file-backed fixture test;
- independent-handle preservation through the existing fixture harness;
- `.mylite` preamble preservation.

## Performance And SQLite Integration

This fixture stays on existing descriptor-driven paths. DDL writes catalog
descriptors and generated physical SQLite objects. DML uses existing prepared
insert paths and descriptor-owned conversion/default handling. Metadata queries
read MyLite descriptors. No new row materialization strategy, dependency, public
SQLite extension, or fork patch is required.
