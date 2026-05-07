# Baseline Current Database Function

## Status

This feature specifies the next narrow runtime identity slice for file-backed
`.mylite` handles. It adds `DATABASE()` and `SCHEMA()` as selected-schema
information functions on top of `mylite_execute()`, statement context, parser
scaffolding, durable catalog schema descriptors, public schema lifecycle, and
the existing selected-schema policy.

The feature is intentionally not general scalar expression support. It admits
only zero-argument `DATABASE()` and `SCHEMA()` in one-row `SELECT` statements
without a table source, plus the same expressions with `FROM DUAL`.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- Baseline schema lifecycle:
  `docs/specs/baseline-schema-lifecycle/specs.md`
- MySQL lexer:
  `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- MySQL 8.4 Reference Manual, information functions:
  https://dev.mysql.com/doc/refman/8.4/en/information-functions.html
- MySQL 8.4 Reference Manual, getting database information:
  https://dev.mysql.com/doc/refman/8.4/en/getting-information.html
- MySQL 8.4 Reference Manual, `USE`:
  https://dev.mysql.com/doc/refman/8.4/en/use.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against `SELECT VERSION()` returning `8.4.9`:

- `SELECT DATABASE(), SCHEMA(), @@warning_count` without a selected database
  returns one row with `NULL`, `NULL`, and warning count `0`.
- After `CREATE DATABASE db; USE db`, `SELECT DATABASE(), SCHEMA()` returns
  `db`, `db`.
- `SCHEMA()` is a synonym for `DATABASE()`.
- `SELECT DATABASE() FROM DUAL` returns one row with the selected database or
  `NULL`.
- `DROP DATABASE` of the selected database clears the default database, so a
  subsequent `SELECT DATABASE(), SCHEMA()` returns `NULL`, `NULL`.
- The default result column names are the source expression text, such as
  `DATABASE()`, `SCHEMA()`, `database()`, or `DATABASE ()`.
- `DATABASE()` and `SCHEMA()` require parentheses. `SELECT DATABASE` and
  `SELECT SCHEMA` fail with syntax error `1064`, SQLSTATE `42000`.
- `DATABASE(1)` and `SCHEMA(1)` fail with syntax error `1064`, SQLSTATE
  `42000`.
- MySQL accepts wider expression forms such as aliases and `LIMIT`; those are
  deliberately outside this MyLite slice.

## Scope

The implementation must add:

- parser and AST support for zero-argument `DATABASE()` and `SCHEMA()`
  expression nodes;
- execution of `SELECT DATABASE()`, `SELECT SCHEMA()`, multiple-item mixes of
  those two functions, and the same forms with `FROM DUAL`;
- result column names copied from the selected expression source span;
- one result row with the selected schema name as text, or SQL `NULL` when no
  schema is selected;
- connection-local behavior for multiple independent handles;
- interaction with `USE`, `CREATE DATABASE`, and `DROP DATABASE` from the
  schema lifecycle slice;
- diagnostics for unsupported function arguments and unsupported expression
  mixtures through existing parse or unsupported-statement errors;
- tests and a MySQL 8.4.9 expectation artifact for supported behavior and
  deliberately rejected wider forms.

## Non-Goals

This feature must not implement:

- general function calls or a function registry;
- aliases, `AS`, expression labels beyond default source-expression text, or
  protocol-grade metadata;
- table-backed evaluation such as `SELECT DATABASE() FROM table`;
- `WHERE`, `ORDER BY`, `LIMIT`, grouping, joins, subqueries, CTEs, or locking
  clauses on scalar current-database selects;
- `CURRENT_USER()`, `USER()`, `VERSION()`, `ROW_COUNT()`, `FOUND_ROWS()`, or
  other system functions;
- stored routine semantics where MySQL's `DATABASE()` may refer to a routine's
  schema;
- arbitrary SQLite pass-through or SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public validation,
  result-handle ownership, and failure cleanup.
- Statement context owns the statement boundary, diagnostics reset, warning
  count, and existing result conventions.
- Lexer/parser/AST own admission of the two zero-argument function syntaxes and
  their source spans. They stay independent of runtime, catalog, storage, and
  SQLite.
- Analyzer/planner code recognizes scalar one-row selects with no table source
  or `FROM DUAL`, rejects mixed unsupported expressions, and prepares result
  column names from AST spans.
- Runtime execution reads only connection-local session state. It does not
  query or mutate catalog descriptors, physical SQLite schema, catalog
  generation, descriptor caches, or `sqlite_schema_generation`.
- The result builder owns one-row text/`NULL` result construction. SQL `NULL`
  remains represented by a `NULL` pointer returned by
  `mylite_result_value_text()`.
- Storage/VFS and SQLite are not involved beyond the already-open handle. The
  `.mylite` preamble and shifted SQLite payload are untouched.

## Supported SQL Grammar

Supported subset:

```sql
SELECT DATABASE()
SELECT SCHEMA()
SELECT DATABASE(), SCHEMA()
SELECT DATABASE() FROM DUAL
SELECT SCHEMA() FROM DUAL
```

Whitespace between the function name and parentheses is accepted because the
lexer emits separate function-name and parenthesis tokens.

Parentheses around the expression are accepted only through the existing
parenthesized-expression node when the inner expression is still one of the two
supported functions:

```sql
SELECT (DATABASE())
```

MyLite Lemon-syntax grammar snippet:

```lemon
expression ::= current_database_function.

