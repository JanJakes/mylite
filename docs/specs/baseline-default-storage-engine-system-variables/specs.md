# Baseline Default Storage Engine System Variable

## Status

This feature specifies a narrow scalar system-variable slice for
`@@default_storage_engine`.

It builds on the existing `SYSTEM_VARIABLE` lexer/parser token, scalar
`SELECT` execution, diagnostics lifecycle, and MyLite's explicit embedded
InnoDB-only engine surface. MyLite does not implement multiple storage engines,
so this variable exposes the fixed permanent-table default engine used by the
current table lifecycle.

This is not full storage-engine or system-variable management. It does not
implement mutable `SET` behavior, startup options, engine substitution, storage
engine plugins, or `SHOW VARIABLES`.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- Baseline InnoDB engine surface:
  `docs/specs/baseline-innodb-engine-surface/specs.md`
- MySQL lexer: `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold: `docs/specs/mysql-parser-scaffold/specs.md`
- MySQL 8.4 Reference Manual, server system variables:
  https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html
- MySQL 8.4 Reference Manual, setting the storage engine:
  https://dev.mysql.com/doc/refman/8.4/en/storage-engine-setting.html
- MySQL 8.4 Reference Manual, storage engines:
  https://dev.mysql.com/doc/refman/8.4/en/storage-engines.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_default_storage_engine_system_variables_expectations.sh`
records the runtime probes for this feature. Observed behavior:

- `SELECT @@default_storage_engine`, `@@global.default_storage_engine`,
  `@@session.default_storage_engine`, and `@@local.default_storage_engine`
  return `InnoDB` in the tested default MySQL 8.4.9 runtime.
- The variable has global and session scope. After
  `SET SESSION default_storage_engine=MEMORY`, unscoped, `session`, and
  `local` reads return `MEMORY`, while `global` still returns `InnoDB`.
- Variable and scope names are case-insensitive.
- Backtick-quoted final variable-name components are accepted.
- Backtick-quoted scope names, such as
  ``@@`session`.default_storage_engine``, are syntax errors.
- Unknown variables fail with error `1193`, SQLSTATE `HY000`, and an
  `Unknown system variable` message.
- A scalar `SELECT` that reads this variable is nondiagnostic. It reads the
  previous diagnostics snapshot for any `@@warning_count` or `@@error_count`
  items in the same select list, then clears diagnostics for following
  diagnostic statements.
- MySQL accepts wider expression forms such as
  `SELECT @@default_storage_engine + 1`, coercing the string and warning.
  Those forms remain outside this MyLite slice.

The official MySQL system-variable documentation classifies
`default_storage_engine` as a dynamic enumeration variable with global and
session scope and default value `InnoDB`. The storage-engine documentation
describes `InnoDB` as the default storage engine in MySQL 8.4.

## Scope

The implementation must add:

- runtime recognition of `default_storage_engine` inside the existing scalar
  `SELECT` subset;
- support for no scope, `session`, `local`, and `global` scope qualifiers;
- case-insensitive matching for unquoted scope and variable names;
- backtick-quoted final variable-name components;
- one-row scalar result sets with existing source-span column labels;
- fixed value `InnoDB` for all supported scopes;
- MySQL-compatible unknown-variable diagnostics for unsupported names;
- deterministic rejection of quoted scopes;
- fast C tests and a MySQL 8.4.9 expectation artifact.

Supported SQL examples:

```sql
SELECT @@default_storage_engine
SELECT @@default_storage_engine FROM DUAL
SELECT @@session.default_storage_engine, @@local.default_storage_engine
SELECT @@global.default_storage_engine
SELECT @@session.`default_storage_engine`, @@`default_storage_engine`
SELECT @@default_storage_engine, @@warning_count, ROW_COUNT()
```

## Non-Goals

This feature must not implement:

- `SET`, startup options, persisted variables, `SET_VAR` hints, or mutable
  global/session default-engine state;
- variables other than `default_storage_engine`;
- `default_tmp_storage_engine`, `disabled_storage_engines`, or
  engine-substitution SQL-mode behavior;
- alternate engines, plugin loading, storage-engine metadata tables, or
  `INFORMATION_SCHEMA.ENGINES`;
