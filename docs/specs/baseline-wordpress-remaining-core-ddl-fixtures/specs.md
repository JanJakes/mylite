# Baseline WordPress Remaining Core DDL Fixtures

## Status

This phase completes the current representative WordPress core table setup
fixture family by adding the remaining common metadata, taxonomy, relationship,
and links tables:

- `wp_commentmeta`
- `wp_usermeta`
- `wp_termmeta`
- `wp_terms`
- `wp_term_taxonomy`
- `wp_term_relationships`
- `wp_links`

It is a fixture-integration slice over existing MyLite DDL, DML, metadata,
default, charset/collation, auto-increment, primary-key, unique-key,
secondary-index, prefix-index, text, temporal, and file-backed storage behavior.
It does not introduce new SQL grammar.

The goal is to let common WordPress schema setup reach query execution instead
of failing during table creation or basic metadata validation. This remains
representative fixture coverage, not a full WordPress, `dbDelta()`, or broad
schema-migration compatibility claim.

## Sources And Evidence

- MyLite project context:
  - `README.md`
  - `AGENTS.md`
  - `COMPATIBILITY.md`
  - `docs/architecture/engineering-standards.md`
  - `third_party/sqlite/README.md`
- Existing MyLite feature specifications:
  - `docs/specs/baseline-wordpress-core-ddl-fixtures/specs.md`
  - `docs/specs/baseline-wordpress-posts-comments-ddl-fixtures/specs.md`
  - `docs/specs/baseline-auto-increment-lifecycle/specs.md`
  - `docs/specs/baseline-primary-key-lifecycle/specs.md`
  - `docs/specs/baseline-composite-unique-indexes/specs.md`
  - `docs/specs/baseline-index-prefix-key-parts/specs.md`
  - `docs/specs/baseline-text-type/specs.md`
  - `docs/specs/baseline-zero-temporal-sql-modes/specs.md`
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

With `SET sql_mode = ''`, MySQL 8.4.9 accepts the representative table shapes
covered by this phase and reports these deprecated integer display-width warning
counts:

- `wp_commentmeta`: `2`
- `wp_usermeta`: `2`
- `wp_termmeta`: `2`
- `wp_terms`: `2`
- `wp_term_taxonomy`: `4`
- `wp_term_relationships`: `3`
- `wp_links`: `3`

The runtime probes also verify:

- `BIGINT(20)` and `INT(11)` display widths are not preserved in rendered
  metadata.
- `TEXT`, `MEDIUMTEXT`, and `LONGTEXT` values inherit the table collation where
  applicable and display SQL `NULL` defaults when no explicit default exists.
- Prefix secondary indexes such as `meta_key(191)`, `slug(191)`, and
  `name(191)` are accepted and reported through `SHOW INDEX` and
  `INFORMATION_SCHEMA.STATISTICS`.
- Composite primary keys and composite unique secondary keys are accepted and
  reflected in column-key metadata.
- Simple inserts that provide required text values and omit supported defaults
  produce generated ids or composite key values, materialize defaults, and
  report `ROW_COUNT() = 1` with `@@warning_count = 0`.

## Supported Scope

Supported fixture additions:

- Meta-table shapes for `wp_commentmeta`, `wp_usermeta`, and `wp_termmeta`:
  unsigned auto-increment `BIGINT` primary key, unsigned owner id defaulting to
  zero, nullable `VARCHAR(255)` `meta_key`, nullable `LONGTEXT` `meta_value`,
  owner-id secondary key, and `meta_key(191)` prefix key.
- `wp_terms`: unsigned auto-increment `BIGINT` primary key, `name` and `slug`
  string defaults, signed `BIGINT` term group default, and prefix keys on
  `slug(191)` and `name(191)`.
- `wp_term_taxonomy`: unsigned auto-increment primary key, unsigned term and
  parent defaults, `taxonomy` string default, required `LONGTEXT` description,
  signed `BIGINT` count default, composite unique key on `(term_id, taxonomy)`,
  and secondary taxonomy key.
- `wp_term_relationships`: composite primary key on
  `(object_id, term_taxonomy_id)`, integer term order default, and secondary key
  on `term_taxonomy_id`.
