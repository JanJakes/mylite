# Baseline SQL Slave Skip Counter System Variable

## Status

This feature specifies a narrow scalar system-variable slice for
`@@sql_slave_skip_counter`, the deprecated MySQL compatibility alias for
`@@sql_replica_skip_counter`.

It builds on the existing `SYSTEM_VARIABLE` lexer/parser token, scalar
`SELECT` execution, diagnostics lifecycle, warning storage, and the fixed
global-only `@@sql_replica_skip_counter` baseline. MySQL exposes the alias as a
deprecated global dynamic integer variable that reads and writes the same
counter as `sql_replica_skip_counter`. MyLite does not implement replication
channels, relay logs, `START REPLICA`, GTID state, or mutable replica state in
the baseline, so this slice exposes scalar reads of the alias, the required
deprecation warnings, and exact/default global assignments that preserve the
fixed `0` counter.

This is not replication support. It does not implement nonzero `SET GLOBAL
sql_slave_skip_counter`, mutable counter state, replica SQL thread state, event
skipping, channel rules, GTID restrictions, startup options, or privilege
semantics.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- SQL replica skip counter baseline:
  `docs/specs/baseline-sql-replica-skip-counter-system-variable/specs.md`
- MySQL lexer: `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold: `docs/specs/mysql-parser-scaffold/specs.md`
- MySQL 8.4 Reference Manual, server system variables:
  https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html
- MySQL 8.4 Reference Manual, dynamic system variables:
  https://dev.mysql.com/doc/refman/8.4/en/dynamic-system-variables.html
- MySQL 8.4 Reference Manual, replica server options and variables:
  https://dev.mysql.com/doc/refman/8.4/en/replication-options-replica.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_sql_slave_skip_counter_system_variable_expectations.sh`
records the runtime probes for this feature. The primary probes were run
against container `mylite-mysql-849` with:

```sh
docker exec -i mylite-mysql-849 mysql \
  --protocol=TCP -h127.0.0.1 -uroot \
  --batch --raw --skip-column-names --default-character-set=utf8mb4
```

Observed behavior:

- `SELECT VERSION()` returned `8.4.9`.
- `SELECT @@sql_slave_skip_counter`, `@@global.sql_slave_skip_counter`,
  `@@SQL_SLAVE_SKIP_COUNTER`, and quoted final-name forms return the same value
  as `@@sql_replica_skip_counter`, `0` in the tested default runtime.
- Every successful scalar reference to the alias records warning `1287`,
  SQLSTATE `HY000`, with the message that `@@sql_slave_skip_counter` is
  deprecated and `sql_replica_skip_counter` should be used instead.
- A scalar `SELECT` with multiple alias references records one deprecation
  warning per alias reference.
- If a scalar `SELECT` contains alias references and `@@warning_count` in the
  same select list, `@@warning_count` reports the current statement's alias
  deprecation warning count, independent of select-list order.
- In the same successful warning-producing `SELECT`, `@@error_count` reports
  `0` and `ROW_COUNT()` reports `-1`.
- `@@session.sql_slave_skip_counter` and `@@local.sql_slave_skip_counter` fail
  with error `1238`, SQLSTATE `HY000`, and a message that the variable is
  global-only. These error paths do not add the alias deprecation warning.
- If a scalar `SELECT` reads a valid `@@sql_slave_skip_counter` alias before an
  invalid scoped alias, the diagnostics area contains the deprecation warning
  followed by the global-only error. If the invalid scoped alias appears first,
  only the global-only error is stored.
- `SET GLOBAL sql_slave_skip_counter=0`,
  `SET GLOBAL sql_slave_skip_counter=DEFAULT`, and
  `SET @@global.sql_slave_skip_counter=0` succeed upstream, record the same
  deprecation warning, and leave `@@sql_replica_skip_counter` at `0`.
