# Core metadata catalog

## Scope

This feature extends MyLite's schema lifecycle catalog into the first core
metadata catalog for schemas, tables, columns, and indexes. It exposes the
minimum MySQL-compatible `INFORMATION_SCHEMA` surface needed before
`CREATE TABLE` exists:

- `INFORMATION_SCHEMA.SCHEMATA`
- `INFORMATION_SCHEMA.TABLES`
- `INFORMATION_SCHEMA.COLUMNS`
- `INFORMATION_SCHEMA.STATISTICS`

The implementation is deliberately narrow. It supports `SELECT * FROM
INFORMATION_SCHEMA.<table>` for these catalog-backed tables, with
case-insensitive resolution of `information_schema` and object names. The
current system-view inventory also includes `INFORMATION_SCHEMA.ENGINES` and
`INFORMATION_SCHEMA.CHARACTER_SETS`,
specified separately in
[INFORMATION_SCHEMA.ENGINES](../information-schema-engines/specs.md) and
[INFORMATION_SCHEMA.CHARACTER_SETS](../information-schema-character-sets/specs.md). General
`SELECT` projection, explicit duplicate modifiers, filtering, ordering,
aliases, joins, expressions over metadata tables, and privilege-sensitive
metadata visibility are later features.

## Sources

- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA` table reference:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-table-reference.html
- MySQL 8.4 Reference Manual, The `INFORMATION_SCHEMA` `SCHEMATA` table:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-schemata-table.html
- MySQL 8.4 Reference Manual, The `INFORMATION_SCHEMA` `TABLES` table:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-tables-table.html
- MySQL 8.4 Reference Manual, The `INFORMATION_SCHEMA` `COLUMNS` table:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-columns-table.html
- MySQL 8.4 Reference Manual, The `INFORMATION_SCHEMA` `STATISTICS` table:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-statistics-table.html
- Observed MySQL 8.4.9 runtime behavior from Docker container
  `mylite-mysql-849`.

This specification is independently authored from the official documentation
and observed runtime behavior. It does not copy MySQL grammar or implementation
sources.

## MySQL 8.4.9 behavior summary

- `INFORMATION_SCHEMA` tables are queryable as SQL tables.
- The schema qualifier and table names are resolved case-insensitively on the
  verified Linux MySQL 8.4.9 runtime for `information_schema.schemata`,
  `INFORMATION_SCHEMA.schemata`, and backtick-quoted mixed-case forms.
- `SCHEMATA` returns one row per visible schema. The first column is the
  catalog name `def`. `SQL_PATH` is `NULL`.
- `DEFAULT_ENCRYPTION` is exposed as `NO` or `YES`, while schema DDL accepts
  `ENCRYPTION='N'` or `ENCRYPTION='Y'`.
- A schema created with `DEFAULT CHARACTER SET latin1 COLLATE
  latin1_swedish_ci ENCRYPTION='Y'` returns those defaults and
  `DEFAULT_ENCRYPTION='YES'` from `SCHEMATA`.
- The observed system schema defaults are:
  - `information_schema`: `utf8mb3`, `utf8mb3_general_ci`, `NO`
  - `mysql`: `utf8mb4`, `utf8mb4_0900_ai_ci`, `NO`
  - `performance_schema`: `utf8mb4`, `utf8mb4_0900_ai_ci`, `NO`
  - `sys`: `utf8mb4`, `utf8mb4_0900_ai_ci`, `NO`
- Dropping a schema removes its `SCHEMATA` row immediately.
- An empty user-created schema has no rows in `TABLES`, `COLUMNS`, or
  `STATISTICS`.
- `INFORMATION_SCHEMA.TABLES` contains rows for the
  `INFORMATION_SCHEMA.CHARACTER_SETS`, `SCHEMATA`, `TABLES`, `COLUMNS`,
  `ENGINES`, and `STATISTICS` system views. In the verified runtime, each has
  `TABLE_TYPE='SYSTEM VIEW'`,
  `ENGINE=NULL`, `VERSION=10`, `TABLE_ROWS=0`, `TABLE_COLLATION=NULL`, and an
  empty `TABLE_COMMENT`.

## Column shape

MyLite exposes the MySQL 8.4.9 column names and order for the catalog-backed
tables in this spec. Types, flags, character sets, lengths, and protocol-level
metadata are not yet exposed by the public MyLite C API and remain part of
later result metadata work.

`SCHEMATA` columns:

1. `CATALOG_NAME`
2. `SCHEMA_NAME`
3. `DEFAULT_CHARACTER_SET_NAME`
4. `DEFAULT_COLLATION_NAME`
5. `SQL_PATH`
6. `DEFAULT_ENCRYPTION`

`TABLES` columns:

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

`COLUMNS` columns:

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

`STATISTICS` columns:

1. `TABLE_CATALOG`
2. `TABLE_SCHEMA`
3. `TABLE_NAME`
4. `NON_UNIQUE`
5. `INDEX_SCHEMA`
6. `INDEX_NAME`
7. `SEQ_IN_INDEX`
8. `COLUMN_NAME`
9. `COLLATION`
10. `CARDINALITY`
11. `SUB_PART`
12. `PACKED`
13. `NULLABLE`
14. `INDEX_TYPE`
15. `COMMENT`
16. `INDEX_COMMENT`
17. `IS_VISIBLE`
18. `EXPRESSION`

## MyLite behavior

### Internal catalog

MyLite stores metadata in internal SQLite tables inside the `.mylite` file:

- `__mylite_schema_catalog`
- `__mylite_table_catalog`
- `__mylite_column_catalog`
- `__mylite_index_catalog`

The table, column, and index catalog tables are intentionally future-facing.
This feature creates them so later `CREATE TABLE`, `ALTER TABLE`, `CREATE
INDEX`, and `DROP INDEX` work has a stable storage target, but no user table
rows are inserted until those DDL features exist.

System schema rows are seeded when a MyLite database handle opens. The
`information_schema` row uses MySQL-observed defaults `utf8mb3` and
`utf8mb3_general_ci`; the `mysql`, `performance_schema`, and `sys` rows use
`utf8mb4` and `utf8mb4_0900_ai_ci`.

### `INFORMATION_SCHEMA.SCHEMATA`

Supported query:

```sql
SELECT * FROM INFORMATION_SCHEMA.SCHEMATA
```

Behavior:

- Returns all MyLite schema catalog rows.
- Uses column names and order from MySQL 8.4.9.
- Returns `CATALOG_NAME='def'`.
- Returns `SQL_PATH=NULL`.
- Maps stored encryption `Y` or `y` to `YES`, and every other stored value to
  `NO`.
- Reflects `CREATE DATABASE`, `ALTER DATABASE`, and `DROP DATABASE` changes.
- Orders rows by schema name using bytewise ordering. MySQL does not guarantee
  semantic ordering for an unqualified query; this deterministic ordering keeps
  MyLite tests stable.

### `INFORMATION_SCHEMA.TABLES`

Supported query:

```sql
SELECT * FROM INFORMATION_SCHEMA.TABLES
```

Behavior:

- Returns rows from `__mylite_table_catalog`.
- Also exposes system-view rows for `INFORMATION_SCHEMA.CHARACTER_SETS`,
  `SCHEMATA`, `TABLES`, `COLUMNS`, `ENGINES`, and `STATISTICS`.
- For these system views, MyLite returns `TABLE_TYPE='SYSTEM VIEW'`,
  `ENGINE=NULL`, `VERSION=10`, `TABLE_ROWS=0`, `TABLE_COLLATION=NULL`, and
  `TABLE_COMMENT=''`, matching observed MySQL 8.4.9 behavior for the fields
  this feature verifies.
- Does not expose user table rows until table DDL writes metadata.

### `INFORMATION_SCHEMA.COLUMNS`

Supported query:

```sql
SELECT * FROM INFORMATION_SCHEMA.COLUMNS
```

Behavior:

- Returns rows from `__mylite_column_catalog`.
- Returns no user-object rows before `CREATE TABLE` exists.
- Does not yet synthesize rows for MyLite's supported
  `INFORMATION_SCHEMA` system views. This is a documented boundary for the
  early catalog task; the current requirement is result-set shape and empty
  user-object behavior.

### `INFORMATION_SCHEMA.STATISTICS`

Supported query:

```sql
SELECT * FROM INFORMATION_SCHEMA.STATISTICS
```

Behavior:

- Returns rows from `__mylite_index_catalog`.
- Returns no rows before index metadata exists.

### Unsupported query forms

The following are intentionally out of scope and should return MyLite's normal
parse or unsupported-statement diagnostic until the relevant SELECT feature
lands:

- explicit projections such as `SELECT SCHEMA_NAME FROM INFORMATION_SCHEMA.SCHEMATA`
- explicit `ALL` / `DISTINCT` modifiers
- `WHERE`, `ORDER BY`, `LIMIT`, joins, aliases, and expressions over metadata
  tables
- unqualified `SELECT * FROM SCHEMATA`
- metadata tables other than the supported current surfaces

## Lemon grammar snippets

These snippets describe MyLite's intended narrow grammar for this feature:

```lemon
select_statement ::= SELECT STAR FROM metadata_table_name.

