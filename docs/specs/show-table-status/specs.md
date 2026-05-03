# SHOW TABLE STATUS

## Scope

This feature implements the first executable `SHOW TABLE STATUS` slice:

- `SHOW TABLE STATUS`
- `SHOW TABLE STATUS FROM db_name`
- `SHOW TABLE STATUS IN db_name`
- `SHOW TABLE STATUS [{FROM | IN} db_name] LIKE 'pattern'`
- `SHOW TABLE STATUS [{FROM | IN} db_name] WHERE expr`

The slice lists supported persistent MyLite base tables and the existing
`information_schema` system views with the MySQL-compatible status result
shape. `WHERE` is parser-supported but execution is deferred until shared SHOW
filtering exists.

Deferred surfaces:

- `SHOW TABLE STATUS ... WHERE expr` execution
- `SHOW FULL TABLE STATUS` and `SHOW EXTENDED TABLE STATUS`, which are not part
  of the MySQL 8.4 syntax
- temporary tables
- ordinary user views, until `CREATE VIEW` and view metadata exist
- privilege filtering
- lower-case table-name modes beyond the current case-sensitive catalog
- accurate storage-engine statistics, locks, command counters, and
  performance-schema accounting

## Compatibility Sources

- MySQL 8.4 Reference Manual, `SHOW TABLE STATUS` Statement:
  https://dev.mysql.com/doc/refman/8.4/en/show-table-status.html
- MySQL 8.4 Reference Manual, `SHOW TABLES` Statement:
  https://dev.mysql.com/doc/refman/8.4/en/show-tables.html
- Runtime observations verified against `mylite-mysql-849`, MySQL `8.4.9`.

## MySQL 8.4.9 Runtime Observations

The following behavior was verified against MySQL 8.4.9:

| SQL | Result |
| --- | --- |
| `SHOW TABLE STATUS` with no selected database | Error `1046`, SQLSTATE `3D000`, message `No database selected`. |
| `SHOW TABLE STATUS FROM missing_show_table_status` | Error `1049`, SQLSTATE `42000`, message `Unknown database 'missing_show_table_status'`. |
| `SHOW TABLE STATUS` | Columns are `Name`, `Engine`, `Version`, `Row_format`, `Rows`, `Avg_row_length`, `Data_length`, `Max_data_length`, `Index_length`, `Data_free`, `Auto_increment`, `Create_time`, `Update_time`, `Check_time`, `Collation`, `Checksum`, `Create_options`, `Comment`. |
| Empty InnoDB base table | `Engine=InnoDB`, `Version=10`, `Row_format=Dynamic`, `Rows=0`, numeric storage counters are nonnegative, `Auto_increment=NULL`, `Check_time=NULL`, default table collation, `Checksum=NULL`, and empty `Create_options` / `Comment`. |
| Table created with `COMMENT='table comment' AUTO_INCREMENT=42` and two inserted rows | `Comment` is `table comment`; next `Auto_increment` is `44`. |
| `SHOW TABLE STATUS LIKE 'camel%'` with table `CamelCase` | No row on the local Linux runtime. |
| `SHOW TABLE STATUS LIKE 'Camel%'` with table `CamelCase` | Returns `CamelCase`. |
| `SHOW TABLE STATUS LIKE 'beta\_%'` with table `beta_1` | Returns `beta_1`. |
| `SHOW TABLE STATUS FROM db` / `SHOW TABLE STATUS IN db` | Both forms return the same rows. |
| `SHOW TABLE STATUS FROM information_schema LIKE 'tables'` | Returns a row named `TABLES`. |
| `SHOW TABLE STATUS FROM Information_Schema LIKE 'tables'` | Returns a row named `TABLES`. |
| `SHOW TABLE STATUS WHERE Name = 'simple'` | MySQL filters rows; MyLite parses this and returns an unsupported diagnostic for this slice. |
| `SHOW TABLE STATUS LIKE 1` | Syntax error. |
| `SHOW TABLE STATUS LIKE 'a%' WHERE Name = 'a'` | Syntax error. |
| `SHOW FULL TABLE STATUS` | Syntax error. |
| `SHOW EXTENDED TABLE STATUS` | Syntax error. |

## Syntax

MyLite owns the grammar below; it is authored for MyLite's Lemon parser rather
than copied from MySQL sources:

```lemon
statement ::= show_table_status_statement.

show_table_status_statement ::= SHOW TABLE STATUS
                                opt_show_tables_schema
                                opt_show_tables_filter.

opt_show_tables_schema ::= .
opt_show_tables_schema ::= FROM identifier.
opt_show_tables_schema ::= IN identifier.

opt_show_tables_filter ::= .
opt_show_tables_filter ::= LIKE STRING.
opt_show_tables_filter ::= where_clause.
```

The production deliberately does not accept `FULL` or `EXTENDED` modifiers, and
the shared SHOW filter grammar prevents combining `LIKE` and `WHERE`.

## AST

