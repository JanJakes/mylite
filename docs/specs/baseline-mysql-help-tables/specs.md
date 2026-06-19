# Baseline mysql HELP tables

This slice adds metadata-compatible, read-only placeholder support for the four
`mysql` schema tables that back MySQL's server-side HELP feature:

- `mysql.help_category`
- `mysql.help_keyword`
- `mysql.help_relation`
- `mysql.help_topic`

The MySQL 8.4.9 target runtime initializes these tables with manual-derived
help content. MyLite does not copy that content in this slice, because the data
is a large documentation payload maintained outside MyLite and carrying license
constraints unrelated to the embedded compatibility layer. Instead, MyLite
exposes MySQL-shaped metadata and empty read-only table reads.

## Compatibility Authority

- MySQL 8.4 Reference Manual, `mysql` system schema:
  <https://dev.mysql.com/doc/refman/8.4/en/system-schema.html>
- MySQL 8.4 Reference Manual, server-side help support:
  <https://dev.mysql.com/doc/refman/8.4/en/server-side-help-support.html>
- MySQL 8.4 Reference Manual, HELP statement:
  <https://dev.mysql.com/doc/refman/8.4/en/help.html>
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_mysql_help_tables_expectations.sh`.

The MySQL manual describes the HELP statement as reading initialized help-topic
information from tables in the `mysql` schema. It also documents initialization
and upgrade of the help-table content from distribution-provided SQL.

## Supported Behavior

MyLite supports the help tables as read-only metadata placeholders:

```sql
SELECT COUNT(*) FROM mysql.help_category;
SELECT COUNT(*) FROM mysql.help_keyword;
SELECT COUNT(*) FROM mysql.help_relation;
SELECT COUNT(*) FROM mysql.help_topic;
```

Each query succeeds and returns zero rows for non-aggregate reads or `0` for
`COUNT(*)`. Unqualified reads after `USE mysql` are supported for the same
placeholder tables.

The following metadata surfaces expose MySQL 8.4.9-shaped table metadata:

- `SHOW COLUMNS` / `SHOW FULL COLUMNS` / `DESCRIBE`
- `SHOW INDEX` / `SHOW INDEXES` / `SHOW KEYS`
- `INFORMATION_SCHEMA.COLUMNS`
- `INFORMATION_SCHEMA.STATISTICS`
- `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`
- `INFORMATION_SCHEMA.KEY_COLUMN_USAGE`
- `INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS`
- `INFORMATION_SCHEMA.TABLES`
- `SHOW TABLE STATUS`

`mysql.help_category`, `mysql.help_keyword`, and `mysql.help_topic` expose a
primary key plus the MySQL-observed unique `name` index. `mysql.help_relation`
exposes its composite primary key in MySQL-observed key-column order.

## Syntax

No parser change is required. Existing MyLite grammar already admits the
targeted statement shapes:

```lemon
select_stmt ::= SELECT select_options select_list from_clause select_tail.
from_clause ::= FROM table_factor.
table_factor ::= qualified_name alias_opt index_hint_list_opt.
cmd ::= SHOW show_columns_kind FROM qualified_name show_columns_tail.
cmd ::= DESCRIBE qualified_name.
cmd ::= SHOW show_index_kind FROM qualified_name show_index_tail.
cmd ::= SHOW TABLE STATUS show_table_status_tail.
cmd ::= USE identifier.
```

The `HELP` statement itself is out of scope for this slice.

## Semantics

The table definitions are static runtime metadata. They are not persisted to
the MyLite catalog and do not create physical SQLite tables, views, indexes, or
triggers.

Direct `SELECT` support uses the existing mysql system-table query path. Rows
are intentionally empty. The metadata column definitions, key labels,
privileges, primary-key order, unique `name` indexes, table comments, collation,
and table-status fields match the observed MySQL 8.4.9 target metadata where
the values are stable enough for introspection.

The MySQL target runtime contained initialized help rows:

| Table | Observed count |
| --- | ---: |
| `mysql.help_category` | 53 |
| `mysql.help_keyword` | 962 |
| `mysql.help_relation` | 1985 |
| `mysql.help_topic` | 695 |

MyLite intentionally returns empty placeholders until a separately specified
help-content feature defines an independently authored or appropriately
licensed content source.

## Metadata Shape

Initial observed table-status estimates used by MyLite's placeholder runtime
tests:

| Table | Rows | Avg row length | Data length | Index length | Data free | Comment |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| `help_category` | 53 | 309 | 16384 | 16384 | 4194304 | `help categories` |
| `help_keyword` | 1142 | 114 | 131072 | 147456 | 4194304 | `help keywords` |
| `help_relation` | 1608 | 50 | 81920 | 0 | 4194304 | `keyword-topic relation` |
| `help_topic` | 902 | 1761 | 1589248 | 98304 | 4194304 | `help topics` |

`CREATE_OPTIONS` is `row_format=DYNAMIC stats_persistent=0`.
`TABLE_COLLATION` is `utf8mb3_general_ci`.

The MySQL expectation artifact treats these storage estimates and index
cardinalities as live InnoDB statistics. It verifies their shape on the MySQL
runtime and keeps exact checks to stable metadata such as engine, row format,
collation, create options, comments, index names, key order, nullability,
visibility, and index type.

Initial observed primary-key cardinalities:

| Table | Key columns | Cardinality |
| --- | --- | --- |
| `help_category` | `help_category_id` | 53 |
| `help_keyword` | `help_keyword_id` | 551 |
| `help_relation` | `help_keyword_id`, `help_topic_id` | 1393, 2258 |
| `help_topic` | `help_topic_id` | 596 |

Initial observed unique `name` index cardinalities:

| Table | Cardinality |
| --- | ---: |
| `help_category` | 53 |
| `help_keyword` | 551 |
| `help_topic` | 596 |

## Diagnostics And Limits

- Writes remain rejected by the existing built-in schema write protection.
- Direct reads support projection, filtering, ordering, and limits through the
  existing mysql system-table metadata query path, but operate on empty rows.
- No `HELP` statement parsing or execution is added.
- No help-topic/manual content is bundled.
- No import of `fill_help_tables.sql`, help-table upgrade path, privilege
  filtering, or mutable help storage is added.

## Ownership Boundary

- Public API: unchanged.
- Parser/AST: unchanged.
- Analyzer/runtime: add mysql system-table definitions and empty-row read
  support for the four help tables.
- Metadata: extend mysql system-table index metadata to cover secondary unique
  `name` indexes for the help tables.
- Catalog/storage/SQLite: unchanged.

## Test Plan

- Add a MySQL expectation script that verifies target row counts, columns,
  indexes, constraints, table-status metadata, and the initialized HELP output
  dependency.
- Add focused C runtime coverage for empty direct reads, selected-schema reads,
  columns, indexes, constraint metadata, table metadata, and table status.
- Run:
  - `sh -n packages/libmylite/tests/mysql_baseline_mysql_help_tables_expectations.sh`
  - `packages/libmylite/tests/mysql_baseline_mysql_help_tables_expectations.sh`
  - `cmake --build --preset dev --target mylite_runtime_mysql_help_tables_test`
  - `ctest --preset dev -R '^libmylite\.runtime\.mysql_help_tables$' --output-on-failure`
  - `git diff --check`
  - `git diff --cached --check`
  - `cmake --workflow --preset check`
