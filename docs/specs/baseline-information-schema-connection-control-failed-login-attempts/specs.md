# Baseline INFORMATION_SCHEMA CONNECTION_CONTROL_FAILED_LOGIN_ATTEMPTS

## Status

This phase adds `INFORMATION_SCHEMA.CONNECTION_CONTROL_FAILED_LOGIN_ATTEMPTS`
as a queryable synthetic information-schema system view. MySQL exposes this
view when the connection-control failed-login-attempts information-schema
plugin is active. MyLite exposes an empty compatibility view directly so
applications can introspect the table shape without depending on a plugin
loader or account subsystem.

The slice is metadata-only. It does not add connection-control plugins,
authentication, account descriptors, failed-login counters, delayed login
behavior, connection-control system variables, or plugin inventory rows.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing static information-schema specs, especially
  `docs/specs/baseline-information-schema-column-statistics/specs.md`
- MySQL 8.4 Reference Manual,
  `INFORMATION_SCHEMA.CONNECTION_CONTROL_FAILED_LOGIN_ATTEMPTS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-connection-control-failed-login-attempts-table.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, and
existing MyLite source code. It does not copy MySQL, MariaDB, Percona, SQLite
implementation internals, or other restrictively licensed implementation
sources.

## MySQL 8.4.9 Observations

Runtime probes were run against the local `mysql:8.4.9` Docker runtime named
`mylite-mysql-849`.

Observed behavior shaping this slice:

- Before the connection-control information-schema plugin is active,
  `INFORMATION_SCHEMA.TABLES` has no
  `CONNECTION_CONTROL_FAILED_LOGIN_ATTEMPTS` row, and direct reads fail with
  unknown-table diagnostics.
- After installing `CONNECTION_CONTROL` and
  `CONNECTION_CONTROL_FAILED_LOGIN_ATTEMPTS` from `connection_control.so`,
  `INFORMATION_SCHEMA.PLUGINS` reports active `CONNECTION_CONTROL` and
  `CONNECTION_CONTROL_FAILED_LOGIN_ATTEMPTS` plugin rows.
- With the plugin active,
  `INFORMATION_SCHEMA.CONNECTION_CONTROL_FAILED_LOGIN_ATTEMPTS` exists as an
  `information_schema` `SYSTEM VIEW`.
- `INFORMATION_SCHEMA.TABLES` reports the system row with
  `TABLE_SCHEMA = 'information_schema'`,
  `TABLE_NAME = 'CONNECTION_CONTROL_FAILED_LOGIN_ATTEMPTS'`,
  `TABLE_TYPE = 'SYSTEM VIEW'`, `ENGINE = NULL`, `VERSION = 10`,
  `ROW_FORMAT = NULL`, `TABLE_ROWS = 0`, `DATA_LENGTH = 0`, and
  `AUTO_INCREMENT = NULL`.
- `INFORMATION_SCHEMA.COLUMNS` reports two columns in order: `USERHOST` and
  `FAILED_ATTEMPTS`.
- `USERHOST` is non-null `varchar(357)`, character set `utf8mb3`, collation
  `utf8mb3_general_ci`, character maximum length `119`, octet length `357`,
  and `COLUMN_DEFAULT` is the empty string.
- `FAILED_ATTEMPTS` is non-null `int unsigned`, with SQL `NULL` numeric and
  character metadata, and `COLUMN_DEFAULT` is the empty string.
- A newly activated runtime with no failed login attempts returned zero rows.
- Successful reads leave `@@warning_count = 0`, and `ROW_COUNT()` reports
  `-1`.

Representative probe:

```sh
docker exec -i mylite-mysql-849 mysql --protocol=TCP -h127.0.0.1 -uroot \
  --batch --raw --skip-column-names <<'SQL'
INSTALL PLUGIN CONNECTION_CONTROL SONAME 'connection_control.so';
INSTALL PLUGIN CONNECTION_CONTROL_FAILED_LOGIN_ATTEMPTS SONAME 'connection_control.so';
SELECT VERSION();
SELECT PLUGIN_NAME,PLUGIN_STATUS,PLUGIN_TYPE,PLUGIN_LIBRARY
  FROM INFORMATION_SCHEMA.PLUGINS
 WHERE PLUGIN_NAME LIKE 'CONNECTION_CONTROL%'
 ORDER BY PLUGIN_NAME;
SHOW FULL TABLES FROM INFORMATION_SCHEMA
 WHERE Tables_in_INFORMATION_SCHEMA =
       'CONNECTION_CONTROL_FAILED_LOGIN_ATTEMPTS';
