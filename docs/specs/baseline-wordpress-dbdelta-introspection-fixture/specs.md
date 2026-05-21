# Baseline WordPress dbDelta Introspection Fixture

## Status

This feature locks down the representative WordPress table-introspection path
that runs around schema setup. It builds on the existing WordPress core DDL
fixture coverage, but narrows the contract to the metadata statements commonly
used to compare an expected table definition with the live schema:

- `DESCRIBE`, `DESC`, and `EXPLAIN table`;
- `SHOW FULL COLUMNS ... WHERE`;
- `SHOW INDEX ... WHERE`;
- `SHOW CREATE TABLE`;
- selected `INFORMATION_SCHEMA.COLUMNS` and
  `INFORMATION_SCHEMA.STATISTICS` queries.

This is not a `dbDelta()` implementation and does not parse WordPress code. It
is a MySQL-compatible SQL fixture over representative WordPress table shapes so
future work can separate table-setup failures from later query failures.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing WordPress fixture coverage:
  `docs/specs/baseline-wordpress-core-ddl-fixtures/specs.md`,
  `docs/specs/baseline-wordpress-posts-comments-ddl-fixtures/specs.md`, and
  `docs/specs/baseline-wordpress-remaining-core-ddl-fixtures/specs.md`
- Existing metadata specs:
  `docs/specs/baseline-show-full-columns-introspection/specs.md`,
  `docs/specs/baseline-show-index-where/specs.md`,
  `docs/specs/baseline-show-create-table/specs.md`, and
  `docs/specs/baseline-information-schema-core/specs.md`
- MySQL 8.4 Reference Manual, `DESCRIBE`:
  https://dev.mysql.com/doc/refman/8.4/en/describe.html
- MySQL 8.4 Reference Manual, `EXPLAIN`:
  https://dev.mysql.com/doc/refman/8.4/en/explain.html
- MySQL 8.4 Reference Manual, `SHOW COLUMNS`:
  https://dev.mysql.com/doc/refman/8.4/en/show-columns.html
- MySQL 8.4 Reference Manual, `SHOW INDEX`:
  https://dev.mysql.com/doc/refman/8.4/en/show-index.html
- MySQL 8.4 Reference Manual, `SHOW CREATE TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/show-create-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.COLUMNS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-columns-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.STATISTICS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-statistics-table.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, WordPress implementation
code, or other restrictively licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes were run against MySQL 8.4.9 with:

```sh
MYLITE_MYSQL_BIN=/opt/homebrew/opt/mysql@8.4/bin/mysql
MYLITE_MYSQL_SOCKET=/tmp/mylite-mysql-849.jsgoZE/mysql.sock
```

The fixture expectation script records the exact result sets. Observed behavior
that shapes this slice:

- `DESCRIBE`, `DESC`, and `EXPLAIN table` return the six-column
  `SHOW COLUMNS` shape: `Field`, `Type`, `Null`, `Key`, `Default`, `Extra`.
- `SHOW FULL COLUMNS FROM wp_options` returns the nine-column full metadata
  shape and reports `utf8mb4_unicode_520_ci` collation for `VARCHAR` and
  `LONGTEXT` columns inherited from the table default.
- `SHOW FULL COLUMNS ... WHERE Field IN (...)` filters over the displayed
  output columns and keeps the original descriptor order.
- `SHOW INDEX ... WHERE` filters over displayed index rows and reports
  WordPress prefix keys such as `meta_key(191)` with `Sub_part = 191`.
- `SHOW CREATE TABLE wp_options` renders the WordPress-like table with
  descriptor default values, table collation, primary/unique/nonunique keys,
  and no display widths in MySQL 8.4.9 metadata.
- `INFORMATION_SCHEMA.COLUMNS` and `INFORMATION_SCHEMA.STATISTICS` expose the
  same descriptor metadata used by the `SHOW` statements.
- Successful metadata statements leave `@@warning_count = 0` and
  `ROW_COUNT() = -1`.

## Scope

Supported fixture setup:

```sql
CREATE DATABASE wp DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_520_ci
USE wp
SET sql_mode = ''
CREATE TABLE wp_options (...)
CREATE TABLE wp_postmeta (...)
```

Supported introspection statements for this fixture:

```sql
DESCRIBE wp_options
DESC wp_options
EXPLAIN wp_options
SHOW FULL COLUMNS FROM wp_options
SHOW FULL COLUMNS FROM wp_options WHERE Field IN (...)
SHOW INDEX FROM wp_postmeta WHERE Key_name IN (...)
SHOW INDEX FROM wp_options WHERE Non_unique = '0' AND Column_name = 'option_name'
SHOW CREATE TABLE wp_options
SELECT ... FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = 'wp' ...
SELECT ... FROM INFORMATION_SCHEMA.STATISTICS WHERE TABLE_SCHEMA = 'wp' ...
    AND INDEX_NAME = 'meta_key'
