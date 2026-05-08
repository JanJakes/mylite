# Baseline SQL Replica Skip Counter System Variable

## Status

This feature specifies a narrow scalar system-variable slice for
`@@sql_replica_skip_counter`.

It builds on the existing `SYSTEM_VARIABLE` lexer/parser token, scalar
`SELECT` execution, diagnostics lifecycle, and statement context. MySQL exposes
`sql_replica_skip_counter` as a mutable global replication counter that controls
how many source events a future replica start should skip. MyLite does not
implement replication channels, relay logs, `START REPLICA`, GTID state, or
mutable system-variable assignment in the baseline, so this slice exposes only
the fixed default scalar value.

This is not replication support. It does not implement
`SET GLOBAL sql_replica_skip_counter`, replica SQL thread state, event
skipping, channel rules, GTID restrictions, startup options, the deprecated
`sql_slave_skip_counter` alias, or privilege semantics.

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
`packages/libmylite/tests/mysql_baseline_sql_replica_skip_counter_system_variable_expectations.sh`
records the runtime probes for this feature. The primary probes were run
against container `mylite-mysql-849` with:

```sh
docker exec -i mylite-mysql-849 mysql \
  --protocol=TCP -h127.0.0.1 -uroot \
  --batch --raw --skip-column-names --default-character-set=utf8mb4
```

Observed behavior:

- `SELECT VERSION()` returned `8.4.9`.
- `SELECT @@sql_replica_skip_counter`,
  `@@global.sql_replica_skip_counter`, `@@SQL_REPLICA_SKIP_COUNTER`, and
  quoted final-name forms return `0` in the tested default runtime.
- `@@session.sql_replica_skip_counter` and
  `@@local.sql_replica_skip_counter` fail with error `1238`, SQLSTATE `HY000`,
  and a message that the variable is global-only.
- `SET GLOBAL sql_replica_skip_counter=1` succeeds upstream and changes
  unscoped and global reads to `1`; `SET GLOBAL sql_replica_skip_counter=0`
  restores the default value. MyLite does not implement this mutable state.
- Variable and scope names are case-insensitive.
- Backtick-quoted final variable-name components are accepted.
- Backtick-quoted scope names, such as
  ``@@`session`.sql_replica_skip_counter``, are syntax errors.
- Unknown variables fail with error `1193`, SQLSTATE `HY000`, and an
  `Unknown system variable` message.
- A scalar `SELECT` that reads this variable is nondiagnostic. It reads the
  previous diagnostics snapshot for any `@@warning_count` or `@@error_count`
  items in the same select list, then clears diagnostics for following
  diagnostic statements.
- `@@sql_slave_skip_counter` is accepted by MySQL as a deprecated alias for
  `@@sql_replica_skip_counter` and emits deprecation warning `1287`. MyLite
  intentionally defers that alias and its warning semantics.
- MySQL accepts wider expression forms such as
  `SELECT @@sql_replica_skip_counter + 1`. Those forms remain outside this
  MyLite slice.

The official MySQL replication-variable documentation classifies
`sql_replica_skip_counter` as a dynamic global integer with default `0`, minimum
`0`, and maximum `4294967295`. It specifies the number of source events a
replica should skip on a later `START REPLICA`, resets to `0` after that start,
has channel restrictions when nonzero, and is incompatible with GTID-based
replication except for documented anonymous-transaction cases. MyLite returns
the fixed default value `0`, so this slice does not change replication,
catalog, storage, or descriptor behavior.

## Scope

The implementation must add:

- runtime recognition of `sql_replica_skip_counter` inside the existing scalar
  `SELECT` subset;
- support for no scope and `global` scope qualifiers;
- global-only diagnostics for `session` and `local` scope qualifiers;
- case-insensitive matching for unquoted scope and variable names;
- backtick-quoted final variable-name components;
- one-row scalar result sets with existing source-span column labels;
- fixed value `0` for supported scopes;
- MySQL-compatible unknown-variable diagnostics for unsupported names;
- deterministic rejection of quoted scopes;
- fast C tests and a MySQL 8.4.9 expectation artifact.

Supported SQL examples:

```sql
SELECT @@sql_replica_skip_counter
SELECT @@sql_replica_skip_counter FROM DUAL
SELECT @@global.sql_replica_skip_counter
SELECT @@global.`sql_replica_skip_counter`, @@`sql_replica_skip_counter`
SELECT @@sql_replica_skip_counter, @@warning_count, ROW_COUNT()
```

## Non-Goals

This feature must not implement:

- `SET`, startup options, persisted variables, `SET_VAR` hints, or mutable
  global `sql_replica_skip_counter` state;
- `START REPLICA`, `STOP REPLICA`, replication channels, relay logs, binary
  logs, event skipping, GTID checks, anonymous-transaction assignment, source
  metadata, applier workers, or replication status;
- the deprecated `sql_slave_skip_counter` alias or deprecation warnings;
- privilege checks for restricted global variables;
- variables other than `sql_replica_skip_counter`;
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
  preserve the previous diagnostics snapshot until nondiagnostic successful
  completion replaces it.
