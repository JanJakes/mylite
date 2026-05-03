# SHOW TABLES

## Scope

This feature implements the first executable `SHOW TABLES` slice needed by
common MySQL applications:

- `SHOW TABLES`
- `SHOW FULL TABLES`
- `SHOW EXTENDED TABLES`
- `SHOW EXTENDED FULL TABLES`
- `SHOW TABLES FROM db_name`
- `SHOW TABLES IN db_name`
- `SHOW [EXTENDED] [FULL] TABLES [FROM | IN db_name] LIKE 'pattern'`
- `SHOW [EXTENDED] [FULL] TABLES [FROM | IN db_name] WHERE expr`

The slice lists supported MyLite base tables from the internal table catalog and
the existing MyLite `information_schema` metadata views as system views. It is a
read-only metadata statement with MySQL-compatible result column names,
case-sensitive table-name pattern matching for the current catalog model, and
deterministic diagnostics for missing schema context.

Deferred surfaces:

- `SHOW TABLES ... WHERE expr` execution
- temporary tables and temporary-table shadowing
- ordinary user views, until `CREATE VIEW` exists
- privilege filtering
- lower-case table-name modes beyond the current case-sensitive catalog
- metadata locks, protocol status counters, and performance-schema accounting

## Compatibility Sources

- MySQL 8.4 Reference Manual, `SHOW TABLES` Statement:
  https://dev.mysql.com/doc/refman/8.4/en/show-tables.html
- MySQL 8.4 Reference Manual, `SHOW` Statements:
  https://dev.mysql.com/doc/refman/8.4/en/show.html
- Runtime observations verified against `mylite-mysql-849`, MySQL `8.4.9`.

## MySQL 8.4.9 Runtime Observations

The following behavior was verified against MySQL 8.4.9:

| SQL | Result |
| --- | --- |
| `SHOW TABLES` with no selected database | Error `1046`, SQLSTATE `3D000`, message `No database selected`. |
| `SHOW FULL TABLES` with no selected database | Error `1046`, SQLSTATE `3D000`, message `No database selected`. |
| `SHOW TABLES FROM missing_schema` | Error `1049`, SQLSTATE `42000`, message `Unknown database 'missing_schema'`. |
| `SHOW TABLES` in schema `mylite_show_tables_probe` | One column named `Tables_in_mylite_show_tables_probe`; rows are table and view names ordered by name. |
| `SHOW FULL TABLES` in schema `mylite_show_tables_probe` | Columns `Tables_in_mylite_show_tables_probe`, `Table_type`; base table rows use `BASE TABLE`, view rows use `VIEW`. |
| `SHOW TABLES LIKE 'alpha%'` | First column name is `Tables_in_mylite_show_tables_probe (alpha%)`; rows matching the pattern are returned. |
| `SHOW FULL TABLES LIKE 'alpha%'` | First column name includes the pattern suffix and the second column remains `Table_type`. |
| `SHOW TABLES FROM db LIKE 'beta\_%'` | Backslash escapes `_`, so only literal-underscore names match. |
| `SHOW TABLES FROM information_schema LIKE 'TABLES'` | Returns `TABLES`. |
| `SHOW FULL TABLES FROM information_schema LIKE 'TABLES'` | Returns `TABLES`, `SYSTEM VIEW`. |
| `SHOW FULL TABLES FROM information_schema LIKE 'tables'` | Returns `TABLES`, `SYSTEM VIEW`, and the first column label uses `(TABLES)`. |
| `SHOW TABLES FROM Information_Schema LIKE 'tables'` | Returns `TABLES`; the first column label uses `Tables_in_information_schema (TABLES)`. |
| `SHOW EXTENDED TABLES` | Returns the same rows as `SHOW TABLES` when no hidden failed-ALTER tables exist. |
| `SHOW FULL TABLES WHERE Table_type = 'BASE TABLE'` | Parses and filters rows in MySQL; MyLite parses it but rejects execution until SHOW WHERE filtering is implemented. |
| `CREATE TABLE tables (id INT)` | `TABLES` is usable as an unquoted identifier in table-name contexts. |
| `CREATE TABLE full (id INT)` | `FULL` is usable as an unquoted identifier in table-name contexts. |
| `CREATE TABLE extended (id INT)` | `EXTENDED` is usable as an unquoted identifier in table-name contexts. |

On the Linux MySQL runtime used for verification, `lower_case_table_names = 0`;
`SHOW TABLES ... LIKE 'camel%'` did not match table `CamelCase`, while
`LIKE 'Camel%'` did.

## Syntax

MyLite owns the grammar below; it is intentionally authored for MyLite's Lemon
parser rather than copied from MySQL sources:

