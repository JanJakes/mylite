# Baseline SQL Require Primary Key System Variable

## Status

This feature specifies a narrow scalar system-variable slice for
`@@sql_require_primary_key`.

This document describes the original scalar-only baseline. The later
`docs/specs/baseline-sql-require-primary-key-ddl/specs.md` slice extends this
surface with handle-local session assignment and limited DDL enforcement.

It builds on the existing `SYSTEM_VARIABLE` lexer/parser token, scalar
`SELECT` execution, diagnostics lifecycle, and descriptor-driven statement
paths. MySQL exposes `sql_require_primary_key` as mutable global and session
state that can require primary keys for table creation and table-structure
changes. MyLite did not implement mutable system-variable assignment,
primary-key requirement enforcement, `ALTER TABLE` structure changes,
temporary tables, or replication behavior in this scalar baseline, so this
slice exposed only the fixed default disabled scalar value.

This original slice was not primary-key enforcement support. It did not
implement `SET sql_require_primary_key`, mutable global/session state, primary
key DDL, table-change enforcement, replication applier policy, or privilege
semantics.

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
- MySQL 8.4 Reference Manual, system variable privileges:
  https://dev.mysql.com/doc/refman/8.4/en/system-variable-privileges.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_sql_require_primary_key_system_variable_expectations.sh`
records the runtime probes for this feature. The primary probes were run
against container `mylite-mysql-849` with:

```sh
docker exec -i mylite-mysql-849 mysql \
  --protocol=TCP -h127.0.0.1 -uroot \
  --batch --raw --skip-column-names --default-character-set=utf8mb4
```

Observed behavior:

- `SELECT VERSION()` returned `8.4.9`.
- `SELECT @@sql_require_primary_key`,
  `@@global.sql_require_primary_key`,
  `@@session.sql_require_primary_key`, `@@local.sql_require_primary_key`, and
  `@@SQL_REQUIRE_PRIMARY_KEY` return `0` in the tested default runtime.
- The variable has global and session scope. After
  `SET SESSION sql_require_primary_key=1`, unscoped, `session`, and `local`
  reads return `1`, while `global` still returns `0`; assigning `DEFAULT`
  restores the default session value.
- Variable and scope names are case-insensitive.
- Backtick-quoted final variable-name components are accepted.
- Backtick-quoted scope names, such as
  ``@@`session`.sql_require_primary_key``, are syntax errors.
- Unknown variables fail with error `1193`, SQLSTATE `HY000`, and an
  `Unknown system variable` message.
- When MySQL's session value is enabled, `CREATE TABLE no_pk (id INT)` fails
  with error `3750`, SQLSTATE `HY000`, and a message that a table cannot be
  created or changed without a primary key while the variable is set.
- A scalar `SELECT` that reads this variable is nondiagnostic. It reads the
  previous diagnostics snapshot for any `@@warning_count` or `@@error_count`
  items in the same select list, then clears diagnostics for following
  diagnostic statements.
- MySQL accepts wider expression forms such as
  `SELECT @@sql_require_primary_key + 1`. Those forms remain outside this
  MyLite slice.

The official MySQL system-variable documentation classifies
`sql_require_primary_key` as a dynamic boolean variable with global and session
scope, `SET_VAR` applicability, and default value `OFF`. It also documents
that the enabled value affects table creation, table-structure changes, import
operations, temporary tables, base tables, and replication primary-key policy.
MyLite returns the fixed default disabled value `0`, so this slice does not
change `CREATE TABLE`, `ALTER TABLE`, replication, or descriptor behavior.

## Scope

The implementation must add:

- runtime recognition of `sql_require_primary_key` inside the existing scalar
  `SELECT` subset;
- support for no scope, `session`, `local`, and `global` scope qualifiers;
- case-insensitive matching for unquoted scope and variable names;
- backtick-quoted final variable-name components;
- one-row scalar result sets with existing source-span column labels;
- fixed value `0` for all supported scopes;
- MySQL-compatible unknown-variable diagnostics for unsupported names;
- deterministic rejection of quoted scopes;
- fast C tests and a MySQL 8.4.9 expectation artifact.

Supported SQL examples:

```sql
SELECT @@sql_require_primary_key
SELECT @@sql_require_primary_key FROM DUAL
SELECT @@session.sql_require_primary_key, @@local.sql_require_primary_key
SELECT @@global.sql_require_primary_key
SELECT @@session.`sql_require_primary_key`, @@`sql_require_primary_key`
SELECT @@sql_require_primary_key, @@warning_count, ROW_COUNT()
```

## Non-Goals

This feature must not implement:

