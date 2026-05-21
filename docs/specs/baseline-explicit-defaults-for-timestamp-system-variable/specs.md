# Baseline Explicit Defaults For Timestamp System Variable

## Status

This feature specifies a narrow server-system-variable slice for
`explicit_defaults_for_timestamp`.

MySQL 8.4 exposes `explicit_defaults_for_timestamp` as a deprecated dynamic
boolean with global and session scope. Its default value is `ON`, which means
`TIMESTAMP` columns use ordinary explicit default handling rather than the old
implicit first-`TIMESTAMP` behavior. MyLite's current temporal descriptor code
already follows the modern explicit-defaults model, so this slice exposes the
variable as fixed `ON` and admits only no-op assignments that preserve that
value.

This is not deprecated `explicit_defaults_for_timestamp=OFF` support. It does
not add legacy implicit `TIMESTAMP` defaults, legacy automatic first-column
updates, startup option state, persisted variables, or mutable global/session
state.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- SQL mode/session variable behavior:
  `docs/specs/baseline-sql-mode-system-variable/specs.md`
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
`packages/libmylite/tests/mysql_baseline_explicit_defaults_for_timestamp_system_variable_expectations.sh`
records the runtime probes for this feature. Observed behavior:

- `SELECT VERSION()` returned `8.4.9`.
- `SELECT @@explicit_defaults_for_timestamp`,
  `@@global.explicit_defaults_for_timestamp`,
  `@@session.explicit_defaults_for_timestamp`,
  `@@local.explicit_defaults_for_timestamp`, and
  `@@EXPLICIT_DEFAULTS_FOR_TIMESTAMP` return `1` in the tested runtime.
- `SHOW VARIABLES LIKE 'explicit_defaults_for_timestamp'`, `SHOW GLOBAL
  VARIABLES LIKE ...`, `SHOW SESSION VARIABLES LIKE ...`, and limited
  `SHOW VARIABLES WHERE Variable_name = ...` return `ON`.
- Variable and scope names are case-insensitive.
- Backtick-quoted final variable-name components are accepted.
- Backtick-quoted scope names, such as
  ``@@`session`.explicit_defaults_for_timestamp``, are syntax errors.
- Unknown variables fail with error `1193`, SQLSTATE `HY000`, and an
  `Unknown system variable` message.
- The variable has global and session scope. Exact no-op assignments such as
  `SET SESSION explicit_defaults_for_timestamp=DEFAULT`, `=ON`, `=1`, and
  `=TRUE` leave the session value at `1` with no warnings.
- Assigning `OFF` emits deprecation warning `1287` and changes MySQL's session
  value to `0`. MyLite deliberately rejects that path in this slice because it
  would require the deprecated timestamp-default behavior.
- Assigning `2` or `NULL` fails with error `1231`, SQLSTATE `42000`.
- A scalar `SELECT` that reads this variable is nondiagnostic. It reads the
  previous diagnostics snapshot for any `@@warning_count` or `@@error_count`
  items in the same select list, then clears diagnostics for following
  diagnostic statements.
- MySQL accepts wider expression forms such as
  `SELECT @@explicit_defaults_for_timestamp + 1`. Those forms remain outside
  this MyLite slice.

The official MySQL system-variable documentation classifies
`explicit_defaults_for_timestamp` as a deprecated dynamic boolean variable
with global and session scope and default value `ON`. MyLite exposes that
modern default as fixed state and documents `OFF` as unsupported until the
legacy timestamp descriptor semantics are specified.

## Scope

The implementation must add:

- runtime recognition of `explicit_defaults_for_timestamp` inside the existing
  scalar `SELECT` subset;
- support for no scope, `session`, `local`, and `global` scope qualifiers;
- case-insensitive matching for unquoted scope and variable names;
- backtick-quoted final variable-name components;
- one-row scalar result sets with existing source-span column labels;
- `SHOW VARIABLES` rows for session/local/global/default scopes with value
  `ON`;
- limited no-op `SET` assignment for unscoped/session/local/direct
  session-variable forms when the assigned value is `DEFAULT`, `ON`, `TRUE`,
  or integer `1`;
- fixed scalar value `1` and fixed `SHOW VARIABLES` value `ON` for all
  supported scopes;
- MySQL-compatible unknown-variable diagnostics for unsupported names;
- deterministic rejection of quoted scopes, mutable `OFF` assignments, and
  unsupported assignment value forms;
- fast C tests and a MySQL 8.4.9 expectation artifact.

Supported SQL examples:

```sql
SELECT @@explicit_defaults_for_timestamp
SELECT @@explicit_defaults_for_timestamp FROM DUAL
SELECT @@session.explicit_defaults_for_timestamp
SELECT @@local.explicit_defaults_for_timestamp
SELECT @@global.explicit_defaults_for_timestamp
SELECT @@session.`explicit_defaults_for_timestamp`
SELECT @@`explicit_defaults_for_timestamp`
SHOW VARIABLES LIKE 'explicit_defaults_for_timestamp'
SHOW GLOBAL VARIABLES LIKE 'explicit_defaults_for_timestamp'
SHOW VARIABLES WHERE Variable_name = 'explicit_defaults_for_timestamp'
SET explicit_defaults_for_timestamp = DEFAULT
SET SESSION explicit_defaults_for_timestamp = ON
SET LOCAL explicit_defaults_for_timestamp = TRUE
SET @@session.explicit_defaults_for_timestamp = 1
```

## Non-Goals

This feature must not implement:

- `explicit_defaults_for_timestamp=OFF`;
- legacy implicit first-`TIMESTAMP` `DEFAULT CURRENT_TIMESTAMP` or
  `ON UPDATE CURRENT_TIMESTAMP` behavior;
