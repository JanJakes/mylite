# Baseline INFORMATION_SCHEMA Conditional Table Absence

This slice documents and tests MyLite's target-build behavior for conditional
`INFORMATION_SCHEMA` tables that are documented for MySQL 8.4 but are absent
from the standard MySQL 8.4.9 runtime used by MyLite's compatibility suite.

The covered names are:

- `INFORMATION_SCHEMA.MYSQL_FIREWALL_USERS`
- `INFORMATION_SCHEMA.MYSQL_FIREWALL_WHITELIST`
- `INFORMATION_SCHEMA.ndb_transid_mysql_connection_map`
- `INFORMATION_SCHEMA.TP_THREAD_GROUP_STATE`
- `INFORMATION_SCHEMA.TP_THREAD_GROUP_STATS`
- `INFORMATION_SCHEMA.TP_THREAD_STATE`

## Compatibility Authority

- MySQL 8.4 Reference Manual, MySQL Enterprise Firewall
  `INFORMATION_SCHEMA` tables:
  <https://dev.mysql.com/doc/refman/8.4/en/firewall-information-schema-tables.html>
- MySQL 8.4 Reference Manual, `MYSQL_FIREWALL_USERS`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-mysql-firewall-users-table.html>
- MySQL 8.4 Reference Manual, `MYSQL_FIREWALL_WHITELIST`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-mysql-firewall-whitelist-table.html>
- MySQL 8.4 Reference Manual, thread pool `INFORMATION_SCHEMA` tables:
  <https://dev.mysql.com/doc/refman/8.4/en/thread-pool-information-schema-tables.html>
- MySQL 8.4 Reference Manual, NDB Cluster `INFORMATION_SCHEMA` tables:
  <https://dev.mysql.com/doc/refman/8.4/en/mysql-cluster-information-schema-tables.html>
- MySQL 8.4 Reference Manual, `ndb_transid_mysql_connection_map`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-ndb-transid-mysql-connection-map-table.html>
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_information_schema_conditional_table_absence_expectations.sh`.

The MySQL manual documents the firewall tables as Enterprise Firewall plugin
views, the thread-pool tables as deprecated Enterprise Thread Pool plugin
views, and the NDB transaction map as an NDB Cluster-specific
`INFORMATION_SCHEMA` plugin. These names are not present in the standard
MySQL 8.4.9 runtime used by this project.

## Supported Behavior

MyLite matches the standard target-runtime absence for the covered names.
Direct reads reject with MySQL's observed unknown-table diagnostic:

```sql
SELECT COUNT(*) FROM INFORMATION_SCHEMA.MYSQL_FIREWALL_USERS;
SELECT COUNT(*) FROM INFORMATION_SCHEMA.TP_THREAD_STATE;
SELECT COUNT(*) FROM INFORMATION_SCHEMA.ndb_transid_mysql_connection_map;
```

The diagnostic is `1109 / 42S02`, with message text:

```text
Unknown table '<name>' in information_schema
```

Metadata statements targeting the absent names also reject with the same
diagnostic:

```sql
SHOW COLUMNS FROM INFORMATION_SCHEMA.MYSQL_FIREWALL_USERS;
SHOW COLUMNS FROM INFORMATION_SCHEMA.TP_THREAD_GROUP_STATS;
```

After `USE information_schema`, unqualified direct reads of the covered names
use the same diagnostic.

The covered names are absent from public directory metadata:

```sql
SELECT COUNT(*)
  FROM INFORMATION_SCHEMA.TABLES
 WHERE TABLE_SCHEMA = 'information_schema'
   AND TABLE_NAME IN (...);

SELECT COUNT(*)
  FROM INFORMATION_SCHEMA.COLUMNS
 WHERE TABLE_SCHEMA = 'information_schema'
   AND TABLE_NAME IN (...);

SHOW FULL TABLES FROM information_schema
 WHERE Tables_in_information_schema IN (...);
