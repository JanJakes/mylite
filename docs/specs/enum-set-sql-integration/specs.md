# ENUM and SET SQL Integration

## Status

Planned first executable slice for routing MyLite-declared `ENUM` and `SET`
columns into the SQLite fork's native value-list descriptors.

## Sources

- MySQL 8.4 `ENUM` type documentation:
  https://dev.mysql.com/doc/refman/8.4/en/enum.html
- MySQL 8.4 `SET` type documentation:
  https://dev.mysql.com/doc/refman/8.4/en/set.html
- SQLite C interface extension overview:
  https://www.sqlite.org/cintro.html#extending_sqlite
- SQLite application-defined SQL functions:
  https://www.sqlite.org/appfunc.html
- SQLite collations:
  https://sqlite.org/c3ref/create_collation.html
- SQLite virtual table modules:
  https://www.sqlite.org/c3ref/create_module.html
- Observed MySQL 8.4.9 runtime behavior captured in
  `mysql-enum-set-sql-integration.sql` and
  `mysql-enum-set-sql-integration.expected.tsv`.

## Goal

`CREATE TABLE` should accept MySQL `ENUM(...)` and `SET(...)` column
definitions, preserve their metadata in the MyLite catalog, create ordinary
SQLite storage with integer affinity, and attach SQLite-fork column descriptors
automatically before DML writes. Applications should use ordinary MySQL-shaped
SQL, not fork test APIs.

This slice deliberately depends on the existing fork-native enum and set
coercion implementations:

- `docs/specs/sqlite-fork-enum-type-descriptors/specs.md`
- `docs/specs/sqlite-fork-set-type-descriptors/specs.md`

## Extension Point Decision

SQLite's public extension surface remains valuable, but it is not sufficient
for ordinary MySQL column semantics:

- `sqlite3_create_function()` covers scalar, aggregate, and window functions.
- `sqlite3_create_collation()` covers ordering and equality under named
  collations.
- `sqlite3_create_module()` covers virtual tables and table-valued functions.
- `sqlite3_vfs_register()` covers file I/O and the `.mylite` offset model.

Those APIs do not allow an extension to add native declared column types for
ordinary tables, intercept every assignment into a column, store column-owned
value-list payloads in SQLite's schema, or rewrite column readback while
preserving numeric context. MyLite therefore needs fork extension points at the
ordinary-table schema, VDBE assignment, and VDBE column-read layers. The public
SQLite extension surface should still be used for MySQL functions, collations,
virtual information-schema helpers, and file format integration where it fits.

## Syntax

MyLite Lemon grammar snippet:

```lemon
column_type(A) ::= value_list_column_type(B). {
    A = B;
}

value_list_column_type(A) ::= ENUM(T) LPAREN(L) value_list_literals(B) RPAREN(R)
        character_type_attribute_list(C). {
    A = mylite_sql_parser_apply_column_type_attributes(
        state,
        mylite_sql_parser_set_column_type_value_list(
            state,
            mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_ENUM),
            L,
            B,
            R),
        C);
}

value_list_column_type(A) ::= SET(T) LPAREN(L) value_list_literals(B) RPAREN(R)
        character_type_attribute_list(C). {
    A = mylite_sql_parser_apply_column_type_attributes(
        state,
        mylite_sql_parser_set_column_type_value_list(
            state,
            mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_SET),
            L,
            B,
            R),
        C);
}

value_list_literals(A) ::= STRING(T). {
    A = mylite_sql_parser_make_expression_list(
        state, mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING));
}
value_list_literals(A) ::= value_list_literals(B) COMMA STRING(T). {
    A = mylite_sql_parser_append_expression(
        state, B, mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING));
}
```

The first slice accepts normal string literal members. National string, hex,
bit, and expression members remain syntax errors until verified against MySQL
and intentionally designed.

## Metadata

The internal create-table plan owns the decoded value list and a canonical
`COLUMN_TYPE` string:

- `ENUM('draft','published')` is stored as `enum('draft','published')`.
- `SET('a','b')` is stored as `set('a','b')`.
- Single quotes and backslashes in values are emitted with MySQL-compatible
  single-quoted SQL escaping.

`INFORMATION_SCHEMA.COLUMNS` rows should use:

- `DATA_TYPE`: `enum` or `set`.
- `COLUMN_TYPE`: the canonical value-list string.
- `CHARACTER_SET_NAME` and `COLLATION_NAME`: explicit column attributes when
  present, otherwise table/schema defaults.
- `CHARACTER_MAXIMUM_LENGTH`:
  - `ENUM`: maximum member character length.
  - `SET`: maximum comma-joined display character length.
- `CHARACTER_OCTET_LENGTH`: character maximum length multiplied by the
  effective character set's maximum bytes per character.
- numeric and temporal metadata: `NULL`.

Physical SQLite storage should use integer affinity for both value-list types.

## DDL Validation

MyLite rejects:

- empty value lists;
- duplicate values, using byte-for-byte comparison after literal decoding;
- `SET` members containing a comma;
- `SET` lists with more than 64 values;
- `ENUM` lists with more than 65,535 values;
- unknown or incompatible charset/collation attributes.

The first executable slice may use existing MyLite diagnostics rather than exact
MySQL error numbers for DDL validation. Runtime coercion errors are still
reported by the SQLite fork descriptors and mapped through the existing fork
condition path.

## DML Integration

Before `INSERT`, `REPLACE`, `UPDATE`, ODKU update branches, or other write
paths execute against a base table, MyLite already loads column metadata and
attaches scalar descriptors to live SQLite schema columns. This slice extends
that loader:

- `DATA_TYPE='enum'` parses the canonical catalog `COLUMN_TYPE` and calls
  `mylite_sqlite_fork_set_enum_column_type()`.
- `DATA_TYPE='set'` parses the canonical catalog `COLUMN_TYPE` and calls
  `mylite_sqlite_fork_set_set_column_type()`.
- Other scalar descriptors continue using `mylite_sqlite_fork_set_column_type()`.
- Unsupported or malformed catalog value-list text is a MyLite execution error,
  not silent fallback.

The catalog parser should be strict because MyLite owns the canonical catalog
text. Broader import compatibility can be added later.

## Tests

Fast tests must cover:

- parsing `ENUM` and `SET` column definitions;
- `CREATE TABLE`, `SHOW COLUMNS`, and `INFORMATION_SCHEMA.COLUMNS` metadata;
- `INSERT`, `INSERT ... SET`, `UPDATE`, `REPLACE`, and ODKU with label, numeric,
  quoted numeric, NULL, and empty values;
- descriptor-backed readback through `SELECT col` and numeric context through
  `SELECT col + 0`;
- duplicate value DDL rejection and `SET` comma-member rejection;
- `DELETE`, `TRUNCATE`, and `DROP TABLE` around value-list tables to keep the
  foundation CRUD script broad.

The MySQL fixture records the comparable metadata and CRUD behavior from MySQL
8.4.9.

## Deferred Work

- exact MySQL error codes/messages for DDL validation;
- SQL-mode-aware warning demotion for invalid enum/set assignments;
- default-value subtleties for non-null value-list columns;
- import of noncanonical catalog text;
- `ALTER TABLE` rebuild behavior for value-list type changes beyond the shared
  create-column descriptor path;
- richer charset byte-length accounting beyond the current supported charset
  registry.
