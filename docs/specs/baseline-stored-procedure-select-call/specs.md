# Baseline stored procedure SELECT call

## Scope

This slice provides a narrow stored-procedure compatibility bridge for clients
that create a no-argument procedure containing one `SELECT` statement, inspect it
with `SHOW CREATE PROCEDURE`, call it, and drop it in the same MyLite session.
The immediate compatibility driver is the WordPress PHPUnit database test that
uses a procedure to verify mysqli result flushing.

Supported MySQL 8.4 references:

- <https://dev.mysql.com/doc/refman/8.4/en/create-procedure.html>
- <https://dev.mysql.com/doc/refman/8.4/en/drop-procedure.html>
- <https://dev.mysql.com/doc/refman/8.4/en/show-create-procedure.html>
- <https://dev.mysql.com/doc/refman/8.4/en/call.html>

Observed MySQL 8.4.9 behavior for this slice:

- `CREATE PROCEDURE p() BEGIN SELECT ...; END` succeeds and reports zero
  affected rows.
- Duplicate procedure creation returns `1304 / 42000` with a
  `PROCEDURE name already exists` diagnostic.
- `SHOW CREATE PROCEDURE p` returns six columns: `Procedure`, `sql_mode`,
  `Create Procedure`, `character_set_client`, `collation_connection`, and
  `Database Collation`.
- `CALL p` returns the result set from the procedure body. For the supported
  single-`SELECT` body, a following `ROW_COUNT()` returns `0`.
- `CALL missing` and `DROP PROCEDURE missing` return `1305 / 42000` with a
  schema-qualified procedure name when a default schema is selected.
- `SHOW CREATE PROCEDURE missing` returns `1305 / 42000`; unqualified missing
  names are reported without the default schema prefix.
- `DROP PROCEDURE IF EXISTS missing` succeeds, reports one note warning, and
  `SHOW WARNINGS` exposes note `1305` with the schema-qualified missing name.

## Syntax

MyLite admits only this routine grammar subset:

```lemon
statement ::= create_procedure_statement.
statement ::= drop_procedure_statement.
statement ::= show_create_procedure_statement.
statement ::= call_statement.

create_procedure_statement ::=
    CREATE PROCEDURE table_name LPAREN RPAREN BEGIN select_statement SEMICOLON END.

drop_procedure_statement ::= DROP PROCEDURE drop_if_exists_opt table_name.

show_create_procedure_statement ::= SHOW CREATE PROCEDURE table_name.

call_statement ::= CALL table_name.
call_statement ::= CALL table_name LPAREN RPAREN.
```

`table_name` is reused intentionally so ordinary identifier and
schema-qualified identifier handling matches other MyLite schema objects.

## Semantics

Procedure descriptors are session-local. `CREATE PROCEDURE` stores the resolved
schema name, routine name, the parsed `SELECT` body text, creation-time SQL
mode, creation-time client character set/collation metadata, database collation,
and the MySQL-shaped `SHOW CREATE PROCEDURE` text. Descriptors are freed when
the MyLite handle closes and are not written to the `.mylite` catalog.

`CALL` resolves the procedure name in the current session descriptor list and
executes the stored `SELECT` through the existing MyLite SQL execution path.
That keeps result-set metadata, expression behavior, table resolution,
warnings, and errors aligned with supported ordinary `SELECT` behavior instead
of introducing a second SELECT engine. After the supported `CALL`, MyLite
reports `ROW_COUNT() = 0`, matching the observed MySQL single-SELECT routine
behavior.

`CREATE PROCEDURE` and `DROP PROCEDURE` are treated as DDL for transaction
boundary purposes: an active user transaction is implicitly committed before
execution. Because descriptors are session-local, no persistent catalog
generation is changed.

`SHOW CREATE PROCEDURE` returns the stored descriptor row. It does not consult
`INFORMATION_SCHEMA.ROUTINES`, `SHOW PROCEDURE STATUS`, or a persistent routine
catalog because those surfaces remain metadata placeholders in this slice.

## Diagnostics

Supported diagnostics:

- Duplicate routine: `1304 / 42000`, `PROCEDURE name already exists`.
- Missing routine in `SHOW CREATE PROCEDURE`: `1305 / 42000`,
  `PROCEDURE name does not exist` for unqualified names and
  `PROCEDURE schema.name does not exist` for schema-qualified names.
- Missing routine in `CALL` or `DROP PROCEDURE`: `1305 / 42000`,
  `PROCEDURE schema.name does not exist`.
- Missing routine in `DROP PROCEDURE IF EXISTS`: one note warning with code
  `1305` and the schema-qualified missing name.
- Missing selected schema and unknown schemas reuse the existing MyLite schema
  object diagnostics.

## Non-Goals

This slice does not implement persistent stored routines, parameters, local
variables, handlers, cursors, compound statement execution beyond the accepted
single-`SELECT` body wrapper, `CREATE FUNCTION`, `ALTER PROCEDURE`,
routine-characteristic clauses, definers other than the session identity,
privilege checks, binary logging, routine dependencies, loaded
`INFORMATION_SCHEMA.ROUTINES` rows, loaded `INFORMATION_SCHEMA.PARAMETERS`
rows, or `SHOW PROCEDURE STATUS` rows.
