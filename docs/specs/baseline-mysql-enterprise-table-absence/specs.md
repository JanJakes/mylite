# Baseline mysql Enterprise table absence

This slice documents and tests MyLite's target-build behavior for Enterprise
Audit and Enterprise Firewall `mysql` schema tables that are documented for
MySQL 8.4 but are absent from the standard MySQL 8.4.9 runtime used by
MyLite's compatibility suite.

The covered names are:

- `mysql.audit_log_filter`
- `mysql.audit_log_user`
- `mysql.firewall_group_allowlist`
- `mysql.firewall_groups`
- `mysql.firewall_membership`
- `mysql.firewall_users`
- `mysql.firewall_whitelist`

## Compatibility Authority

- MySQL 8.4 Reference Manual, `mysql` system schema:
  <https://dev.mysql.com/doc/refman/8.4/en/system-schema.html>
- MySQL 8.4 Reference Manual, MySQL Enterprise Audit:
  <https://dev.mysql.com/doc/refman/8.4/en/audit-log.html>
- MySQL 8.4 Reference Manual, audit log filtering:
  <https://dev.mysql.com/doc/refman/8.4/en/audit-log-filtering.html>
- MySQL 8.4 Reference Manual, audit log reference:
  <https://dev.mysql.com/doc/refman/8.4/en/audit-log-reference.html>
- MySQL 8.4 Reference Manual, MySQL Enterprise Firewall reference:
  <https://dev.mysql.com/doc/refman/8.4/en/firewall-reference.html>
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_mysql_enterprise_table_absence_expectations.sh`.

The MySQL manual documents the audit tables as Enterprise Audit persistent
storage and the firewall tables as Enterprise Firewall persistent storage.
Those plugins and tables are not present in the standard target runtime used by
this project.

## Supported Behavior

MyLite matches the standard target-runtime absence for the covered names.
Direct reads reject with MySQL's observed table-not-found diagnostic:

```sql
SELECT COUNT(*) FROM mysql.audit_log_filter;
SELECT COUNT(*) FROM mysql.firewall_users;
USE mysql;
SELECT COUNT(*) FROM firewall_users;
```

The diagnostic is `1146 / 42S02`, with message text:

```text
Table 'mysql.<name>' doesn't exist
```

Metadata statements targeting the absent names also reject with the same
diagnostic:

```sql
SHOW COLUMNS FROM mysql.audit_log_filter;
DESCRIBE mysql.audit_log_user;
SHOW INDEX FROM mysql.firewall_groups;
```

The covered names are absent from public directory and metadata surfaces:

```sql
SELECT COUNT(*)
  FROM INFORMATION_SCHEMA.TABLES
 WHERE TABLE_SCHEMA = 'mysql'
   AND TABLE_NAME IN (...);

SELECT COUNT(*)
  FROM INFORMATION_SCHEMA.COLUMNS
 WHERE TABLE_SCHEMA = 'mysql'
   AND TABLE_NAME IN (...);

SELECT COUNT(*)
  FROM INFORMATION_SCHEMA.STATISTICS
 WHERE TABLE_SCHEMA = 'mysql'
   AND TABLE_NAME IN (...);

SHOW FULL TABLES FROM mysql
 WHERE Tables_in_mysql IN (...);

SHOW TABLE STATUS FROM mysql
 WHERE Name IN (...);
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
cmd ::= DESCRIBE qualified_name.
cmd ::= SHOW show_index_kind FROM qualified_name show_index_tail.
cmd ::= SHOW TABLE STATUS show_table_status_tail.
cmd ::= USE identifier.
```

This slice documents and tests target-build absence; it does not add grammar.

## Semantics

The covered names are not added to MyLite's queryable `mysql` system-table
definition registry. They do not expose column metadata, rows, indexes,
constraints, `INFORMATION_SCHEMA.TABLES` / `COLUMNS` / `STATISTICS` /
`TABLE_CONSTRAINTS` / `KEY_COLUMN_USAGE` / `TABLE_CONSTRAINTS_EXTENSIONS`
rows, `SHOW FULL TABLES` entries, or `SHOW TABLE STATUS` rows.

MyLite uses a narrow target-absent diagnostic path so direct reads and
metadata statements report MySQL's observed table-not-found diagnostic instead
of treating `mysql` as an unknown user schema. If a future target runtime
enables Enterprise Audit or Enterprise Firewall support, those tables should be
implemented as a separate feature with observed table shapes, rows, plugin
lifecycle behavior, privileges, and writable-table decisions.

## Diagnostics And Limits

- `SELECT`, `SHOW COLUMNS`, `DESCRIBE`, and `SHOW INDEX` for the covered
  absent names return `1146 / 42S02`.
- No placeholder empty tables are created for these names in the standard
  target-build baseline.
- No Enterprise Audit or Enterprise Firewall plugin, function, stored
  procedure, persistent table, Performance Schema cache table, or mutable
  firewall/audit state is added.
- No physical SQLite table, virtual table, or SQLite fork hook is added.

## Ownership Boundary

- Public API: unchanged.
- Parser/AST: unchanged.
- Analyzer/runtime: no new table definitions; a narrow target-absent diagnostic
  path reports MySQL-shaped table-not-found errors for direct reads.
- Metadata: documentation and tests clarify target-build absence.
- Catalog/storage/SQLite: unchanged.

## MySQL Runtime Evidence

The recorded MySQL 8.4.9 probe verifies:

```sql
SELECT COUNT(*) FROM mysql.audit_log_filter;
SHOW COLUMNS FROM mysql.audit_log_filter;
DESC mysql.audit_log_user;
SHOW INDEX FROM mysql.firewall_groups;
USE mysql;
SELECT COUNT(*) FROM firewall_users;
SELECT COUNT(*)
  FROM information_schema.tables
 WHERE TABLE_SCHEMA = 'mysql'
   AND TABLE_NAME IN ('audit_log_filter',
                      'audit_log_user',
                      'firewall_group_allowlist',
                      'firewall_groups',
                      'firewall_membership',
                      'firewall_users',
                      'firewall_whitelist');
SHOW FULL TABLES FROM mysql
 WHERE Tables_in_mysql IN (...);
SHOW TABLE STATUS FROM mysql
 WHERE Name IN (...);
```

The target runtime returned `1146 / 42S02` for direct and metadata access and
returned no directory rows.

## Test Plan

- Add a MySQL expectation script that verifies direct reads, representative
  metadata statements, selected-schema reads, and directory absence for the
  covered names.
- Add focused C runtime coverage for the same MyLite behavior.
- Run:
  - `sh -n packages/libmylite/tests/mysql_baseline_mysql_enterprise_table_absence_expectations.sh`
  - `packages/libmylite/tests/mysql_baseline_mysql_enterprise_table_absence_expectations.sh`
  - `cmake --build --preset dev --target mylite_runtime_mysql_enterprise_table_absence_test`
  - `ctest --preset dev -R '^libmylite\.runtime\.mysql_enterprise_table_absence$' --output-on-failure`
  - `git diff --check`
  - `git diff --cached --check`
  - `cmake --workflow --preset check`
