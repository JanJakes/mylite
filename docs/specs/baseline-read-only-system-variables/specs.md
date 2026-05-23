# Baseline Read-Only System Variables

## Status

This feature specifies a narrow system-variable compatibility slice for
`read_only`, `super_read_only`, and `innodb_read_only`.

MySQL exposes these variables as server-level read-only controls. MyLite is an
embedded library with independent handles rather than a privileged server-wide
administration plane, so this slice exposes the default disabled values and
the MySQL-compatible scope/read-only diagnostics that client bootstraps often
probe. It does not implement a mutable global read-only mode or block writes.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- Baseline `SHOW VARIABLES`:
  `docs/specs/baseline-show-variables/specs.md`
- MySQL 8.4 Reference Manual, server system variables:
  https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html
- MySQL 8.4 Reference Manual, InnoDB startup options and system variables:
  https://dev.mysql.com/doc/refman/8.4/en/innodb-parameters.html
- MySQL 8.4 Reference Manual, dynamic system variables:
  https://dev.mysql.com/doc/refman/8.4/en/dynamic-system-variables.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_read_only_system_variables_expectations.sh`
records the runtime probes for this feature. The probes are run against
container `mylite-mysql-849` with:

```sh
docker exec -i mylite-mysql-849 mysql \
  --protocol=TCP -h127.0.0.1 -uroot \
  --batch --raw --skip-column-names --default-character-set=utf8mb4
```

Observed behavior in the tested default runtime:

- `SELECT VERSION()` returns `8.4.9`.
- `@@read_only`, `@@GLOBAL.read_only`, `@@super_read_only`,
  `@@GLOBAL.super_read_only`, `@@innodb_read_only`, and
  `@@GLOBAL.innodb_read_only` return `0`.
- `SHOW VARIABLES` and `SHOW GLOBAL VARIABLES` display `OFF` for all three
  variables.
- Variable names and unquoted scopes are case-insensitive, and backtick-quoted
  final variable-name components are accepted.
- `@@SESSION.*` and `@@LOCAL.*` reads for all three variables fail with
  error `1238`, SQLSTATE `HY000`, and a message that the variable is global.
- `SET read_only = 0`, `SET SESSION read_only = 0`,
  `SET @@read_only = 0`, and equivalent non-global `super_read_only` forms
  fail with error `1229`, SQLSTATE `HY000`, and a message that the global
  variable should be set with `SET GLOBAL`.
- `SET GLOBAL read_only = OFF`, `SET @@GLOBAL.read_only = 0`, and
  `SET GLOBAL read_only = DEFAULT` succeed and leave `ROW_COUNT() = 0`,
  `@@warning_count = 0`, and `@@error_count = 0`.
- `SET GLOBAL super_read_only = OFF`, `SET @@GLOBAL.super_read_only = 0`, and
  `SET GLOBAL super_read_only = DEFAULT` have the same no-warning successful
  shape.
- MySQL permits `SET GLOBAL read_only = ON` and
  `SET GLOBAL super_read_only = ON` with appropriate privileges, but those
  state-changing forms are outside this embedded MyLite slice.
- All `SET innodb_read_only = 0` forms, including global forms, fail with
  error `1238`, SQLSTATE `HY000`, and a read-only variable message.

The official documentation classifies `read_only` and `super_read_only` as
global dynamic boolean variables with default `OFF`, while `innodb_read_only`
is global, non-dynamic, boolean, and default `OFF`. It also documents that
`super_read_only = ON` implies `read_only = ON`, and that
`read_only = OFF` clears `super_read_only`. This slice keeps both values fixed
at `OFF`, so those interactions do not create mutable state in MyLite.

## Scope

Supported SQL examples:

```sql
SELECT @@read_only, @@super_read_only, @@innodb_read_only
SELECT @@GLOBAL.read_only, @@global.`super_read_only`
SHOW VARIABLES WHERE Variable_name IN ('read_only', 'super_read_only')
SHOW GLOBAL VARIABLES LIKE 'innodb_read_only'
SET GLOBAL read_only = OFF
SET @@GLOBAL.super_read_only = 0
SET GLOBAL read_only = DEFAULT
```

The implementation must add:

- runtime recognition of all three variables in the existing system-variable
  registry;
- global-only scalar read semantics with fixed value `0`;
- `SHOW VARIABLES` / `SHOW GLOBAL VARIABLES` rows with value `OFF`;
- MySQL-compatible session/local scalar diagnostics;
- MySQL-compatible non-global `SET` diagnostics for `read_only` and
  `super_read_only`;
- exact no-op global `SET` forms for `read_only` and `super_read_only` using
  `OFF`, `FALSE`, `0`, and `DEFAULT`;
- deterministic unsupported diagnostics for attempts to set
  `read_only` / `super_read_only` to `ON`, `TRUE`, or `1`;
- MySQL-compatible read-only diagnostics for every `innodb_read_only`
  assignment form;
- C tests and a MySQL 8.4.9 expectation artifact.

## Non-Goals

This feature must not implement:

- mutable global `read_only`, `super_read_only`, or `innodb_read_only` state;
- write blocking, privilege checks, server-global state shared across handles,
  replication-thread exceptions, lock/transaction waits, or administrative
  concurrency behavior;
- `SET PERSIST`, `SET_VAR` hints, startup options, option files, or Performance
  Schema variable tables;
- the `super_read_only`/`read_only` implication rules for enabled values;
- protocol status changes or OK-packet metadata beyond the existing public
  result conventions;
- catalog, descriptor, storage, VFS, SQLite schema, or `.mylite` preamble
  mutations;
- SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public validation,
  diagnostics, result allocation, and row-count state.
- Statement context preserves the existing diagnostics lifecycle. Successful
  scalar reads are nondiagnostic; successful no-op `SET GLOBAL` statements
  clear diagnostics and report zero affected rows.
- Lexer/parser/AST already own `SYSTEM_VARIABLE` and `SET` target syntax. This
  slice needs no new grammar productions.
- Runtime system-variable resolution owns scope validation, fixed values,
  no-op `SET` validation, and MySQL-compatible diagnostics.
- `SHOW VARIABLES` uses the existing registry-driven row builder and WHERE/LIKE
  filtering.
- Catalog descriptors, result-set storage, file-backed storage, VFS, and
  SQLite physical rows are not involved.

## Supported Grammar

The existing grammar remains sufficient:

```lemon
expression ::= SYSTEM_VARIABLE.
set_statement ::= SET set_assignment_list.
set_assignment ::= set_system_variable_target EQ set_value.
show_statement ::= SHOW show_scope_opt VARIABLES show_filter_opt.
```

The admitted variable paths are:

```sql
@@read_only
@@global.read_only
@@super_read_only
@@global.super_read_only
@@innodb_read_only
@@global.innodb_read_only
```

The admitted no-op assignment paths are:

```sql
SET GLOBAL read_only = OFF
SET @@GLOBAL.read_only = 0
SET GLOBAL read_only = DEFAULT
SET GLOBAL super_read_only = OFF
SET @@GLOBAL.super_read_only = 0
SET GLOBAL super_read_only = DEFAULT
```

`SET innodb_read_only = ...` is always rejected as read-only.

## Runtime Semantics

Scalar reads return text values through the existing scalar result path:

| Variable | Scalar value | SHOW value |
| --- | --- | --- |
| `read_only` | `0` | `OFF` |
| `super_read_only` | `0` | `OFF` |
| `innodb_read_only` | `0` | `OFF` |

The values are independent of schemas, transactions, close/reopen, and
independent handles. They do not alter DDL, DML, or transaction behavior.

Successful no-op `SET GLOBAL read_only = OFF` and
`SET GLOBAL super_read_only = OFF`:

- return no result rows;
- report `affected_rows == 0`;
- report `warning_count == 0`;
- leave scalar and `SHOW VARIABLES` values unchanged;
- do not mutate session state, catalog generation, SQLite schema generation,
  descriptors, or file bytes beyond normal diagnostics/result objects.

## Diagnostics

Diagnostics must follow the existing MyLite conventions and MySQL 8.4.9
observations for this slice:

- unknown variables: existing `1193 / HY000` unknown-system-variable behavior;
- malformed variable paths or quoted scope: existing deterministic parser or
  unsupported diagnostics;
- `@@SESSION.read_only`, `@@LOCAL.read_only`,
  `@@SESSION.super_read_only`, `@@LOCAL.super_read_only`,
  `@@SESSION.innodb_read_only`, and `@@LOCAL.innodb_read_only`:
  `1238 / HY000`, `Variable '<name>' is a GLOBAL variable`;
- non-global `SET read_only` or `SET super_read_only` forms:
  `1229 / HY000`,
  `Variable '<name>' is a GLOBAL variable and should be set with SET GLOBAL`;
- global `SET read_only` / `SET super_read_only` to enabled values:
  deterministic MyLite unsupported diagnostic because mutable global read-only
  state is intentionally deferred;
- every `SET innodb_read_only` form:
  `1238 / HY000`, `Variable 'innodb_read_only' is a read only variable`;
- allocation failures use the existing `MYLITE_NOMEM` path.

## Tests

Tests must cover:

- MySQL expectation script for values, `SHOW` rows, scope diagnostics,
  no-op global assignments, rejected non-global assignments, rejected enabled
  global assignments in MyLite, and `innodb_read_only` read-only assignment
  diagnostics;
- scalar default/global reads, case-insensitive names, quoted final names, and
  session/local scope errors;
- `SHOW VARIABLES`, `SHOW GLOBAL VARIABLES`, `LIKE`, and limited `WHERE`;
- successful no-op global `SET` result metadata and unchanged values;
- deterministic unsupported errors for state-changing `read_only` and
  `super_read_only`;
- `innodb_read_only` assignment diagnostics;
- mixed scalar reads with already supported fixed variables;
- independent handles showing fixed values;
- existing `SHOW VARIABLES`, `SET`, diagnostics, statement context, runtime
  lifecycle, and storage tests still passing.
