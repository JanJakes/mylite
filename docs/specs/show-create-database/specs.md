# SHOW CREATE DATABASE

## Scope

This feature implements the first executable `SHOW CREATE DATABASE` slice for
schemas stored in MyLite's schema catalog:

- `SHOW CREATE DATABASE db_name`
- `SHOW CREATE DATABASE IF NOT EXISTS db_name`
- `SHOW CREATE SCHEMA db_name`
- `SHOW CREATE SCHEMA IF NOT EXISTS db_name`

`SHOW CREATE SCHEMA` is a synonym for parsing and lookup. The successful result
shape always uses the MySQL column names `Database` and `Create Database`, and
the generated text always starts with `CREATE DATABASE`.

Deferred surfaces:

- `LIKE` and `WHERE` suffixes; MySQL rejects these for this statement and
  MyLite keeps them syntactically rejected in this slice
- privilege-sensitive visibility and `skip_show_database`
- warning records, metadata locks, and command counters such as
  `Com_show_create_db`
- `sql_quote_show_create = 0`; MyLite always emits quoted identifiers in this
  slice

## Compatibility Sources

- MySQL 8.4 Reference Manual, `SHOW CREATE DATABASE` Statement:
  https://dev.mysql.com/doc/refman/8.4/en/show-create-database.html
- MySQL 8.4 Reference Manual, `SHOW` Statements:
  https://dev.mysql.com/doc/refman/8.4/en/show.html
- Runtime observations verified against local `mylite-mysql-849`, MySQL
  `8.4.9`, and rechecked during review on 2026-05-03.

This specification is independently authored from official documentation and
observed runtime behavior. It does not copy MySQL grammar, documentation prose,
or implementation sources.

## MySQL 8.4.9 Runtime Observations

The following behavior was verified against MySQL 8.4.9:

| SQL | Result |
| --- | --- |
| `SHOW CREATE DATABASE mylite_show_create_db_default` | Columns `Database`, `Create Database`; one row with `CREATE DATABASE `, the backtick-quoted schema name, ` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */`, and ` /*!80016 DEFAULT ENCRYPTION='N' */`. |
| `SHOW CREATE SCHEMA mylite_show_create_db_default` | Same result shape and generated text as `SHOW CREATE DATABASE`. |
| `SHOW CREATE DATABASE IF NOT EXISTS mylite_show_create_db_default` | Generated text includes `CREATE DATABASE /*!32312 IF NOT EXISTS*/ ` before the quoted schema name. |
| Latin1 schema with `DEFAULT CHARACTER SET latin1 COLLATE latin1_swedish_ci ENCRYPTION='Y'` | Generated text includes `DEFAULT CHARACTER SET latin1`, omits `COLLATE latin1_swedish_ci`, and includes `DEFAULT ENCRYPTION='Y'`. |
| Altered schema defaults | `SHOW CREATE DATABASE` reflects the current cataloged defaults, not the original `CREATE DATABASE` text. |
| Schema name ``My`Show`Db`` | Output backtick-quotes the name and doubles embedded backticks. |
| `SHOW CREATE DATABASE information_schema` | Returns a catalog-backed row for `information_schema` with `DEFAULT CHARACTER SET utf8mb3` and `DEFAULT ENCRYPTION='N'`. |
| `SHOW CREATE DATABASE mysql` | Returns a catalog-backed row for `mysql` with `DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci` and encryption `N`. |
| Missing schema | Error `1049`, SQLSTATE `42000`, message containing `Unknown database '<name>'`. |
| Omitted schema name, `LIKE`, or `WHERE` suffix | Syntax error. |

Collation display is character-set dependent. MySQL omits the default
collations for `latin1`, `utf8mb3`, and `binary`, but includes
`utf8mb4_0900_ai_ci` for `utf8mb4`. Non-default collations such as
`latin1_bin`, `utf8mb3_bin`, and `utf8mb4_bin` are displayed.

The verified result metadata has two non-null `VAR_STRING` columns using
`utf8mb3` charset id `8`: `Database` has declared length `64` and
`Create Database` has declared length `1024`. Both report decimals `31`.

## Syntax

MyLite owns the grammar below; it is intentionally authored for MyLite's Lemon
parser rather than copied from MySQL sources:

```lemon
statement ::= show_create_schema_statement.

show_create_schema_statement ::= SHOW CREATE schema_keyword
                                 opt_if_not_exists identifier.
schema_keyword ::= DATABASE.
schema_keyword ::= SCHEMA.
opt_if_not_exists ::= .
opt_if_not_exists ::= IF NOT EXISTS.
```