SELECT TABLE_SCHEMA,TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,
       TABLE_ROWS,DATA_LENGTH,AUTO_INCREMENT
  FROM INFORMATION_SCHEMA.TABLES
 WHERE TABLE_SCHEMA='information_schema'
   AND TABLE_NAME='CONNECTION_CONTROL_FAILED_LOGIN_ATTEMPTS';
SELECT TABLE_NAME,COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,
       DATA_TYPE,CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,
       NUMERIC_PRECISION,NUMERIC_SCALE,DATETIME_PRECISION,
       CHARACTER_SET_NAME,COLLATION_NAME,COLUMN_TYPE,PRIVILEGES
  FROM INFORMATION_SCHEMA.COLUMNS
 WHERE TABLE_SCHEMA='information_schema'
   AND TABLE_NAME='CONNECTION_CONTROL_FAILED_LOGIN_ATTEMPTS'
 ORDER BY ORDINAL_POSITION;
SELECT COUNT(*) FROM INFORMATION_SCHEMA.CONNECTION_CONTROL_FAILED_LOGIN_ATTEMPTS;
USE information_schema;
SELECT COUNT(*) FROM CONNECTION_CONTROL_FAILED_LOGIN_ATTEMPTS
 WHERE FAILED_ATTEMPTS > 0;
SELECT @@warning_count, ROW_COUNT();
SQL
```

## Scope

Supported:

- `SELECT` from
  `INFORMATION_SCHEMA.CONNECTION_CONTROL_FAILED_LOGIN_ATTEMPTS` using the
  existing information-schema query subset;
- wildcard projection with the MySQL-observed column order;
- case-insensitive information-schema table name lookup;
- table aliases, simple predicates, and `COUNT(*)` through the existing
  metadata query path;
- unqualified reads while `information_schema` is the selected schema;
- stable empty-row behavior, including after ordinary MyLite schema and table
  activity;
- system metadata through `INFORMATION_SCHEMA.TABLES`,
  `INFORMATION_SCHEMA.COLUMNS`, `SHOW TABLES`, `SHOW FULL TABLES`, and
  `SHOW TABLE STATUS` via the existing built-in table directory.

Out of scope:

- connection-control plugin loading or `INSTALL PLUGIN` support;
- `INFORMATION_SCHEMA.PLUGINS` connection-control rows;
- authentication, accounts, password checking, failed-login tracking, or
  account lockout state;
- delayed login behavior or the connection-control system variables;
- privilege filtering or account-specific visibility;
- SQLite storage, VFS, extension, or fork changes.

## Semantics

MyLite registers
`INFORMATION_SCHEMA.CONNECTION_CONTROL_FAILED_LOGIN_ATTEMPTS` as a static
information-schema system view with two columns:

| Column | Nullability | Type metadata | MyLite value source |
| --- | --- | --- | --- |
| `USERHOST` | `NO` | `varchar(357)`, `utf8mb3_general_ci`, maximum length `119`, octet length `357`, empty default | none in this slice |
| `FAILED_ATTEMPTS` | `NO` | `int unsigned`, empty default, SQL `NULL` precision and scale | none in this slice |

The row set is always empty. Predicates, aliases, ordering, limits, wildcard
projection, and `COUNT(*)` use the same limited information-schema query
machinery as the neighboring static empty system views.

Because MyLite has no plugin activation subsystem, the table is visible
unconditionally once this feature is compiled in. This is an intentional
embedded compatibility decision: it avoids application unknown-table failures
while documenting that failed-login attempt data is unavailable.

## Parser, AST, and Runtime

No grammar change is required. The existing information-schema table-name path
recognizes the new static view name case-insensitively. No MyLite Lemon-syntax
snippet applies.

Runtime changes are limited to MyLite-owned metadata:

- add the table definition and column definitions to the static
  information-schema registry;
- add the name to the built-in schema table directory used by
  `SHOW TABLES`, `SHOW FULL TABLES`, and `SHOW TABLE STATUS`;
- make direct and catalog-backed reads return an empty row set.

No SQLite fork change is needed.

## Tests

The focused runtime test covers:

- wildcard projection and zero-row reads;
- `COUNT(*)`;
- case-insensitive table name lookup;
- alias and predicate handling;
- unqualified reads after `USE information_schema`;
- MySQL-shaped `INFORMATION_SCHEMA.TABLES` metadata;
- exact `INFORMATION_SCHEMA.COLUMNS` metadata for both columns, including the
  empty-string defaults;
- status behavior after a successful read.

The MySQL expectation script activates the connection-control plugins when
needed, verifies MySQL 8.4.9, and records the observed plugin, table, column,
row-count, unqualified-read, warning, and row-count status behavior.
