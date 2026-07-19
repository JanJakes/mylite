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
[LIMIT offset, row_count]
[LIMIT row_count OFFSET offset]
```

Supported select items:

- `*`;
- unqualified metadata column names;
- source-table-qualified metadata column names when the source has no alias;
- source-alias-qualified metadata column names when the source has an alias;
- `COUNT(*)`, with optional alias;
- the narrow `CAST(numeric_metadata_expression AS UNSIGNED)` projection used
  by WordPress table-size introspection, where the expression is made from
  metadata columns, integer literals, parentheses, and `/`.

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
- metadata column `IN (...)` for admitted metadata predicate values;
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

Supported limits use nonnegative decimal integer literals accepted by the
existing `SELECT` limit conversion:

- `LIMIT row_count`;
- `LIMIT offset, row_count`;
- `LIMIT row_count OFFSET offset`.

The offset applies after filtering, grouping, and ordering. An offset also
suppresses the single aggregate row from `COUNT(*)` when it is greater than
zero.

Compatibility query bridges:

- MyLite admits a narrow WordPress-oriented derived-table metadata query over
  `INFORMATION_SCHEMA.COLUMNS`, a user table named `t`, and
  `INFORMATION_SCHEMA.SCHEMATA` when the query joins `t.db_name` to
  `CONCAT(COALESCE(c.table_schema, 'default'), '')`, filters
  `c.table_name = 't'`, and orders by `ordinal_position`. The result is built
  from descriptor-backed column metadata and the first matching `t.id` row for
  the selected schema.
- MyLite admits a narrow WordPress-oriented metadata join between
  `INFORMATION_SCHEMA.COLUMNS` and `INFORMATION_SCHEMA.STATISTICS` when the
  join keys are table schema, table name, and column name, the filters are
  literal `TABLE_SCHEMA` and `TABLE_NAME` predicates, and the projection is the
  column data type plus matching index and column names. The result is
  synthesized from descriptor-backed `COLUMNS` and `STATISTICS` rowsets rather
  than from SQLite schema reflection.
- MyLite admits the WordPress-shaped `SELECT s.* FROM
  INFORMATION_SCHEMA.SCHEMATA s LEFT JOIN INFORMATION_SCHEMA.TABLES t ON
  t.table_schema = s.schema_name ORDER BY s.schema_name` metadata query. It
  returns descriptor-backed `SCHEMATA` rows with left-join multiplicity derived
  from descriptor-backed `TABLES` rows.
- MyLite admits the exact WordPress-shaped `WITH` query that builds a
  column/index name list from `INFORMATION_SCHEMA.COLUMNS` and
  `INFORMATION_SCHEMA.STATISTICS` using two CTEs, `UNION ALL`,
  `CONCAT(name, ' (column)')`, `CONCAT(name, ' (index)')`, and
  `ORDER BY name`. This is a compatibility bridge for the observed query
  shape, not general CTE, arbitrary `UNION`, or expression projection support.
- MyLite admits the WordPress-shaped grouped table-size metadata query over
  `INFORMATION_SCHEMA.TABLES` that projects `TABLE_NAME`, `TABLE_ROWS`, and
  `SUM(DATA_LENGTH + INDEX_LENGTH)` for a literal `TABLE_SCHEMA` and literal
  table-name set. The production WordPress form has no `ORDER BY`; an optional
  ascending `ORDER BY TABLE_NAME` is also admitted for deterministic callers.
  This is a compatibility bridge for descriptor-backed metadata rows, not
  general expression aggregate support.

Bridge admission is based on the parsed statement structure, including source
tables, aliases, projections, predicates, join keys, grouping, ordering, union
modifier, and filter values. Filter values may be string literals or native
prepared parameters for the listed bridge shapes. Parameter values are
deferred during prepare-time object validation and resolved from typed bindings
for each execution. Text inside comments or string literals cannot select a
bridge. Extra clauses that a bridge does not implement are rejected instead of
being silently ignored.

Bridge results expose the same typed field descriptors as direct
`INFORMATION_SCHEMA` projections. Source columns retain their source schema,
table or source alias, origin table and column, logical type, flags, connection
collation, display length, and nullability. The dynamic WordPress bridge also
retains the descriptor of the selected user-table `id` column. The CTE name
expression is a nullable connection-collated `VAR_STRING` with a 73-character
maximum, and the grouped byte total is a nullable binary `NEWDECIMAL` with
display length 45 and scale zero.

Prepared statements admit parameter markers wherever the supported metadata
predicate forms admit a scalar value, including comparison, `BETWEEN`, `IN`,
and `LIKE` patterns. Parameters are resolved through the native typed binding
API at execution; text remains data under every SQL mode, `NULL` preserves
three-valued predicate semantics, and prepare-time source/column validation is
performed by the metadata planner rather than ordinary user-table planning.
`LIKE ... ESCAPE` retains a literal escape character in this baseline.

## Deliberately excluded surface

This phase does not implement:

- unqualified access through selected `information_schema`;
- physical `information_schema` SQLite tables;
- `mysql`, `performance_schema`, or `sys` schema tables;
- joins, subqueries, CTEs, unions, windows, grouping other than `COUNT(*)`,
  aggregate functions other than `COUNT(*)`, expression predicates, `LIKE`,
  `REGEXP`, `BETWEEN`, prepared-statement metadata, privileges, roles, or
  metadata locks, except for the explicitly listed projection, predicate,
  prepared-value, and compatibility query bridge forms;
- exact volatile timestamp/statistics fidelity;
- `INFORMATION_SCHEMA` tables beyond `SCHEMATA`, `TABLES`, and `COLUMNS`.

Unsupported `INFORMATION_SCHEMA` tables fail with MySQL-compatible
`ERROR 1109 (42S02): Unknown table '<name>' in information_schema`.

## Grammar

The existing `table_name`, `select_item_list`, `where_clause_opt`,
`order_clause_opt`, and `limit_clause_opt` grammar is reused. Metadata predicate
values admit string/integer literals, current-database functions, and native
prepared parameters. The specialized `LIKE` pattern grammar admits a parameter
node only when prepared parsing enables parameter markers.

Independently authored Lemon-syntax snippets:

```lemon
identifier ::= TABLES.

