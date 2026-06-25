# Baseline SHOW VARIABLES

## Summary

This phase adds a deliberately small `SHOW VARIABLES` introspection surface over
the fixed system variables MyLite already exposes through scalar `@@` reads and
the fixed no-op `SET` slice:

```sql
SHOW [GLOBAL | SESSION | LOCAL] VARIABLES [LIKE 'pattern']
```

The result is MyLite-owned metadata, not a pass-through to SQLite. It exposes
only the system variables already represented in the current runtime registry,
using MySQL-style `SHOW VARIABLES` row labels and display values. It does not
add mutable session state, server-wide process state, `SHOW STATUS`,
`performance_schema`, `INFORMATION_SCHEMA`, `WHERE` filters, privileges, or a
complete MySQL variable catalog.

## Compatibility Authority

- Official MySQL 8.4 documentation:
  - `SHOW VARIABLES`: <https://dev.mysql.com/doc/refman/8.4/en/show-variables.html>
  - server system variables: <https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html>
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_show_variables_expectations.sh`.

Runtime probes against MySQL 8.4.9 establish these expectations for the
admitted subset:

- result columns are `Variable_name` and `Value`;
- rows are ordered by variable name in the supported subset;
- `SHOW VARIABLES`, `SHOW SESSION VARIABLES`, and `SHOW LOCAL VARIABLES`
  expose session-visible values;
- `SHOW GLOBAL VARIABLES` omits session-only variables such as
  `warning_count`, `error_count`, and `sql_log_bin`;
- `SHOW SESSION VARIABLES` and `SHOW LOCAL VARIABLES` still show fixed global
  variables that MySQL exposes through session-variable introspection;
- boolean-like variables render as `ON` or `OFF` in `SHOW VARIABLES`, even when
  scalar `@@` reads return `1` or `0`;
- `updatable_views_with_limit` renders as `YES`;
- `LIKE` filters are case-insensitive over variable names and use the same
  `%`, `_`, and backslash escape behavior already specified for baseline SHOW
  filters;
- successful `SHOW VARIABLES` sets statement warning count `0` and makes
  `ROW_COUNT()` return `-1`;
- `SHOW VARIABLES LIKE 'sql_slave_skip_counter'` does not emit the deprecation
  warning that scalar `SELECT @@sql_slave_skip_counter` emits;
- MySQL accepts `WHERE` filters, but MyLite defers them in this baseline.

## Ownership Boundaries

- Public API: no ABI or public-header change. `mylite_execute()` returns a
  normal row result through the existing result handle.
- Statement context: successful `SHOW VARIABLES` is a result-producing
  statement. It reports affected rows `0`, statement warning count `0`, and
  updates the previous row count to `-1`.
- Lexer/parser/AST: the parser adds one `SHOW VARIABLES` statement node with
  an optional scope token and the existing string-only `LIKE` clause node. It
  does not add `WHERE`, `LIMIT`, `FROM`, `FULL`, or arbitrary expressions.
- Runtime/analyzer: runtime resolves the optional scope against a MyLite-owned
  enum and iterates a static variable catalog that maps names, variable kinds,
  scalar values, `SHOW` display values, and scope visibility.
- Catalog: not involved. System variables are runtime session/server metadata,
  not MyLite catalog schemas or table descriptors.
- Result builder: result rows are built directly with the existing result API.
- Storage/VFS/file format: no file reads beyond normal handle state, no writes,
  and no `.mylite` preamble change.
- SQLite physical storage: no generated SQLite SQL and no SQLite fork patch.

## Syntax

The independent MyLite subset is:

```ebnf
show_variables_statement:
    SHOW show_variables_scope_opt VARIABLES show_like_clause_opt

show_variables_scope_opt:
    empty
  | GLOBAL
  | SESSION
  | LOCAL

show_like_clause_opt:
    empty
  | LIKE string_literal
```

The grammar intentionally excludes `WHERE`, `LIMIT`, `FULL`, schema qualifiers,
user variables, parameters, non-string `LIKE` operands, and expression filters.
`LOCAL` is accepted as a session-scope synonym because MySQL 8.4.9 accepts it
for this statement. `VARIABLES`, `GLOBAL`, `SESSION`, and `LOCAL` remain
nonreserved identifiers outside this statement.

### MyLite Lemon-Syntax Snippet

```lemon
statement(A) ::= show_variables_statement(B). {
    A = B;
}

