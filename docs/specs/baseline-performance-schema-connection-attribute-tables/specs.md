# Baseline Performance Schema connection attribute tables

## Scope

This baseline covers the MySQL 8.4.9 `performance_schema` connection attribute
tables:

- `performance_schema.session_account_connect_attrs`
- `performance_schema.session_connect_attrs`

The implemented surface includes MySQL-shaped table descriptors, column
metadata, indexes, primary-key constraints, table-status metadata, read-only
query results, and write protection through the existing built-in schema access
rules.

## Compatibility authority

The specification is based on the MySQL 8.4 Reference Manual pages for
Performance Schema connection attribute tables:

- <https://dev.mysql.com/doc/refman/8.4/en/performance-schema-connection-attribute-tables.html>
- <https://dev.mysql.com/doc/refman/8.4/en/performance-schema-session-account-connect-attrs-table.html>
- <https://dev.mysql.com/doc/refman/8.4/en/performance-schema-session-connect-attrs-table.html>

Runtime metadata and representative rows were verified against a MySQL 8.4.9
container named `mylite-mysql-849` with:

```sql
SELECT VERSION();
SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, DATA_TYPE,
       CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION,
       NUMERIC_SCALE, CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE,
       COLUMN_KEY
  FROM information_schema.columns
 WHERE TABLE_SCHEMA = 'performance_schema'
   AND TABLE_NAME IN ('session_connect_attrs',
                      'session_account_connect_attrs')
 ORDER BY TABLE_NAME, ORDINAL_POSITION;
SELECT TABLE_NAME, INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME,
       COLLATION IS NULL, CARDINALITY IS NULL, INDEX_TYPE, IS_VISIBLE,
       NULLABLE
  FROM information_schema.statistics
 WHERE TABLE_SCHEMA = 'performance_schema'
   AND TABLE_NAME IN ('session_connect_attrs',
                      'session_account_connect_attrs')
 ORDER BY TABLE_NAME, INDEX_NAME, SEQ_IN_INDEX;
SELECT TABLE_NAME, CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED
  FROM information_schema.table_constraints
 WHERE TABLE_SCHEMA = 'performance_schema'
   AND TABLE_NAME IN ('session_connect_attrs',
                      'session_account_connect_attrs')
 ORDER BY TABLE_NAME, CONSTRAINT_NAME;
SHOW TABLE STATUS FROM performance_schema
 WHERE Name IN ('session_connect_attrs', 'session_account_connect_attrs');
SELECT PROCESSLIST_ID, ATTR_NAME, ATTR_VALUE, ORDINAL_POSITION
  FROM performance_schema.session_connect_attrs
 WHERE PROCESSLIST_ID = CONNECTION_ID()
 ORDER BY ORDINAL_POSITION, ATTR_NAME;
```

Observed MySQL 8.4.9 metadata:

- Both tables have columns `PROCESSLIST_ID bigint unsigned NOT NULL`,
  `ATTR_NAME varchar(32) utf8mb4_bin NOT NULL`,
  `ATTR_VALUE varchar(1024) utf8mb4_bin NULL`, and
  `ORDINAL_POSITION int NULL`.
- Both tables have a HASH primary key on `(PROCESSLIST_ID, ATTR_NAME)`.
- Both tables are `BASE TABLE` objects with `ENGINE='PERFORMANCE_SCHEMA'`,
  `ROW_FORMAT='Dynamic'`, `AUTO_INCREMENT=NULL`, and
  `TABLE_COLLATION='utf8mb4_bin'`.
- The target `mysql` command-line client exposed connector-dependent rows such
  as `_pid`, `_platform`, `_os`, `_client_name`, `os_user`,
  `_client_version`, and `program_name`.

## MyLite behavior

MyLite exposes these tables as read-only synthetic `performance_schema` tables.
The descriptor metadata matches the MySQL 8.4.9 column, index, primary-key, and
table-status shape.

MyLite does not currently have a public connection-attribute API or a MySQL wire
handshake attribute parser. To provide useful embedded metadata without
inventing client-supplied attributes, MyLite emits a deterministic attribute set
for every currently open MyLite handle:

- `_client_name = mylite`
- `_client_version = <mylite_version()>`
- `program_name = mylite`

`session_connect_attrs` includes every open handle visible through MyLite's
processlist snapshot. `session_account_connect_attrs` uses the same rows because
MyLite currently exposes one embedded account identity, `root@%`, for all
handles. Attribute ordinal positions start at `0`, matching MySQL's observed
ordering convention.

## Explicit gaps

- No user-defined or connector-provided connection attributes are accepted yet.
- No 64KB connection-attribute buffer validation is performed because there is
  no wire-protocol attribute ingestion in this slice.
- No `performance_schema_session_connect_attrs_size` truncation behavior,
  `_truncated` attribute row, error-log message, or per-connection lost counter
  side effect is implemented.
- Account filtering is simplified to the current embedded `root@%` identity.

## Runtime and storage design

This is a MyLite wrapper/metadata implementation. It uses the existing
processlist snapshot registry and does not require SQLite public extension APIs,
new SQLite storage, or targeted SQLite fork hooks.

Rows are generated on read. There is no persistent storage, no catalog write,
and no new dependency.

## Tests

- `packages/libmylite/tests/mysql_baseline_performance_schema_connection_attribute_tables_expectations.sh`
  verifies MySQL 8.4.9 metadata and representative target rows.
- `packages/libmylite/tests/runtime_performance_schema_connection_attribute_tables_test.c`
  verifies MyLite descriptor metadata, deterministic rows, processlist ids,
  count behavior, table-status metadata, and write protection.