- `SET GLOBAL sql_slave_skip_counter=1` succeeds upstream, records the same
  deprecation warning, and changes `@@sql_replica_skip_counter` to `1`;
  `SET GLOBAL sql_replica_skip_counter=0` restores the default value. MyLite
  does not implement this mutable nonzero state.
- Unscoped, session, local, and `@@session` assignment forms record the alias
  deprecation warning and then fail with error `1229`, SQLSTATE `HY000`, and a
  message that the variable is global and should be set with `SET GLOBAL`.
- Variable and scope names are case-insensitive.
- Backtick-quoted final variable-name components are accepted.
- Backtick-quoted scope names, such as
  ``@@`session`.sql_slave_skip_counter``, are syntax errors.
- Unknown variables fail with error `1193`, SQLSTATE `HY000`, and an
  `Unknown system variable` message.
- MySQL accepts wider expression forms such as
  `SELECT @@sql_slave_skip_counter + 1` and records the deprecation warning.
  Those expression forms remain outside this MyLite slice.

The official MySQL replication-variable documentation classifies
`sql_slave_skip_counter` as a deprecated alias for `sql_replica_skip_counter`.
The target variable is a dynamic global integer with default `0`, minimum `0`,
and maximum `4294967295`. MyLite returns the fixed default value `0`, so this
slice does not change replication, catalog, storage, or descriptor behavior.

## Scope

The implementation must add:

- runtime recognition of `sql_slave_skip_counter` inside the existing scalar
  `SELECT` subset;
- support for no scope and `global` scope qualifiers;
- global-only diagnostics for `session` and `local` scope qualifiers;
- case-insensitive matching for unquoted scope and variable names;
- backtick-quoted final variable-name components;
- one-row scalar result sets with existing source-span column labels;
- fixed value `0` for supported scopes;
- one deprecation warning per successful alias reference;
- same-statement `@@warning_count` behavior for alias warnings;
- accepted no-op assignment for `SET GLOBAL sql_slave_skip_counter=0`,
  `SET GLOBAL sql_slave_skip_counter=DEFAULT`, and
  `SET @@global.sql_slave_skip_counter=0`, each with deprecation warning
  `1287`;
- alias deprecation warning before the global-only error for non-global `SET`;
- MySQL-compatible unknown-variable diagnostics for unsupported names;
- deterministic rejection of quoted scopes;
- fast C tests and a MySQL 8.4.9 expectation artifact.

Supported SQL examples:

```sql
SELECT @@sql_slave_skip_counter
SELECT @@sql_slave_skip_counter FROM DUAL
SELECT @@global.sql_slave_skip_counter
SELECT @@global.`sql_slave_skip_counter`, @@`sql_slave_skip_counter`
SELECT @@sql_slave_skip_counter, @@warning_count, ROW_COUNT()
SET GLOBAL sql_slave_skip_counter = 0
SET GLOBAL sql_slave_skip_counter = DEFAULT
SET @@global.sql_slave_skip_counter = 0
```

## Non-Goals

This feature must not implement:

- nonzero `SET`, startup options, persisted variables, `SET_VAR` hints, or
  mutable global `sql_slave_skip_counter` state;
- `START REPLICA`, `STOP REPLICA`, replication channels, relay logs, binary
  logs, event skipping, GTID checks, anonymous-transaction assignment, source
  metadata, applier workers, or replication status;
- warning-producing mutable `SET GLOBAL sql_slave_skip_counter` behavior;
- privilege checks for restricted global variables;
- variables other than `sql_slave_skip_counter`;
- changed descriptor-backed DDL, DML, or `SELECT` execution;
- `SHOW VARIABLES` or Performance Schema variable tables;
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
  preserve the previous diagnostics snapshot until successful completion
  replaces it. For this warning-producing scalar slice, the successful
  statement snapshot contains the alias deprecation warnings.
- Lexer/parser/AST own syntax admission and source spans for
  `SYSTEM_VARIABLE` expressions. No new grammar is needed beyond the existing
  `expression ::= SYSTEM_VARIABLE` rule.