show_variables_statement(A) ::=
    SHOW(S) show_variables_scope_opt(O) VARIABLES(V) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_variables_statement(state, S, O, V, L);
}

show_variables_scope_opt(A) ::= . {
    A = NULL;
}
show_variables_scope_opt(A) ::= GLOBAL(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
show_variables_scope_opt(A) ::= SESSION(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
show_variables_scope_opt(A) ::= LOCAL(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
```

These snippets are independently authored for MyLite's admitted subset and are
not MySQL's full grammar.

## Variable Catalog

This phase exposes only the runtime variables already supported by MyLite.
The `SHOW` result uses MySQL display strings where those differ from scalar
`@@` values:

| Variable | Default/session/LOCAL visibility | GLOBAL visibility | SHOW value |
| --- | --- | --- | --- |
| `autocommit` | yes | yes | `ON` |
| `character_set_client` | yes | yes | current fixed session value |
| `character_set_connection` | yes | yes | current fixed session value |
| `character_set_database` | yes | yes | `utf8mb4` |
| `character_set_filesystem` | yes | yes | `binary` |
| `character_set_results` | yes | yes | current fixed session value |
| `character_set_server` | yes | yes | `utf8mb4` |
| `character_set_system` | yes | yes | `utf8mb3` |
| `collation_connection` | yes | yes | current fixed session value |
| `collation_database` | yes | yes | `utf8mb4_0900_ai_ci` |
| `collation_server` | yes | yes | `utf8mb4_0900_ai_ci` |
| `default_collation_for_utf8mb4` | yes | yes | `utf8mb4_0900_ai_ci` |
| `default_storage_engine` | yes | yes | `InnoDB` |
| `default_tmp_storage_engine` | yes | yes | `InnoDB` |
| `end_markers_in_json` | yes | yes | `OFF` |
| `error_count` | yes | no | `0` |
| `foreign_key_checks` | yes | yes | `ON` |
| `interactive_timeout` | yes | yes | current session value or fixed global `28800` |
| `keep_files_on_create` | yes | yes | `OFF` |
| `lower_case_file_system` | yes | yes | `OFF` |
| `lower_case_table_names` | yes | yes | `0` |
| `max_allowed_packet` | yes | yes | `67108864` |
| `old_alter_table` | yes | yes | `OFF` |
| `print_identified_with_as_hex` | yes | yes | `OFF` |
| `require_row_format` | yes | no | `OFF` |
| `resultset_metadata` | yes | no | `FULL` |
| `select_into_disk_sync` | yes | yes | `OFF` |
| `session_track_gtids` | yes | yes | `OFF` |
| `session_track_schema` | yes | yes | `ON` |
| `session_track_state_change` | yes | yes | `OFF` |
| `session_track_transaction_info` | yes | yes | `OFF` |
| `show_create_table_skip_secondary_engine` | yes | no | `OFF` |
| `show_create_table_verbosity` | yes | yes | `OFF` |
| `sql_auto_is_null` | yes | yes | `OFF` |
| `sql_big_selects` | yes | yes | `ON` |
| `sql_buffer_result` | yes | yes | `OFF` |
| `sql_generate_invisible_primary_key` | yes | yes | `OFF` |
| `sql_log_bin` | yes | no | `ON` |
| `sql_log_off` | yes | yes | `OFF` |
| `sql_mode` | yes | yes | MySQL 8.4 default SQL mode string |
| `sql_notes` | yes | yes | `ON` |
| `sql_quote_show_create` | yes | yes | `ON` |
| `sql_replica_skip_counter` | yes | yes | `0` |
| `sql_require_primary_key` | yes | yes | `OFF` |
| `sql_safe_updates` | yes | yes | `OFF` |
| `sql_select_limit` | yes | yes | `18446744073709551615` |
| `sql_slave_skip_counter` | yes | yes | `0` |
| `sql_warnings` | yes | yes | `OFF` |
| `transaction_isolation` | yes | yes | current session value or fixed global `REPEATABLE-READ` |
| `transaction_read_only` | yes | yes | current session value as `ON`/`OFF` or fixed global `OFF` |
| `unique_checks` | yes | yes | `ON` |
| `updatable_views_with_limit` | yes | yes | `YES` |
| `use_secondary_engine` | yes | no | `ON` |
| `version` | yes | yes | `mylite_version()` |
| `version_comment` | yes | yes | `MyLite` |
| `wait_timeout` | yes | yes | current session value or fixed global `28800` |
| `warning_count` | yes | no | `0` |

`warning_count` and `error_count` are fixed to `0` inside `SHOW VARIABLES`
results because MySQL reports the current `SHOW` statement's clean diagnostics
state, not the previous statement's diagnostics snapshot used by scalar
`@@warning_count` and `@@error_count`.

Rows are emitted in bytewise lowercase variable-name order for the supported
catalog. MyLite does not claim the complete MySQL catalog order because it
does not expose variables outside this baseline registry.

## Scope Semantics

No explicit scope, `SESSION`, and `LOCAL` use session-visible semantics. A row
is visible if MyLite can expose it as either a fixed session variable or a
fixed global variable through MySQL-style session introspection. This matches
the observed MySQL behavior that `SHOW SESSION VARIABLES` includes global
variables that have no distinct session value.

`GLOBAL` includes only rows that are visible in MySQL's global variable
introspection. Session-only variables such as `warning_count`, `error_count`,
and `sql_log_bin` are omitted rather than raising diagnostics.

Unsupported scope syntax is rejected by the parser. `SHOW FULL VARIABLES`,
`SHOW VARIABLES FROM schema`, and `SHOW VARIABLES LIMIT n` are syntax errors
for this baseline.

## LIKE Semantics

The optional `LIKE` operand must be a regular string literal, reusing the
existing MyLite string-literal token accepted by other `SHOW ... LIKE` forms.
The pattern is matched against `Variable_name` only. Matching is ASCII
case-insensitive, `%` matches any byte sequence, `_` matches one byte, and
backslash escapes the following pattern byte, following the existing baseline
SHOW filter implementation.

Unsupported forms such as numeric `LIKE`, `LIKE NULL`, national-string
literals, introducer literals, `WHERE`, and expression predicates are deferred.

## Result Semantics

Successful statements return a row result with columns:

```text
Variable_name
Value
```

Rows contain text values only. Successful statements report:

- affected rows `0`;
- warning count `0`;
- no stored warnings for the supported in-range subset;
- `ROW_COUNT()` as `-1` for the following statement.

The statement does not change fixed system-variable state, session character
set state, diagnostics state beyond normal statement-boundary cleanup, catalog
state, SQLite schema generation, or storage contents.

## Diagnostics

- Syntax errors for unsupported grammar use the existing parser diagnostic
  policy, currently `1064` / `42000`.
- Non-string `LIKE` operands remain parse-time syntax errors because the
  admitted grammar only accepts string literals.
- Allocation failures use the existing `MYLITE_NOMEM` / out-of-memory
  diagnostic policy.
- Internal result-builder failures set an allocation or runtime diagnostic
  before returning failure.
- Physical SQLite failures are not expected because this statement generates no
  SQLite SQL. Any unexpected SQLite involvement is a bug in this phase.

## Performance

`SHOW VARIABLES` is intentionally O(number of supported variables) over a small
static table. It does not query SQLite metadata, materialize table rows, or
scan catalog descriptors. Pattern filtering happens while appending rows so
there is no extra intermediate result set.

## Tests

The feature is covered by:

- parser tests for default, `GLOBAL`, `SESSION`, `LOCAL`, `LIKE`, and
  nonreserved identifier behavior;
- a fast C runtime test for values, scope filtering, `LIKE` filtering, result
  columns, row count/warning semantics, diagnostics clearing, persistence/no
  file mutation, and independent handles;
- MySQL 8.4.9 expectation script
  `packages/libmylite/tests/mysql_baseline_show_variables_expectations.sh`;
- existing SHOW-like-filter, scalar system-variable, fixed-SET, diagnostics,
  parser, runtime-handle, file-format, VFS, and full check workflows.

## Deferred

Full variable catalog coverage, `SHOW STATUS`, `WHERE` filters,
`performance_schema` / `INFORMATION_SCHEMA` exposure, privilege-dependent
visibility, mutable session/global state, persisted variables, plugin-provided
variables, complete metadata, and protocol-specific column flags remain
unsupported.