```lemon
statement ::= show_tables_statement.

show_tables_statement ::= SHOW opt_extended opt_full TABLES
                          opt_show_tables_schema opt_show_tables_filter.

opt_extended ::= .
opt_extended ::= EXTENDED.

opt_full ::= .
opt_full ::= FULL.

opt_show_tables_schema ::= .
opt_show_tables_schema ::= FROM identifier.
opt_show_tables_schema ::= IN identifier.

opt_show_tables_filter ::= .
opt_show_tables_filter ::= LIKE STRING.
opt_show_tables_filter ::= where_clause.
```

`TABLES`, `EXTENDED`, and `FULL` must remain available as nonreserved
identifiers outside the `SHOW TABLES` production.

## AST

Add a `show_tables_statement` AST node with:

- a boolean `full` marker when `FULL` is present
- a boolean `extended` marker when `EXTENDED` is present
- optional schema-name child
- optional string-literal `LIKE` pattern child or `WHERE` clause child

The statement must preserve the source span from `SHOW` through the last token.

## Runtime Semantics

Target schema resolution:

- If `FROM db_name` or `IN db_name` is present, use that schema.
- Otherwise use the session selected schema.
- `information_schema` is recognized case-insensitively and normalized to the
  lower-case result label MySQL reports.
- If no target schema is available, return `No database selected`.
- If the target schema is not present in the MyLite schema catalog, return
  `Unknown database '<schema>'`.

Rows:

- User schemas are backed by `__mylite_table_catalog`.
- `information_schema` additionally exposes MyLite's current metadata views:
  `CHARACTER_SETS`, `COLLATIONS`, `SCHEMATA`, `TABLES`, `COLUMNS`, `ENGINES`,
  and `STATISTICS`.
- User-created views are deferred and therefore not listed until view metadata
  exists.
- `SHOW TABLES` returns only table/view names.
- `SHOW FULL TABLES` returns names plus table type.
- `SHOW EXTENDED TABLES` is accepted and currently returns the same rows as
  `SHOW TABLES`; MyLite has no hidden failed-ALTER table catalog yet.
- `SHOW TABLES ... WHERE expr` is accepted by the parser but returns
  `MYLITE_UNSUPPORTED` with message `SHOW TABLES WHERE is not supported` after
  target schema validation. Expression filtering should be added when SHOW
  result-set predicates are shared across metadata statements.
- Rows are ordered by table name using MyLite's current case-sensitive catalog
  order.

Column names:

- Without `LIKE`, the first column is `Tables_in_<schema>`.
- With `LIKE`, the first column is `Tables_in_<schema> (<pattern>)`, where
  `<pattern>` is the decoded SQL string value used for matching.
- For `information_schema`, current MySQL behavior uppercases the displayed
  pattern and matches the existing system-view names case-insensitively for
  this slice.
- The `FULL` table type column is named `Table_type`.

LIKE filtering:

- `%` matches any byte sequence.
- `_` matches one byte.
- Backslash escapes the following byte for SHOW-pattern purposes.
- Matching is case-sensitive for user schemas in the current MyLite catalog
  model. The explicit `information_schema` metadata-view rows are matched after
  uppercasing the pattern, matching the verified MySQL 8.4.9 behavior for
  those names.

Warnings and affected rows:

- Successful `SHOW TABLES` produces no warnings.
- `mylite_affected_rows()` remains `-1` for the read-only SQLite-backed result.

## Storage And Performance

This feature is read-only. It must not mutate the schema, table, column, or
index catalogs. Runtime execution should be lowered to a SQLite read statement
over compact metadata queries, avoiding per-row C-side materialization unless a
future compatibility surface requires it.

## Tests

Parser coverage:

- `SHOW TABLES`
- `SHOW FULL TABLES`
- `SHOW EXTENDED TABLES`
- `SHOW EXTENDED FULL TABLES`
- `SHOW TABLES FROM db`
- `SHOW TABLES IN db LIKE 'a%'`
- `SHOW FULL TABLES FROM db LIKE 'a%'`
- `SHOW FULL TABLES WHERE Table_type = 'BASE TABLE'`
- `TABLES`, `EXTENDED`, and `FULL` as unquoted identifiers in table definitions
- syntax rejection for `SHOW TABLES LIKE 1`, combined `LIKE` plus `WHERE`, and
  duplicate `FULL`

Runtime coverage:

- selected-schema listing with exact column name
- schema-qualified listing via `FROM` and `IN`
- `FULL` result shape and `BASE TABLE` values
- `EXTENDED` accepted as no-op over the current catalog
- parsed `WHERE` returns a deterministic unsupported diagnostic
- `information_schema` existing metadata views as `SYSTEM VIEW`
- `SHOW FULL TABLES FROM information_schema LIKE 'collations'` returns the
  `COLLATIONS` system-view row
- `SHOW FULL TABLES FROM information_schema LIKE 'character_sets'` returns the
  `CHARACTER_SETS` system-view row
- `LIKE` column-label suffix and wildcard filtering
- escaped `_` in `LIKE`
- empty result with stable metadata
- no selected schema diagnostic
- missing schema diagnostic
- keyword interaction for base tables named `tables`, `extended`, and `full`
