# Baseline Unique Checks System Variable

## Status

This feature specifies a narrow scalar system-variable slice for
`@@unique_checks`.

It builds on the existing `SYSTEM_VARIABLE` lexer/parser token, scalar
`SELECT` execution, diagnostics lifecycle, and MyLite's current no-index
descriptor model. MySQL exposes `unique_checks` as mutable global and session
state that can let storage engines relax secondary unique-index checking during
bulk loads. MyLite does not implement unique indexes, index descriptors, or
mutable system-variable assignment in the baseline yet, so this slice exposes
only the default enabled scalar value.

This is not unique-index support. It does not implement `SET unique_checks`,
unique index DDL, duplicate-key enforcement, index metadata, optimizer changes,
or bulk-load behavior changes.

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
`packages/libmylite/tests/mysql_baseline_unique_checks_system_variable_expectations.sh`
records the runtime probes for this feature. Observed behavior:

- `SELECT @@unique_checks`, `@@global.unique_checks`,
  `@@session.unique_checks`, `@@local.unique_checks`, and `@@UNIQUE_CHECKS`
  return `1` in the tested default runtime.
- The variable has global and session scope. After
  `SET SESSION unique_checks=0`, unscoped, `session`, and `local` reads return
  `0`, while `global` still returns `1`; resetting the session value to `1`
  restores the default.
- Variable and scope names are case-insensitive.
- Backtick-quoted final variable-name components are accepted.
- Backtick-quoted scope names, such as ``@@`session`.unique_checks``, are
  syntax errors.
- Unknown variables fail with error `1193`, SQLSTATE `HY000`, and an
  `Unknown system variable` message.
- A scalar `SELECT` that reads this variable is nondiagnostic. It reads the
  previous diagnostics snapshot for any `@@warning_count` or `@@error_count`
  items in the same select list, then clears diagnostics for following
  diagnostic statements.
- MySQL accepts wider expression forms such as `SELECT @@unique_checks + 1`.
  Those forms remain outside this MyLite slice.

The official MySQL system-variable documentation classifies `unique_checks` as
a dynamic boolean variable with global and session scope and default value
`ON`. It also states that disabling this variable permits storage engines to
assume duplicate keys are absent, but does not require an engine to skip
duplicate-key checks.

## Scope

The implementation must add:

- runtime recognition of `unique_checks` inside the existing scalar `SELECT`
  subset;
- support for no scope, `session`, `local`, and `global` scope qualifiers;
- case-insensitive matching for unquoted scope and variable names;
- backtick-quoted final variable-name components;
- one-row scalar result sets with existing source-span column labels;
- fixed value `1` for all supported scopes;
- MySQL-compatible unknown-variable diagnostics for unsupported names;
- deterministic rejection of quoted scopes;
- fast C tests and a MySQL 8.4.9 expectation artifact.

Supported SQL examples:

```sql
SELECT @@unique_checks
SELECT @@unique_checks FROM DUAL
SELECT @@session.unique_checks, @@local.unique_checks
SELECT @@global.unique_checks
SELECT @@session.`unique_checks`, @@`unique_checks`
SELECT @@unique_checks, @@warning_count, ROW_COUNT()
```

## Non-Goals

This feature must not implement:

- `SET`, startup options, persisted variables, `SET_VAR` hints, or mutable
  global/session `unique_checks` state;
- variables other than `unique_checks`;
- unique, primary-key, secondary-index, or constraint syntax;
- index descriptors, index storage, duplicate-key checks, index metadata,
  optimizer changes, import optimizations, or DDL behavior changes;
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
- The catalog remains authoritative for descriptors. This variable slice does
  not create index descriptors or affect table lifecycle behavior.
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
@@unique_checks
@@session.unique_checks
@@local.unique_checks
@@global.unique_checks
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

For this slice, all scopes return the same fixed value. This is a deliberate
MyLite limitation: descriptor-owned unique indexes remain always enforced and
no mutable system-variable state exists.

## Runtime Semantics

The supported variable returns:

| Variable | Value |
| --- | --- |
| `unique_checks` | `1` |

The value is independent of selected schema, close/reopen, table DDL, DML, and
independent handles. It is a compatibility scalar only. It must not affect
accepted or rejected DDL because unique-index syntax and descriptors remain out
of scope.

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
- public API misuse through the existing execution/result API behavior;
- allocation failures through existing MyLite allocation diagnostics.

Supported reads of `@@unique_checks` do not emit warnings. This slice does not
implement MySQL's mutable `SET SESSION unique_checks=...` surface, so
assignment diagnostics and index side effects are out of scope.

## Tests

Tests must cover:

- unscoped, `global`, `session`, and `local` forms;
- case-insensitive names and scopes;
- backtick-quoted final variable names;
- quoted scope rejection;
- exact column labels for representative source spellings;
- `FROM DUAL`;
- mixed scalar reads with existing diagnostics, charset, engine, autocommit,
  quote-control, foreign-key-check, and version variables;
- diagnostics read-and-clear behavior after warnings and errors;
- unknown unscoped and scoped variable names;
- unsupported wider expressions;
- selected schema, close/reopen, table DDL, and independent handles do not
  change the fixed value;
- `.mylite` preamble preservation and unchanged catalog/SQLite generation after
  variable reads;
- existing parser/runtime/system-variable and table lifecycle tests still pass.

The MySQL expectation script verifies the MySQL 8.4.9 reference behavior for
the supported SQL forms and explicitly records mutable and wider MySQL
behavior that this slice leaves unsupported.

## Compatibility Documentation

Update:

- `COMPATIBILITY.md`;
- `docs/compatibility/runtime-system-variables.md`;
- `docs/compatibility/sql-indexes-constraints.md`.

Do not overclaim mutable system variables, `SET`, `SHOW VARIABLES`, unique
index DDL, duplicate-key enforcement, index metadata, optimizer effects, or
bulk-load behavior.