```

The fixture covers persistent base tables only. It uses the current descriptor
types, defaults, collation metadata, auto-increment metadata, primary keys,
unique keys, nonunique keys, and prefix keys that the existing WordPress DDL
fixtures already admit.

## Non-Goals

This feature does not add:

- a WordPress-specific parser mode or application shim;
- full `dbDelta()` behavior, schema-diff planning, or automatic DDL repair;
- `DESCRIBE table 'pattern'` or `EXPLAIN table 'pattern'`;
- `SHOW COLUMNS` / `SHOW INDEX` `ORDER BY` or `LIMIT`;
- broad `INFORMATION_SCHEMA` query planning beyond existing supported `SELECT`
  shapes;
- optimizer use of indexes or real storage statistics;
- privilege semantics, view metadata, generated columns, or triggers;
- SQLite fork patches.

## Ownership Boundaries

- Public API: unchanged. Applications continue to execute the fixture through
  `mylite_execute()` and inspect normal result handles.
- Statement context: owns diagnostics, `warning_count`, result-set row-count
  state, and cleanup. Successful fixture introspection returns result sets and
  leaves later `ROW_COUNT()` at `-1`.
- Parser/AST: no grammar expansion is intended. The fixture uses already
  admitted metadata statement forms.
- Runtime/analyzer: resolves schemas, tables, columns, keys, and metadata
  filters through MyLite descriptors and existing predicate evaluators.
- Catalog: remains authoritative for all visible logical metadata. The fixture
  must not read SQLite schema text to reconstruct MySQL metadata.
- Result builders: render `DESCRIBE`, `SHOW FULL COLUMNS`, `SHOW INDEX`,
  `SHOW CREATE TABLE`, and `INFORMATION_SCHEMA` rows from descriptors.
- SQLite physical storage: unchanged. DDL uses existing descriptor-built
  physical tables and indexes; introspection uses no generated SQLite SQL
  except existing information-schema query machinery.
- Storage/VFS: `.mylite` preamble and shifted SQLite payload invariants remain
  unchanged.

## Grammar

No new grammar is added. The fixture relies on already specified grammar:

```sql
describe_table_statement:
    DESCRIBE table_name
  | DESC table_name

explain_table_statement:
    EXPLAIN table_name

show_full_columns_statement:
    SHOW FULL COLUMNS FROM table_name show_columns_filter_opt

show_index_statement:
    SHOW INDEX FROM table_name show_index_filter_opt

show_create_table_statement:
    SHOW CREATE TABLE table_name
```

The MyLite Lemon syntax snippets remain owned by the individual feature specs
listed in the source section.

## Semantics

- `DESCRIBE`, `DESC`, and `EXPLAIN table` are aliases for the non-full
  `SHOW COLUMNS` result shape.
- `SHOW FULL COLUMNS` reports descriptor-owned `Collation`, `Privileges`, and
  `Comment` fields; current privileges remain the fixed baseline string
  `select,insert,update,references`.
- `SHOW FULL COLUMNS WHERE` and `SHOW INDEX WHERE` filters evaluate only over
  displayed output columns, using the existing string/`NULL` metadata predicate
  subset.
- Prefix index metadata reports the descriptor prefix length in `SHOW
  INDEX.Sub_part` and `INFORMATION_SCHEMA.STATISTICS.SUB_PART`.
- `SHOW CREATE TABLE` renders descriptor-owned table DDL and does not consult
  SQLite schema text.
- `INFORMATION_SCHEMA` rows use the same descriptor data as the `SHOW`
  statements. The fixture deliberately queries a small projected subset to
  avoid claiming broad SQL support.

## Diagnostics

This fixture adds no new diagnostics. Existing metadata statement diagnostics
continue to apply for syntax errors, missing default schema, unknown schema,
unknown table, unknown output-column filters, unsupported predicate values, and
allocation or physical SQLite failures.

## Tests

Add one fast C runtime test and one MySQL expectation script.

The C test must cover:

- file-backed fixture setup and `.mylite` preamble preservation;
- `DESCRIBE`, `DESC`, and `EXPLAIN table` equivalence over `wp_options`;
- `SHOW FULL COLUMNS` full output and `WHERE` output-column filtering;
- `SHOW INDEX WHERE` over unique, nonunique, and prefix keys;
- `SHOW CREATE TABLE` rendering for the representative table;
- selected `INFORMATION_SCHEMA.COLUMNS` and `STATISTICS` projections;
- `ROW_COUNT()` state after metadata statements;
- close/reopen persistence for the same metadata path;
- independent file-backed handles with independent descriptor metadata.

The MySQL expectation script must verify the same user-visible behavior against
MySQL 8.4.9.

## Performance And SQLite Fit

The fixture stays on existing metadata paths. It iterates descriptors for
`SHOW` statements and uses existing information-schema query machinery for the
small selected metadata projections. It does not materialize user rows, does
not scan SQLite schema text, and does not require SQLite fork hooks.