- Lexer/parser/AST own syntax admission and source spans for
  `SYSTEM_VARIABLE` expressions. No new grammar is needed beyond the existing
  `expression ::= SYSTEM_VARIABLE` rule.
- Runtime execution owns system-variable path parsing, scope validation, fixed
  value selection, and diagnostics for unsupported names.
- Descriptor-driven statement execution remains unchanged because this scalar
  variable does not influence MyLite DDL, DML, storage, planning, conversion,
  row visibility, or replication behavior in this slice.
- The catalog remains authoritative for descriptors. This variable slice does
  not create replication metadata or affect table lifecycle.
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
@@sql_replica_skip_counter
@@global.sql_replica_skip_counter
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

For this slice, supported scopes return the same fixed value. This is a
deliberate MyLite limitation: no mutable `sql_replica_skip_counter` state,
replica thread state, or event skipping exists yet.

## Runtime Semantics

The supported variable returns:

| Variable | Value |
| --- | --- |
| `sql_replica_skip_counter` | `0` |

The value is independent of selected schema, close/reopen, table DDL, DML,
replication state, and independent handles. It is a compatibility scalar only.
Existing DDL and DML behavior must not change.

Successful scalar reads:

- return one row and one text column for each selected expression;
- use the original source expression as the column label unless the general
  scalar-select path later adds alias support;
- leave `warning_count == 0` for supported forms;
- do not mutate catalog rows, descriptor versions, descriptor caches, catalog
  generation, physical SQLite schema, or `.mylite` preamble bytes;
- follow existing scalar `SELECT` row-count behavior, so `ROW_COUNT()` after a
  successful scalar row result is `-1`.

Unsupported expressions such as `@@sql_replica_skip_counter + 1` remain rejected
by the current scalar expression rules even though MySQL accepts them.

## Diagnostics

The implementation must preserve existing diagnostics conventions:

- unsupported variable names: MySQL error `1193`, SQLSTATE `HY000`, message
  containing `Unknown system variable '<name>'`;
- unsupported `session` and `local` scopes for this variable: MySQL error
  `1238`, SQLSTATE `HY000`, message containing
  `Variable 'sql_replica_skip_counter' is a GLOBAL variable`;
- quoted scope components: deterministic parse error `1064`, SQLSTATE
  `42000`, using MyLite's unsupported quoted-scope message;
- malformed variable paths: deterministic unknown-system-variable or parse
  diagnostics from the existing resolver;
- unsupported scalar expression forms, aliases, clauses, table-backed `FROM`,
  or `SET`: deterministic parse/unsupported diagnostics from the existing
  parser and scalar executor;
- allocation failures: existing `MYLITE_NOMEM` or diagnostic path;
- public API misuse: unchanged `mylite_execute()` validation behavior.

Successful supported reads must report `warning_count == 0`.

## Storage, Catalog, and SQLite

No SQLite SQL is generated for this variable. The implementation is a pure
runtime scalar resolver addition.

It must not:

- query SQLite metadata for variable values;
- create, modify, or delete SQLite schema objects;
- modify MyLite catalog rows or descriptor generations;
- change selected schema, transaction state, or storage handles;
- mutate `.mylite` preamble bytes;
- add SQLite fork patches.

## Test Plan

Add a focused C test, preferably
`packages/libmylite/tests/runtime_sql_replica_skip_counter_system_variable_test.c`,
registered as `libmylite.runtime.sql_replica_skip_counter_system_variable`.

The tests must cover:

- values for no-scope and `global` reads;
- labels, case-insensitive names, quoted final names, parenthesized variables,
  and `FROM DUAL`;
- deterministic global-only diagnostics for `session` and `local` scopes;
- unknown unscoped and scoped names;
- quoted-scope rejection;
- rejected `SET GLOBAL sql_replica_skip_counter`;
- rejected deprecated `@@sql_slave_skip_counter` alias for this MyLite slice;
- rejected general expressions such as `@@sql_replica_skip_counter + 1`;
- warning and error diagnostics snapshot behavior;
- independence from selected schema, create/insert/select/delete lifecycle,
  close/reopen, table rename/drop, catalog generations, SQLite schema
  generation, and file preamble bytes;
- independent file-backed handles.

The MySQL expectation artifact must verify:

- MySQL 8.4.9 version;
- default value, global-only scope behavior, labels, quoted final names, and
  case-insensitive names;
- upstream mutable global state and reset to `0`;
- diagnostics for `session`, `local`, unknown variables, and quoted scope;
- deprecated `sql_slave_skip_counter` alias warning as explicitly deferred;
- MySQL acceptance of expression forms that MyLite still rejects.

## Compatibility Documentation

Update:

- `COMPATIBILITY.md`;
- `docs/compatibility/runtime-system-variables.md`;
- `docs/compatibility/sql-replication.md`.

Do not claim support for mutable global state, `SET`, replication event
skipping, `START REPLICA`, channels, GTID checks, the deprecated alias,
privileges, `SHOW VARIABLES`, Performance Schema variable tables, or any
descriptor-backed statement behavior change.