- `SHOW VARIABLES` or Performance Schema variable tables;
- changes to `CREATE TABLE` engine-option handling;
- table-backed variable evaluation, aliases, clauses, subqueries, arithmetic,
  functions over variables, parameters, prepared statements, or arbitrary
  SQLite pass-through;
- SQLite metadata reads or SQLite fork patches.

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
  engine value selection, and diagnostics for unsupported names.
- Catalog and table lifecycle code remain authoritative for persistent base
  table descriptors. This feature reads no catalog rows and does not mutate
  catalog generation.
- Result builder owns scalar result column labels and one-row text values.
- Storage, VFS, and SQLite physical row storage are not involved. This feature
  must not touch `.mylite` preamble bytes, generated SQLite SQL, or SQLite
  schema state.

## Supported SQL Grammar

This slice uses the existing system-variable expression atom:

```lemon
expression ::= SYSTEM_VARIABLE.
```

The supported runtime variable paths are:

```sql
@@default_storage_engine
@@session.default_storage_engine
@@local.default_storage_engine
@@global.default_storage_engine
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
- it rejects malformed paths and unsupported variables with MySQL error
  `1193`, SQLSTATE `HY000`;
- it preserves the original source text as the scalar result column label.

For this slice, all scopes return the same fixed value. This is a deliberate
MyLite limitation: MyLite currently has one embedded permanent-table engine and
does not implement mutable system-variable state.

## Runtime Semantics

The supported variable returns:

| Variable | Value |
| --- | --- |
| `default_storage_engine` | `InnoDB` |

The value is independent of selected schema, close/reopen, and independent
handles. It reflects the same InnoDB-compatible permanent-table default exposed
by `SHOW [STORAGE] ENGINES` and the accepted `CREATE TABLE ... ENGINE [=]
InnoDB` subset.

Successful scalar reads:

- return one row and one text column for each selected expression;
- use the original source expression as the column label unless the general
  scalar-select path later adds alias support;
- leave `warning_count == 0` for supported in-range forms;
- do not mutate catalog rows, descriptor versions, descriptor caches, catalog
  generation, physical SQLite schema, or `.mylite` preamble bytes;
- follow existing scalar `SELECT` row-count behavior, so `ROW_COUNT()` after a
  successful scalar row result is `-1`.

## Diagnostics

This slice uses existing diagnostics for:

- syntax errors, including quoted scopes and unsupported scalar-select clauses;
- unknown system variables: error `1193`, SQLSTATE `HY000`;
- unsupported scope for future variable kinds if shared resolver policy grows;
- unsupported expressions such as arithmetic over system variables;
- public API misuse through the existing execution/result API behavior;
- allocation failures through existing MyLite allocation diagnostics.

Supported reads of `@@default_storage_engine` do not emit warnings. This slice
does not implement MySQL's mutable `SET SESSION default_storage_engine=...`
surface, so engine-substitution warnings and dynamic assignment diagnostics are
out of scope.

## Tests

Tests must cover:

- unscoped, `global`, `session`, and `local` forms;
- case-insensitive names and scopes;
- backtick-quoted final variable names;
- quoted scope rejection;
- exact column labels for representative source spellings;
- `FROM DUAL`;
- mixed scalar reads with existing engine, character-set, diagnostics, and
  version variables;
- diagnostics read-and-clear behavior after warnings and errors;
- unknown unscoped and scoped variable names;
- unsupported wider expressions;
- selected schema, close/reopen, and independent handles do not change the
  fixed value;
- `.mylite` preamble preservation and unchanged catalog/SQLite generation;
- existing parser/runtime/engine/system-variable tests still pass.

The MySQL expectation script verifies the MySQL 8.4.9 reference behavior for
the supported SQL forms and explicitly records wider MySQL behavior that this
slice leaves unsupported.

## Compatibility Documentation

Update:

- `COMPATIBILITY.md`;
- `docs/compatibility/runtime-system-variables.md`;
- `docs/compatibility/embedded-design-decisions.md`.

Do not overclaim mutable system variables, `SET`, `SHOW VARIABLES`, alternate
storage engines, engine substitution, temporary-table default engines,
`INFORMATION_SCHEMA.ENGINES`, or plugin behavior.
