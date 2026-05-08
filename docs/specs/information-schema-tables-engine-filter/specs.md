# INFORMATION_SCHEMA.TABLES ENGINE Filter

## Scope

This feature originally extended MyLite's `INFORMATION_SCHEMA.TABLES` support
beyond `SELECT *` so applications can run common metadata probes that filter by
the storage engine:

- `SELECT TABLE_NAME, ENGINE FROM information_schema.TABLES WHERE ...`
- `SELECT TABLE_NAME FROM information_schema.TABLES WHERE ...`
- `WHERE TABLE_SCHEMA = '<schema>'`
- `WHERE TABLE_SCHEMA = DATABASE()`
- `WHERE TABLE_NAME = '<table>'`
- `WHERE ENGINE = '<engine>'`
- `WHERE ENGINE IS NULL`
- conjunctions of the supported predicates using `AND`
- optional `ORDER BY TABLE_NAME`
- optional bare or `AS` table aliases, including qualified column references
  such as `t.ENGINE`

The narrow filter path is now superseded by the composable
`information_schema` system-view path described in
[Composable `information_schema` SELECTs](../information-schema-composable-select/specs.md).
The engine-filter probes below remain compatibility examples that must continue
to work through the general `SELECT` binder and runtime. Privilege filtering
and exact MySQL field metadata remain deferred.

The motivating MySQL-compatible behavior is metadata introspection for user
base tables created by MyLite's supported `CREATE TABLE` subset. Since MyLite
currently exposes those tables as the SQLite-backed `InnoDB` facade, filters
on `ENGINE = 'InnoDB'` should return the matching base-table rows.

## Sources

- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA` `TABLES` table:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-tables-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA` introduction:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-introduction.html
- MySQL 8.4 Reference Manual, `SHOW TABLE STATUS` equivalence:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-tables-table.html
- Existing MyLite specs:
  - `docs/specs/core-metadata-catalog/specs.md`
  - `docs/specs/create-table-base-execution/specs.md`
  - `docs/specs/show-table-status/specs.md`
  - `docs/specs/show-tables/specs.md`
  - `docs/specs/show-engines/specs.md`

Runtime observations were verified against Docker container
`mylite-mysql-849-regexp` running MySQL `8.4.9` with:

```sh
docker exec -i mylite-mysql-849-regexp mysql -h127.0.0.1 -uroot -pmylite \
  --batch --raw --column-type-info --show-warnings --force
```

This specification is independently authored from official documentation and
observed runtime behavior. It does not copy MySQL grammar, documentation prose,
or implementation sources.

## MySQL 8.4.9 Behavior Summary

`INFORMATION_SCHEMA.TABLES` includes one row per visible table or view. The
`ENGINE` column contains a storage engine name for base tables and `NULL` for
`INFORMATION_SCHEMA` system views.
System-view rows expose a stable non-NULL `CREATE_TIME` for the lifetime of the
connection; the value matches `SHOW TABLE STATUS FROM information_schema`.

Verified probes:

| SQL | MySQL outcome |
| --- | --- |
| `SELECT TABLE_NAME, ENGINE FROM information_schema.TABLES WHERE TABLE_SCHEMA = 'db' AND ENGINE = 'InnoDB' ORDER BY TABLE_NAME` | Returns user base tables in name order with `ENGINE='InnoDB'`. |
| `SELECT TABLE_NAME, ENGINE FROM information_schema.TABLES WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'alpha' AND ENGINE = 'InnoDB'` | Returns the selected table when the default schema is `db`. |
| same query with `ENGINE = 'innodb'` | Returns the same row on the verified runtime; the comparison is case-insensitive under the connection collation. |
| `SELECT TABLE_NAME, ENGINE FROM information_schema.TABLES WHERE TABLE_SCHEMA = 'information_schema' AND ENGINE IS NULL AND TABLE_NAME = 'TABLES'` | Returns the `TABLES` system-view row with `ENGINE=NULL`. |
| `SELECT TABLE_NAME FROM information_schema.TABLES WHERE ENGINE = 'InnoDB' AND TABLE_SCHEMA = DATABASE() ORDER BY TABLE_NAME` | Returns only table names for matching user base tables. |

The official `TABLES` table also exposes many statistic columns. This feature
does not change the existing MyLite `SELECT *` column shape or values.

## MyLite Behavior

MyLite keeps the existing `SELECT * FROM INFORMATION_SCHEMA.TABLES` behavior and
also exposes `TABLES` as a read-only system view that composes with ordinary
projection, aliases, expressions, `WHERE`, `IN`, `OR`, grouping, aggregates,
ordering, distinct, and limits where those `SELECT` features are otherwise
implemented.

Identifier matching for `information_schema` schema/table names is
case-insensitive. Column references, expression semantics, and string
comparison behavior are delegated to the regular MyLite `SELECT` runtime.
MyLite's broader collation engine remains deferred.

## Execution

The implementation can synthesize SQLite SQL from the existing
`information_schema_tables_sql` source query and add a simple outer `WHERE`
and `ORDER BY` over supported predicates. Predicate values are embedded with
SQLite-safe quoting or bound parameters. `DATABASE()` is resolved from the
handle-owned selected schema; with no selected schema it evaluates to `NULL`,
so `TABLE_SCHEMA = DATABASE()` matches no rows.

Rows and columns must keep the existing names:

- `TABLE_NAME`
- `ENGINE`

No result metadata ABI changes are required beyond the names already produced
by SQLite-backed statement preparation.

## Test Expectations

Implementation tests should cover:

- `TABLE_NAME, ENGINE` projection filtered by literal `TABLE_SCHEMA` and
  `ENGINE='InnoDB'`
- `TABLE_NAME` projection filtered by `TABLE_SCHEMA = DATABASE()` and
  `ENGINE='InnoDB'`
- case-insensitive engine literal comparison with `ENGINE='innodb'`
- `TABLE_NAME` equality combined with `ENGINE`
- `ENGINE IS NULL` for an `information_schema` system view
- aliases and qualified references such as `t.ENGINE = 'InnoDB'`
- `OR`, `IN`, aggregate expressions, grouping, aliases, and ordered projection
  queries through the general system-view path

## Compatibility Status

After implementation and tests, the core metadata catalog remains a scoped
information-schema implementation, but supported `information_schema` tables are
selectable through the regular `SELECT` path. Suggested compatibility wording
should state that `INFORMATION_SCHEMA.TABLES` supports composable metadata
queries over its current row set, including the engine-based probes above.
