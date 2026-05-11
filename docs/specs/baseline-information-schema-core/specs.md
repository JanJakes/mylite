# Baseline INFORMATION_SCHEMA core

## Summary

This phase adds the first queryable `INFORMATION_SCHEMA` surface:
`INFORMATION_SCHEMA.SCHEMATA`, `INFORMATION_SCHEMA.TABLES`, and
`INFORMATION_SCHEMA.COLUMNS`.

The goal is practical application introspection. The implementation is a
synthetic MyLite result path backed by MyLite catalog descriptors, not physical
SQLite tables and not `sqlite_schema` reflection.

## Compatibility authority

Authoritative references:

- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA` introduction:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema.html>
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.SCHEMATA`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-schemata-table.html>
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.TABLES`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-tables-table.html>
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.COLUMNS`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-columns-table.html>

Observed MySQL runtime behavior must be verified with
`packages/libmylite/tests/mysql_baseline_information_schema_core_expectations.sh`
against MySQL 8.4.9.

## Included surface

Supported source tables:

- `INFORMATION_SCHEMA.SCHEMATA`
- `INFORMATION_SCHEMA.TABLES`
- `INFORMATION_SCHEMA.COLUMNS`

The schema and table names are matched ASCII case-insensitively for resolving
the system view itself. User schema, table, and column values returned inside
rows preserve the descriptor spelling.

Supported query shape:

```sql
SELECT select_items
FROM INFORMATION_SCHEMA.information_schema_table [AS] [alias]
[WHERE metadata_predicate]
[ORDER BY metadata_column [ASC|DESC]]
[LIMIT row_count]
```

Supported select items:

- `*`;
- unqualified metadata column names;
- source-table-qualified metadata column names when the source has no alias;
- source-alias-qualified metadata column names when the source has an alias;
- `COUNT(*)`, with optional alias.

Supported predicates:

- metadata column compared to a string literal with `=`, `<>`, or `!=`;
- numeric metadata column compared to a string literal containing parseable
  signed decimal integer text with `=`, `<>`, `!=`, `<`, `<=`, `>`, `>=`, or
  `<=>`; this uses numeric comparison and returns no warning for in-range
  values;
- metadata column compared to an unsigned integer literal with `=`, `<>`, `!=`,
  `<`, `<=`, `>`, `>=`, or `<=>`;
- metadata column compared to `DATABASE()` or `SCHEMA()` with `=`, `<>`, or
  `!=`;
- metadata column `IS NULL` and `IS NOT NULL`;
- `AND`, `OR`, `XOR`, `NOT`, and parentheses over the supported predicate atoms.

Text metadata comparisons use the declared metadata column collation for the
supported ASCII surface: `_bin` metadata remains case-sensitive, while the
admitted `_general_ci`, `_tolower_ci`, and `utf8mb4_0900_ai_ci` metadata
collations compare ASCII letters case-insensitively.

Supported ordering:

- one unqualified metadata column, source-table-qualified metadata column when
  the source has no alias, or source-alias-qualified metadata column when the
  source has an alias;
- optional `ASC` or `DESC`;
- SQL `NULL` values sort before non-`NULL` values for ascending order and after
  non-`NULL` values for descending order, matching the current baseline DML
  ordering policy.

Supported limits:

- `LIMIT row_count` where `row_count` is a nonnegative decimal integer literal
  accepted by the existing `SELECT` limit conversion.

## Deliberately excluded surface

This phase does not implement:

- unqualified access through selected `information_schema`;
- physical `information_schema` SQLite tables;
- `mysql`, `performance_schema`, or `sys` schema tables;
- joins, subqueries, CTEs, unions, windows, grouping other than `COUNT(*)`,
  aggregate functions other than `COUNT(*)`, expressions in projection,
  expression predicates, `LIKE`, `REGEXP`, `IN`, `BETWEEN`, parameters,
  prepared-statement metadata, privileges, roles, or metadata locks;
- protocol-grade field flags, charset metadata, origin metadata, or exact
  volatile timestamp/statistics fidelity;
- `INFORMATION_SCHEMA` tables beyond `SCHEMATA`, `TABLES`, and `COLUMNS`.

Unsupported `INFORMATION_SCHEMA` tables fail with MySQL-compatible
`ERROR 1109 (42S02): Unknown table '<name>' in information_schema`.

## Grammar

