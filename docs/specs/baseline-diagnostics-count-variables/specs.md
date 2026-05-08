# Baseline Diagnostics Count Variables

## Status

This feature specifies a narrow scalar system-variable slice for
`@@warning_count` and `@@error_count`. It builds on the previous diagnostics
snapshot used by `SHOW WARNINGS`, `SHOW COUNT(*) WARNINGS`, `SHOW ERRORS`,
and `SHOW COUNT(*) ERRORS`.

The feature is intentionally not general system-variable support. MyLite adds
only read access to the two diagnostics count variables inside the existing
one-row session scalar `SELECT` path.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- Baseline diagnostics snapshots:
  `docs/specs/baseline-show-warnings-diagnostics/specs.md` and
  `docs/specs/baseline-show-errors-diagnostics/specs.md`
- MySQL lexer: `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold: `docs/specs/mysql-parser-scaffold/specs.md`
- MySQL 8.4 Reference Manual, server system variables:
  https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html
- MySQL 8.4 Reference Manual, the diagnostics area:
  https://dev.mysql.com/doc/refman/8.4/en/diagnostics-area.html
- MySQL 8.4 Reference Manual, `SHOW WARNINGS`:
  https://dev.mysql.com/doc/refman/8.4/en/show-warnings.html
- MySQL 8.4 Reference Manual, `SHOW ERRORS`:
  https://dev.mysql.com/doc/refman/8.4/en/show-errors.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against the local `mysql:8.4.9` runtime:

- `SELECT @@warning_count`, `SELECT @@session.warning_count`, and
  `SELECT @@local.warning_count` are accepted and return the number of
  previous statement conditions that count as warnings, errors, or notes.
- `SELECT @@error_count`, `SELECT @@session.error_count`, and
  `SELECT @@local.error_count` are accepted and return the number of previous
  statement error conditions.
- The variable names and `session` / `local` qualifiers are
  case-insensitive.
- `@@session.`quoted variable name`` and `@@`quoted variable name`` are
  accepted for these variables; quoted `session` or `local` scope names are
  syntax errors.
- `SELECT @@global.warning_count` and `SELECT @@global.error_count` fail with
  error `1238`, SQLSTATE `HY000`, because both variables are session-only.
- Unknown system variables fail with error `1193`, SQLSTATE `HY000`.
- `SHOW COUNT(*) WARNINGS` and `SHOW COUNT(*) ERRORS` are diagnostic
  statements and preserve the diagnostics list. A `SELECT @@warning_count` or
  `SELECT @@error_count` is a nondiagnostic `SELECT`: it returns the previous
  count and then clears the diagnostics list for following diagnostic
  statements.
- After a warning-only statement such as `SHOW PROCESSLIST`,
  `SELECT @@warning_count, @@error_count, ROW_COUNT()` returns `1`, `0`, and
  the previous row-count value `-1`; a following `SHOW COUNT(*) WARNINGS`
  returns `0` because the scalar `SELECT` cleared the diagnostics list.
- After a parse error, `SELECT @@error_count, @@warning_count, ROW_COUNT()`
  returns `1`, `1`, and `-1`; following `SHOW COUNT(*) ERRORS` and
  `SHOW COUNT(*) WARNINGS` return `0` after that scalar `SELECT`.
- Column labels for scalar selects are the original expression source text,
  including qualifiers, case, and parentheses.

The local MySQL client can start a session with an initialization warning, so
expectation scripts first execute a harmless nondiagnostic statement before
checking empty diagnostics counts.

## Scope

The implementation must add:

- parser and AST support for system-variable tokens as expressions;
- runtime recognition of `@@warning_count`, `@@session.warning_count`,
  `@@local.warning_count`, `@@error_count`, `@@session.error_count`, and
  `@@local.error_count` inside the existing session scalar `SELECT` subset;
- case-insensitive variable and scope matching;
- support for a quoted variable-name component for the two admitted variables;
- MySQL-compatible diagnostics for `@@global.warning_count`,
  `@@global.error_count`, and unknown system variables;
- one-row scalar result sets with the existing source-span column-name
  behavior;
- nondiagnostic statement lifecycle behavior: successful scalar variable
  selects read the previous diagnostics snapshot and then clear it;
- fast C tests and a MySQL 8.4.9 expectation artifact.

Supported SQL examples:

```sql
SELECT @@warning_count
SELECT @@error_count
SELECT @@session.warning_count, @@local.error_count
SELECT (@@warning_count), @@session.`warning_count`, @@`error_count`
SELECT @@warning_count, ROW_COUNT() FROM DUAL
```

## Non-Goals

This feature must not implement:

- `SHOW VARIABLES`, `SET`, `SET_VAR` hints, persisted variables, privilege
  checks, global system-variable values, or variable assignment;
- any variable other than `warning_count` and `error_count`;
- `@@version`, `@@max_error_count`, `@@sql_notes`, connection character-set
  variables, SQL mode variables, status variables, user variables, or
  Performance Schema variable tables;
- general expression evaluation involving system variables, such as
  `@@warning_count + 1`;
- table-backed variable evaluation, aliases, clauses, subqueries, functions
  over variables, parameters, prepared statements, or protocol metadata
  beyond existing result conventions;
- notes, counted-but-not-stored diagnostics, `max_error_count`,
  `sql_notes`, `GET DIAGNOSTICS`, diagnostics stacks, or multiple stored
  error conditions beyond the current MyLite diagnostics model;
- catalog mutations, storage mutations, SQLite metadata reads, arbitrary
  SQLite SQL pass-through, or SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public
  validation, parse/execution orchestration, result ownership, statement
  row-count state, diagnostics snapshot replacement, and failure cleanup.
- Statement context continues to reset live diagnostics at statement start.
  This slice reads the connection-owned previous snapshot during execution,
  then lets normal successful nondiagnostic statement completion replace that
  snapshot with the now-empty live diagnostics.
- Lexer/parser/AST own syntax admission and source spans for system-variable
  expressions. They remain independent of runtime, catalog, storage, and
  SQLite.
- Runtime execution owns variable-name normalization, scope validation,
  count calculation from MyLite diagnostics, and result value formatting.
- Result builder owns scalar result column labels and one-row text values.
- Catalog, storage, VFS, and SQLite physical row storage are not involved.
  This feature must not touch `.mylite` preamble bytes or SQLite schema state.

## Supported SQL Grammar

This slice extends the existing scalar `SELECT` expression grammar with one
new expression atom:

```sql
@@warning_count
@@error_count
@@session.warning_count
@@session.error_count
@@local.warning_count
@@local.error_count
```

MyLite Lemon-syntax grammar snippet:

```lemon
expression ::= SYSTEM_VARIABLE.
```

The lexer already produces one token for a system variable beginning with
`@@`. The parser stores it as a system-variable AST expression with the raw
source span. Runtime validation decides whether the token names one of the
two admitted variables.

The existing scalar `SELECT` limits continue to apply:

```lemon
select_statement ::= SELECT select_item_list from_dual_opt.
select_item ::= expression.
from_dual_opt ::= .
from_dual_opt ::= FROM DUAL.
```

System variables are admitted only when every selected expression is in the
existing session scalar expression set. Clauses such as `WHERE`, `ORDER BY`,
`LIMIT`, table-backed `FROM`, aliases, and general expressions remain outside
this slice.

## Variable Resolution

Runtime normalizes the raw token as follows:

- it requires a leading `@@`;
- it accepts no scope, `session`, or `local` for the admitted variables;
- it rejects `global` for the admitted variables with MySQL error `1238`;
- it treats all unquoted letters case-insensitively;
- it accepts a backtick-quoted final variable-name component and unescapes
  doubled backticks before comparison;
- it rejects quoted scope names and malformed paths with deterministic syntax
  or unknown-variable diagnostics;
- it reports unknown names with MySQL error `1193`, SQLSTATE `HY000`.

The result column name is not normalized. It remains the original expression
source text, matching the existing scalar result convention and observed MySQL
labels.

## Count Semantics

`@@warning_count` returns:

```text
(previous error condition present ? 1 : 0) + previous warning record count
```

`@@error_count` returns:

```text
previous error condition present ? 1 : 0
```

These counts are connection-local and in-memory. They are not stored in the
`.mylite` file and are not shared between independent handles.

Successful scalar variable `SELECT` statements are nondiagnostic. They read
the previous snapshot during expression evaluation, then normal statement
completion replaces the previous snapshot with the empty live diagnostics
area. This intentionally differs from `SHOW COUNT(*) WARNINGS` and
`SHOW COUNT(*) ERRORS`, which preserve the previous snapshot.

Successful scalar variable selects return a row result set, set result
`warning_count` to `0`, and make following `ROW_COUNT()` return `-1` through
the existing row-result convention. `ROW_COUNT()` inside the same select list
continues to return the previous statement row count captured at statement
start.

## Diagnostics

Diagnostics follow existing MyLite policy plus MySQL-runtime-verified system
variable errors:

- `@@global.warning_count` and `@@global.error_count`: error `1238`,
  SQLSTATE `HY000`, message containing `SESSION variable`;
- unsupported or unknown system variables: error `1193`, SQLSTATE `HY000`,
  message containing `Unknown system variable`;
- system variables outside the limited scalar `SELECT` subset:
  deterministic unsupported or syntax diagnostics;
- allocation failure while formatting count text, copying column labels, or
  appending rows: `HY001`;
- public API misuse remains unchanged.

Successful in-range reads produce no warnings.

## SQLite, Catalog, And File Format Policy

This feature is implemented entirely in MyLite runtime code. It must not:

- query SQLite;
- generate SQLite SQL;
- bind SQLite parameters;
- read or mutate catalog descriptor rows;
- change catalog generation or SQLite schema generation;
- alter physical user tables;
- alter the `.mylite` preamble or shifted SQLite payload invariant;
- add SQLite fork patches.

## Tests

Fast C tests should cover:

- parser acceptance for unqualified, session-qualified, local-qualified,
  case-varied, parenthesized, and quoted-component system variables;
- parser rejection or runtime unsupported behavior for aliases, clauses,
  table-backed `FROM`, arithmetic, user variables, parameters, and malformed
  variables;
- scalar result labels and values for empty diagnostics;
- warning-only diagnostics from `SHOW PROCESSLIST`;
- parse-error diagnostics;
- diagnostic `SHOW COUNT(*)` preservation versus scalar-variable clearing;
- `ROW_COUNT()` in the same select list and after the scalar select;
- global-scope and unknown-variable diagnostics;
- independent handles;
- file-backed preamble and generation invariants.

The MySQL expectation artifact must verify result headers, values, scope
forms, quoted variable components, case-insensitivity, diagnostic clearing,
global-scope errors, unknown-variable errors, and parse-error behavior against
MySQL 8.4.9.