metadata_table_name ::= qualified_identifier.
metadata_table_name ::= identifier.

qualified_identifier ::= identifier DOT identifier.
```

The parser may accept broader `SELECT <select_item_list> FROM <identifier>`
forms so the runtime can return a clean unsupported status for projections
until general `SELECT` is implemented. Runtime execution must restrict support
to a wildcard select from `INFORMATION_SCHEMA.SCHEMATA`, `TABLES`, `COLUMNS`,
or `STATISTICS`.

## Runtime and storage impact

- Catalog initialization creates all four internal catalog tables.
- Information schema queries are read-only custom translations to internal
  SQLite queries over the catalog tables.
- No new process-global state is introduced.
- Query cost is linear over catalog row count for the current unfiltered
  result sets. Later filtered SELECT support can add indexes or predicates
  where needed.

## MySQL 8.4.9 verified expectations

The following observations were verified against `mylite-mysql-849`:

| SQL | Expected behavior |
| --- | --- |
| `SHOW COLUMNS FROM INFORMATION_SCHEMA.SCHEMATA` | Returns columns `CATALOG_NAME`, `SCHEMA_NAME`, `DEFAULT_CHARACTER_SET_NAME`, `DEFAULT_COLLATION_NAME`, `SQL_PATH`, `DEFAULT_ENCRYPTION`. |
| `SHOW COLUMNS FROM INFORMATION_SCHEMA.TABLES` | Returns the 21-column shape listed above. |
| `SHOW COLUMNS FROM INFORMATION_SCHEMA.COLUMNS` | Returns the 22-column shape listed above. |
| `SHOW COLUMNS FROM INFORMATION_SCHEMA.STATISTICS` | Returns the 18-column shape listed above. |
| `CREATE DATABASE mylite_metadata_catalog_a DEFAULT CHARACTER SET latin1 COLLATE latin1_swedish_ci ENCRYPTION='Y'; SELECT * FROM INFORMATION_SCHEMA.SCHEMATA ...` | The schema row contains `def`, the schema name, `latin1`, `latin1_swedish_ci`, `NULL`, and `YES`. |
| `SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA='mylite_metadata_catalog_a'` on an empty created schema | Returns `0`. |
| `SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA='mylite_metadata_catalog_a'` on an empty created schema | Returns `0`. |
| `SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS WHERE TABLE_SCHEMA='mylite_metadata_catalog_a'` on an empty created schema | Returns `0`. |
| `DROP DATABASE mylite_metadata_catalog_a; SELECT COUNT(*) FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME='mylite_metadata_catalog_a'` | Returns `0`. |
| `SELECT COUNT(*) FROM information_schema.schemata`, `INFORMATION_SCHEMA.schemata`, and backtick-quoted `` `information_schema`.`SCHEMATA` `` | All resolve successfully. |
| `SELECT ... FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA='information_schema' AND TABLE_NAME IN (...)` | The scoped system views are reported as `SYSTEM VIEW` rows with `ENGINE=NULL`, `VERSION=10`, `TABLE_ROWS=0`, `TABLE_COLLATION=NULL`, and empty comments. |

## Test plan

- Parser tests:
  - parse `SELECT * FROM INFORMATION_SCHEMA.SCHEMATA`
  - parse lower-case and backtick-qualified information schema object names
  - parse an explicit projection from an information schema table so runtime
    can reject it as unsupported
  - reject `WHERE` filtering until general predicates land
- Runtime tests:
  - `SCHEMATA` exposes the six MySQL column names in order
  - seeded system schema rows include MySQL-observed defaults
  - created schema defaults and encryption are reflected in `SCHEMATA`
  - dropping a schema removes its `SCHEMATA` row
  - information schema table resolution is case-insensitive
  - `TABLES` exposes the 21 MySQL column names and system-view rows
  - `TABLES` has no row for an empty user-created schema
  - `COLUMNS` exposes the 22 MySQL column names and has no rows before table
    metadata exists
  - `STATISTICS` exposes the 18 MySQL column names and has no rows before
    index metadata exists
  - unsupported projection and filtering forms fail without silently returning
    misleading metadata
