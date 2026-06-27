# Baseline Performance Schema host and keyring placeholders

## Scope

This baseline covers three MySQL 8.4.9 Performance Schema tables that expose
server host-cache and keyring service state:

- `performance_schema.host_cache`
- `performance_schema.keyring_component_status`
- `performance_schema.keyring_keys`

The implemented surface is metadata-compatible and read-only. MyLite exposes
table descriptors, column metadata, HASH index metadata for `host_cache`, table
status metadata, empty selected rows, selected-schema resolution, and
built-in-schema write protection.

## Compatibility authority

The specification is based on the MySQL 8.4 Reference Manual pages for the
covered Performance Schema tables:

- <https://dev.mysql.com/doc/refman/8.4/en/performance-schema-host-cache-table.html>
- <https://dev.mysql.com/doc/refman/8.4/en/performance-schema-keyring-component-status-table.html>
- <https://dev.mysql.com/doc/refman/8.4/en/performance-schema-keyring-keys-table.html>

Runtime metadata was verified against a MySQL 8.4.9 container named
`mylite-mysql-849` with:

```sql
SELECT VERSION();
SELECT 'host_cache', COUNT(*) FROM performance_schema.host_cache
UNION ALL SELECT 'keyring_component_status', COUNT(*)
  FROM performance_schema.keyring_component_status
UNION ALL SELECT 'keyring_keys', COUNT(*) FROM performance_schema.keyring_keys;
SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, COLUMN_TYPE,
       COLUMN_KEY, COLLATION_NAME, CHARACTER_SET_NAME,
       CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH,
       NUMERIC_PRECISION, NUMERIC_SCALE, DATETIME_PRECISION, COLUMN_DEFAULT,
       EXTRA, PRIVILEGES
  FROM information_schema.columns
 WHERE TABLE_SCHEMA = 'performance_schema'
   AND TABLE_NAME IN ('host_cache', 'keyring_component_status', 'keyring_keys')
 ORDER BY FIELD(TABLE_NAME, 'host_cache', 'keyring_component_status',
                'keyring_keys'),
          ORDINAL_POSITION;
SELECT TABLE_NAME, INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME,
       COLLATION IS NULL, CARDINALITY IS NULL, INDEX_TYPE, IS_VISIBLE
  FROM information_schema.statistics
 WHERE TABLE_SCHEMA = 'performance_schema'
   AND TABLE_NAME IN ('host_cache', 'keyring_component_status', 'keyring_keys')
 ORDER BY FIELD(TABLE_NAME, 'host_cache', 'keyring_component_status',
                'keyring_keys'),
          INDEX_NAME, SEQ_IN_INDEX;
SELECT TABLE_NAME, CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED
  FROM information_schema.table_constraints
 WHERE TABLE_SCHEMA = 'performance_schema'
   AND TABLE_NAME IN ('host_cache', 'keyring_component_status', 'keyring_keys')
 ORDER BY FIELD(TABLE_NAME, 'host_cache', 'keyring_component_status',
                'keyring_keys'),
          CONSTRAINT_NAME;
SELECT TABLE_NAME, ENGINE, ROW_FORMAT, AUTO_INCREMENT, TABLE_COLLATION
  FROM information_schema.tables
 WHERE TABLE_SCHEMA = 'performance_schema'
   AND TABLE_NAME IN ('host_cache', 'keyring_component_status', 'keyring_keys')
 ORDER BY FIELD(TABLE_NAME, 'host_cache', 'keyring_component_status',
                'keyring_keys');
```

Observed MySQL 8.4.9 metadata:

- `host_cache` has 29 columns. Its primary key is `IP`; it also exposes a
  non-unique HASH index on nullable `HOST`. Host-name text uses `ascii`, while
  `IP` and `HOST_VALIDATED enum('YES','NO')` use `utf8mb4_0900_ai_ci`.
- `keyring_component_status` has `STATUS_KEY varchar(256)` and
  `STATUS_VALUE varchar(1024)` with no indexes or table constraints.
- `keyring_keys` has `KEY_ID`, nullable `KEY_OWNER`, and nullable
  `BACKEND_KEY_ID`, all `varchar(255)` using `utf8mb4_bin`, with no indexes or
  table constraints.
- All three tables are `BASE TABLE` objects with
  `ENGINE='PERFORMANCE_SCHEMA'`, `ROW_FORMAT='Dynamic'`,
  `AUTO_INCREMENT=NULL`, and the MySQL-observed table collation.
- The target runtime returned zero rows for all three tables in the baseline
  container.

## MyLite behavior

MyLite exposes all three tables as read-only synthetic Performance Schema
tables. Each table returns an empty result set. MyLite does not implement a
server host cache, DNS validation cache, keyring component status service, or
keyring key inventory.

The placeholders let applications and diagnostics discover the expected table
shapes and distinguish "no embedded service rows" from "table does not exist".

## Explicit gaps

- No host-cache address tracking, validation state, DNS error counters, first
  seen timestamps, or cache flush behavior is implemented.
- No keyring component status rows or key metadata are exposed.
- `TRUNCATE TABLE` and other write-like operations remain blocked by MyLite's
  built-in schema write-protection policy rather than MySQL's per-table
  Performance Schema mutation rules.

## Runtime and storage design

This is descriptor-only metadata plus empty query dispatch. It uses MyLite's
existing built-in table descriptor framework. No SQLite table, file format
change, public SQLite extension API, targeted SQLite fork hook, or new
dependency is required.

No new SQL grammar is required. Existing metadata query parsing covers:

```lemon
select_statement ::= SELECT select_list FROM qualified_table_name where_clause_opt order_limit_opt.
qualified_table_name ::= ident DOT ident.
describe_statement ::= DESCRIBE qualified_table_name ident_opt.
show_columns_statement ::= SHOW full_opt COLUMNS FROM qualified_table_name show_filter_opt.
show_index_statement ::= SHOW INDEX FROM qualified_table_name show_filter_opt.
show_table_status_statement ::= SHOW TABLE STATUS from_schema_opt show_filter_opt.
```

## Tests

- `packages/libmylite/tests/mysql_baseline_performance_schema_host_keyring_placeholders_expectations.sh`
  verifies MySQL 8.4.9 row counts, column, index, constraint, and table
  metadata.
- `packages/libmylite/tests/runtime_performance_schema_host_keyring_placeholders_test.c`
  verifies MyLite empty query results, metadata surfaces, selected-schema
  resolution, and write protection.
