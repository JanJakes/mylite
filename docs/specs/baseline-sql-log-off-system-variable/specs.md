# Baseline SQL Log Off System Variable

## Status

This feature specifies the baseline session-variable slice for
`@@sql_log_off`.

It builds on the existing `SYSTEM_VARIABLE` lexer/parser token, scalar
`SELECT` execution, diagnostics lifecycle, generic Boolean session-variable
assignment, `SHOW VARIABLES`, and descriptor-driven statement paths. MySQL
exposes `sql_log_off` as mutable global and session state that controls whether
the current session suppresses general query log writes when the general query
log itself is enabled. MyLite implements the embedded compatibility baseline:
session `SET`/readback and `SHOW VARIABLES` state, fixed global default `OFF`,
and no changed execution because MyLite does not implement general query logs.

This is not general query logging support. It does not implement
server-global mutation, log file or log-table writes, restricted variable
privileges, or logging changes for SQL statements.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- MySQL lexer: `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold: `docs/specs/mysql-parser-scaffold/specs.md`
- MySQL 8.4 Reference Manual, server system variables:
  https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_sql_log_off_system_variable_expectations.sh`
records the runtime probes for this feature. The primary probes were run
against container `mylite-mysql-849` with:

```sh
docker exec -i mylite-mysql-849 mysql \
  --protocol=TCP -h127.0.0.1 -uroot \
  --batch --raw --skip-column-names --default-character-set=utf8mb4
```

Observed behavior:

- `SELECT VERSION()` returned `8.4.9`.
- `SELECT @@sql_log_off`, `@@global.sql_log_off`,
  `@@session.sql_log_off`, `@@local.sql_log_off`, and
  `@@SQL_LOG_OFF` return `0` in the tested default runtime.
- The variable has global and session scope. After
  `SET SESSION sql_log_off=1`, unscoped, `session`, and `local` reads return
  `1`, while `global` still returns `0`; assigning `DEFAULT` restores the
  default session value.
- `SHOW VARIABLES LIKE 'sql_log_off'` reflects the session value; `SHOW GLOBAL
  VARIABLES LIKE 'sql_log_off'` reflects the fixed global value.
- Variable and scope names are case-insensitive.
- Backtick-quoted final variable-name components are accepted.
- Backtick-quoted scope names, such as ``@@`session`.sql_log_off``, are syntax
  errors.
- Unknown variables fail with error `1193`, SQLSTATE `HY000`, and an
  `Unknown system variable` message.
- A scalar `SELECT` that reads this variable is nondiagnostic. It reads the
  previous diagnostics snapshot for any `@@warning_count` or `@@error_count`
  items in the same select list, then clears diagnostics for following
  diagnostic statements.
- MySQL accepts wider expression forms such as `SELECT @@sql_log_off + 1`.
  Those forms remain outside this MyLite slice.

The official MySQL system-variable documentation classifies `sql_log_off` as
a dynamic boolean variable with global and session scope and default value
`OFF`. When enabled for a session, MySQL disables general query logging for
that session, assuming the general log itself is enabled. MyLite records the
session value but does not alter statement logging or create log storage.

## Scope

The implementation must add:

- runtime recognition of `sql_log_off` inside the existing scalar `SELECT`
  subset;
- support for no scope, `session`, `local`, and `global` scope qualifiers;
- session `SET sql_log_off = 0|1|ON|OFF|TRUE|FALSE|DEFAULT` handling through
  the existing Boolean system-variable override path;
- `SHOW VARIABLES` and `SHOW GLOBAL VARIABLES` values for the session/global
  baseline;
- case-insensitive matching for unquoted scope and variable names;
- backtick-quoted final variable-name components;
- one-row scalar result sets with existing source-span column labels;
- fixed global value `0` and session-local values that default to `0`;
- MySQL-compatible unknown-variable diagnostics for unsupported names;
- deterministic rejection of quoted scopes;
- fast C tests and a MySQL 8.4.9 expectation artifact.

Supported SQL examples:

```sql
SELECT @@sql_log_off
SELECT @@sql_log_off FROM DUAL
SELECT @@session.sql_log_off, @@local.sql_log_off
SELECT @@global.sql_log_off
SELECT @@session.`sql_log_off`, @@`sql_log_off`
SELECT @@sql_log_off, @@warning_count, ROW_COUNT()
SET SESSION sql_log_off = 1
SHOW VARIABLES LIKE 'sql_log_off'
SHOW GLOBAL VARIABLES LIKE 'sql_log_off'
```

## Non-Goals

This feature must not implement:

- startup options, persisted variables, `SET_VAR` hints, or mutable
  server-global `sql_log_off` state;
- general query log enabling, log output routing, log file writes, log-row
  writes to `mysql.general_log`, slow query log behavior, log redaction, or
  restricted variable privilege checks;
- variables other than `sql_log_off`;
- changed descriptor-backed DDL, DML, or `SELECT` execution;
- Performance Schema variable tables;
- table-backed variable evaluation, aliases, clauses, subqueries, arithmetic,
  functions over variables, parameters, prepared statements, or arbitrary
  SQLite pass-through;
- catalog mutations, storage mutations, SQLite metadata reads, or SQLite fork
  patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public validation,
  parse/execution orchestration, result ownership, row-count state,
  diagnostics snapshot replacement, and failure cleanup.
