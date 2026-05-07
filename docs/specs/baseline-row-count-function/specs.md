# Baseline Row Count Function

## Status

This feature specifies the narrow scalar system-function slice for
`ROW_COUNT()`. It builds on `mylite_execute()`, statement context, parser
scaffolding, the scalar session-select execution path, baseline schema/table
lifecycle, row values, descriptor-driven `SELECT`, single-table `DELETE`, and
single-table `UPDATE`.

`ROW_COUNT()` exposes connection-local statement state. It does not add a new
public API, protocol OK-packet behavior, client capability flags, warnings
protocol metadata, or general diagnostics area support.

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
- Baseline row values:
  `docs/specs/baseline-row-values-lifecycle/specs.md`
- Baseline select where:
  `docs/specs/baseline-select-where-lifecycle/specs.md`
- Baseline select order limit:
  `docs/specs/baseline-select-order-limit-lifecycle/specs.md`
- Baseline delete:
  `docs/specs/baseline-delete-lifecycle/specs.md`
- Baseline update:
  `docs/specs/baseline-update-lifecycle/specs.md`
- Baseline current database/current user/version scalar functions:
  `docs/specs/baseline-current-database-function/specs.md`,
  `docs/specs/baseline-current-user-identity/specs.md`,
  `docs/specs/baseline-version-function/specs.md`
- MySQL lexer:
  `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- MySQL 8.4 Reference Manual, information functions:
  https://dev.mysql.com/doc/refman/8.4/en/information-functions.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against the local `mysql:8.4.9` runtime:

- `ROW_COUNT()` starts at `-1` for a new connection before user-visible DML.
- Result-set `SELECT` statements make the next `ROW_COUNT()` return `-1`,
  including `SELECT ROW_COUNT()` and `SELECT ROW_COUNT() FROM DUAL`.
- `CREATE DATABASE db` makes the next `ROW_COUNT()` return `1`.
- `USE db` makes the next `ROW_COUNT()` return `0`.
- `CREATE TABLE`, `DROP TABLE`, `RENAME TABLE`, and `TRUNCATE TABLE` make the
  next `ROW_COUNT()` return `0`.
- `DROP DATABASE db` makes the next `ROW_COUNT()` return `-1`, even when the
  client OK status reports removed table descriptors.
- `INSERT` returns the inserted row count.
- `DELETE` returns the deleted row count.
- `UPDATE` returns changed rows by default, not matched rows. A no-op update
  over matched rows returns `0`.
- Supported successful statements in this baseline report warning count `0`
  except documented warning-producing unsupported forms outside this slice.
- `ROW_COUNT(1)`, `ROW_COUNT(NULL)`, and `ROW_COUNT(1, 2)` fail with syntax
  error `1064`, SQLSTATE `42000`.
- Bare `ROW_COUNT` is not a function call and fails as an unknown column in a
  general MySQL scalar expression context. MyLite may continue to reject it
  through the existing unsupported scalar-select path until general scalar name
  resolution exists.

## Scope

The implementation must add:

- parser and AST support for zero-argument `ROW_COUNT()`;
- execution of one-row scalar `SELECT ROW_COUNT()` and the same form with
  `FROM DUAL`;
- support for mixing `ROW_COUNT()` with the existing supported scalar session
  functions in the same select list;
- connection-local tracking of the prior successful statement's MySQL-style
  row-count value;
- statement-boundary updates for the existing supported baseline statements:
  empty statement, `USE`, `CREATE/DROP DATABASE`, `SHOW DATABASES`,
  `CREATE/DROP/TRUNCATE/RENAME TABLE`, `SHOW TABLES`, `INSERT`, `SELECT`,
  `DELETE`, and `UPDATE`;
- reset to `-1` after failed SQL execution with a database handle, matching
  observed MySQL behavior for syntax and name-resolution failures in this
  slice;
- result column names copied from the selected expression source span;
- one result row containing a decimal signed integer string;
- deterministic diagnostics for unsupported `ROW_COUNT(...)` calls, bare
  `ROW_COUNT`, and wider scalar-select shapes;
- tests and a MySQL 8.4.9 expectation artifact for supported MySQL behavior
  and deliberately rejected wider forms.

## Non-Goals

This feature must not implement:

- public API additions or ABI changes;
- MySQL protocol OK-packet state, client capability flags such as
  `CLIENT_FOUND_ROWS`, or protocol-grade status metadata;
- general diagnostics area support, `GET DIAGNOSTICS`, or full warning state;
- table-backed scalar evaluation such as `SELECT ROW_COUNT() FROM table`;
- aliases, `AS`, explicit column labels, clauses, joins, grouping, CTEs,
  subqueries, locking clauses, or arbitrary scalar expressions;
- `ROW_COUNT` as a bare identifier-compatible expression;
- prepared-statement parameter behavior;
- SQLite function registration, arbitrary SQLite pass-through, or SQLite fork
  patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public validation,
  result-handle ownership, failure cleanup, and advancing connection-local
  last-row-count state at statement boundaries.
- Statement context owns per-statement reset and already carries a
  `previous_row_count` field. This slice initializes it from connection state
  for the current statement but keeps the persistent value in the connection
  session.
- Lexer/parser/AST own admission of `ROW_COUNT()` and source spans. They stay
  independent of runtime, catalog, storage, and SQLite.
- Analyzer/planner code recognizes one-row scalar selects with no table source
  or `FROM DUAL`, including mixes with existing supported scalar functions.
- Runtime execution formats the saved connection-local row-count value as text.
  It does not read or mutate catalog descriptors, physical SQLite schema,
  descriptor caches, selected schema, identity state, catalog generation, or
  `sqlite_schema_generation`.
- The result builder owns one-row text result construction. `ROW_COUNT()`
  always returns non-`NULL` text in this slice.
- Storage/VFS and SQLite are not involved beyond the existing statements whose
  public results already come from descriptor-driven execution. The `.mylite`
  preamble and shifted SQLite payload are untouched.

## Supported SQL Grammar

Supported subset:

```sql
SELECT ROW_COUNT()
SELECT ROW_COUNT() FROM DUAL
SELECT ROW_COUNT(), DATABASE(), USER(), CURRENT_USER, VERSION()
```

Whitespace between `ROW_COUNT` and parentheses is accepted. Parentheses around
the expression are accepted through the existing parenthesized-expression node:

```sql
SELECT (ROW_COUNT())
```

MyLite Lemon-syntax grammar snippet:

```lemon
expression ::= row_count_function.