current_database_function ::= DATABASE LPAREN RPAREN.
current_database_function ::= SCHEMA LPAREN RPAREN.
```

The parser must reject argument forms such as `DATABASE(1)` and bare names such
as `DATABASE` or `SCHEMA` as syntax errors for this slice.

## Runtime Semantics

`DATABASE()` returns the connection's selected schema name as text when
`session.has_selected_schema` is true. If no schema is selected, it returns SQL
`NULL`.

`SCHEMA()` is evaluated identically to `DATABASE()`.

Successful scalar current-database selects:

- return one result row;
- return one result column per select item;
- use the source expression text as the column name;
- use `affected_rows == 0` under the existing MyLite row-result convention;
- use `warning_count == 0`;
- do not mutate session state, catalog state, physical SQLite schema, or
  storage.

`USE schema_name` changes the value returned by these functions. Dropping the
selected schema through the same handle clears the selected-schema flag, so the
functions return `NULL` after the drop. A new handle starts with no selected
schema even when catalog schemas exist in the file.

Multiple handles keep independent selected-schema state. If two handles select
different schemas in the same `.mylite` file, `DATABASE()` returns each handle's
own selected schema.

## Diagnostics

The supported function calls do not produce warnings.

Unsupported syntax that is not admitted by the grammar fails with the existing
parse error, MySQL error `1064`, SQLSTATE `42000`.

Unsupported scalar-select shapes may fail either at parse time or with the
existing unsupported-statement diagnostic class, depending on whether the
current MyLite grammar admits the wider form for another feature. Examples
include mixed non-function expressions, table-backed function evaluation,
aliases, and clauses not admitted by this scalar function slice.

Allocation failures return `MYLITE_NOMEM` and set the existing out-of-memory
diagnostic. Public API misuse behavior is unchanged.

## SQLite And Storage Handling

No generated SQLite SQL is required. The value is connection-local runtime
state, not physical storage state. This feature must not create tables,
register SQLite functions, use SQLite callbacks, change VFS behavior, or patch
SQLite.

## Tests

Fast C tests must cover:

- no selected schema returns `NULL` for both `DATABASE()` and `SCHEMA()`;
- selected schema returns that schema through both functions;
- `FROM DUAL` returns the same value;
- default column names preserve source-expression text, including lower-case
  function names and whitespace before parentheses;
- dropping the selected schema clears the returned value;
- close/reopen resets selected schema to `NULL` while persistent schemas remain
  selectable;
- two handles against one file keep independent selected-schema values;
- unsupported syntax and admitted unsupported scalar-select shapes fail
  deterministically;
- result row count, affected rows, warning count, and null representation.

The MySQL expectation script must verify the supported result values, column
names, zero warning count, and syntax errors against MySQL 8.4.9. A missing
MySQL 8.4.9 runtime blocks implementation.