- Runtime execution owns system-variable path parsing, scope validation, fixed
  value selection, deprecation warning emission, and diagnostics for
  unsupported names.
- Descriptor-driven statement execution remains unchanged because this scalar
  variable does not influence MyLite DDL, DML, storage, planning, conversion,
  row visibility, or replication behavior in this slice.
- The catalog remains authoritative for descriptors. This variable slice does
  not create replication metadata or affect table lifecycle.
- Result builder owns scalar result column labels, one-row text values, and the
  public warning count on the result object.
- Storage, VFS, and SQLite physical row storage are not involved. This feature
  must not touch `.mylite` preamble bytes or SQLite schema state.

## Supported SQL Grammar

This slice uses the existing system-variable expression atom:

```lemon
expression ::= SYSTEM_VARIABLE.
```

The supported runtime variable paths are:

```sql
@@sql_slave_skip_counter
@@global.sql_slave_skip_counter
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

- it accepts no scope or `global`;
- it treats unquoted names ASCII case-insensitively;
- it accepts a backtick-quoted final variable-name component and unescapes
  doubled backticks before comparison;
- it rejects backtick-quoted scope names with a deterministic syntax
  diagnostic;
- it rejects `session` and `local` scopes with MySQL error `1238`, SQLSTATE
  `HY000`, and a global-only message;
- it rejects malformed paths and unsupported variables with MySQL error `1193`,
  SQLSTATE `HY000`;
- it preserves the original source text as the scalar result column label.

For this slice, supported scopes return the same fixed value as
`@@sql_replica_skip_counter`. Accepted assignment forms are limited to
exact/default global no-ops that preserve `0` and emit the alias warning. This
is a deliberate MyLite limitation: no mutable counter state, replica thread
state, or event skipping exists yet.

## Runtime Semantics

The supported alias returns:

| Variable | Value |
| --- | --- |
| `sql_slave_skip_counter` | `0` |

Every successful alias reference appends this warning:

| Level | Code | SQLSTATE | Message |
| --- | --- | --- | --- |
| `Warning` | `1287` | `HY000` | `'@@sql_slave_skip_counter' is deprecated and will be removed in a future release. Please use sql_replica_skip_counter instead.` |

Successful scalar reads:

- return one row and one text column for each selected expression;
- use the original source expression as the column label unless the general
  scalar-select path later adds alias support;
- set the public result warning count to the number of alias deprecation
  warnings produced by that statement;
- snapshot the deprecation warnings so `SHOW COUNT(*) WARNINGS` and
  `SHOW WARNINGS` expose them after the statement;
- when the same select list contains `@@warning_count`, return the current
  statement warning count if alias warnings are produced, matching MySQL's
  deprecation-warning behavior;
- when the same select list contains `@@error_count`, return `0` for a
  successful alias statement;
- do not mutate catalog rows, descriptor versions, descriptor caches, catalog
  generation, physical SQLite schema, or `.mylite` preamble bytes;
- follow existing scalar `SELECT` row-count behavior, so `ROW_COUNT()` after a
  successful scalar row result is `-1`.

Unsupported expressions such as `@@sql_slave_skip_counter + 1` remain rejected
by the current scalar expression rules even though MySQL accepts them.

## Diagnostics

The implementation must preserve existing diagnostics conventions:

- successful alias references: warning `1287`, SQLSTATE `HY000`, with the
  deprecation message above;
- unsupported variable names: MySQL error `1193`, SQLSTATE `HY000`, message
  containing `Unknown system variable '<name>'`;
- unsupported `session` and `local` scopes for this variable: MySQL error
  `1238`, SQLSTATE `HY000`, message containing
  `Variable 'sql_slave_skip_counter' is a GLOBAL variable`;
- unsupported non-global `SET` forms: warning `1287` followed by MySQL error
  `1229`, SQLSTATE `HY000`, message containing
  `Variable 'sql_slave_skip_counter' is a GLOBAL variable and should be set
  with SET GLOBAL`;
- quoted scope components: deterministic parse error `1064`, SQLSTATE
  `42000`, using MyLite's unsupported quoted-scope message;
- malformed variable paths: deterministic unknown-system-variable or parse
  diagnostics from the existing resolver;
- unsupported scalar expression forms, aliases, clauses, table-backed `FROM`,
  non-global `SET`, or nonzero `SET`: deterministic parse/unsupported
  diagnostics from the existing parser and scalar executor;
- allocation failures while recording warnings: existing `MYLITE_NOMEM` or
  diagnostic path;
- public API misuse: unchanged `mylite_execute()` validation behavior.

Error paths for invalid scalar alias scopes must not also append the alias
deprecation warning. Assignment paths append the alias warning before
assignment-specific errors, matching observed MySQL behavior.

## Storage, Catalog, and SQLite

No SQLite SQL is generated for this variable. The implementation is a pure
runtime scalar resolver and diagnostics addition.

It must not:

- query SQLite metadata for variable values;
- create, modify, or delete SQLite schema objects;
- modify MyLite catalog rows or descriptor generations;
- change selected schema, transaction state, or storage handles;
- mutate `.mylite` preamble bytes;
- add SQLite fork patches.

## Test Plan

Add a focused C test, preferably
`packages/libmylite/tests/runtime_sql_slave_skip_counter_system_variable_test.c`,
registered as `libmylite.runtime.sql_slave_skip_counter_system_variable`.

The tests must cover:

- values for no-scope and `global` reads;
- labels, case-insensitive names, quoted final names, parenthesized variables,
  and `FROM DUAL`;
- one warning per successful alias reference, including multiple references in
  one select list;
- same-statement `@@warning_count` and `@@error_count` behavior for alias
  warnings, including select-list order independence;
- `SHOW COUNT(*) WARNINGS` and `SHOW WARNINGS` after alias reads;
- deterministic global-only diagnostics for `session` and `local` scopes
  without deprecation warnings;
- unknown unscoped and scoped names;
- quoted-scope rejection;
- accepted `SET GLOBAL sql_slave_skip_counter = 0`,
  `SET GLOBAL sql_slave_skip_counter = DEFAULT`, and
  `SET @@global.sql_slave_skip_counter = 0`, including deprecation warnings;
- rejected non-global and nonzero `SET` forms;
- rejected general expressions such as `@@sql_slave_skip_counter + 1`;
- warning and error diagnostics snapshot behavior;
- independence from selected schema, create/insert/update/select/delete,
  rename/drop lifecycle, close/reopen, catalog generations, SQLite schema
  generation, and file preamble bytes;
- independent file-backed handles.

The MySQL expectation artifact must verify:

- MySQL 8.4.9 version;
- default value, global-only scope behavior, labels, quoted final names, and
  case-insensitive names;
- one warning per alias reference and exact deprecation warning text;
- same-statement `@@warning_count` behavior for alias warnings;
- accepted upstream global `0`, `DEFAULT`, and `@@global` assignments with
  deprecation warnings;
- upstream nonzero `SET GLOBAL sql_slave_skip_counter` mutability and warning
  behavior as outside the MyLite slice;
- diagnostics for `session`, `local`, unknown variables, and quoted scope;
- MySQL acceptance of expression forms that MyLite still rejects.

## Compatibility Documentation

Update:

- `COMPATIBILITY.md`;
- `docs/compatibility/runtime-system-variables.md`;
- `docs/compatibility/sql-replication.md`.

Do not claim support for mutable global state, nonzero `SET`, replication event
skipping, `START REPLICA`, channels, GTID checks, privileges, `SHOW VARIABLES`,
Performance Schema variable tables, or any descriptor-backed statement behavior
change.
