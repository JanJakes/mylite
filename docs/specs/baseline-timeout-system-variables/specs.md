# Baseline Timeout System Variables

## Summary

This phase adds a narrow MyLite compatibility slice for MySQL's
`wait_timeout` and `interactive_timeout` system variables.

The variables are exposed as connection-local session values with fixed global
defaults. MyLite supports scalar `@@` reads, `SHOW VARIABLES` rows, and the
common `SET` session forms with MySQL 8.4.9-compatible integer conversion and
clamp warnings for the admitted value subset. The values are metadata only in
this slice: they do not close idle embedded handles, affect protocol
handshakes, or create mutable process-wide global state.

## Compatibility Authority

- MySQL 8.4 Reference Manual, server system variables:
  <https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html>
- MySQL 8.4 Reference Manual, using system variables:
  <https://dev.mysql.com/doc/refman/8.4/en/using-system-variables.html>
- MySQL 8.4 Reference Manual, `SHOW VARIABLES`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-variables.html>
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_timeout_system_variables_expectations.sh`.

Runtime probes against MySQL 8.4.9 showed:

- both variables default to decimal value `28800`;
- scalar no-scope, `GLOBAL`, `SESSION`, and `LOCAL` reads are accepted;
- `SHOW VARIABLES`, `SHOW SESSION VARIABLES`, `SHOW LOCAL VARIABLES`, and
  `SHOW GLOBAL VARIABLES` expose both rows;
- unqualified, `SESSION`, `LOCAL`, direct `@@name`, `@@SESSION.name`, and
  `@@LOCAL.name` assignments update the current session value;
- `DEFAULT` restores the session value to `28800`;
- integer values in `1..31536000`, including unary plus forms, succeed without
  warnings;
- `TRUE` stores `1` without a warning;
- `FALSE`, `0`, and negative integer values store `1` with warning
  `1292 / HY000`, `Truncated incorrect <variable> value: '<input>'`;
- integer values greater than `31536000` store `31536000` with the same
  warning form;
- string, decimal, `NULL`, `ON`, and `OFF` assignment values fail with
  `1232 / 42000`, `Incorrect argument type to variable '<name>'`;
- integer user variables can be assigned to the timeout variables, while string
  and `NULL` user-variable values are rejected with the same `1232` diagnostic;
- global assignment is mutable in MySQL, but MyLite defers mutable global
  system-variable state for this baseline.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## Scope

This feature adds:

- per-handle session fields for `wait_timeout` and `interactive_timeout`,
  initialized to `28800`;
- scalar reads for `@@wait_timeout`, `@@interactive_timeout`, and their
  `GLOBAL`, `SESSION`, and `LOCAL` qualified forms;
- fixed global scalar readback exposing `28800`;
- `SHOW VARIABLES`, `SHOW SESSION VARIABLES`, `SHOW LOCAL VARIABLES`, and
  `SHOW GLOBAL VARIABLES` rows for both variables;
- `SHOW VARIABLES LIKE` and existing limited `SHOW VARIABLES WHERE` filtering
  over the new rows;
- session/local/unqualified `SET` assignment for both variables through
  identifier targets and direct `@@` targets;
- `DEFAULT`, decimal integer, unary-plus integer, unary-minus integer, `TRUE`,
  and `FALSE` assignment values with MySQL-compatible storage and warnings for
  the admitted subset;
- integer user-variable assignment values for supported `SET` forms;
- exact fixed no-op `SET GLOBAL` / `SET @@GLOBAL` assignments to `DEFAULT`,
  `28800`, or `+28800` that preserve MyLite's fixed global value;
- deterministic MyLite-specific rejection for global assignments that would
  change the fixed value;
- independent handle state and non-persistence across close/reopen;
- normal successful non-query result conventions: no result rows, affected
  rows `0`, warning count as produced by assignment conversion.

## Non-Goals

This feature does not implement:

- idle timeout enforcement, connection termination, network protocol behavior,
  client interactive/noninteractive bootstrap differences, or handshake status;
- mutable process-wide global timeout values, privilege checks, startup
  options, option files, persisted variables, `SET PERSIST`, `SET_VAR` hints,
  or Performance Schema variable tables;
- expression-valued assignments beyond the existing admitted literal and
  user-variable forms;
- string, decimal, float, hex, bit, `NULL`, `ON`, or `OFF` successful
  assignment conversion;
- suffix units, arithmetic, function calls, subqueries, parameters, or
  prepared-statement-specific assignment behavior beyond current MyLite
  prepared execution expansion;
- SQLite fork patches.

## Ownership Boundary

- Public API remains unchanged. `mylite_execute()` owns public call
  validation, result ownership, diagnostics, and statement-boundary cleanup.
- Statement context owns successful non-query result conventions, affected rows
  `0`, warning count, previous diagnostics, and rollback of multi-assignment
  `SET` statements on failure.
- Session state owns the two mutable timeout values per `mylite_db` handle. A
  new handle starts from fixed defaults; no process-global mutable store is
  introduced.
- Lexer/parser/AST require no grammar changes. Existing system-variable
  scalar expressions, `SET` targets, `SET` values, user variables, and `SHOW
  VARIABLES` statements already represent the admitted SQL shapes.
- Runtime resolves variable names through the system-variable registry,
  validates scope, performs MyLite-owned integer conversion, appends warnings,
  mutates session state, and renders scalar/`SHOW` values.
- Catalog descriptors remain authoritative for schema/table metadata and are
  not touched by this runtime-only feature.
- Result builders materialize scalar and `SHOW VARIABLES` rows from MyLite
  state.
- Storage, VFS, file format, and SQLite physical row storage are not involved.
  Supported `SET` statements do not write the `.mylite` preamble, catalog
  rows, user rows, or SQLite schema.

## Supported SQL Surface

The feature uses the existing MyLite `SET`, scalar-expression, and
`SHOW VARIABLES` grammar:

```ebnf
system_variable_expr:
    @@ timeout_variable
  | @@ GLOBAL . timeout_variable
  | @@ SESSION . timeout_variable
  | @@ LOCAL . timeout_variable

