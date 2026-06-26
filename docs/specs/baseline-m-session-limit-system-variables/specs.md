# Baseline M Session Limit System Variables

## Scope

This slice exposes MySQL 8.4.9-shaped readback, `SHOW VARIABLES` rows, and
embedded session placeholder assignment for twelve M-range limit variables:

- `max_delayed_threads`
- `max_execution_time`
- `max_heap_table_size`
- `max_insert_delayed_threads`
- `max_join_size`
- `max_length_for_sort_data`
- `max_points_in_geometry`
- `max_seeks_for_key`
- `max_sort_length`
- `max_sp_recursion_depth`
- `max_user_connections`
- `min_examined_row_limit`

The official MySQL 8.4 server-system-variable manual describes these as server
or session tuning variables. MyLite records MySQL-shaped values and
diagnostics, but does not implement query timeout enforcement, MEMORY temporary
table sizing, optimizer row-examination aborts, delayed insert behavior,
geometry point limits, stored-program recursion effects, connection admission,
or slow-query logging filters.

References:

- <https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html>
- <https://dev.mysql.com/doc/refman/8.4/en/show-variables.html>

## Observed MySQL 8.4.9 Behavior

Runtime probes against `mysql:8.4.9` showed these defaults and ranges:

| Variable | Default | Min | Max | Notes |
| --- | --- | --- | --- | --- |
| `max_delayed_threads` | `20` | `0` | `16384` | Deprecated; accepts only `DEFAULT` or `0` in the tested runtime |
| `max_execution_time` | `0` | `0` | `18446744073709551615` | Session/global numeric |
| `max_heap_table_size` | `16777216` | `16384` | `18446744073709550592` | Values clamp silently to range |
| `max_insert_delayed_threads` | `20` | `0` | `16384` | Deprecated; accepts only `DEFAULT` or `0` in the tested runtime |
| `max_join_size` | `18446744073709551615` | `1` | `18446744073709551615` | Setting a non-default session value sets `@@sql_big_selects` to `0` |
| `max_length_for_sort_data` | `4096` | `4` | `8388608` | Deprecated; values clamp silently to range |
| `max_points_in_geometry` | `65536` | `3` | `1048576` | Values clamp silently to range |
| `max_seeks_for_key` | `18446744073709551615` | `1` | `18446744073709551615` | Values clamp silently to range |
| `max_sort_length` | `1024` | `4` | `8388608` | Values clamp silently to range |
| `max_sp_recursion_depth` | `0` | `0` | `255` | Values clamp silently to range |
| `max_user_connections` | `0` | `0` | `4294967295` | Session reads are allowed; session assignment is read-only |
| `min_examined_row_limit` | `0` | `0` | `18446744073709551615` | Session/global numeric |

All variables appear in default, global, and session `SHOW VARIABLES` output.
Default, global, session, and local scalar reads return the current value.

Direct session assignment accepts integer literals and `TRUE` / `FALSE` for the
numeric variables. Out-of-range values clamp silently to the observed range.
Non-numeric strings, `NULL`, `ON`, and values outside unsigned 64-bit parsing
return `1232 / 42000`.

`max_delayed_threads` and `max_insert_delayed_threads` are deprecated aliases in
the target runtime. Scalar reads and successful assignments append warning
`1287 / HY000`. Non-zero assignments other than `DEFAULT` fail with
`1231 / 42000`.

`max_length_for_sort_data` also appends warning `1287 / HY000` on scalar reads
and successful assignments.

`max_user_connections` rejects non-global assignment with `1621 / HY000`,
`SESSION variable 'max_user_connections' is read-only. Use SET GLOBAL to assign
the value`.

## MyLite Semantics

MyLite supports:

- scalar reads for default, `GLOBAL`, `SESSION`, and `LOCAL` scopes;
- `SHOW VARIABLES`, `SHOW GLOBAL VARIABLES`, and `SHOW SESSION VARIABLES` rows;
- handle-local session placeholder assignment for the mutable session variables;
- MySQL-shaped numeric parsing, clamping, and diagnostics for the tested direct
  and integer user-variable paths;
- `max_join_size` readback plus the MySQL-observed session interaction that
  makes `@@sql_big_selects` `0` for non-default values and `1` for default;
- `max_user_connections` fixed session readback, session read-only diagnostics,
  and exact/default global no-op assignments;
- deprecation warning `1287 / HY000` for scalar reads and successful
  assignments of the three deprecated variables.

MyLite intentionally does not support:

- mutable shared server-global state;
- actual optimizer, timeout, delayed insert, connection admission,
  stored-program, geometry, or slow-log side effects;
- persisted variables, startup options, privilege checks, `SET_VAR`, or
  Performance Schema variable tables.

Global assignments are accepted only for `DEFAULT` or the fixed default value.
State-changing global values return MyLite's deterministic fixed-placeholder
unsupported diagnostic.

## Parser And Runtime Design

No new grammar is required. Existing scalar system-variable, `SHOW VARIABLES`,
and `SET` syntax admits the supported forms:

```lemon
scalar_expression ::= system_variable_reference.
set_statement ::= SET set_assignment_list.
set_assignment ::= system_variable_target EQ set_value.
show_statement ::= SHOW show_scope_opt VARIABLES show_filter_opt.
```

The implementation adds descriptors, defaults, scope wiring, bounded numeric
session assignment through the existing session override store, fixed global
no-op validation, deprecation warning registration, and a special
`max_join_size` side effect on the session `sql_big_selects` placeholder.

This is pure MyLite runtime logic. It does not require SQLite extension APIs,
SQLite fork hooks, file-format changes, catalog storage, or mutable
process-global state.

## Tests

- `packages/libmylite/tests/mysql_baseline_m_session_limit_system_variables_expectations.sh`
  verifies defaults, `SHOW` rows, scope, direct and user-variable assignments,
  clamping, deprecation warnings, `max_join_size` / `sql_big_selects`, and
  diagnostics against MySQL 8.4.9.
- `packages/libmylite/tests/runtime_m_session_limit_system_variables_test.c`
  verifies MyLite readback, `SHOW` rows, session assignment, user-variable
  paths, global no-op behavior, diagnostics, warnings, and rollback-sensitive
  session state.
