# Baseline sys.sys_config Trigger Metadata

This slice adds MySQL-shaped metadata for the two built-in triggers on
`sys.sys_config`:

- `sys.sys_config_insert_set_user`
- `sys.sys_config_update_set_user`

MyLite exposes these rows through `INFORMATION_SCHEMA.TRIGGERS` and
`SHOW TRIGGERS`. It does not implement trigger execution, trigger DDL, writable
`sys.sys_config`, or persisted trigger descriptors.

## Compatibility Authority

- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.TRIGGERS`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-triggers-table.html>
- MySQL 8.4 Reference Manual, `SHOW TRIGGERS`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-triggers.html>
- MySQL 8.4 Reference Manual, `sys_config_insert_set_user`:
  <https://dev.mysql.com/doc/refman/8.4/en/sys-sys-config-insert-set-user.html>
- MySQL 8.4 Reference Manual, `sys_config_update_set_user`:
  <https://dev.mysql.com/doc/refman/8.4/en/sys-sys-config-update-set-user.html>
- MySQL 8.4 Reference Manual, `sys` schema overview:
  <https://dev.mysql.com/doc/refman/8.4/en/sys-schema.html>
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_sys_sys_config_triggers_expectations.sh`.

The MySQL manual describes the insert trigger as setting `set_by` to the current
user for inserted `sys_config` rows, and the update trigger as the analogous
behavior for updates. The manual also defines the public trigger metadata
surfaces. The target MySQL 8.4.9 runtime exposes the two sys triggers with
`DEFINER = 'mysql.sys@localhost'`, `CHARACTER_SET_CLIENT = 'utf8mb4'`,
`COLLATION_CONNECTION = 'utf8mb4_0900_ai_ci'`, and
`DATABASE_COLLATION = 'utf8mb4_0900_ai_ci'`.

## Supported Behavior

Supported metadata queries include:

```sql
SELECT TRIGGER_NAME, EVENT_MANIPULATION, EVENT_OBJECT_TABLE
  FROM INFORMATION_SCHEMA.TRIGGERS
 WHERE TRIGGER_SCHEMA = 'sys'
 ORDER BY TRIGGER_NAME;

SHOW TRIGGERS FROM sys LIKE 'sys_config';

USE sys;
SHOW FULL TRIGGERS LIKE 'sys_config';
```

The `INFORMATION_SCHEMA.TRIGGERS` rows are synthetic system rows. They pass
through the existing information-schema projection, alias, `WHERE`, `ORDER BY`,
and `LIMIT` planner. Queries report zero affected rows and preserve MySQL's
post-select `ROW_COUNT() = -1` behavior.

`SHOW TRIGGERS` resolves the selected schema or explicit `FROM` / `IN` schema
through the existing schema resolver. The `LIKE` pattern matches the trigger's
event object table name, not the trigger name, matching MySQL behavior. The
existing limited implementation still does not support `WHERE`.

## Trigger Rows

Both rows use:

- `TRIGGER_CATALOG = 'def'`
- `TRIGGER_SCHEMA = 'sys'`
- `EVENT_OBJECT_CATALOG = 'def'`
- `EVENT_OBJECT_SCHEMA = 'sys'`
- `EVENT_OBJECT_TABLE = 'sys_config'`
- `ACTION_ORDER = 1`
- `ACTION_CONDITION = NULL`
- `ACTION_ORIENTATION = 'ROW'`
- `ACTION_TIMING = 'BEFORE'`
- `ACTION_REFERENCE_OLD_TABLE = NULL`
- `ACTION_REFERENCE_NEW_TABLE = NULL`
- `ACTION_REFERENCE_OLD_ROW = 'OLD'`
- `ACTION_REFERENCE_NEW_ROW = 'NEW'`
- `SQL_MODE =
  'ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION'`
- `DEFINER = 'mysql.sys@localhost'`
- `CHARACTER_SET_CLIENT = 'utf8mb4'`
- `COLLATION_CONNECTION = 'utf8mb4_0900_ai_ci'`
- `DATABASE_COLLATION = 'utf8mb4_0900_ai_ci'`