timeout_variable:
    wait_timeout
  | interactive_timeout

set_system_variable_statement:
    SET timeout_variable = set_timeout_value
  | SET SESSION timeout_variable = set_timeout_value
  | SET LOCAL timeout_variable = set_timeout_value
  | SET GLOBAL timeout_variable = set_timeout_value
  | SET @@ timeout_variable = set_timeout_value
  | SET @@ GLOBAL . timeout_variable = set_timeout_value
  | SET @@ SESSION . timeout_variable = set_timeout_value
  | SET @@ LOCAL . timeout_variable = set_timeout_value

set_timeout_value:
    DEFAULT
  | unsigned_decimal_integer_literal
  | + unsigned_decimal_integer_literal
  | - unsigned_decimal_integer_literal
  | TRUE
  | FALSE
  | user_variable
```

Only `DEFAULT`, integer literals with optional unary sign, booleans, and
integer-valued user variables can succeed for session/local/unqualified
assignments. Other parser-admitted values fail with the diagnostics below.

### MyLite Lemon-Syntax Snippet

No Lemon grammar expansion is required. The existing productions remain
sufficient:

```lemon
expr(A) ::= SYSTEM_VARIABLE(T). {
    A = mylite_sql_parser_make_system_variable_expr(state, T);
}

set_statement(A) ::= SET(S) set_assignment_list(L). {
    A = mylite_sql_parser_make_set_statement(state, S, L);
}

set_assignment(A) ::=
    set_system_variable_target(T) set_assignment_operator(O)
    set_system_variable_value(V). {
    A = mylite_sql_parser_make_set_assignment(state, T, O, V);
}