```

These queries return zero rows or a zero count for the covered names.

## Syntax

No parser change is required. Existing MyLite grammar already admits the
targeted statement shapes:

```lemon
select_stmt ::= SELECT select_options select_list from_clause select_tail.
from_clause ::= FROM table_factor.
table_factor ::= qualified_name alias_opt index_hint_list_opt.
cmd ::= SHOW show_columns_kind FROM qualified_name show_columns_tail.
cmd ::= USE identifier.
```

This slice documents and tests the target-build absence path, and routes
`SHOW COLUMNS` targets for absent `INFORMATION_SCHEMA` names through the same
unknown-table diagnostic used by direct reads.

## Semantics

The covered names are not added to MyLite's `INFORMATION_SCHEMA` table
definition registry. They do not expose system-view columns, rows,
`INFORMATION_SCHEMA.TABLES` rows, `INFORMATION_SCHEMA.COLUMNS` rows, or
`SHOW FULL TABLES` directory entries.

MyLite's existing unknown `INFORMATION_SCHEMA` table resolution remains the
correct behavior for the target MySQL 8.4.9 build. If a future target runtime
enables one of the corresponding plugins, that should be implemented as a
separate feature with observed columns, rows, metadata, and plugin lifecycle
decisions.

## Diagnostics And Limits

- `SELECT` and `SHOW COLUMNS` for the covered absent names return
  `1109 / 42S02`.
- No placeholder empty views are created for these names in the standard
  target-build baseline.
- No Enterprise Firewall, Enterprise Thread Pool, NDB Cluster, or
  Performance Schema thread-pool table implementation is added.
- `SHOW PLUGINS` remains MyLite's current limited plugin catalog and does not
  expose the corresponding `INFORMATION SCHEMA` plugins.

## Ownership Boundary

- Public API: unchanged.
- Parser/AST: unchanged.
- Analyzer/runtime: no new table definitions; `SHOW COLUMNS` resolves
  `INFORMATION_SCHEMA` targets before catalog-schema lookup so absent
  information-schema names report the MySQL-shaped unknown-table diagnostic.
- Metadata: documentation and tests clarify target-build absence.
- Catalog/storage/SQLite: unchanged.

## MySQL Runtime Evidence

The recorded MySQL 8.4.9 probe verifies:

```sql
SELECT COUNT(*)
  FROM INFORMATION_SCHEMA.TABLES
 WHERE TABLE_SCHEMA = 'information_schema'
   AND TABLE_NAME IN ('MYSQL_FIREWALL_USERS',
                      'MYSQL_FIREWALL_WHITELIST',
                      'ndb_transid_mysql_connection_map',
                      'TP_THREAD_GROUP_STATE',
                      'TP_THREAD_GROUP_STATS',
                      'TP_THREAD_STATE');

SELECT COUNT(*) FROM INFORMATION_SCHEMA.MYSQL_FIREWALL_USERS;
SHOW COLUMNS FROM INFORMATION_SCHEMA.MYSQL_FIREWALL_USERS;
```

The target runtime returned zero directory rows and `1109 / 42S02` for direct
and metadata access to each covered name.

## Test Plan

- Add a MySQL expectation script that verifies:
  - direct reads reject with `1109 / 42S02`;
  - `SHOW COLUMNS` rejects with `1109 / 42S02`;
  - unqualified direct reads after `USE information_schema` reject with the
    same diagnostic;
  - `INFORMATION_SCHEMA.TABLES`, `INFORMATION_SCHEMA.COLUMNS`, and
    `SHOW FULL TABLES` do not list the covered names.
- Add focused C runtime coverage for the same MyLite behavior.
- Run:
  - `sh -n packages/libmylite/tests/mysql_baseline_information_schema_conditional_table_absence_expectations.sh`
  - `packages/libmylite/tests/mysql_baseline_information_schema_conditional_table_absence_expectations.sh`
  - `cmake --build --preset dev --target mylite_runtime_information_schema_conditional_table_absence_test`
  - `ctest --preset dev -R '^libmylite\.runtime\.information_schema_conditional_table_absence$' --output-on-failure`
  - `git diff --check`
  - `git diff --cached --check`
  - `cmake --workflow --preset check`