Add a `show_table_status_statement` AST node with:

- optional schema-name child
- optional string-literal `LIKE` pattern child or `WHERE` clause child

The statement must preserve the source span from `SHOW` through the last token.

## Runtime Semantics

Target schema resolution:

- If `FROM db_name` or `IN db_name` is present, use that schema.
- Otherwise use the session selected schema.
- `information_schema` is recognized case-insensitively and normalized to the
  lower-case schema name used by current MyLite `SHOW TABLES` behavior.
- If no target schema is available, return `No database selected`.
- If the target schema is not present in the schema catalog, return
  `Unknown database '<schema>'`.

Rows:

- User schemas are backed by `__mylite_table_catalog`.
- `information_schema` exposes the same known system-view names as
  `SHOW TABLES` and `INFORMATION_SCHEMA.TABLES`:
  `CHARACTER_SETS`, `CHECK_CONSTRAINTS`,
  `COLLATION_CHARACTER_SET_APPLICABILITY`, `COLLATIONS`, `SCHEMATA`, `TABLES`,
  `COLUMNS`, `ENGINES`, `KEYWORDS`, `KEY_COLUMN_USAGE`,
  `REFERENTIAL_CONSTRAINTS`, `STATISTICS`, and `TABLE_CONSTRAINTS`.
- Rows are ordered by table name using bytewise order.
- `SHOW TABLE STATUS ... WHERE expr` returns `MYLITE_UNSUPPORTED` with
  `SHOW TABLE STATUS WHERE is not supported` after target schema validation.

Column mapping:

- `Name`: `table_name`
- `Engine`: `engine`
- `Version`: `version`
- `Row_format`: `COALESCE(row_format, 'Dynamic')` for user base tables because
  MyLite currently stores `NULL` for its InnoDB facade while MySQL reports
  `Dynamic` for ordinary InnoDB tables.
- `Rows`: `table_rows`
- `Avg_row_length`, `Data_length`, `Max_data_length`, `Index_length`,
  `Data_free`: catalog values when present; for current user base tables these
  are deterministic `0` placeholders until MyLite maintains physical storage
  statistics.
- `Auto_increment`: `auto_increment`
- `Create_time`: `create_time`
- `Update_time`: `update_time`
- `Check_time`: `check_time`
- `Collation`: `table_collation`
- `Checksum`: `checksum`
- `Create_options`: `create_options`
- `Comment`: `table_comment`

For `information_schema` system-view rows, MyLite returns deterministic
MySQL-shaped placeholder rows:

- `Engine=NULL`
- `Version=10`
- `Row_format=NULL`
- numeric status columns `Rows`, `Avg_row_length`, `Data_length`,
  `Max_data_length`, `Index_length`, and `Data_free` as `0`
- `Auto_increment=NULL`
- `Create_time='1970-01-01 00:00:00'`
- `Update_time=NULL`
- `Check_time=NULL`
- `Collation=NULL`
- `Checksum=NULL`
- `Create_options=''`
- `Comment=''`

LIKE filtering:

- `%` matches any byte sequence.
- `_` matches one byte.
- Backslash escapes the following byte.
- Matching is case-sensitive for user schemas in the current MyLite catalog
  model.
- For `information_schema`, the display and match pattern are uppercased before
  conversion to MyLite's current bytewise glob filter, matching the existing
  `SHOW TABLES` information-schema behavior.

Warnings and affected rows:

- Successful `SHOW TABLE STATUS` produces no warnings.
- `mylite_affected_rows()` remains `-1` for the read-only result.

## Storage And Performance

This feature is read-only. It must not mutate schema, table, column, or index
catalogs. Runtime execution should lower to one SQLite metadata query over
compact catalog rows and deterministic inline `information_schema` rows. No new
storage dependencies are introduced.

## Tests

Parser coverage:

- `SHOW TABLE STATUS`
- `SHOW TABLE STATUS FROM db`
- `SHOW TABLE STATUS IN db LIKE 'a%'`
- `SHOW TABLE STATUS LIKE 'solo%'`
- `SHOW TABLE STATUS WHERE Name = 'simple'`
- `LIKE` literal child and `WHERE` child shapes
- syntax rejection for `SHOW TABLE STATUS LIKE 1`
- syntax rejection for combined `LIKE` plus `WHERE`
- syntax rejection for `SHOW FULL TABLE STATUS`
- syntax rejection for `SHOW EXTENDED TABLE STATUS`
- syntax rejection for malformed repeated `FROM` / `IN`

Runtime coverage:

- no selected schema diagnostic
- missing schema diagnostic
- selected-schema result shape and rows
- `FROM` and `IN` synonyms
- empty schema with stable metadata
- `LIKE` case sensitivity
- escaped `_` in `LIKE`
- `WHERE` unsupported diagnostic
- base-table metadata, including collation, comment, and next auto-increment
- `information_schema` lower-case and mixed-case schema/pattern behavior
- deterministic status placeholders for system views