No trailing filter or clause is accepted by this slice.

## AST

Add a `MYLITE_SQL_AST_SHOW_CREATE_SCHEMA_STATEMENT` node with one child: the
schema identifier. The node records whether `IF NOT EXISTS` was present so
runtime formatting can include MySQL's version-commented
`/*!32312 IF NOT EXISTS*/` marker.

## Runtime Semantics

Target resolution:

- The statement names the schema explicitly; it does not consult or require the
  selected default schema.
- The schema name is resolved directly against `__mylite_schema_catalog.name`.
- The row's `Database` value is the cataloged schema name, preserving stored
  case and bytes.
- Missing schemas return MyLite's existing unknown database diagnostic with a
  message containing `Unknown database '<name>'`.
- System schemas are ordinary catalog rows for this statement.

Result shape:

- Successful execution returns exactly two columns:
  `Database`, `Create Database`.
- The result-column descriptors match MySQL 8.4.9: non-null
  `VAR_STRING(64)` for `Database` and non-null `VAR_STRING(1024)` for
  `Create Database`, both with charset id `8` and decimals `31`.
- Successful execution returns one row.
- Successful execution produces no warnings.
- `mylite_affected_rows()` remains `-1` because the result is read-only.

Formatting:

- The generated text begins with `CREATE DATABASE `.
- When `IF NOT EXISTS` appears in the `SHOW` statement, the generated text
  begins with `CREATE DATABASE /*!32312 IF NOT EXISTS*/ `.
- The schema name is always backtick-quoted. Embedded backticks are doubled by
  the shared `SHOW CREATE` identifier renderer.
- The default character set clause is always emitted in the version-commented
  form `/*!40100 DEFAULT CHARACTER SET <charset>[ COLLATE <collation>] */`.
- The collation suffix is emitted when MySQL displays it for the current
  catalog defaults. In the initial MyLite registry this means:
  - include `utf8mb4_0900_ai_ci` and any non-default collation
  - omit `latin1_swedish_ci`
  - omit `utf8mb3_general_ci`
  - omit `binary`
- The encryption clause is always emitted as
  `/*!80016 DEFAULT ENCRYPTION='<Y_or_N>' */`.

## Storage And Performance

This feature is read-only and must not mutate schema, table, column, index, or
physical SQLite storage. Runtime execution reads one schema catalog row during
prepare and materializes a tiny SQLite `SELECT` over string literals, matching
the existing `SHOW CREATE TABLE` architecture.

The catalog fields used are:

- `name`
- `default_character_set`
- `default_collation`
- `default_encryption`

Lookup is primary-key based on the schema name and should remain cheap for the
expected catalog size.

## Tests

Parser coverage:

- `SHOW CREATE DATABASE db_name`
- `SHOW CREATE SCHEMA db_name`
- `SHOW CREATE DATABASE IF NOT EXISTS db_name`
- `SHOW CREATE SCHEMA IF NOT EXISTS db_name`
- quoted schema identifiers with escaped backticks
- syntax rejection for omitted schema name
- syntax rejection for `LIKE` and `WHERE` suffixes
- unchanged rejection of unrelated `SHOW CREATE VIEW`

Runtime coverage:

- default user schema result shape and full generated text
- MySQL 8.4.9-derived result-column descriptors
- `SCHEMA` synonym with unchanged column names and `CREATE DATABASE` output
- `IF NOT EXISTS` version-comment formatting
- latin1 default collation omission and encryption `Y`
- utf8mb3 and binary default collation omission
- latin1_bin, utf8mb3_bin, and utf8mb4_bin non-default collation display
- altered schema defaults reflected in generated text
- escaped backticks in schema identifiers
- `information_schema` and `mysql` rendered from catalog rows
- missing schema diagnostic containing `Unknown database '<name>'`
- no selected default schema required
- `SHOW CREATE TABLE` regression coverage remains unchanged

## Compatibility Decisions

`SHOW CREATE DATABASE` is catalog-backed. MyLite does not preserve the original
database DDL text as source of truth because `ALTER DATABASE` changes the
defaults MySQL displays.

MyLite currently quotes all identifiers in `SHOW CREATE` output because
`sql_quote_show_create` is not implemented. That matches MySQL's default
session behavior and the current `SHOW CREATE TABLE` slice.

The initial collation-display helper is intentionally scoped to the supported
MyLite charset/collation registry. When broader MySQL collations land, this
helper must be expanded alongside the registry and compatibility tests.