The existing `table_name`, `select_item_list`, `where_clause_opt`,
`order_clause_opt`, and `limit_clause_opt` grammar is reused. This phase only
widens predicate values enough to admit metadata string comparisons and current
database functions.

Independently authored Lemon-syntax snippets:

```lemon
identifier ::= TABLES.

predicate_atom ::= qualified_identifier comparison_operator metadata_predicate_value.
predicate_atom ::= qualified_identifier IS NULL.
predicate_atom ::= qualified_identifier IS NOT NULL.

metadata_predicate_value ::= INTEGER.
metadata_predicate_value ::= STRING.
metadata_predicate_value ::= DATABASE LPAREN RPAREN.
metadata_predicate_value ::= SCHEMA LPAREN RPAREN.
```

The parser remains intentionally narrower than MySQL. `LIKE`, `IN`, `BETWEEN`,
and arbitrary expressions are deferred for metadata predicates unless another
baseline phase specifies them.

## Result columns

`SCHEMATA` returns MySQL's documented columns:

1. `CATALOG_NAME`
2. `SCHEMA_NAME`
3. `DEFAULT_CHARACTER_SET_NAME`
4. `DEFAULT_COLLATION_NAME`
5. `SQL_PATH`
6. `DEFAULT_ENCRYPTION`

Rows include:

- one synthetic `information_schema` row with MySQL-compatible character set
  metadata (`utf8mb3`, `utf8mb3_general_ci`);
- one row for each MyLite catalog schema with `utf8mb4`,
  `utf8mb4_0900_ai_ci`, `SQL_PATH` `NULL`, and `DEFAULT_ENCRYPTION` `NO`.

`TABLES` returns MySQL's documented columns:

1. `TABLE_CATALOG`
2. `TABLE_SCHEMA`
3. `TABLE_NAME`
4. `TABLE_TYPE`
5. `ENGINE`
6. `VERSION`
7. `ROW_FORMAT`
8. `TABLE_ROWS`
9. `AVG_ROW_LENGTH`
10. `DATA_LENGTH`
11. `MAX_DATA_LENGTH`
12. `INDEX_LENGTH`
13. `DATA_FREE`
14. `AUTO_INCREMENT`
15. `CREATE_TIME`
16. `UPDATE_TIME`
17. `CHECK_TIME`
18. `TABLE_COLLATION`
19. `CHECKSUM`
20. `CREATE_OPTIONS`
21. `TABLE_COMMENT`

Rows include:

- synthetic `SYSTEM VIEW` rows for `SCHEMATA`, `TABLES`, and `COLUMNS` in
  `information_schema`;
- one `BASE TABLE` row for each MyLite catalog base-table descriptor.

Base table metadata is descriptor-driven and aligns with current
`SHOW TABLE STATUS` behavior: fixed `InnoDB`, version `10`, row format
`Dynamic`, exact current row count from the physical descriptor table, fixed
baseline length placeholders, `AUTO_INCREMENT` from the descriptor only when an
auto-increment column exists, `utf8mb4_0900_ai_ci` table collation, and empty
options/comment. Volatile create/update/check timestamps are currently `NULL`.

`COLUMNS` returns MySQL's documented columns:

1. `TABLE_CATALOG`
2. `TABLE_SCHEMA`
3. `TABLE_NAME`
4. `COLUMN_NAME`
5. `ORDINAL_POSITION`
6. `COLUMN_DEFAULT`
7. `IS_NULLABLE`
8. `DATA_TYPE`
9. `CHARACTER_MAXIMUM_LENGTH`
10. `CHARACTER_OCTET_LENGTH`
11. `NUMERIC_PRECISION`
12. `NUMERIC_SCALE`
13. `DATETIME_PRECISION`
14. `CHARACTER_SET_NAME`
15. `COLLATION_NAME`
16. `COLUMN_TYPE`
17. `COLUMN_KEY`
18. `EXTRA`
19. `PRIVILEGES`
20. `COLUMN_COMMENT`
21. `GENERATION_EXPRESSION`
22. `SRS_ID`

Rows include:

- metadata rows for the three supported `information_schema` system views;
- one row for each MyLite catalog column descriptor, including invisible
  columns.

For MyLite base-table columns:

