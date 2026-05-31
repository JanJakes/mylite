# Baseline sys.version View Definition Metadata

This slice completes the first `sys.version` catalog surface by exposing the
view-definition metadata that MySQL reports for the built-in sys-schema version
view. MyLite keeps `sys.version` synthetic and read-only, but reports the
MySQL-shaped `INFORMATION_SCHEMA.VIEWS`, `SHOW CREATE VIEW`, and
`SHOW CREATE TABLE` rows for that view.

## Compatibility Authority

- MySQL 8.4 Reference Manual, `sys.version`:
  <https://dev.mysql.com/doc/refman/8.4/en/sys-version.html>
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.VIEWS`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-views-table.html>
- MySQL 8.4 Reference Manual, `SHOW CREATE VIEW`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-create-view.html>
- MySQL 8.4 Reference Manual, `SHOW CREATE TABLE`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-create-table.html>
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_sys_version_view_definition_expectations.sh`.

Runtime probes against MySQL 8.4.9 show one `INFORMATION_SCHEMA.VIEWS` row for
`sys.version`, no `VIEW_TABLE_USAGE` or `VIEW_ROUTINE_USAGE` dependency rows,
and `SHOW CREATE VIEW` / `SHOW CREATE TABLE` output whose view name is schema
qualified only when the statement target is schema qualified.

## Supported Behavior

`INFORMATION_SCHEMA.VIEWS` exposes one built-in row:

| Column | Value |
| --- | --- |
| `TABLE_CATALOG` | `def` |
| `TABLE_SCHEMA` | `sys` |
| `TABLE_NAME` | `version` |
| `VIEW_DEFINITION` | `select '2.1.3' AS \`sys_version\`,version() AS \`mysql_version\`` |
| `CHECK_OPTION` | `NONE` |
| `IS_UPDATABLE` | `NO` |
| `DEFINER` | `mysql.sys@localhost` |
| `SECURITY_TYPE` | `INVOKER` |
| `CHARACTER_SET_CLIENT` | `utf8mb4` |
| `COLLATION_CONNECTION` | `utf8mb4_0900_ai_ci` |

Supported SHOW forms:

```sql
SHOW CREATE VIEW sys.version;
SHOW CREATE TABLE sys.version;

USE sys;
SHOW CREATE VIEW version;
SHOW CREATE TABLE version;
```

Qualified targets render the view name as `` `sys`.`version` ``. Unqualified
targets resolved through `USE sys` render the view name as `` `version` ``.
Both forms return MySQL-shaped result columns `View`, `Create View`,
`character_set_client`, and `collation_connection`.

The returned `Create View` text uses MySQL's observed sys definition:

```sql
CREATE ALGORITHM=UNDEFINED DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW `sys`.`version` (`sys_version`,`mysql_version`) AS select '2.1.3' AS `sys_version`,version() AS `mysql_version`
```

The unqualified selected-schema form is identical except for the view name.

## Dependency Metadata

MySQL 8.4.9 reports zero rows for:

- `INFORMATION_SCHEMA.VIEW_TABLE_USAGE` where `VIEW_SCHEMA = 'sys'` and
  `VIEW_NAME = 'version'`;
- `INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE` where `TABLE_SCHEMA = 'sys'` and
  `TABLE_NAME = 'version'`.

MyLite follows that baseline and does not add synthetic dependency rows.

## Unsupported Behavior

This slice intentionally does not implement:

- the broader sys-schema view catalog;
- persisted catalog descriptors for built-in sys views;
- physical SQLite view objects;
- sys helper functions beyond the existing `version()` use in rendered text;
- privilege filtering, definer validation, or SQL SECURITY enforcement;
- mutable sys-schema installation state.

`sys.version` remains read-only through the existing built-in schema write
guard. The `VIEW_DEFINITION` text is metadata; direct reads continue to return
MyLite's fixed MySQL-compatible server-version value from the existing
synthetic row path.

## Parser And Grammar

No grammar changes are required. The slice uses existing `SHOW CREATE VIEW`,
`SHOW CREATE TABLE`, qualified table references, selected-schema resolution, and
`INFORMATION_SCHEMA` query support.

## Architecture

- Public API: unchanged.
- Parser/AST: unchanged.
- Runtime metadata: adds static `sys.version` view-definition constants next to
  the existing synthetic `sys.version` column descriptor.
- Information schema: appends a built-in `INFORMATION_SCHEMA.VIEWS` row before
  descriptor-backed user view rows.
- SHOW metadata: short-circuits `SHOW CREATE VIEW` and `SHOW CREATE TABLE` for
  the resolved `sys.version` target, then renders the same result shape used by
  descriptor-backed views.
- Storage/SQLite: unchanged. No catalog descriptor, physical view, or SQLite
  fork hook is required.

## Tests

MySQL 8.4.9 expectation coverage:

- exact `INFORMATION_SCHEMA.VIEWS` row for `sys.version`;
- qualified and selected-schema `SHOW CREATE VIEW`;
- qualified and selected-schema `SHOW CREATE TABLE`;
- `ROW_COUNT() = -1` and zero warnings after `SHOW CREATE VIEW`;
- empty `VIEW_TABLE_USAGE` and `VIEW_ROUTINE_USAGE` dependency metadata.

MyLite runtime coverage extends `runtime_sys_version_view_test.c` with the same
view-definition metadata and empty dependency counts.
