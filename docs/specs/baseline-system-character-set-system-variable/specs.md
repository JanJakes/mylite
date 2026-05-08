# Baseline System Character Set System Variable

## Status

This feature specifies a narrow scalar system-variable slice for
`@@character_set_system`.

It builds on the existing `SYSTEM_VARIABLE` lexer/parser token, scalar
`SELECT` execution, diagnostics lifecycle, and MyLite's fixed charset variable
surface. MySQL exposes this as the character set used for storing identifiers,
with value `utf8mb3` in MySQL 8.4.9. MyLite does not yet implement general
identifier character-set semantics, so this slice exposes only the fixed
read-only scalar value.

This is not full `utf8mb3` character-set support. It does not implement string
storage, conversions, collations, metadata catalogs, or mutable system-variable
state.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- Baseline character set system variables:
  `docs/specs/baseline-character-set-system-variables/specs.md`
- Baseline server character set system variables:
  `docs/specs/baseline-server-character-set-system-variables/specs.md`
- MySQL lexer: `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold: `docs/specs/mysql-parser-scaffold/specs.md`
- MySQL 8.4 Reference Manual, server system variables:
  https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html
- MySQL 8.4 Reference Manual, Unicode character sets:
  https://dev.mysql.com/doc/refman/8.4/en/charset-unicode-sets.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_system_character_set_system_variable_expectations.sh`
records the runtime probes for this feature. Observed behavior:

- `SELECT @@character_set_system`, `@@global.character_set_system`, and
  `@@CHARACTER_SET_SYSTEM` return `utf8mb3`.
- `@@session.character_set_system` and `@@local.character_set_system` fail with
  error `1238`, SQLSTATE `HY000`, and a message that the variable is global.
- Variable and scope names are case-insensitive.
- Backtick-quoted final variable-name components are accepted.
- Backtick-quoted scope names, such as
  ``@@`session`.character_set_system``, are syntax errors.
- Unknown variables fail with error `1193`, SQLSTATE `HY000`, and an
  `Unknown system variable` message.
- A scalar `SELECT` that reads this variable is nondiagnostic. It reads the
  previous diagnostics snapshot for any `@@warning_count` or `@@error_count`
  items in the same select list, then clears diagnostics for following
  diagnostic statements.
- MySQL accepts wider expression forms such as
  `SELECT @@character_set_system + 1`; those forms remain outside this MyLite
  slice.

The official MySQL system-variable documentation classifies
`character_set_system` as a read-only global string variable with value
`utf8mb3`. MySQL's Unicode character-set documentation states that `utf8mb3`
is deprecated but remains supported for the MySQL 8.4 LTS series.

## Scope

The implementation must add:

- runtime recognition of `character_set_system` inside the existing scalar
  `SELECT` subset;
- support for no scope and `global` scope;
- rejection of `session` and `local` scope with the existing global-only
  system-variable diagnostic;
- case-insensitive matching for unquoted scope and variable names;
- backtick-quoted final variable-name components;
- one-row scalar result sets with existing source-span column labels;
- fixed value `utf8mb3`;
- MySQL-compatible unknown-variable diagnostics for unsupported names;
- deterministic rejection of quoted scopes;
- fast C tests and a MySQL 8.4.9 expectation artifact.

Supported SQL examples:

```sql
SELECT @@character_set_system
SELECT @@character_set_system FROM DUAL
SELECT @@global.character_set_system
SELECT @@global.`character_set_system`, @@`character_set_system`
SELECT @@character_set_system, @@warning_count, ROW_COUNT()
```

## Non-Goals

This feature must not implement:

- `SET`, startup options, persisted variables, `SET_VAR` hints, or mutable
  system-variable state;
- variables other than `character_set_system`;
- session or local scope for `character_set_system`;
- `character_sets_dir`, `character_set_filesystem`, or
  `default_collation_for_utf8mb4`;
- general `utf8mb3` table, column, literal, conversion, or collation support;
- `SHOW VARIABLES`, Performance Schema variable tables, or
  `INFORMATION_SCHEMA` character-set/collation tables;
- client charset negotiation through a wire protocol;
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
  system charset value selection, and diagnostics for unsupported names.
- Result builder owns scalar result column labels and one-row text values.
- Catalog, storage, VFS, and SQLite physical row storage are not involved.
  This feature must not touch `.mylite` preamble bytes or SQLite schema state.

## Supported SQL Grammar

This slice uses the existing system-variable expression atom:

```lemon
expression ::= SYSTEM_VARIABLE.
```

The supported runtime variable paths are:

```sql
@@character_set_system
@@global.character_set_system
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
- it rejects `session` and `local` scope with the existing global-only
  diagnostic: error `1238`, SQLSTATE `HY000`;
- it rejects malformed paths and unsupported variables with MySQL error
  `1193`, SQLSTATE `HY000`;
- it preserves the original source text as the scalar result column label.

## Runtime Semantics

The supported variable returns:

| Variable | Value |
| --- | --- |
| `character_set_system` | `utf8mb3` |

The value is a MyLite system-charset placeholder initialized from a constant.
Since this slice does not implement identifier character-set conversion or
charset-changing statements, it does not change across `CREATE DATABASE`,
`USE`, table DDL, DML, close/reopen, or independent handles.

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

- syntax errors, including quoted scopes and unsupported scalar-select clauses;
- unknown system variables: error `1193`, SQLSTATE `HY000`;
- `session` and `local` scope for the global-only variable: error `1238`,
  SQLSTATE `HY000`;
- unsupported expressions such as arithmetic over system variables;
- public API misuse through the existing execution/result API behavior;
- allocation failures through existing MyLite allocation diagnostics.

Supported reads of `@@character_set_system` do not emit warnings. This slice
does not implement MySQL's general expression coercion for this variable, so
coercion warnings remain out of scope.

## Tests

Tests must cover:

- unscoped and `global` forms;
- `session` and `local` scope rejection;
- case-insensitive names and scopes;
- backtick-quoted final variable names;
- quoted scope rejection;
- exact column labels for representative source spellings;
- `FROM DUAL`;
- mixed scalar reads with existing charset, diagnostics, version, and engine
  variables;
- diagnostics read-and-clear behavior after warnings and errors;
- unknown unscoped and scoped variable names;
- unsupported wider expressions;
- selected schema, close/reopen, and independent handles do not change the
  fixed value;
- `.mylite` preamble preservation and unchanged catalog/SQLite generation;
- existing parser/runtime/charset/system-variable tests still pass.

The MySQL expectation script verifies the MySQL 8.4.9 reference behavior for
the supported SQL forms and explicitly records wider MySQL behavior that this
slice leaves unsupported.

## Compatibility Documentation

Update:

- `COMPATIBILITY.md`;
- `docs/compatibility/runtime-system-variables.md`;
- `docs/compatibility/character-sets.md`.

Do not overclaim `utf8mb3` storage/conversion/collation support, mutable
system variables, `SET`, `SHOW VARIABLES`, client/server negotiation, or
identifier character-set conversion semantics.
