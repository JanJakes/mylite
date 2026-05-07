# CHECKSUM TABLE

## Scope

This feature accepts MySQL 8.4 `CHECKSUM TABLE` syntax and returns the
MySQL-shaped result set for table checksum probes:

- `Table`
- `Checksum`

The first implementation is an embedded compatibility surface. It resolves table
names, exposes MySQL-compatible metadata, returns `NULL` for targets where MySQL
does, and returns a deterministic placeholder checksum for MyLite base tables.
It does not yet implement MySQL's storage-engine row checksum algorithm.

## Sources

- MySQL 8.4 Reference Manual, `CHECKSUM TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/checksum-table.html
- Runtime probes against MySQL 8.4.9.

The specification is independently authored from official documentation and
observed MySQL 8.4.9 runtime behavior.

## Syntax

MyLite Lemon grammar shape:

```lemon
table_maintenance_statement ::= CHECKSUM TABLE maintenance_table_name_list
    checksum_table_option.

checksum_table_option ::= .
checksum_table_option ::= QUICK.
checksum_table_option ::= EXTENDED.
```

The option belongs after the complete table list. `QUICK EXTENDED`, `EXTENDED
QUICK`, `LOCAL`, `NO_WRITE_TO_BINLOG`, `FOR UPGRADE`, and `CHECK TABLE` options
are syntax errors for `CHECKSUM TABLE`.

## Runtime Semantics

Table names may be unqualified or schema-qualified. Unqualified names require a
selected schema.

For existing MyLite base tables:

- default and `EXTENDED` forms return one row with checksum `0`.
- `QUICK` returns one row with checksum `NULL`, matching MySQL InnoDB behavior
  when no live MyISAM checksum is available.

For missing tables in a known user schema, MyLite returns one row with checksum
`NULL` and appends error condition 1146. For unknown user schemas, it returns one
row with checksum `NULL` and appends error condition 1049.

For supported `information_schema` tables, MyLite returns one row with checksum
`NULL` and appends error condition 1347 because they are system views rather than
base tables. For unknown `information_schema` tables, MyLite fails the statement
with error condition 1109.

## Metadata

The result metadata mirrors observed MySQL 8.4.9 metadata:

- `Table`: nullable `VAR_STRING`, latin1 collation id `8`, length `384`,
  decimals `31`.
- `Checksum`: nullable `LONGLONG`, binary collation id `63`, length `22`,
  decimals `0`, `BINARY` and `NUM` flags.

## Deferred

- MySQL-compatible row checksum calculation for MyLite base tables.
- Content-sensitive checksums for ordinary tables.
- View support beyond current system-view handling.
- Read-lock behavior and privilege checks.

## Tests

Coverage includes:

- parser acceptance for default, `QUICK`, `EXTENDED`, qualified names, and
  identifiers named `checksum`
- parser rejection for misplaced or multiple options and `CHECK TABLE` options
- runtime `No database selected` handling for unqualified names
- existing persistent and temporary tables
- missing tables and unknown schemas
- selected-schema and fully-qualified table names
- supported and unknown `information_schema` targets
- MySQL-compatible result-column metadata and warning/error codes for the
  covered behavior