The insert row has:

- `TRIGGER_NAME = 'sys_config_insert_set_user'`
- `EVENT_MANIPULATION = 'INSERT'`

The update row has:

- `TRIGGER_NAME = 'sys_config_update_set_user'`
- `EVENT_MANIPULATION = 'UPDATE'`

Both rows use this independently authored representation of the observed body:

```sql
BEGIN
    IF @sys.ignore_sys_config_triggers != true AND NEW.set_by IS NULL THEN
        SET NEW.set_by = USER();
    END IF;
END
```

MySQL stores an installation-time `CREATED` value with `TIMESTAMP(2)` display.
MyLite has no durable sys trigger installation timestamp, so it renders the
current statement timestamp with hundredths precision as a synthetic non-`NULL`
`CREATED` value.

## SHOW TRIGGERS

`SHOW TRIGGERS` and `SHOW FULL TRIGGERS` expose the MySQL 8.4.9 column labels:

| Column | Value source |
| --- | --- |
| `Trigger` | `TRIGGER_NAME` |
| `Event` | `EVENT_MANIPULATION` |
| `Table` | `EVENT_OBJECT_TABLE` |
| `Statement` | `ACTION_STATEMENT` |
| `Timing` | `ACTION_TIMING` |
| `Created` | `CREATED` |
| `sql_mode` | `SQL_MODE` |
| `Definer` | `DEFINER` |
| `character_set_client` | `CHARACTER_SET_CLIENT` |
| `collation_connection` | `COLLATION_CONNECTION` |
| `Database Collation` | `DATABASE_COLLATION` |

`SHOW TRIGGERS FROM sys LIKE 'sys_config'` returns both trigger rows.
`SHOW TRIGGERS FROM sys LIKE 'sys_config_insert%'` returns zero rows because
MySQL applies the pattern to table names.

## Unsupported Behavior

This slice intentionally does not implement:

- `CREATE TRIGGER`, `DROP TRIGGER`, or `SHOW CREATE TRIGGER`;
- trigger execution for `INSERT`, `UPDATE`, or `DELETE`;
- writable `sys.sys_config` rows or trigger side effects;
- persisted trigger descriptors, definers, privileges, or metadata locks;
- user-created trigger rows for ordinary MyLite catalog tables;
- `SHOW TRIGGERS ... WHERE` filtering.

Writes to `sys.sys_config` remain blocked by the built-in schema write guard.

## Parser And Grammar

No new grammar is required. The existing parser already accepts `SHOW TRIGGERS`,
`SHOW FULL TRIGGERS`, optional `FROM` / `IN` schema names, and optional `LIKE`
patterns. `INFORMATION_SCHEMA.TRIGGERS` uses the existing limited
information-schema query grammar.

## Architecture

- Public API: unchanged.
- Parser/AST: unchanged.
- Runtime metadata: adds two static sys trigger descriptors consumed by
  information-schema and SHOW paths.
- `INFORMATION_SCHEMA.TRIGGERS`: appends the two sys rows as synthetic system
  rows before the existing catalog row phase, which remains empty for triggers.
- `SHOW TRIGGERS`: appends matching sys trigger rows when the resolved schema is
  `sys`.
- Storage/SQLite: unchanged. No physical trigger or SQLite catalog mutation is
  required.

## Performance

The feature adds two static rows. Query execution remains bounded and uses the
existing in-memory metadata row filtering and projection paths.

## Tests

MySQL 8.4.9 expectation coverage:

- exact trigger names, events, object table, action body, definer, SQL mode,
  charset, connection collation, and database collation;
- non-`NULL` `CREATED` values;
- `SHOW TRIGGERS` and `SHOW FULL TRIGGERS` rows;
- `LIKE` pattern behavior over the table name rather than trigger name.

MyLite runtime coverage:

- `INFORMATION_SCHEMA.TRIGGERS` rows for `sys`;
- selected-schema and explicit-schema `SHOW TRIGGERS` rows;
- `LIKE` filtering over `sys_config`;
- empty trigger-name-shaped `LIKE` pattern result;
- row-count status after a metadata query.
