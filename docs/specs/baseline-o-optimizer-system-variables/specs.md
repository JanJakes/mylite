# Baseline O Optimizer System Variables

## Scope

This slice exposes MySQL 8.4.9-shaped readback, `SHOW VARIABLES` rows, and
embedded placeholder assignment for the remaining optimizer, optimizer-trace,
parser-memory, and partial-revokes O/P variables that are not tied to optional
plugins:

- `optimizer_prune_level`
- `optimizer_search_depth`
- `optimizer_switch`
- `optimizer_trace`
- `optimizer_trace_features`
- `optimizer_trace_limit`
- `optimizer_trace_max_mem_size`
- `optimizer_trace_offset`
- `parser_max_mem_size`
- `partial_revokes`

The official MySQL 8.4 server-system-variable manual documents the optimizer
variables as global/session dynamic tuning variables, `parser_max_mem_size` as
a global/session parser memory limit, and `partial_revokes` as a global dynamic
privilege-policy variable.

References:

- <https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html>
- <https://dev.mysql.com/doc/refman/8.4/en/show-variables.html>

## Observed MySQL 8.4.9 Behavior

Runtime probes against `mysql:8.4.9` showed these defaults:

| Variable | Default | Scope | Notes |
| --- | --- | --- | --- |
| `optimizer_prune_level` | `1` | Global, session | Integer, clamps to `0..1` with warning `1292` |
| `optimizer_search_depth` | `62` | Global, session | Integer, clamps to `0..62` with warning `1292` |
| `optimizer_switch` | MySQL canonical optimizer flag list | Global, session | Set-valued string; unknown flags or values fail with `1231` |
| `optimizer_trace` | `enabled=off,one_line=off` | Global, session | Set-valued string; unknown flags or values fail with `1231` |
| `optimizer_trace_features` | `greedy_search=on,range_optimizer=on,dynamic_range=on,repeated_subselect=on` | Global, session | Set-valued string; unknown flags or values fail with `1231` |
| `optimizer_trace_limit` | `1` | Global, session | Signed-negative input clamps to `0` with warning `1292`; very large unsigned input clamps to `9223372036854775807` with warning `1292` |
| `optimizer_trace_max_mem_size` | `1048576` | Global, session | Accepts unsigned 64-bit values in the target runtime |
| `optimizer_trace_offset` | `-1` | Global, session | Signed integer; very large unsigned input clamps to `9223372036854775807`, and below `-9223372036854775807` clamps to that lower bound with warning `1292` |
| `parser_max_mem_size` | `18446744073709551615` | Global, session | Integer, values below `10000000` clamp upward with warning `1292` |
| `partial_revokes` | `OFF` | Global only | Session reads and assignments fail with global-variable diagnostics |

All variables appear in default, global, and session `SHOW VARIABLES` output
except that `partial_revokes` is global-only for scalar scope purposes while
still appearing in default/global/session `SHOW VARIABLES` rows.

Direct assignment accepts integer literals and integer user-variable values for
the numeric variables. Direct string assignment and string user-variable
assignment are accepted for the set-valued optimizer variables. `DEFAULT` and
the string value `default` restore the canonical default for the set-valued
variables.

Wrong literal families, such as assigning a string to an integer variable or
`NULL` to `parser_max_mem_size`, return `1232 / 42000`. Invalid set-valued flag
names or values return `1231 / 42000`.

## MyLite Semantics

MyLite supports:

- scalar reads for default, `GLOBAL`, `SESSION`, and `LOCAL` scopes where MySQL
  permits the variable scope;
- `SHOW VARIABLES`, `SHOW GLOBAL VARIABLES`, and `SHOW SESSION VARIABLES` rows;
- handle-local session placeholder assignment for the global/session variables;
- direct integer and integer user-variable assignment for numeric variables;
- direct string and string user-variable assignment for `optimizer_switch`,
  `optimizer_trace`, and `optimizer_trace_features`;
- canonical default restoration through SQL `DEFAULT` and string `default`;
- MySQL-shaped clamping warnings for the numeric paths documented by the
  runtime probes;
- `partial_revokes` global-only scalar diagnostics and fixed global no-op
  assignment for `DEFAULT`, `OFF`, `FALSE`, or `0`.

MyLite intentionally does not support:

- actual optimizer plan selection, optimizer trace row production, parser memory
  budgeting, partial-revoke privilege behavior, or persisted variable state;
- mutable shared server-global state;
- full MySQL optimizer switch grammar beyond the documented flag/value forms;
- `SET_VAR` hints, privilege checks, startup options, or Performance Schema
  variable tables.

Global assignments are accepted only for `DEFAULT` or the fixed default value.
State-changing global values return MyLite's deterministic fixed-placeholder
unsupported diagnostic.

## Parser And Runtime Design

No new grammar is required. Existing system-variable expression, `SHOW
VARIABLES`, and `SET` syntax admits the supported forms:

```lemon
scalar_expression ::= system_variable_reference.
set_statement ::= SET set_assignment_list.
set_assignment ::= system_variable_target EQ set_value.
show_statement ::= SHOW show_scope_opt VARIABLES show_filter_opt.
```

The implementation adds descriptors, defaults, scope wiring, an O optimizer
system-variable SET module, string flag normalization, bounded numeric
assignment through the existing session override store, and fixed global no-op
validation.

This is pure MyLite runtime logic. It does not require SQLite extension APIs,
SQLite fork hooks, file-format changes, catalog storage, or mutable
process-global state.

## Tests

- `packages/libmylite/tests/mysql_baseline_o_optimizer_system_variables_expectations.sh`
  verifies defaults, `SHOW` rows, scope diagnostics, direct and user-variable
  assignments, canonical set-valued output, clamping warnings, invalid-value
  diagnostics, and fixed global behavior against MySQL 8.4.9.
- `packages/libmylite/tests/runtime_o_optimizer_system_variables_test.c`
  verifies MyLite readback, `SHOW` rows, session assignment, user-variable
  paths, global no-op behavior, diagnostics, warnings, and rollback-sensitive
  session state.
