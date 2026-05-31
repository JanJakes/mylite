# Baseline sys.schema_redundant_indexes Views

This slice adds MySQL-shaped metadata and deterministic read-only rows for
`sys.schema_redundant_indexes` and its helper view
`sys.x$schema_flattened_keys`. MyLite does not execute the physical MySQL sys
schema views, so the rows are synthesized from MyLite's persistent user
base-table index descriptors.

## Compatibility Authority

- MySQL 8.4 Reference Manual, `sys.schema_redundant_indexes` and
  `sys.x$schema_flattened_keys`:
  <https://dev.mysql.com/doc/refman/8.4/en/sys-schema-redundant-indexes.html>
- MySQL 8.4 Reference Manual, `SHOW CREATE VIEW`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-create-view.html>
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_sys_schema_redundant_indexes_views_expectations.sh`.

Runtime probes against the local `mylite-mysql-849` MySQL 8.4.9 container
verified column metadata, redundant-index rows, helper-view rows, view
metadata, dependency metadata, selected-schema access, and SHOW metadata.

## Supported Behavior

Supported direct reads:

```sql
SELECT * FROM sys.`x$schema_flattened_keys`;
SELECT * FROM sys.schema_redundant_indexes;

USE sys;
SELECT table_schema, table_name, index_name FROM `x$schema_flattened_keys`;
SELECT table_schema, table_name, redundant_index_name FROM schema_redundant_indexes;
```

`sys.x$schema_flattened_keys` emits one row per persistent user base-table BTREE
index descriptor. It omits temporary tables, built-in schemas, metadata-only
views, FULLTEXT indexes, SPATIAL indexes, and unsupported functional indexes.
Each row contains the schema, table, index name, MySQL `NON_UNIQUE` value,
whether any key part uses a prefix length, and a comma-separated list of key
part column names in index order.

`sys.schema_redundant_indexes` derives rows from those flattened helper rows.
For indexes on the same table, an index is redundant when MySQL 8.4.9's sys
view rules classify another index as dominant:

- same indexed column list, where the redundant index is nonunique and the
  dominant index is unique;
- same indexed column list and same uniqueness, where the redundant index name
  sorts after the dominant index name after treating `PRIMARY` as an empty
  string;
- a nonunique index's column list is a left prefix of another index's column
  list;
- a unique index's column list is a left prefix of another index's column list,
  making the longer index redundant.

`subpart_exists` is `1` when either side of the redundant pair has a prefix key
part, otherwise `0`. `sql_drop_index` renders the MySQL-shaped drop statement
using backtick-quoted schema, table, and index names.

## Column Metadata

`sys.schema_redundant_indexes` has ten columns:

| Column | Type | Null | Default | Collation |
| --- | --- | --- | --- | --- |
| `table_schema` | `varchar(64)` | `NO` | `NULL` | `utf8mb3_bin` |
| `table_name` | `varchar(64)` | `NO` | `NULL` | `utf8mb3_bin` |
| `redundant_index_name` | `varchar(64)` | `YES` | `NULL` | `utf8mb3_tolower_ci` |
| `redundant_index_columns` | `text` | `YES` | `NULL` | `utf8mb3_tolower_ci` |
| `redundant_index_non_unique` | `int` | `YES` | `NULL` | SQL `NULL` |
| `dominant_index_name` | `varchar(64)` | `YES` | `NULL` | `utf8mb3_tolower_ci` |
| `dominant_index_columns` | `text` | `YES` | `NULL` | `utf8mb3_tolower_ci` |
| `dominant_index_non_unique` | `int` | `YES` | `NULL` | SQL `NULL` |
| `subpart_exists` | `int` | `NO` | `0` | SQL `NULL` |
| `sql_drop_index` | `varchar(223)` | `YES` | `NULL` | `utf8mb3_tolower_ci` |

`sys.x$schema_flattened_keys` has six columns:

| Column | Type | Null | Default | Collation |
| --- | --- | --- | --- | --- |
| `table_schema` | `varchar(64)` | `NO` | `NULL` | `utf8mb3_bin` |
| `table_name` | `varchar(64)` | `NO` | `NULL` | `utf8mb3_bin` |
| `index_name` | `varchar(64)` | `YES` | `NULL` | `utf8mb3_tolower_ci` |
| `non_unique` | `int` | `YES` | `NULL` | SQL `NULL` |
| `subpart_exists` | `bigint` | `YES` | `NULL` | SQL `NULL` |
| `index_columns` | `text` | `YES` | `NULL` | `utf8mb3_tolower_ci` |

`SHOW COLUMNS`, `SHOW FULL COLUMNS`, `DESCRIBE`, and
`INFORMATION_SCHEMA.COLUMNS` expose this shape. The views have no index or
constraint metadata, so `SHOW INDEX`, `INFORMATION_SCHEMA.STATISTICS`,
`TABLE_CONSTRAINTS`, `KEY_COLUMN_USAGE`, and
`TABLE_CONSTRAINTS_EXTENSIONS` return zero rows for the view objects
themselves.

## View Metadata

`INFORMATION_SCHEMA.VIEWS` exposes built-in rows for both views with:

- `TABLE_CATALOG = 'def'`
- `TABLE_SCHEMA = 'sys'`
- `CHECK_OPTION = 'NONE'`
- `IS_UPDATABLE = 'NO'`
- `DEFINER = 'mysql.sys@localhost'`
- `SECURITY_TYPE = 'INVOKER'`
- `CHARACTER_SET_CLIENT = 'utf8mb4'`
- `COLLATION_CONNECTION = 'utf8mb4_0900_ai_ci'`

`SHOW CREATE VIEW` and `SHOW CREATE TABLE` return MySQL-shaped metadata with
`ALGORITHM=TEMPTABLE` and `DEFINER=\`mysql.sys\`@\`localhost\``. Qualified
targets render the view name as `` `sys`.`schema_redundant_indexes` `` or
`` `sys`.`x$schema_flattened_keys` ``. Unqualified targets resolved after
`USE sys` render the unqualified view name.