- mutable global/session system-variable state, persisted variables, startup
  options, `SET PERSIST`, or `SET_VAR` hints;
- global assignment support in MyLite's current fixed-boolean `SET` baseline;
- variables other than `explicit_defaults_for_timestamp`;
- changed descriptor-backed DDL, DML, default materialization, automatic
  temporal update behavior, or `SHOW CREATE TABLE` output;
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
  `SYSTEM_VARIABLE` expressions and existing `SET` variable targets. No new
  grammar is needed beyond the current variable-expression and assignment
  grammar.
- Runtime execution owns system-variable path parsing, scope validation, fixed
  value selection, fixed no-op assignment validation, `SHOW VARIABLES` row
  generation, and diagnostics for unsupported names or value changes.
- Descriptor-driven table DDL and DML remain unchanged because this fixed
  modern-mode variable does not change MyLite's temporal default semantics.
- The catalog remains authoritative for descriptors. This variable slice does
  not create, rewrite, or version descriptors.
- Result builder owns scalar result column labels, one-row text values, and
  non-row statement reporting for successful `SET`.
- Storage, VFS, and SQLite physical row storage are not involved. This feature
  must not touch `.mylite` preamble bytes or SQLite schema state.

## Supported SQL Grammar

This slice uses existing grammar:

```lemon
expression ::= SYSTEM_VARIABLE.
set_statement ::= SET set_assignment_list.
set_assignment ::= system_variable_target EQ set_value.
system_variable_target ::= IDENTIFIER.
system_variable_target ::= system_variable_reference.
set_value ::= DEFAULT.
set_value ::= literal.
```

The supported runtime variable paths are:

```sql
@@explicit_defaults_for_timestamp
@@session.explicit_defaults_for_timestamp
@@local.explicit_defaults_for_timestamp
@@global.explicit_defaults_for_timestamp
```

The existing scalar `SELECT`, `SHOW VARIABLES`, and `SET` limits continue to
apply. General expressions over the variable, `SET GLOBAL`, persisted
assignment, table-backed evaluation, aliases, and arbitrary clauses remain
outside this slice.

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

All scopes return the same fixed value. This is a deliberate MyLite limitation:
no mutable deprecated timestamp mode exists yet.

## Runtime Semantics

The supported variable returns:

| Context | Value |
| --- | --- |
| scalar `SELECT @@explicit_defaults_for_timestamp` | `1` |
| `SHOW VARIABLES` value | `ON` |

Successful scalar reads:

- return one row and one text column for each selected expression;
- use the original source expression as the column label unless the general
  scalar-select path later adds alias support;
- leave `warning_count == 0` for supported forms;
- do not mutate catalog rows, descriptor versions, descriptor caches, catalog
  generation, physical SQLite schema, or `.mylite` preamble bytes;
- follow existing scalar `SELECT` row-count behavior, so `ROW_COUNT()` after a
  successful scalar row result is `-1`.

Successful supported `SET` forms:

- return through the existing public non-row result conventions;
- preserve the fixed visible value `1`;
- produce `affected_rows == 0` and `warning_count == 0`;
- clear prior diagnostics like other successful nondiagnostic statements;
- do not mutate catalog or storage state.

Unsupported `OFF` assignments are rejected rather than accepted with MySQL's
deprecated warning because accepting them would make later temporal DDL/DML
readbacks inaccurate.

## Diagnostics

This slice uses existing diagnostics for:

- syntax errors, including quoted scopes and unsupported scalar-select clauses;
- unknown system variables: error `1193`, SQLSTATE `HY000`;
- global fixed-boolean assignment in the current MyLite `SET` baseline:
  deterministic unsupported syntax diagnostic;
- unsupported no-op assignment values, including `OFF`, `FALSE`, integer `0`,
  integer values other than `1`, `NULL`, strings, parameters, functions, and
  arbitrary expressions: deterministic unsupported syntax diagnostic;
- unsupported expressions such as arithmetic over system variables;
- public API misuse through the existing execution/result API behavior;
- allocation failures through existing MyLite allocation diagnostics.

Supported reads and supported no-op assignments do not emit warnings.

## Tests

Tests must cover:

- unscoped, `global`, `session`, and `local` scalar forms;
- fixed scalar `1` and `SHOW VARIABLES` `ON` value;
- case-insensitive names and scopes;
- backtick-quoted final variable names;
- quoted scope rejection;
- exact column labels for representative source spellings;
- `FROM DUAL`;
- mixed scalar reads with existing diagnostics, charset, engine, autocommit,
  quote-control, and other fixed variables;
- `SHOW VARIABLES LIKE`, `SHOW GLOBAL VARIABLES LIKE`, and limited
  `SHOW VARIABLES WHERE` forms;
- no-op `SET` values `DEFAULT`, `ON`, `TRUE`, and integer `1`;
- rejection of `OFF`, `FALSE`, integer `0`, string, `NULL`, expression, and
  global assignment forms;
- selected-schema and descriptor-table operations proving no DDL/DML side
  effects;
- file-backed close/reopen persistence and `.mylite` preamble preservation;
- independent handles;
- MySQL 8.4.9 expectation script coverage for user-visible behavior.

## SQLite and Storage Impact

No SQLite fork patch, SQLite extension hook, physical SQL translation, or
storage change is needed. The implementation is a MyLite runtime registry and
result-generation change. It must not query SQLite metadata, generate SQLite
SQL, or rely on SQLite system-variable behavior.

## Compatibility Documentation

Update:

- `COMPATIBILITY.md` with the limited fixed-ON system-variable support row;
- `docs/compatibility/runtime-system-variables.md` with the precise fixed-ON
  scalar, `SHOW VARIABLES`, and no-op `SET` scope;
- no table-DDL or temporal-literal compatibility rows, because this slice does
  not change descriptor temporal behavior.