predicate_atom ::= qualified_identifier comparison_operator metadata_predicate_value.
predicate_atom ::= qualified_identifier IS NULL.
predicate_atom ::= qualified_identifier IS NOT NULL.
predicate_atom ::= qualified_identifier IN LPAREN metadata_predicate_value_list RPAREN.

metadata_predicate_value ::= INTEGER.
metadata_predicate_value ::= STRING.
metadata_predicate_value ::= DATABASE LPAREN RPAREN.
metadata_predicate_value ::= SCHEMA LPAREN RPAREN.

metadata_predicate_value_list ::= metadata_predicate_value.
metadata_predicate_value_list ::= metadata_predicate_value_list COMMA metadata_predicate_value.
```

The parser remains intentionally narrower than MySQL. `LIKE`, `BETWEEN`, and
arbitrary expressions are deferred for metadata predicates unless another
baseline phase specifies them.

The compatibility bridge for the observed WordPress `WITH` query accepts only
the independently authored shape below:

```lemon
statement ::= with_select_statement.

with_select_statement ::=
    WITH common_table_expression_list with_union_select with_select_order_clause.

common_table_expression_list ::= common_table_expression.
common_table_expression_list ::= common_table_expression_list COMMA common_table_expression.

common_table_expression ::= identifier AS LPAREN with_columns_cte_select RPAREN.
common_table_expression ::= identifier AS LPAREN with_indexes_cte_select RPAREN.

with_columns_cte_select ::=
    SELECT identifier AS identifier FROM table_name WHERE with_table_filter.

with_indexes_cte_select ::=
    SELECT DISTINCT identifier AS identifier FROM table_name WHERE with_table_filter.

with_table_filter ::= identifier EQUAL STRING AND identifier EQUAL STRING.

with_union_select ::=
    SELECT CONCAT LPAREN identifier COMMA STRING RPAREN AS identifier FROM identifier
    UNION ALL
    SELECT CONCAT LPAREN identifier COMMA STRING RPAREN AS identifier FROM identifier.

with_select_order_clause ::= ORDER BY identifier.
```

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
baseline length placeholders, `AUTO_INCREMENT` as `NULL` unless an explicit
table auto-increment status value is available, `utf8mb4_0900_ai_ci` table
collation, and empty options/comment. Volatile create/update/check timestamps
are currently `NULL`.

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
- the WordPress-shaped grouped `TABLES` metadata projection with
  `SUM(DATA_LENGTH + INDEX_LENGTH)`, both without ordering and with ascending
  `ORDER BY TABLE_NAME`;
- result descriptors for source columns, bridge aliases, the CTE `CONCAT`
  expression, the grouped byte total, and the dynamic user-table `id` source;
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