- `wp_links`: unsigned auto-increment primary key, string default columns,
  signed integer rating default, zero `DATETIME` updated timestamp under empty
  SQL mode, required `MEDIUMTEXT` notes, and secondary key on `link_visible`.

All fixture tables are persistent base tables in a selected MyLite catalog
schema. The fixture explicitly uses `SET sql_mode = ''` before zero temporal
defaults.

## Non-Goals

This phase does not add:

- full WordPress schema, data, migration, or `dbDelta()` compatibility;
- broad table options beyond existing admitted charset/collation and engine
  behavior;
- full optimizer use of declared indexes;
- full-text search, spatial search, triggers, cascades, or privilege behavior;
- broad non-strict DML coercion beyond the simple fixture rows;
- new public APIs;
- SQLite fork patches.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` remains the execution boundary.
- Statement context: owns warning counts, affected rows, insert ids, row-count
  state, and diagnostics for fixture DDL, DML, and metadata queries.
- Lexer/parser/AST: no grammar changes are intended. The tables use existing
  `CREATE TABLE`, `INSERT`, `SHOW`, `SET`, and `INFORMATION_SCHEMA` query
  shapes.
- Analyzer/planner: continues to validate descriptors, defaults, key parts,
  charset/collation metadata, and table options before SQLite SQL generation.
- Catalog module: remains authoritative for all user-visible table, column,
  default, key, charset/collation, and auto-increment metadata.
- SQLite physical storage: receives descriptor-generated physical tables and
  indexes using existing stable physical names and quoted identifiers.
- Storage/VFS/file format: fixture creation and row writes must preserve the
  `.mylite` preamble and shifted SQLite payload invariants.

## Grammar

No grammar expansion is intended. The admitted SQL is covered by existing
independently authored grammar slices:

```sql
CREATE TABLE table_name (column_definition_or_key_definition[, ...])
    DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_520_ci
INSERT INTO table_name [(column[, ...])] VALUES (value[, ...])
SHOW COLUMNS FROM table_name
SHOW INDEX FROM table_name
SHOW CREATE TABLE table_name
SELECT ... FROM INFORMATION_SCHEMA.COLUMNS|STATISTICS WHERE ...
```

The MyLite Lemon syntax snippets are inherited from the existing feature
specifications listed above.

## Semantics

- Schema resolution follows the existing selected-schema policy.
- Display-width attributes are accepted through the existing deprecated
  display-width path and produce MySQL-verified warning counts.
- Quoted and unquoted integer defaults are converted through existing
  descriptor-owned integer default conversion rules.
- Zero `DATETIME` defaults are accepted only because the fixture uses an empty
  SQL mode.
- Text-family columns without explicit defaults retain no explicit default;
  metadata displays SQL `NULL` default values like MySQL.
- Prefix, composite primary, composite unique, and nonunique secondary indexes
  are descriptor metadata plus existing generated SQLite physical indexes.
- Successful setup statements return through existing non-row result
  conventions. Simple fixture inserts report one affected row and zero
  warnings.

## Diagnostics

This feature adds no diagnostics. It relies on existing errors and warnings for
unsupported syntax, invalid defaults, unsupported key parts, unknown
charsets/collations, duplicate keys, allocation failures, physical SQLite
failures, and public API misuse.

## Test Plan

Extend the existing WordPress fixture coverage with:

- MySQL 8.4.9 expectation output for the seven remaining fixture tables;
- runtime creation of each table with MySQL-compatible warning counts;
- `SHOW COLUMNS`, `SHOW CREATE TABLE`, `SHOW INDEX`, and selected
  `INFORMATION_SCHEMA.COLUMNS` / `STATISTICS` assertions;
- simple inserted rows proving defaults, generated ids, composite primary-key
  rows, zero datetimes, text values, and prefix/composite index metadata;
- close/reopen persistence through the existing file-backed fixture test;
- independent-handle preservation through the existing fixture harness;
- `.mylite` preamble preservation.

## Performance And SQLite Integration

This fixture stays on existing descriptor-driven paths. DDL writes catalog
descriptors and generated physical SQLite table/index objects. DML uses
existing prepared insert paths and descriptor-owned conversion/default handling.
Metadata queries read MyLite descriptors. No new row materialization strategy,
dependency, public SQLite extension, or SQLite fork patch is required.
