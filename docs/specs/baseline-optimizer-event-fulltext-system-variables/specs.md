# Baseline Optimizer, Event, And Full-Text System Variables

## Summary

This slice exposes MySQL 8.4.9-shaped metadata placeholders for thirteen
runtime system variables:

- `enforce_gtid_consistency`
- `eq_range_index_dive_limit`
- `event_scheduler`
- `explain_format`
- `explain_json_format_version`
- `external_user`
- `flush`
- `flush_time`
- `ft_boolean_syntax`
- `ft_max_word_len`
- `ft_min_word_len`
- `ft_query_expansion_limit`
- `ft_stopword_file`

MyLite supports scalar reads, `SHOW VARIABLES` rows, scope diagnostics,
read-only diagnostics, and exact/default no-op `SET` forms where MySQL treats
the variable as dynamic. It does not implement GTID consistency enforcement,
optimizer plan changes, event scheduling, `EXPLAIN` output selection, external
authentication state, table flushing, or MyISAM full-text behavior.

## Compatibility Authority

- MySQL 8.4 Reference Manual, server system variables:
  <https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html>
- MySQL 8.4 Reference Manual, GTID system variables:
  <https://dev.mysql.com/doc/refman/8.4/en/replication-options-gtids.html>
- MySQL 8.4 Reference Manual, `SHOW VARIABLES`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-variables.html>
- MySQL 8.4.9 runtime observations captured by
  `packages/libmylite/tests/mysql_baseline_compatibility_system_variables_expectations.sh`.

The manual establishes the variable names, scope, mutability, and feature
families. Runtime probes establish pinned 8.4.9 defaults, `SHOW` display
values, assignment diagnostics, and upstream mutable behavior.

## MySQL 8.4.9 Observations

| Variable | Scalar value | `SHOW VARIABLES` value | Scalar scope |
| --- | --- | --- | --- |
| `enforce_gtid_consistency` | `OFF` | `OFF` | global |
| `eq_range_index_dive_limit` | `200` | `200` | global/session |
| `event_scheduler` | `ON` | `ON` | global |
| `explain_format` | `TRADITIONAL` | `TRADITIONAL` | global/session |
| `explain_json_format_version` | `1` | `1` | global/session |
| `external_user` | `NULL` | empty string | session |
| `flush` | `0` | `OFF` | global |
| `flush_time` | `0` | `0` | global |
| `ft_boolean_syntax` | `+ -><()~*:""&|` | `+ -><()~*:""&|` | global |
| `ft_max_word_len` | `84` | `84` | global |
| `ft_min_word_len` | `4` | `4` | global |
| `ft_query_expansion_limit` | `20` | `20` | global |
| `ft_stopword_file` | `(built-in)` | `(built-in)` | global |

All variables except `external_user` appear in `SHOW VARIABLES`,
`SHOW GLOBAL VARIABLES`, and `SHOW SESSION VARIABLES`. `external_user` appears
only in the default/session `SHOW VARIABLES` surface, returns SQL `NULL` for
scalar session reads, and rejects global scalar reads with the MySQL session-only
diagnostic. Global-only variables reject explicit `@@SESSION.name` reads with
`1238 / HY000`. `external_user`, `ft_max_word_len`, `ft_min_word_len`,
`ft_query_expansion_limit`, and `ft_stopword_file` reject `SET` with read-only
diagnostics. Non-global `SET` forms for dynamic global-only variables return
`1229 / HY000`.

MySQL can mutate the dynamic variables in this batch. MyLite intentionally
accepts only exact current/default assignments as no-ops until the underlying
feature effects are implemented.

## MyLite Scope

MyLite supports:

- unscoped and `GLOBAL` scalar reads for the global variables;
- unscoped and `SESSION` / `LOCAL` scalar reads for the session-scoped
  variables;
- SQL `NULL` scalar readback and blank `SHOW VARIABLES` value for
  `external_user`;
- MySQL-style scalar scope diagnostics for global-only and session-only
  variables;
- `SHOW VARIABLES`, `SHOW GLOBAL VARIABLES`, and `SHOW SESSION VARIABLES`
  rows with MySQL-shaped display values;
- read-only diagnostics for `external_user` and read-only full-text variables;
- exact/default no-op assignment forms for the dynamic variables;
- deterministic unsupported diagnostics for state-changing assignments and
  user-variable-backed assignments.

MyLite intentionally does not support:

- mutable server-global or session state for this batch;
- GTID consistency enforcement, binary-log interaction, replication, or
  privilege effects;
- optimizer row-estimate, range-planning, or `EXPLAIN` output-format changes;
- event scheduler threads or event execution;
- external authentication state;
- `flush` / `flush_time` table, log, or cache flushing side effects;
- MyISAM full-text parser, token-length, query-expansion, or stopword-file
  behavior;
- startup options, persisted variables, privilege checks, or Performance
  Schema variable tables.

## Syntax

The existing system-variable, `SHOW VARIABLES`, and `SET` productions admit the
supported forms. MyLite also treats `FLUSH` as an identifier fallback in SET
system-variable target position so the MySQL variable name `flush` can be
assigned without quoting:

```sql
SELECT @@GLOBAL.event_scheduler, @@SESSION.explain_format;
SHOW VARIABLES LIKE 'ft\_%';
SET GLOBAL event_scheduler = DEFAULT;
SET SESSION eq_range_index_dive_limit = 200;
```

### MyLite Lemon-Syntax Snippet

```lemon
%fallback IDENTIFIER ... FLUSH.

set_system_variable_target(A) ::= identifier(N). {
    A = mylite_sql_parser_make_set_system_variable_target(state, NULL, N);
}
set_system_variable_target(A) ::= GLOBAL(G) identifier(N). {
    A = mylite_sql_parser_make_set_system_variable_target(state, G, N);
}
```

The snippets are independently authored for MyLite's admitted subset. The
fallback is intentionally narrow: it lets `flush` be parsed as a system-variable
identifier when the SET grammar expects an identifier, while existing `FLUSH`
administrative statements still parse through their own statement productions.

## Diagnostics

- Unknown variables continue to use `1193 / HY000`.
- `@@SESSION` / `@@LOCAL` reads of global-only variables use
  `1238 / HY000`, `Variable '<name>' is a GLOBAL variable`.
- `@@GLOBAL.external_user` uses `1238 / HY000`,
  `Variable 'external_user' is a SESSION variable`.
- Non-global `SET` forms for dynamic global-only variables use
  `1229 / HY000`, `Variable '<name>' is a GLOBAL variable and should be set
  with SET GLOBAL`.
- Read-only variables use `1238 / HY000`,
  `Variable '<name>' is a read only variable`.
- State-changing values and user-variable-backed assignments use MyLite's
  deterministic unsupported fixed-no-op diagnostics.

## Runtime And Storage

This slice is implemented in MyLite's system-variable registry, scalar
readback, `SHOW VARIABLES` display path, and `SET` validation. It does not add
public ABI, catalog rows, SQLite SQL, SQLite extension API use, fork patches,
file-format state, VFS behavior, persistent state, or mutable process-global
state.

## Tests

Coverage includes:

- a MySQL 8.4.9 expectation script for defaults, `SHOW` rows, scope
  diagnostics, read-only diagnostics, no-op assignment forms, and upstream
  mutable behavior;
- a runtime C test for MyLite scalar values, `SHOW` rows, diagnostics, no-op
  fixed assignments, unsupported state-changing assignments, and user-variable
  assignment rejection;
- full `SHOW VARIABLES` registry regression coverage.