`INFORMATION_SCHEMA.VIEW_TABLE_USAGE` reports one dependency from
`schema_redundant_indexes` to `sys.x$schema_flattened_keys`, and one dependency
from `x$schema_flattened_keys` to `information_schema.STATISTICS`.
`INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE` reports zero rows.

## Unsupported Behavior

This slice intentionally does not implement:

- physical MySQL sys view execution;
- Performance Schema or optimizer-driven index usage analysis;
- unsupported functional-index row synthesis;
- rows for temporary tables, built-in schemas, FULLTEXT indexes, or SPATIAL
  indexes;
- privilege filtering, definer validation, or SQL SECURITY enforcement;
- physical SQLite views or persisted catalog descriptors for built-in sys
  views;
- broader sys view execution.

Writes to the views remain blocked by the existing built-in schema write guard.

## Parser And Grammar

No Lemon grammar changes are required. Existing qualified and selected-schema
table references, quoted identifiers for `x$` view names, `SHOW COLUMNS`,
`SHOW INDEX`, `SHOW CREATE VIEW`, `SHOW CREATE TABLE`, and
`INFORMATION_SCHEMA` query support are sufficient.

## Architecture

- Public API: unchanged.
- Parser/AST: unchanged.
- Runtime metadata: extends the synthetic system-table descriptor table with
  `sys.schema_redundant_indexes` and `sys.x$schema_flattened_keys` view entries.
- Query execution: reuses the existing synthetic system-table SELECT planner
  and builds user-index descriptor rows.
- Information schema: adds `COLUMNS`, `VIEWS`, and `VIEW_TABLE_USAGE` rows
  through the existing synthetic metadata builders.
- SHOW metadata: reuses the synthetic system-table column/index paths and the
  built-in sys view `SHOW CREATE` short-circuit.
- Storage/SQLite: unchanged. No SQLite extension API or fork hook is required.

## Performance

The helper view scans persistent user base-table descriptors and loads index
descriptors for each base table. Redundant-index rows are derived in memory from
the flattened helper rows. The implementation performs no SQLite data-table
scan.

## Tests

MySQL 8.4.9 expectation coverage:

- column metadata for both views;
- flattened helper rows for primary, unique, nonunique, composite, and prefix
  BTREE indexes;
- redundant-index rows for same-column unique dominance, left-prefix dominance,
  and prefix-key same-column dominance;
- view and dependency metadata;
- empty index and constraint metadata for the view objects;
- qualified and selected-schema `SHOW CREATE VIEW` / `SHOW CREATE TABLE`;
- `ROW_COUNT() = -1` and zero warnings after a synthetic view read.

MyLite runtime coverage mirrors the supported metadata, selected-schema, and
descriptor-backed row cases, including omission of unsupported FULLTEXT and
SPATIAL indexes from the helper row inventory.
