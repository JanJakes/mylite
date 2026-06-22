# Baseline SQL Log Bin System Variable

## Status

This feature specifies the baseline session-variable slice for
`@@sql_log_bin`.

It builds on the existing `SYSTEM_VARIABLE` lexer/parser token, scalar
`SELECT` execution, diagnostics lifecycle, generic Boolean session-variable
assignment, `SHOW VARIABLES`, and descriptor-driven table paths. MySQL exposes
`sql_log_bin` as session-only state that controls whether the current session
writes statements to the binary log when binary logging is enabled. MyLite
implements the embedded compatibility baseline: session `SET`/readback and
`SHOW VARIABLES` state, no global scope, and no changed query or DDL/DML
execution because MyLite does not implement binary logs.

This is not binary logging support. It does not implement binary log files,
replication side effects, GTID handling, privilege checks, or logging changes
for DDL/DML.

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
- MySQL 8.4 Reference Manual, binary logging options and variables:
  https://dev.mysql.com/doc/refman/8.4/en/replication-options-binary-log.html
- MySQL 8.4 Reference Manual, `SET sql_log_bin` statement:
  https://dev.mysql.com/doc/refman/8.4/en/set-sql-log-bin.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_sql_log_bin_system_variable_expectations.sh`
records the runtime probes for this feature. The primary probes were run
against container `mylite-mysql-849` with:

```sh
docker exec -i mylite-mysql-849 mysql \
  --protocol=TCP -h127.0.0.1 -uroot \
  --batch --raw --skip-column-names --default-character-set=utf8mb4
```

Observed behavior:

- `SELECT VERSION()` returned `8.4.9`.
- `SELECT @@sql_log_bin`, `@@session.sql_log_bin`,
  `@@local.sql_log_bin`, and `@@SQL_LOG_BIN` return `1` in the tested
  default runtime.
- `SELECT @@global.sql_log_bin` fails with error `1238`, SQLSTATE `HY000`,
  and message `Variable 'sql_log_bin' is a SESSION variable`.
- The variable is session-scoped. After `SET SESSION sql_log_bin=0`,
  unscoped, `session`, and `local` reads return `0`; after
  `SET SESSION sql_log_bin=1`, they return `1` again.
- `SHOW VARIABLES LIKE 'sql_log_bin'` reflects the session value; `SHOW GLOBAL
  VARIABLES LIKE 'sql_log_bin'` returns no rows.
- Variable and scope names are case-insensitive.
- Backtick-quoted final variable-name components are accepted.
- Backtick-quoted scope names, such as ``@@`session`.sql_log_bin``, are
  syntax errors.
- Unknown variables fail with error `1193`, SQLSTATE `HY000`, and an
  `Unknown system variable` message.
- A scalar `SELECT` that reads this variable is nondiagnostic. It reads the
  previous diagnostics snapshot for any `@@warning_count` or `@@error_count`
  items in the same select list, then clears diagnostics for following
  diagnostic statements.
- MySQL accepts wider expression forms such as `SELECT @@sql_log_bin + 1`.
  Those forms remain outside this MyLite slice.

The official MySQL 8.4 documentation classifies `sql_log_bin` as a dynamic
Boolean system variable with session scope and default value `ON`. It controls
binary logging for the current session when the server binary log itself is
enabled. MyLite records the session value but does not create binary logs or
alter statement execution.

## Scope

The implementation must add:

- runtime recognition of `sql_log_bin` inside the existing scalar `SELECT`
  subset;
- support for no scope, `session`, and `local` scope qualifiers;
- deterministic rejection of `global` scope with MySQL's session-only
  diagnostic for this variable;
- session `SET sql_log_bin = 0|1|ON|OFF|TRUE|FALSE|DEFAULT` handling through
  the existing Boolean system-variable override path;
- `SHOW VARIABLES` values for the session baseline and `SHOW GLOBAL VARIABLES`
  omission for the session-only global surface;
- case-insensitive matching for unquoted scope and variable names;
- backtick-quoted final variable-name components;
- one-row scalar result sets with existing source-span column labels;
- session values that default to `1`;
- MySQL-compatible unknown-variable diagnostics for unsupported names;
- deterministic rejection of quoted scopes;
- fast C tests and a MySQL 8.4.9 expectation artifact.

Supported SQL examples:

```sql
SELECT @@sql_log_bin
SELECT @@sql_log_bin FROM DUAL
SELECT @@session.sql_log_bin, @@local.sql_log_bin
SELECT @@session.`sql_log_bin`, @@`sql_log_bin`
SELECT @@sql_log_bin, @@warning_count, ROW_COUNT()
SET SESSION sql_log_bin = 0
SHOW VARIABLES LIKE 'sql_log_bin'
```