- `SET`, startup options, persisted variables, `SET_VAR` hints, or mutable
  global/session `sql_require_primary_key` state;
- primary key constraints, unique indexes, generated invisible primary keys,
  `CREATE TABLE ... LIKE`, `CREATE TABLE ... SELECT`, temporary tables,
  `ALTER TABLE` structure changes, table import, or replication applier
  policy;
- privilege checks for restricted session variables;
- variables other than `sql_require_primary_key`;
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
  or row visibility in this slice.
- The catalog remains authoritative for descriptors. This variable slice does
  not create constraint metadata or affect table lifecycle.
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
@@sql_require_primary_key
@@session.sql_require_primary_key
@@local.sql_require_primary_key
@@global.sql_require_primary_key
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

For this original slice, all scopes returned the same fixed value. The later
DDL-enforcement slice adds handle-local session state while keeping global
state fixed.

## Runtime Semantics

The supported variable returns:

| Variable | Value |
| --- | --- |
| `sql_require_primary_key` | `0` |

The value is independent of selected schema, close/reopen, table DDL, DML, and
independent handles. It is a compatibility scalar only. Because the fixed
value is disabled, existing `CREATE TABLE` behavior must not change, including
the current ability to create baseline tables without primary keys.

Successful scalar reads:

- return one row and one text column for each selected expression;
- use the original source expression as the column label unless the general
  scalar-select path later adds alias support;
- leave `warning_count == 0` for supported forms;
- do not mutate catalog rows, descriptor versions, descriptor caches, catalog
  generation, physical SQLite schema, or `.mylite` preamble bytes;
- follow existing scalar `SELECT` row-count behavior, so `ROW_COUNT()` after a
  successful scalar row result is `-1`.

Unsupported expressions such as `@@sql_require_primary_key + 1` remain rejected
by the current scalar expression rules even though MySQL accepts them.

## Diagnostics

The implementation must preserve existing diagnostics conventions:

- unsupported variable names: MySQL error `1193`, SQLSTATE `HY000`, message
  containing `Unknown system variable '<name>'`;
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

The feature must not:

- create or update catalog rows;
- create constraint metadata;
- inspect SQLite schema metadata;
- write to user tables;
- change physical SQLite table names or row storage;
- alter `.mylite` preamble bytes;
- add SQLite extension points or fork patches.

## Test Plan

Add a focused C runtime test, preferably
`runtime_sql_require_primary_key_system_variable_test.c`, registered as
`libmylite.runtime.sql_require_primary_key_system_variable`.

Cover:

- no-scope, `global`, `session`, and `local` reads;
- case-insensitive variable and scope names;
- backtick-quoted final variable components;
- source-text column labels;
- `FROM DUAL`;
- mixed scalar reads with existing system variables and functions;
- warning/error diagnostics snapshot and clearing behavior;
- unknown variables and quoted-scope rejection;
- explicit rejection of `SET SESSION sql_require_primary_key = 1`;
- successful baseline table creation without primary keys after scalar reads;
- descriptor-backed create/insert/update/delete/select independence;
- selected-schema independence;
- close/reopen persistence of user rows and fixed scalar value;
- unchanged catalog and SQLite schema generation for scalar reads;
- `.mylite` preamble preservation;
- independent handles with independent diagnostics;
- zero-initialized cleanup along existing result/statement cleanup paths.

Add the MySQL expectation script
`packages/libmylite/tests/mysql_baseline_sql_require_primary_key_system_variable_expectations.sh`
to record MySQL 8.4.9 values, scopes, labels, upstream session mutability,
upstream primary-key enforcement when enabled, diagnostics, and unsupported
MyLite forms.

## Compatibility Documentation

Update:

- `COMPATIBILITY.md`;
- `docs/compatibility/runtime-system-variables.md`;
- `docs/compatibility/sql-table-ddl.md`;
- `docs/compatibility/sql-replication.md`.

The docs must describe only limited read-only scalar support. They must not
claim mutable system-variable state, `SET`, primary-key constraints, DDL
enforcement, generated invisible primary keys, import behavior, replication
policy, privileges, `SHOW VARIABLES`, or Performance Schema variable tables.

## Verification

Before committing the implementation, run:

1. `cmake --build --preset dev`
2. `packages/libmylite/tests/mysql_baseline_sql_require_primary_key_system_variable_expectations.sh`
3. `ctest --preset dev -R 'libmylite\.runtime\.sql_require_primary_key_system_variable$' --output-on-failure`
4. Related lexer, parser, runtime system-variable, DDL, DML, and lifecycle CTest
   entries.
5. `cmake --workflow --preset check`