- Statement context continues to reset live diagnostics at statement start and
  preserve the previous diagnostics snapshot until nondiagnostic successful
  completion replaces it.
- Lexer/parser/AST own syntax admission and source spans for
  `SYSTEM_VARIABLE` expressions. No new grammar is needed beyond the existing
  `expression ::= SYSTEM_VARIABLE` rule.
- Runtime execution owns system-variable path parsing, scope validation,
  session/global value selection, and diagnostics for unsupported names.
- Descriptor-driven statement execution remains unchanged because this scalar
  variable does not influence MyLite storage, planning, or row visibility in
  this slice.
- The catalog remains authoritative for descriptors. This variable slice does
  not create log metadata or affect table lifecycle.
- Result builder owns scalar result column labels and one-row text values.
- Storage, VFS, and SQLite physical row storage are not involved. This feature
  must not touch `.mylite` preamble bytes or SQLite schema state.

## Supported SQL Grammar

This slice uses the existing system-variable expression atom:

```lemon
expression ::= SYSTEM_VARIABLE.
```

The supported runtime variable paths are:

```sql
@@sql_log_off
@@session.sql_log_off
@@local.sql_log_off
@@global.sql_log_off
```

The existing scalar `SELECT` limits continue to apply:

```lemon
select_statement ::= SELECT select_item_list from_dual_opt.
select_item ::= expression.
from_dual_opt ::= .
from_dual_opt ::= FROM DUAL.
```

System variables are admitted only when every selected expression is in the
existing scalar expression set. Clauses such as `WHERE`, `ORDER BY`, `LIMIT`,
table-backed `FROM`, aliases, and general expressions remain outside this
slice.

## Variable Resolution

Runtime parses the raw token as a `@@` system variable:

- it accepts no scope, `session`, `local`, or `global`;
- it treats unquoted names ASCII case-insensitively;
- it accepts a backtick-quoted final variable-name component and unescapes
  doubled backticks before comparison;
- it rejects backtick-quoted scope names with a deterministic syntax
  diagnostic;
- it rejects malformed paths and unsupported variables with MySQL error `1193`,
  SQLSTATE `HY000`;
- it preserves the original source text as the scalar result column label.

For this slice, global scope returns the fixed embedded default `0`. Unscoped,
`session`, and `local` scope read the current session override when one exists,
and otherwise return `0`.

## Runtime Semantics

The supported variable returns:

| Scope | Value |
| --- | --- |
| Global | fixed `0` |
| Session/local/unscoped default | `0` |
| Session/local/unscoped after `SET SESSION sql_log_off = 1` | `1` |

The session value is independent per handle and resets on close/reopen.
Existing statement execution must not change, and MyLite must not write
general query log records in this slice.

Successful scalar reads:

- return one row and one text column for each selected expression;
- use the original source expression as the column label unless the general
  scalar-select path later adds alias support;
- leave `warning_count == 0` for supported forms;
- do not mutate catalog rows, descriptor versions, descriptor caches, catalog
  generation, physical SQLite schema, or `.mylite` preamble bytes;
- follow existing scalar `SELECT` row-count behavior, so `ROW_COUNT()` after a
  successful scalar row result is `-1`.

## Diagnostics

This slice uses existing diagnostics for:

- syntax errors, including quoted scopes and unsupported scalar-select
  clauses;
- unknown system variables: error `1193`, SQLSTATE `HY000`;
- unsupported expressions such as arithmetic over system variables;
- allocation failures: MyLite runtime failure diagnostics;
- public API misuse: existing public execution/result misuse behavior.

Successful supported reads and session assignments produce no warnings.

## Tests

Add a focused C runtime test under `packages/libmylite/tests/`, registered as
`libmylite.runtime.sql_log_off_system_variable`.

The C tests must cover:

- default value `0`, mutable session values, fixed global `0`, and
  close/reopen reset behavior;
- source-text labels, case-insensitive names, quoted final variable names,
  `FROM DUAL`, selected-schema independence, and mixed scalar reads with other
  baseline variables;
- warning and error diagnostics snapshot behavior;
- unknown unscoped and scoped variable diagnostics;
- quoted-scope rejection and unsupported expression rejection;
- session and global `SHOW VARIABLES` rows after session assignment;
- close/reopen reset, independent handles, unchanged catalog and SQLite
  generations, and `.mylite` preamble preservation;
- descriptor-backed DDL/DML independence, including that simple table changes
  still work and are not logged or blocked by this scalar variable.

The MySQL expectation script must verify the corresponding MySQL 8.4.9
observable behavior, including upstream session mutability, `SHOW VARIABLES`,
and Boolean assignment forms.

## Compatibility Documentation

Update:

- `COMPATIBILITY.md`
- `docs/compatibility/runtime-system-variables.md`
- `docs/compatibility/metadata-mysql-schema.md`

Do not claim server-global mutation, persisted state, general query log files,
log-row writes to `mysql.general_log`, slow query logging, privileges, or
Performance Schema variable tables.

## Verification

Before committing implementation, run:

```sh
packages/libmylite/tests/mysql_baseline_sql_log_off_system_variable_expectations.sh
ctest --preset dev -R 'libmylite\.runtime\.sql_log_off_system_variable$' --output-on-failure
cmake --workflow --preset check
```

Also run the existing focused parser and runtime lifecycle/system-variable
CTest entries affected by the scalar-select path.