The following form is intentionally parsed as a system-variable path but must
fail at runtime:

```sql
SELECT @@global.sql_log_bin
```

## Non-Goals

This feature must not implement:

- startup options, persisted variables, `SET_VAR` hints, or server-global
  `sql_log_bin` state;
- binary log files, binary log event generation, GTID behavior, replication
  source or replica semantics, `mysqldump` side effects, or privilege checks;
- variables other than `sql_log_bin`;
- changed descriptor-backed DDL or DML execution;
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
  session value selection, and diagnostics for unsupported names or scopes.
- Descriptor-driven statement execution remains unchanged because this scalar
  variable does not influence MyLite storage, planning, or row visibility in
  this slice.
- The catalog remains authoritative for descriptors. This variable slice does
  not create logging metadata or affect table lifecycle.
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
@@sql_log_bin
@@session.sql_log_bin
@@local.sql_log_bin
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

- it accepts no scope, `session`, or `local`;
- it rejects `global` scope for `sql_log_bin` with MySQL error `1238`,
  SQLSTATE `HY000`, and message `Variable 'sql_log_bin' is a SESSION variable`;
- it treats unquoted names ASCII case-insensitively;
- it accepts a backtick-quoted final variable-name component and unescapes
  doubled backticks before comparison;
- it rejects backtick-quoted scope names with a deterministic syntax
  diagnostic;
- it rejects malformed paths and unsupported variables with MySQL error `1193`,
  SQLSTATE `HY000`;
- it preserves the original source text as the scalar result column label.

For this slice, unscoped, `session`, and `local` scope read the current session
override when one exists, and otherwise return `1`.

## Runtime Semantics

The supported variable returns:

| Scope | Value |
| --- | --- |
| Session/local/unscoped default | `1` |
| Session/local/unscoped after `SET SESSION sql_log_bin = 0` | `0` |

The session value is independent per handle and resets on close/reopen.
Existing descriptor-backed DDL and DML behavior must not change, and MyLite
must not write or suppress binary log records in this slice.

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
  expression forms: MySQL error `1064`, SQLSTATE `42000`;
- unsupported variable names: MySQL error `1193`, SQLSTATE `HY000`;
- unsupported `global` scope for `sql_log_bin`: MySQL error `1238`, SQLSTATE
  `HY000`;
- allocation failures: MyLite runtime failure diagnostics;
- public API misuse: existing public execution/result misuse behavior.

Successful supported reads and session assignments produce no warnings.

## Tests

Add a focused C runtime test under `packages/libmylite/tests/`, registered as
`libmylite.runtime.sql_log_bin_system_variable`.

The C tests must cover:

- default value `1`, mutable session values, and close/reopen reset behavior;
- rejection of `global` scope with the session-only diagnostic;
- session and global `SHOW VARIABLES` behavior;
- source-text labels, case-insensitive names, quoted final variable names,
  `FROM DUAL`, selected-schema independence, and mixed scalar reads with other
  baseline variables;
- warning and error diagnostics snapshot behavior;
- unknown unscoped and scoped variable diagnostics;
- quoted-scope rejection and unsupported expression rejection;
- close/reopen reset, independent handles, unchanged catalog and SQLite
  generations, and `.mylite` preamble preservation;
- descriptor-backed DDL/DML independence, including that simple table changes
  still work and are not logged or blocked by this scalar variable.

The MySQL expectation script must verify the corresponding MySQL 8.4.9
observable behavior, including upstream session mutability, `SHOW VARIABLES`,
Boolean assignment forms, and the `@@global.sql_log_bin` session-only error.

## Compatibility Documentation

Update:

- `COMPATIBILITY.md`
- `docs/compatibility/runtime-system-variables.md`
- `docs/compatibility/sql-replication.md`

Do not update query expression, DDL, or DML compatibility claims except to keep
existing "no logging side effects" wording accurate if needed. Do not claim
binary log writes, GTID behavior, replication semantics, Performance Schema
variable tables, or global scope support.

## Verification

Before committing implementation, run:

```sh
packages/libmylite/tests/mysql_baseline_sql_log_bin_system_variable_expectations.sh
ctest --preset dev -R 'libmylite\.runtime\.sql_log_bin_system_variable$' --output-on-failure
cmake --workflow --preset check
```

Also run the existing focused parser and runtime lifecycle/system-variable
CTest entries affected by the scalar-select path.
