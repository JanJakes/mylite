# Baseline INFORMATION_SCHEMA Privileges

## Status

This phase adds a narrow privilege-metadata compatibility surface for:

- `INFORMATION_SCHEMA.USER_PRIVILEGES`
- `INFORMATION_SCHEMA.SCHEMA_PRIVILEGES`
- `INFORMATION_SCHEMA.TABLE_PRIVILEGES`
- `INFORMATION_SCHEMA.COLUMN_PRIVILEGES`

MyLite has no account store, authentication, role graph, grant descriptors, or
privilege enforcement. The supported behavior is therefore synthetic metadata:
`USER_PRIVILEGES` exposes MyLite's embedded `root@%` identity as globally
privileged, and schema/table/column privilege views are queryable MySQL-shaped
system views with no rows until explicit grant descriptors are designed.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing current-user identity surface:
  `docs/specs/baseline-current-user-identity/specs.md`
- Existing information-schema core:
  `docs/specs/baseline-information-schema-core/specs.md`
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA` introduction:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema.html
- MySQL 8.4 Reference Manual,
  `INFORMATION_SCHEMA.USER_PRIVILEGES`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-user-privileges-table.html
- MySQL 8.4 Reference Manual,
  `INFORMATION_SCHEMA.SCHEMA_PRIVILEGES`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-schema-privileges-table.html
- MySQL 8.4 Reference Manual,
  `INFORMATION_SCHEMA.TABLE_PRIVILEGES`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-table-privileges-table.html
- MySQL 8.4 Reference Manual,
  `INFORMATION_SCHEMA.COLUMN_PRIVILEGES`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-column-privileges-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.TABLES`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-tables-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.COLUMNS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-columns-table.html
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- Observed MySQL 8.4.9 runtime behavior recorded by
  `packages/libmylite/tests/mysql_baseline_information_schema_privileges_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against the local `mysql:8.4.9` runtime using TCP:

- `USER_PRIVILEGES` columns are `GRANTEE`, `TABLE_CATALOG`,
  `PRIVILEGE_TYPE`, and `IS_GRANTABLE`.
- `SCHEMA_PRIVILEGES` adds `TABLE_SCHEMA` between `TABLE_CATALOG` and
  `PRIVILEGE_TYPE`.
- `TABLE_PRIVILEGES` adds `TABLE_SCHEMA` and `TABLE_NAME`.
- `COLUMN_PRIVILEGES` adds `TABLE_SCHEMA`, `TABLE_NAME`, and `COLUMN_NAME`.
- Every column is non-null `varchar` metadata using `utf8mb3` and
  `utf8mb3_general_ci`. `COLUMN_DEFAULT` is the empty string.
- `GRANTEE` metadata is `varchar(292)` with character length `97`.
- `TABLE_CATALOG` metadata is `varchar(512)` with character length `170`.
- Schema, table, column, and privilege-name metadata is `varchar(64)` with
  character length `21`.
- `IS_GRANTABLE` metadata is `varchar(3)` with character length `1`.
- `INFORMATION_SCHEMA.TABLES` contains system-view rows for all four tables
  with `TABLE_TYPE = 'SYSTEM VIEW'`, `ENGINE = NULL`, `VERSION = 10`,
  `ROW_FORMAT = NULL`, `TABLE_ROWS = 0`, `DATA_LENGTH = 0`, and
  `AUTO_INCREMENT = NULL`.
- For the root account in the target MySQL 8.4.9 container,
  `USER_PRIVILEGES` contains 70 rows for `GRANTEE = '''root''@''%'''`,
  each with `TABLE_CATALOG = 'def'` and `IS_GRANTABLE = 'YES'`.
- The target MySQL 8.4.9 container has no `COLUMN_PRIVILEGES` rows, and only
  lower-level schema/table rows for built-in system accounts. MyLite does not
  model those accounts in this phase.
- Successful supported `SELECT` statements return warning count `0` and make
  the following `ROW_COUNT()` return `-1`.

## Scope

The implementation must add:

- four synthetic information-schema table definitions;
- one `USER_PRIVILEGES` row for each observed MySQL 8.4.9 global privilege
  granted to the embedded `root@%` identity, with `IS_GRANTABLE = 'YES'`;
- no `SCHEMA_PRIVILEGES`, `TABLE_PRIVILEGES`, or `COLUMN_PRIVILEGES` rows
  until MyLite has explicit grant descriptors;
- matching `INFORMATION_SCHEMA.TABLES` system-view rows;
- matching `INFORMATION_SCHEMA.COLUMNS` rows for every metadata column;
- reuse of the existing information-schema query surface: wildcard
  projection, explicit metadata-column projection, aliases, `COUNT(*)`,
  supported predicates, one-column `ORDER BY`, and `LIMIT row_count`;
- successful result-set behavior through existing public result conventions:
  affected rows `0`, warning count `0`, and subsequent `ROW_COUNT() = -1`;
- fast C tests plus a reproducible MySQL 8.4.9 expectation script.

## Non-Goals

This feature must not implement:

- account storage, authentication, passwords, roles, grant descriptors, grant
  DDL, revocation DDL, `SHOW GRANTS`, `mysql.*` privilege tables, privilege
  enforcement, definer privilege checks, or privilege-dependent row filtering;
- schema/table/column privilege rows;
- `mysql.session`, `mysql.sys`, or other MySQL system-account rows;
- dynamic privilege discovery from plugins or host state;
- joins, grouping, or wider information-schema query support;
- physical `information_schema` SQLite tables, storage-format changes, or
  SQLite fork patches.

## Ownership Boundary

- Public API: unchanged. Applications use `mylite_execute()` and existing
  result accessors.
- Statement context: no new state. Existing SELECT result-set handling owns
  diagnostics reset, warning count, and previous row-count updates.
- Parser/AST: no grammar changes. The existing `SELECT ... FROM
  INFORMATION_SCHEMA.table_name` syntax is reused.
- Analyzer/planner: the existing information-schema query resolver owns
  source matching, projection, aliases, predicates, ordering, and limits
  against synthetic table definitions.
- Catalog module: no persistent catalog rows are read or written. Future
  grant descriptors must be designed before lower-level privilege tables emit
  rows.
- Result builder: emits MySQL-shaped text metadata values through
  `mylite_result`.
- Storage/VFS: no `.mylite` preamble, shifted SQLite payload, or VFS behavior
  changes.
- SQLite physical storage: no metadata introspection or fork patch is needed;
  these are MyLite-owned synthetic metadata views.

## Supported Query Surface

No new Lemon grammar is required. The existing limited information-schema
`SELECT` surface admits:

```sql
SELECT select_list
FROM INFORMATION_SCHEMA.privilege_table [AS alias]
[WHERE supported_information_schema_predicate]
[ORDER BY one_information_schema_column [ASC | DESC]]
[LIMIT row_count]
```

The current information-schema limits still apply:

- wildcard projection, explicit metadata columns, aliases, and `COUNT(*)`;
- one source table;
- schema-qualified `INFORMATION_SCHEMA` source only;
- metadata predicates already supported by the information-schema planner;
- one-column `ORDER BY`;
- existing `LIMIT row_count` subset;
- no joins, subqueries, CTEs, expressions, `LIKE`, `IN`, `BETWEEN`, grouping,
  mutation, or DDL through these tables.

## Columns

`USER_PRIVILEGES`:

| Column | Type metadata | MyLite value source |
| --- | --- | --- |
| `GRANTEE` | non-null `varchar(292)`, `utf8mb3_general_ci` | Fixed `'root'@'%'` |
| `TABLE_CATALOG` | non-null `varchar(512)`, `utf8mb3_general_ci` | Fixed `def` |
| `PRIVILEGE_TYPE` | non-null `varchar(64)`, `utf8mb3_general_ci` | Static MySQL 8.4.9 global privilege list |
| `IS_GRANTABLE` | non-null `varchar(3)`, `utf8mb3_general_ci` | Fixed `YES` |

`SCHEMA_PRIVILEGES`:

| Column | Type metadata | MyLite rows |
| --- | --- | --- |
| `GRANTEE` | non-null `varchar(292)`, `utf8mb3_general_ci` | No rows |
| `TABLE_CATALOG` | non-null `varchar(512)`, `utf8mb3_general_ci` | No rows |
| `TABLE_SCHEMA` | non-null `varchar(64)`, `utf8mb3_general_ci` | No rows |
| `PRIVILEGE_TYPE` | non-null `varchar(64)`, `utf8mb3_general_ci` | No rows |
| `IS_GRANTABLE` | non-null `varchar(3)`, `utf8mb3_general_ci` | No rows |

`TABLE_PRIVILEGES`:

| Column | Type metadata | MyLite rows |
| --- | --- | --- |
| `GRANTEE` | non-null `varchar(292)`, `utf8mb3_general_ci` | No rows |
| `TABLE_CATALOG` | non-null `varchar(512)`, `utf8mb3_general_ci` | No rows |
| `TABLE_SCHEMA` | non-null `varchar(64)`, `utf8mb3_general_ci` | No rows |
| `TABLE_NAME` | non-null `varchar(64)`, `utf8mb3_general_ci` | No rows |
| `PRIVILEGE_TYPE` | non-null `varchar(64)`, `utf8mb3_general_ci` | No rows |
| `IS_GRANTABLE` | non-null `varchar(3)`, `utf8mb3_general_ci` | No rows |

`COLUMN_PRIVILEGES`:

| Column | Type metadata | MyLite rows |
| --- | --- | --- |
| `GRANTEE` | non-null `varchar(292)`, `utf8mb3_general_ci` | No rows |
| `TABLE_CATALOG` | non-null `varchar(512)`, `utf8mb3_general_ci` | No rows |
| `TABLE_SCHEMA` | non-null `varchar(64)`, `utf8mb3_general_ci` | No rows |
| `TABLE_NAME` | non-null `varchar(64)`, `utf8mb3_general_ci` | No rows |
| `COLUMN_NAME` | non-null `varchar(64)`, `utf8mb3_general_ci` | No rows |
| `PRIVILEGE_TYPE` | non-null `varchar(64)`, `utf8mb3_general_ci` | No rows |
| `IS_GRANTABLE` | non-null `varchar(3)`, `utf8mb3_general_ci` | No rows |

## `USER_PRIVILEGES` Rows

MyLite must expose these `PRIVILEGE_TYPE` rows for `GRANTEE = '''root''@''%'''`,
`TABLE_CATALOG = 'def'`, and `IS_GRANTABLE = 'YES'`, ordered here for
deterministic tests:

`ALLOW_NONEXISTENT_DEFINER`, `ALTER`, `ALTER ROUTINE`,
`APPLICATION_PASSWORD_ADMIN`, `AUDIT_ABORT_EXEMPT`, `AUDIT_ADMIN`,
`AUTHENTICATION_POLICY_ADMIN`, `BACKUP_ADMIN`, `BINLOG_ADMIN`,
`BINLOG_ENCRYPTION_ADMIN`, `CLONE_ADMIN`, `CONNECTION_ADMIN`, `CREATE`,
`CREATE ROLE`, `CREATE ROUTINE`, `CREATE TABLESPACE`,
`CREATE TEMPORARY TABLES`, `CREATE USER`, `CREATE VIEW`, `DELETE`, `DROP`,
`DROP ROLE`, `ENCRYPTION_KEY_ADMIN`, `EVENT`, `EXECUTE`, `FILE`,
`FIREWALL_EXEMPT`, `FLUSH_OPTIMIZER_COSTS`, `FLUSH_PRIVILEGES`,
`FLUSH_STATUS`, `FLUSH_TABLES`, `FLUSH_USER_RESOURCES`,
`GROUP_REPLICATION_ADMIN`, `GROUP_REPLICATION_STREAM`, `INDEX`,
`INNODB_REDO_LOG_ARCHIVE`, `INNODB_REDO_LOG_ENABLE`, `INSERT`,
`LOCK TABLES`, `OPTIMIZE_LOCAL_TABLE`, `PASSWORDLESS_USER_ADMIN`,
`PERSIST_RO_VARIABLES_ADMIN`, `PROCESS`, `REFERENCES`, `RELOAD`,
`REPLICATION CLIENT`, `REPLICATION SLAVE`, `REPLICATION_APPLIER`,
`REPLICATION_SLAVE_ADMIN`, `RESOURCE_GROUP_ADMIN`, `RESOURCE_GROUP_USER`,
`ROLE_ADMIN`, `SELECT`, `SENSITIVE_VARIABLES_OBSERVER`,
`SERVICE_CONNECTION_ADMIN`, `SESSION_VARIABLES_ADMIN`, `SET_ANY_DEFINER`,
`SHOW DATABASES`, `SHOW VIEW`, `SHOW_ROUTINE`, `SHUTDOWN`, `SUPER`,
`SYSTEM_USER`, `SYSTEM_VARIABLES_ADMIN`, `TABLE_ENCRYPTION_ADMIN`,
`TELEMETRY_LOG_ADMIN`, `TRANSACTION_GTID_TAG`, `TRIGGER`, `UPDATE`, and
`XA_RECOVER_ADMIN`.

## System Metadata

`INFORMATION_SCHEMA.TABLES` must include one system-view row for each table:
`COLUMN_PRIVILEGES`, `SCHEMA_PRIVILEGES`, `TABLE_PRIVILEGES`, and
`USER_PRIVILEGES`.

Each row uses:

| Column | Value |
| --- | --- |
| `TABLE_CATALOG` | `def` |
| `TABLE_SCHEMA` | `information_schema` |
| `TABLE_TYPE` | `SYSTEM VIEW` |
| `ENGINE` | SQL `NULL` |
| `VERSION` | `10` |
| `ROW_FORMAT` | SQL `NULL` |
| `TABLE_ROWS` | `0` |
| `AVG_ROW_LENGTH` | `0` |
| `DATA_LENGTH` | `0` |
| `MAX_DATA_LENGTH` | `0` |
| `INDEX_LENGTH` | `0` |
| `DATA_FREE` | `0` |
| `AUTO_INCREMENT` | SQL `NULL` |
| `CREATE_TIME` / `UPDATE_TIME` / `CHECK_TIME` | SQL `NULL` |
| `TABLE_COLLATION` | `utf8mb3_general_ci` |
| `CHECKSUM` | SQL `NULL` |
| `CREATE_OPTIONS` | empty string |
| `TABLE_COMMENT` | empty string |

`INFORMATION_SCHEMA.COLUMNS` must expose observed metadata rows for each
privilege table. `PRIVILEGES` is the fixed system-view value `select`,
`COLUMN_KEY` and `EXTRA` are empty strings, `COLUMN_COMMENT` and
`GENERATION_EXPRESSION` are empty strings, and `SRS_ID` is SQL `NULL`.

## Diagnostics

The implementation must preserve existing information-schema diagnostics:

- unsupported query shapes keep deterministic MyLite unsupported errors;
- unknown metadata columns fail with `1054 / 42S22`;
- unknown information-schema tables fail with `1109 / 42S02`;
- allocation failures use existing public API conventions.

Successful supported statements emit no warnings.

## Tests

Tests must cover:

- `USER_PRIVILEGES` root global privilege rows, including `COUNT(*) = 70`;
- `SCHEMA_PRIVILEGES`, `TABLE_PRIVILEGES`, and `COLUMN_PRIVILEGES` with zero
  rows;
- wildcard and explicit projections;
- aliases, supported predicates, one-column order, and `LIMIT`;
- `INFORMATION_SCHEMA.TABLES` system-view metadata for all four tables;
- `INFORMATION_SCHEMA.COLUMNS` metadata for all privilege columns;
- warning count `0`, affected rows `0`, and subsequent `ROW_COUNT() = -1`;
- unknown projection, predicate, and order columns through existing
  information-schema diagnostics;
- reopen persistence and independent handles;
- existing information-schema and privilege-adjacent tests.

## Performance And Storage

This feature adds static table descriptors, static column descriptors, and a
70-row static global privilege list. It does not scan user schemas for
privilege rows, does not query SQLite metadata, and does not alter the MyLite
file format. Query cost remains proportional to the small static row set.
