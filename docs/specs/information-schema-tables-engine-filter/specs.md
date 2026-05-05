# INFORMATION_SCHEMA.TABLES ENGINE Filter

## Scope

This feature extends MyLite's `INFORMATION_SCHEMA.TABLES` support beyond
`SELECT *` so applications can run common metadata probes that filter by the
storage engine:

- `SELECT TABLE_NAME, ENGINE FROM information_schema.TABLES WHERE ...`
- `SELECT TABLE_NAME FROM information_schema.TABLES WHERE ...`
- `WHERE TABLE_SCHEMA = '<schema>'`
- `WHERE TABLE_SCHEMA = DATABASE()`
- `WHERE TABLE_NAME = '<table>'`
- `WHERE ENGINE = '<engine>'`
- `WHERE ENGINE IS NULL`
- conjunctions of the supported predicates using `AND`
- optional `ORDER BY TABLE_NAME`

The implementation is intentionally narrow. It does not introduce a general
information-schema query engine, joins, arbitrary projection expressions,
`OR`, `LIKE`, aggregates, aliases, grouping, limits, subqueries, or privilege
filtering.

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

MyLite should keep the existing `SELECT * FROM INFORMATION_SCHEMA.TABLES`
behavior and add a specific prepared-query path for supported projections and
filters.

Supported projection lists:

- `*`
- `TABLE_NAME`
- `TABLE_NAME, ENGINE`

Supported filters:

- equality against string literals for `TABLE_SCHEMA`, `TABLE_NAME`, and
  `ENGINE`
- `TABLE_SCHEMA = DATABASE()` and `DATABASE() = TABLE_SCHEMA`
- reversed equality where the literal or function appears on the left
- `ENGINE IS NULL`
- `ENGINE IS NOT NULL` may remain unsupported until needed
- `AND` between supported predicates

Identifier matching for projection and predicate column names is
case-insensitive. String comparisons should use case-insensitive behavior for
the supported predicate columns in this slice, matching the observed
`ENGINE='innodb'` result and the existing MyLite case-insensitive system-view
resolution. MyLite's broader collation engine remains deferred.

Unsupported expression shapes must fall back to the existing unsupported
information-schema behavior rather than silently returning incorrect rows.

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
- unsupported predicates, such as `OR`, still return `MYLITE_UNSUPPORTED`
- unsupported projections beyond the scoped list still return
  `MYLITE_UNSUPPORTED`

## Compatibility Status

After implementation and tests, the core metadata catalog remains a scoped
information-schema implementation. Suggested compatibility wording should
state that `INFORMATION_SCHEMA.TABLES` supports `SELECT *` plus focused
`TABLE_NAME`/`ENGINE` projections with `TABLE_SCHEMA`, `TABLE_NAME`, and
`ENGINE` filters for engine-based metadata probes.
