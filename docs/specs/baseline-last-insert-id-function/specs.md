# Baseline Last Insert ID Function

## Status

This feature specifies the narrow scalar system-function slice for
`LAST_INSERT_ID()`. It builds on `mylite_execute()`, statement context, parser
scaffolding, the scalar session-select execution path, baseline row-count
tracking, and current descriptor-driven table DML.

MyLite does not yet implement `AUTO_INCREMENT` columns, generated insert ids,
the C client API, or general expression evaluation. For this baseline slice,
`LAST_INSERT_ID()` therefore exposes the MySQL-compatible no-generated-id
connection value `0`.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- Baseline row values:
  `docs/specs/baseline-row-values-lifecycle/specs.md`
- Baseline update:
  `docs/specs/baseline-update-lifecycle/specs.md`
- Baseline row-count function:
  `docs/specs/baseline-row-count-function/specs.md`
- Baseline current database/current user/version/current role scalar
  functions:
  `docs/specs/baseline-current-database-function/specs.md`,
  `docs/specs/baseline-current-user-identity/specs.md`,
  `docs/specs/baseline-version-function/specs.md`,
  `docs/specs/baseline-current-role-function/specs.md`
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

- A fresh connection returns `0` for `SELECT LAST_INSERT_ID()`.
- `SELECT 1; SELECT LAST_INSERT_ID(), @@warning_count, ROW_COUNT();` returns
  `0`, warning count `0`, and row count `-1`.
- Result column names preserve the source expression text, including
  lower-case spelling, mixed-case spelling, whitespace before parentheses, and
  surrounding parentheses.
- `SELECT LAST_INSERT_ID() FROM DUAL` returns one row with `0`.
- Creating a table without `AUTO_INCREMENT` and inserting rows does not change
  `LAST_INSERT_ID()`; it remains `0`.
- `LAST_INSERT_ID(7)` returns `7` and makes a subsequent
  `LAST_INSERT_ID()` return `7`.
- `LAST_INSERT_ID(NULL)` returns `NULL` and makes a subsequent
  `LAST_INSERT_ID()` return `0`.
- `LAST_INSERT_ID(-1)` returns `18446744073709551615` and makes a subsequent
  `LAST_INSERT_ID()` return that same unsigned value.
- `LAST_INSERT_ID(1, 2)` fails with error `1582`, SQLSTATE `42000`, reporting
  an incorrect parameter count for native function `LAST_INSERT_ID`.
- Bare `LAST_INSERT_ID` is not an information function and fails as an unknown
  column in a general MySQL scalar expression context.

The official information-function documentation describes `LAST_INSERT_ID()`
as returning the first automatically generated value from the most recent
successful `INSERT` into an `AUTO_INCREMENT` column, remaining unchanged when
no row generated such a value. It also documents `LAST_INSERT_ID(expr)` as a
state-setting expression form. This slice implements only the zero-argument
read form because MyLite has no auto-increment storage and no general
state-mutating expression evaluation yet.

## Scope

The implementation must add:

- parser and AST support for zero-argument `LAST_INSERT_ID()`;
- execution of one-row scalar `SELECT LAST_INSERT_ID()` and the same form with
  `FROM DUAL`;
- support for mixing `LAST_INSERT_ID()` with the existing supported scalar
  session functions and variables in the same select list;
- connection-local initialization of the last-insert-id value to `0`;
- no changes to that value for currently supported `CREATE`, `INSERT`,
  `UPDATE`, `DELETE`, `TRUNCATE`, `ALTER`, `DROP`, `RENAME`, `SHOW`, `USE`,
  and scalar `SELECT` statements, because none can generate auto-increment ids
  in this baseline;
- result column names copied from the selected expression source span;
- one result row containing unsigned decimal text `0`;
- deterministic diagnostics for `LAST_INSERT_ID(...)` argument forms, bare
  `LAST_INSERT_ID`, and wider scalar-select shapes that are outside this
  slice;
- tests and a MySQL 8.4.9 expectation artifact for supported MySQL behavior
  and deliberately rejected wider forms.

## Non-Goals

This feature must not implement:

- `AUTO_INCREMENT` column definitions, allocation, metadata, or generated ids;
- `LAST_INSERT_ID(expr)`, including integer, `NULL`, column, arithmetic, or
  sequence-emulation expression forms;
- `mysql_insert_id()` C API behavior, protocol OK-packet insert-id metadata,
  or client capability behavior;
- persistent storage of last-insert-id state;
- table-backed scalar evaluation such as `SELECT LAST_INSERT_ID() FROM table`;
- aliases, `AS`, explicit labels, clauses, joins, grouping, CTEs, subqueries,
  or locking clauses on scalar last-insert-id selects;
- `LAST_INSERT_ID` as a bare identifier-compatible expression;
- SQLite function registration, arbitrary SQLite pass-through, or SQLite fork
  patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public validation,
  result-handle ownership, failure cleanup, and preserving existing
  statement-boundary row-count behavior.
- Statement context owns per-statement reset, diagnostics, warning count, and
  the current row-result conventions. It does not own persistent
  last-insert-id state.
- The connection/session state owns the connection-local last-insert-id value.
  The initial value is `0`, and all currently supported statements leave it
  unchanged.
- Lexer/parser/AST own admission of `LAST_INSERT_ID()` and source spans. They
  stay independent of runtime, catalog, storage, and SQLite.
- Analyzer/planner code recognizes one-row scalar selects with no table source
  or `FROM DUAL`, including mixes with existing supported scalar functions and
  variables.
- Runtime execution formats the connection-local last-insert-id value as text.
  It does not read or mutate catalog descriptors, physical SQLite schema,
  descriptor caches, selected schema, identity state, catalog generation, or
  `sqlite_schema_generation`.
- The result builder owns one-row text result construction.
  `LAST_INSERT_ID()` always returns non-`NULL` text in this slice.
- Storage/VFS and SQLite are not involved beyond the already-open handle. The
  `.mylite` preamble and shifted SQLite payload are untouched.

## Supported SQL Grammar

Supported subset:

```sql
SELECT LAST_INSERT_ID()
SELECT LAST_INSERT_ID() FROM DUAL
SELECT LAST_INSERT_ID(), ROW_COUNT(), DATABASE(), USER(), VERSION()
```

Whitespace between `LAST_INSERT_ID` and parentheses is accepted because MySQL
accepts it and the lexer emits separate function-name and parenthesis tokens.
Parentheses around the expression are accepted through the existing
parenthesized-expression node:

```sql
SELECT (LAST_INSERT_ID())
```

MyLite Lemon-syntax grammar snippet:

```lemon
expression ::= last_insert_id_function.

last_insert_id_function ::= LAST_INSERT_ID LPAREN RPAREN.
```

`LAST_INSERT_ID` remains usable as an ordinary unquoted identifier in
identifier positions where the parser admits nonreserved keywords. Bare
`LAST_INSERT_ID` is not an admitted last-insert-id function form.

The parser should reject or route nonzero-argument forms to a deterministic
unsupported diagnostic. It must not report MySQL's wrong-parameter-count error
for one-argument forms, because `LAST_INSERT_ID(expr)` is valid MySQL syntax
and is deferred for a later state-mutating expression slice.

## Runtime Semantics

`LAST_INSERT_ID()` returns the connection-local MySQL-style last insert id as
unsigned decimal text. In this baseline the value is always `0`.

Initial state:

- a newly opened memory or file-backed handle starts with last insert id `0`;
- close/reopen creates a new handle and therefore starts with `0`;
- last-insert-id state is not persisted in `.mylite` files;
- independent handles maintain independent connection-local state, currently
  initialized to the same `0` value.

Statement-boundary behavior:

- supported statements that do not generate an auto-increment value leave the
  value unchanged;
- failed statements leave the value unchanged for this baseline;
- scalar `SELECT LAST_INSERT_ID()` itself leaves the value unchanged but, like
  other result-set statements, updates `ROW_COUNT()` state to `-1` after the
  select completes.

Successful scalar last-insert-id selects:

- return one result row;
- return one result column per select item;
- use the source expression text as the column name;
- use `affected_rows == 0` under the existing MyLite row-result convention;
- use `warning_count == 0` for supported forms;
- do not mutate catalog state, physical SQLite schema, storage, or the
  `.mylite` file format.

`LAST_INSERT_ID()` in a mixed scalar select observes the value from before the
current select begins. Other supported expressions in the same scalar select
do not change the last-insert-id value.

## Diagnostics

The supported zero-argument `LAST_INSERT_ID()` function call does not produce
warnings.

`LAST_INSERT_ID(expr)` is outside this slice. If the parser admits an argument
form for deterministic diagnostics, runtime must report a MyLite-specific
unsupported-feature error that clearly distinguishes the deferred valid MySQL
one-argument form from wrong parameter count. `LAST_INSERT_ID(1, 2)` may use
MySQL-compatible error `1582`, SQLSTATE `42000`, because MySQL rejects multiple
arguments as an incorrect native-function parameter count.

Bare `LAST_INSERT_ID` is outside this slice. It may fail through the existing
unsupported scalar-select path until general scalar name resolution exists.

Unsupported scalar-select shapes may fail either at parse time or with the
existing unsupported-statement diagnostic class, depending on whether the
current MyLite grammar admits the wider form for another feature. Examples
include table-backed last-insert-id evaluation, aliases, clauses, and mixed
unsupported expressions.

Allocation failures return `MYLITE_NOMEM` and set the existing out-of-memory
diagnostic. Public API misuse behavior is unchanged.

## SQLite And Storage Handling

No generated SQLite SQL is required. The value is connection-local session
state, not physical storage state. This feature must not create tables,
register SQLite functions, use SQLite callbacks, change VFS behavior, or patch
SQLite.

## Tests

Fast C tests must cover:

- `LAST_INSERT_ID()` returning `0` for fresh memory and file-backed handles;
- `FROM DUAL` returning the same value;
- mixed supported scalar last-insert-id, row-count, current database,
  current identity, current role, connection id, and version function selects;
- default column names preserving source-expression text, including lower-case
  spelling, mixed-case spelling, whitespace before parentheses, and
  surrounding parentheses;
- supported `CREATE TABLE`, non-auto-increment `INSERT`, `UPDATE`, `DELETE`,
  `TRUNCATE`, `ALTER`, `RENAME`, `DROP`, `SHOW`, and scalar `SELECT`
  statements leaving `LAST_INSERT_ID()` as `0`;
- `ROW_COUNT()` interaction: a `LAST_INSERT_ID()` scalar select returns the
  prior row count during the select and then stores `-1`;
- selected-schema changes and schema drops not changing the value;
- close/reopen behavior starting a new handle at `0` and preserving the file
  preamble;
- independent handles returning `0` without shared mutable state;
- unsupported one-argument forms rejected with a deterministic unsupported
  diagnostic, not a wrong-parameter-count diagnostic;
- unsupported multiple-argument forms rejected deterministically;
- bare `LAST_INSERT_ID` and unsupported scalar-select shapes failing
  deterministically;
- unchanged catalog generation and `sqlite_schema_generation` for scalar reads;
- zero-initialized cleanup through existing result and statement paths.

The MySQL expectation artifact must verify:

- fresh `LAST_INSERT_ID()` value, warning count, and row-count interaction;
- source-expression labels;
- `FROM DUAL`;
- non-auto-increment insert interaction;
- observed one-argument and multiple-argument MySQL behavior that this slice
  deliberately defers or rejects;
- bare-name behavior;
- MySQL version `8.4.9`.

## Compatibility Documentation

Update `COMPATIBILITY.md` and `docs/compatibility/functions-system.md` to mark
only zero-argument `LAST_INSERT_ID()` as limited support. Do not claim
auto-increment, `LAST_INSERT_ID(expr)`, protocol insert-id metadata, C API
state, sequence emulation, or generated id behavior.

## Verification

Before completion:

1. `cmake --build --preset dev`
2. New parser/runtime CTest entries and nearby scalar function tests.
3. `packages/libmylite/tests/mysql_baseline_last_insert_id_function_expectations.sh`
4. `cmake --workflow --preset check`
