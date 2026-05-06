# System Variable Expression Reads

## Scope

This slice adds expression reads for the system-variable form `@@name`,
`@@SESSION.name`, `@@LOCAL.name`, and `@@GLOBAL.name`. It covers the variables
that MyLite already exposes through the first `SHOW VARIABLES` and focused
`SET` slices: connection character-set and collation state, diagnostics counters,
`sql_mode`, `group_concat_max_len`, autocommit, transaction defaults, and version
variables.

User-defined variables (`@name`), variable assignment, persisted variables,
complete variable catalog coverage, Performance Schema variable tables, global
mutation, and protocol session-state tracking remain separate tasks.

## Sources

The behavior is independently specified from the MySQL 8.4 server-system-variable
documentation and MySQL 8.4.9 runtime probes. The MySQL manual states that
system variable values can be used in expressions and describes global/session
scope. Runtime probes verified values, explicit-scope errors, and metadata for
the variables in this slice.

- <https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html>
- <https://dev.mysql.com/doc/refman/8.4/en/system-variable-privileges.html>

## Syntax

System variables are primary expressions:

```lemon
primary_expression ::= SYSTEM_VARIABLE.
```

The lexer emits the complete system-variable token, including the leading `@@`
and any `GLOBAL.`, `SESSION.`, or `LOCAL.` prefix. Variable and scope names are
case-insensitive.

Supported forms:

- `@@sql_mode`
- `@@SESSION.sql_mode`
- `@@LOCAL.sql_mode`
- `@@GLOBAL.sql_mode`

`LOCAL` is a synonym for `SESSION`.

## Scope Rules

Bare `@@name` resolves to the session value when the variable has a session
scope. For global-only variables, bare `@@name` resolves to the global value.

Explicit `@@GLOBAL.name` resolves only variables that have global scope.
Explicit `@@SESSION.name` and `@@LOCAL.name` resolve only variables that have
session scope.

Invalid explicit scopes are errors:

- `@@GLOBAL.warning_count` reports error 1238 with
  `Variable 'warning_count' is a SESSION variable`.
- `@@SESSION.version` and `@@LOCAL.version` report error 1238 with
  `Variable 'version' is a GLOBAL variable`.
- `@@SESSION.log_bin`, `@@SESSION.gtid_purged`, and
  `@@SESSION.log_bin_trust_function_creators` report error 1238 because those
  variables are global-only.

Unknown variables report error 1193 with
`Unknown system variable '<name>'`.

## Values

String variables return text values. Numeric variables return integer values,
matching MySQL's `SELECT @@...` surface rather than the display strings used by
`SHOW VARIABLES`.

Session values reflect handle-owned MyLite state:

- `sql_mode` reads the current session mode.
- `group_concat_max_len` reads the current session limit.
- `character_set_client`, `character_set_connection`, `character_set_results`,
  and `collation_connection` read the current connection state.
- `character_set_database` and `collation_database` read the selected schema
  defaults, or MyLite defaults when no schema is selected.

Global values reflect MyLite defaults for this embedded runtime:

- `sql_mode` and `group_concat_max_len` read default values.
- Charset and collation globals read the built-in default registry values.
- `version` reads the MySQL compatibility target, currently `8.4.9`.
- `gtid_purged` reads an empty string, and `log_bin` /
  `log_bin_trust_function_creators` read `0` for MyLite's embedded runtime.
- Other version variables read embedded-runtime compile placeholders already
  exposed through `SHOW VARIABLES`.

Diagnostics counters currently follow MyLite's nondiagnostic clearing model and
read as `0` in ordinary `SELECT @@warning_count` / `@@error_count` statements.
`max_error_count` reads as `1024`.

Boolean variables return MySQL-compatible integers:

- `@@autocommit` returns `1`.
- `@@sql_notes` returns `1`.
- `@@transaction_read_only` returns `0`.

`@@transaction_isolation` returns `REPEATABLE-READ`.

## Metadata

MySQL 8.4.9 runtime probes show system-variable expressions use expression
metadata rather than `SHOW VARIABLES` metadata:

- String variables are `VAR_STRING`, collation `latin1_swedish_ci` (id 8),
  declared length `21845`, decimals `31`, and no `NOT_NULL` flag.
- `group_concat_max_len`, `warning_count`, `error_count`, and `max_error_count`
  are unsigned `LONGLONG`, binary collation (id 63), declared length `21`,
  decimals `0`, and no `NOT_NULL` flag.
- Boolean variables such as `autocommit`, `sql_notes`, and
  `transaction_read_only` are signed numeric `LONGLONG`, binary collation
  (id 63), declared length `1`, decimals `0`, and no `NOT_NULL` flag.

Result labels use the SQL expression text unless an alias is supplied, matching
the existing MyLite projection-label rule for scalar expressions.

## Interactions

System-variable reads are accepted wherever MyLite's supported scalar expression
subset is accepted: scalar `SELECT`, table-backed projections, predicates,
ordering, grouping expressions, and currently supported DML expression paths.

System-variable reads are session-dependent and must not be treated as cacheable
compile-time constants. In table scans, they may be evaluated per row by the
existing expression path; this is acceptable for the current small registry and
keeps state changes visible between statement executions where statements are
reused.

## Tests

Runtime tests cover:

- Bare, `SESSION`, `LOCAL`, and `GLOBAL` reads for `sql_mode` and
  `group_concat_max_len`.
- Session mutation through existing `SET` support followed by expression reads.
- Charset/collation state after `SET NAMES` and selected-schema defaults.
- Boolean and version variables.
- Use in table-backed `WHERE`, `ORDER BY`, and projection expressions.
- Metadata for string, unsigned numeric, and boolean variables.
- Unknown-variable and wrong-scope diagnostics.

Parser tests cover `SYSTEM_VARIABLE` as a primary expression in a select list.