show_variables_statement(A) ::=
    SHOW(S) show_variables_scope_opt(O) VARIABLES(V) show_like_or_where_opt(F). {
    A = mylite_sql_parser_make_show_variables_statement(state, S, O, V, F);
}
```

These snippets are independently authored for MyLite's admitted subset and are
not MySQL's full grammar.

## Semantics

Both variables use the same conversion rules.

```text
Default session value: 28800
Fixed global value:    28800
Minimum stored value:  1
Maximum stored value:  31536000
```

No-scope, `SESSION`, and `LOCAL` reads return the handle-local session value.
`GLOBAL` reads return the fixed default. `SHOW VARIABLES`, `SHOW SESSION
VARIABLES`, and `SHOW LOCAL VARIABLES` display the session values. `SHOW
GLOBAL VARIABLES` displays the fixed defaults.

Successful session/local/unqualified assignments set only the current handle's
session value. The values are not written to disk and are reset to `28800` for
new handles and after close/reopen.

Assignment conversion:

- `DEFAULT` stores `28800`;
- integer literals in `1..31536000` store that value;
- unary plus is accepted for integer literals;
- `TRUE` stores `1`;
- `FALSE`, `0`, and negative integer literals store `1` and append warning
  `1292`;
- integer literals above `31536000` store `31536000` and append warning
  `1292`;
- integer-valued user variables follow the same range and warning rules;
- string, decimal, `NULL`, `ON`, `OFF`, and other unsupported values fail with
  `1232 / 42000`.

Global assignment is intentionally limited. `SET GLOBAL timeout_variable =
DEFAULT`, `SET GLOBAL timeout_variable = 28800`, `SET GLOBAL timeout_variable =
+28800`, and equivalent `@@GLOBAL` forms are accepted as no-ops. Any other
global assignment fails with a MyLite unsupported-assignment diagnostic because
this slice has no mutable global system-variable store.

Multi-assignment `SET` remains atomic for session state. If a later assignment
fails, earlier timeout mutations and user-variable mutations in the same
statement are rolled back through the existing session snapshot mechanism.

## Diagnostics

| Condition | Diagnostic |
| --- | --- |
| Unknown variable spelling outside the descriptor registry | Existing unknown-system-variable diagnostic |
| Unsupported quoted scope such as ``@@`global`.wait_timeout`` | Existing unsupported quoted system-variable scope diagnostic |
| Successful session/local/no-scope assignment | Success, affected rows `0`; warning count is `0` or clamp warnings |
| `DEFAULT` assignment | Success, stores `28800`, warning count `0` |
| `FALSE`, `0`, or negative integer assignment | Success, stores `1`, warning `1292 / HY000`, `Truncated incorrect <variable> value: '<input>'` |
| Integer above `31536000` | Success, stores `31536000`, same warning form |
| String, decimal, `NULL`, `ON`, `OFF`, function, arithmetic, parameter, or other unsupported assignment value | `1232 / 42000`, `Incorrect argument type to variable '<name>'`, when represented by current grammar; otherwise existing syntax/unsupported diagnostic |
| Integer-valued user variable | Same conversion as an integer literal |
| String or `NULL` user variable | `1232 / 42000`, `Incorrect argument type to variable '<name>'` |
| Exact no-op global assignment to `DEFAULT`, `28800`, or `+28800` | Success, affected rows `0`, warning count `0` |
| Value-changing global assignment | Existing unsupported-statement diagnostic with timeout no-op message |
| Unsupported `SHOW VARIABLES` syntax | Existing parser diagnostics |
| Allocation failure while building results | Existing MyLite allocation failure diagnostics |
| Public API misuse | Existing public API misuse behavior |

## Performance And Storage Notes

The implementation stays on the existing optimal path for this surface:

- scalar reads format a small integer from session state;
- `SHOW VARIABLES` adds two rows to the existing descriptor-driven row stream;
- `SET` performs a bounded literal/user-variable conversion and updates two
  `uint64_t` fields;
- no SQLite statement is generated for supported reads or writes except the
  existing result materialization path;
- no catalog, descriptor cache, file preamble, VFS, or SQLite schema generation
  mutation occurs.

## Tests

Fast C tests cover:

- scalar reads for no-scope, `GLOBAL`, `SESSION`, and `LOCAL` forms;
- case-insensitive variable and scope spelling, plus quoted final variable
  names already admitted by existing parsing;
- `SHOW VARIABLES`, `SHOW SESSION VARIABLES`, `SHOW LOCAL VARIABLES`, `SHOW
  GLOBAL VARIABLES`, `LIKE`, and limited `WHERE` filtering;
- session/local/no-scope/direct-`@@` assignment forms;
- `DEFAULT`, in-range integer, unary plus, `TRUE`, `FALSE`, `0`, negative, and
  too-large assignment values;
- warning text for clamp cases;
- `1232` diagnostics for unsupported string, decimal, `NULL`, `ON`, and `OFF`
  values;
- integer, negative integer, string, and `NULL` user-variable assignment
  behavior;
- multi-assignment rollback;
- independent handles, close/reopen reset, catalog generation preservation,
  SQLite schema generation preservation, and `.mylite` preamble preservation;
- `SHOW VARIABLES` row-count expectation updates.

The MySQL expectation script verifies the same user-visible behavior against
MySQL 8.4.9 before implementation.

## Compatibility Notes

MyLite deliberately does not enforce idle timeouts in this phase. For embedded
handles, actually closing a connection because a metadata variable elapsed
would be a broader runtime/host integration feature and should be specified
with protocol and API behavior when needed.

Mutable global system variables are also deferred. The fixed global no-op
assignment policy is consistent with other MyLite baseline variables that have
global scope in MySQL but no process-wide mutable store in this embedded
runtime yet.