- `TABLE_CATALOG` is `def`;
- `ORDINAL_POSITION` is the descriptor ordinal;
- integer-family columns report MySQL-compatible `DATA_TYPE`, `COLUMN_TYPE`,
  `NUMERIC_PRECISION`, and `NUMERIC_SCALE`;
- `VARCHAR(n)` reports `DATA_TYPE = varchar`, character length `n`, octet
  length `4 * n`, character set `utf8mb4`, and collation
  `utf8mb4_0900_ai_ci`;
- nullable columns report `IS_NULLABLE = YES`, otherwise `NO`;
- integer defaults render as decimal text except hidden auto-increment defaults,
  which remain `NULL` like current `SHOW COLUMNS` / `SHOW CREATE TABLE`
  behavior;
- primary-key columns report `COLUMN_KEY = PRI`;
- auto-increment columns report `auto_increment` in `EXTRA`;
- invisible columns report `INVISIBLE` in `EXTRA`, or
  `auto_increment INVISIBLE` if both attributes are present;
- `PRIVILEGES` is `select,insert,update,references`;
- comments and generation expressions are empty strings;
- `SRS_ID` is `NULL`.

## Ownership boundaries

- Public API: no ABI changes. Results flow through the existing `mylite_result`
  API.
- Parser/AST: parses the admitted `SELECT` shape and widened metadata predicate
  values. It does not know catalog contents.
- Analyzer/runtime: detects `INFORMATION_SCHEMA.<table>` sources, validates the
  supported table and query shape, resolves projection/predicate/order columns
  against first-party metadata definitions, builds synthetic rows from catalog
  descriptors, filters/sorts/limits them, and returns a normal result.
- Catalog: remains the authoritative source for MyLite schemas, tables,
  columns, primary keys, visibility, defaults, and auto-increment counters.
- Storage/SQLite: base-table row counts are read from generated physical tables
  through descriptor-built SQL. No SQLite schema text, PRAGMA output, or
  `sqlite_schema` data becomes MySQL metadata authority.
- SQLite fork: no fork patch or extension point is needed.

## Diagnostics

Required diagnostics:

- unknown `INFORMATION_SCHEMA` table:
  `ERROR 1109 (42S02): Unknown table '<table>' in information_schema`;
- unknown projection column:
  `ERROR 1054 (42S22): Unknown column '<column>' in 'field list'`;
- unknown predicate column:
  `ERROR 1054 (42S22): Unknown column '<column>' in 'where clause'`;
- unknown order column:
  `ERROR 1054 (42S22): Unknown column '<column>' in 'order clause'`;
- unsupported projection expressions, unsupported predicate values, unsupported
  order expressions, unsupported `LIMIT` forms, joins, aliases that do not
  match the source, and other deferred SQL shapes: deterministic MyLite
  unsupported diagnostics;
- allocation failure: existing `MYLITE_NOMEM` behavior and connection
  diagnostics;
- physical row-count failures: existing physical SQLite row diagnostics.

Supported `INFORMATION_SCHEMA` queries return `warning_count == 0` and
`affected_rows == 0`.

## Performance

This path materializes only metadata rows for three small system views plus
current MyLite descriptor rows. User data is not materialized except for exact
base-table row counts in `TABLES`, which reuse the existing descriptor-built
SQLite count path. Normal user-table `SELECT`, `INSERT`, `UPDATE`, `DELETE`,
and `REPLACE` statements continue to use descriptor-built SQLite execution and
are not routed through the synthetic metadata engine.

## Tests

Fast C tests must cover:

- `SCHEMATA`, `TABLES`, and `COLUMNS` projection and `*` column order;
- string-literal `WHERE`, `DATABASE()`/`SCHEMA()` predicates, `AND`, `OR`,
  `NOT`, `IS NULL`, `IS NOT NULL`;
- `COUNT(*)`;
- `ORDER BY` default, `ASC`, `DESC`, and `LIMIT`;
- source aliases and qualified metadata columns;
- user table metadata for integer, unsigned integer, `VARCHAR`, primary key,
  auto-increment, defaults, nullability, invisible columns, rename/drop, reopen
  persistence, and independent handles;
- system view metadata rows for the supported views;
- deterministic diagnostics for unknown system views, unknown columns,
  unsupported projections, unsupported predicates, unsupported ordering, joins,
  and unsupported unqualified `information_schema` access.

The MySQL expectation script must verify the admitted user-visible behavior
against MySQL 8.4.9 before implementation expectations are trusted.