row_count_function ::= ROW_COUNT LPAREN RPAREN.
```

`ROW_COUNT` remains usable as an ordinary unquoted identifier in identifier
positions where the parser admits nonreserved keywords. Bare `ROW_COUNT` is not
an admitted row-count function form.

Unlike `VERSION()`, nonzero-argument `ROW_COUNT(...)` is not parsed as a native
function argument-count error. MySQL 8.4.9 treats `ROW_COUNT(1)` and similar
forms as syntax errors, so MyLite must preserve parse-time rejection for this
slice.

## Runtime Semantics

`ROW_COUNT()` returns the connection-local MySQL-style row-count value from the
previous statement as a signed decimal text value.

Initial state:

- a newly opened memory or file-backed handle starts with row count `-1`;
- close/reopen creates a new handle and therefore starts with `-1`; row-count
  state is not persisted in `.mylite` files.

Statement-boundary update rules for this baseline:

- result-set statements, including `SELECT`, `SHOW TABLES`, `SHOW DATABASES`,
  `SELECT ROW_COUNT()`, and mixed scalar selects, store `-1` after they
  complete;
- empty statements store `0`;
- `USE` stores `0`;
- `CREATE DATABASE` / `CREATE SCHEMA` store the public affected-row value,
  currently `1`;
- `DROP DATABASE` / `DROP SCHEMA` store `-1`, matching observed MySQL 8.4.9
  `ROW_COUNT()` behavior even though MyLite's public result affected-row value
  remains the number of removed table descriptors from the schema lifecycle
  slice;
- `CREATE TABLE`, `DROP TABLE`, `TRUNCATE TABLE`, and `RENAME TABLE` store
  `0`;
- `INSERT`, `DELETE`, and `UPDATE` store the public affected-row value already
  produced by descriptor-driven execution. For `UPDATE`, that value is changed
  rows, not matched rows;
- failed SQL execution with a valid database handle stores `-1`;
- public API misuse with no valid database handle is unchanged.

Successful scalar row-count selects:

- return one result row;
- return one result column per select item;
- use the source expression text as the column name;
- use `affected_rows == 0` under the existing MyLite row-result convention;
- use `warning_count == 0` for supported in-range forms;
- then update the saved row-count state to `-1` because the select returned a
  result set.

`ROW_COUNT()` in a mixed scalar select observes the row-count value from before
the current select begins. Other expressions in the same scalar select do not
change the value seen by `ROW_COUNT()`.

## Diagnostics

The supported `ROW_COUNT()` function call does not produce warnings.

Unsupported `ROW_COUNT(...)` calls with one or more arguments fail at parse
time with MySQL error `1064`, SQLSTATE `42000`, through the existing parse
diagnostic. Unsupported argument expressions may fail with the same parse class
before any runtime state is mutated.

Bare `ROW_COUNT` is outside this slice. It may fail through the existing
unsupported-table-select diagnostic rather than MySQL's exact unknown-column
diagnostic until general scalar name resolution exists.

Unsupported scalar-select shapes may fail either at parse time or with the
existing unsupported-statement diagnostic class, depending on whether the
current MyLite grammar admits the wider form for another feature. Examples
include table-backed row-count evaluation, aliases, clauses, and mixed
unsupported expressions.

Allocation failures return `MYLITE_NOMEM` and set the existing out-of-memory
diagnostic. Physical SQLite failures from the underlying statements continue
to use their current diagnostics. Public API misuse behavior is unchanged.

## SQLite And Storage Handling

No generated SQLite SQL is required for evaluating `ROW_COUNT()` itself. The
value is connection-local session state, not catalog or physical row storage.

Existing descriptor-driven statements keep their current SQLite behavior:
generated SQL comes only from descriptors and stable physical table names,
uses quoted identifiers, binds values, and preserves the MyLite file preamble
and shifted SQLite payload invariants. This feature must not add tables,
catalog rows, SQLite functions, VFS behavior, or SQLite fork patches.

## Tests

Fast C tests must cover:

- initial `ROW_COUNT()` on memory and file-backed handles;
- result values and default labels for `ROW_COUNT()`, lower/mixed-case
  spelling, whitespace before parentheses, surrounding parentheses, and
  `FROM DUAL`;
- mixed scalar selects with `DATABASE()`, `USER()`, `CURRENT_USER`, and
  `VERSION()`;
- row-count transitions for `CREATE DATABASE`, `USE`, `CREATE TABLE`,
  `INSERT`, result-set `SELECT`, repeated `ROW_COUNT()` selects, no-op and
  changed `UPDATE`, no-match and matched `DELETE`, `TRUNCATE TABLE`,
  `RENAME TABLE`, `DROP TABLE`, `SHOW TABLES`, `SHOW DATABASES`, and
  `DROP DATABASE`;
- `UPDATE` changed-row semantics and no-op update returning `0`;
- close/reopen and independent handle state;
- file-backed row persistence staying independent from nonpersistent
  row-count state;
- unsupported `ROW_COUNT(1)`, `ROW_COUNT(NULL)`, `ROW_COUNT(1, 2)`, bare
  `ROW_COUNT`, aliases, clauses, and table-backed scalar evaluation failing
  deterministically;
- result row count, affected rows, warning count, and non-`NULL` text
  representation;
- zero-initialized cleanup through existing result and statement objects.

The MySQL expectation script must verify MySQL 8.4.9 result values, labels,
warning counts where relevant, DDL/DML/select transitions, changed-row update
semantics, unsupported argument syntax, bare-name behavior, and wider forms. A
missing MySQL 8.4.9 runtime blocks implementation.

## Compatibility Documentation

Update `COMPATIBILITY.md` and `docs/compatibility/functions-system.md` to mark
only the limited scalar `ROW_COUNT()` slice as partial. Do not claim
`CLIENT_FOUND_ROWS`, protocol OK-packet parity, table-backed expression
evaluation, aliases, general diagnostics area behavior, prepared statements,
or general scalar expression support.
