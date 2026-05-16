# Baseline lower_case_file_system System Variable

## Summary

This phase exposes MySQL's `lower_case_file_system` system variable as a fixed
MyLite runtime capability probe.

MyLite reports `lower_case_file_system = 0` for scalar `@@` reads and `OFF` in
`SHOW VARIABLES`. This matches the current Linux MySQL 8.4.9 comparison
runtime and MyLite's case-sensitive catalog contract. The value is read-only
and does not drive filesystem probing, catalog name resolution, or physical
SQLite object naming.

## Compatibility Authority

- Official MySQL 8.4 documentation:
  - server system variables:
    <https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html>
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_lower_case_file_system_variable_expectations.sh`.

The MySQL 8.4 manual defines `lower_case_file_system` as a global, non-dynamic
boolean variable describing whether the server data-directory filesystem is
case-insensitive. MySQL runtime observations on the Linux MySQL 8.4.9
comparison container show scalar value `0`, `SHOW VARIABLES` value `OFF`,
global scalar reads accepted, session/local scalar reads rejected as a global
variable, `SHOW SESSION VARIABLES` still exposing the row, and all tested
`SET` forms rejected as read-only.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## Scope

This feature adds:

- scalar reads for `@@lower_case_file_system` and
  `@@GLOBAL.lower_case_file_system`, returning `0`;
- case-insensitive variable-name matching for scalar reads, including quoted
  variable name components already admitted by existing system-variable parsing;
- MySQL-compatible global-only scalar diagnostics for
  `@@SESSION.lower_case_file_system` and `@@LOCAL.lower_case_file_system`;
- `SHOW VARIABLES`, `SHOW SESSION VARIABLES`, `SHOW LOCAL VARIABLES`, and
  `SHOW GLOBAL VARIABLES` rows for `lower_case_file_system`, displaying `OFF`;
- `SHOW VARIABLES LIKE 'lower_case_file_system'` and existing limited
  `SHOW VARIABLES WHERE` filtering over the new row;
- MySQL-compatible read-only diagnostics for no-scope, session, local, global,
  and `@@`-targeted `SET lower_case_file_system = ...` forms admitted by the
  existing `SET` grammar.

## Non-Goals

This feature does not implement:

- host filesystem case probing, startup configuration, persisted variables,
  mutable global state, privilege checks, or Performance Schema variable
  tables;
- value `1`;
- changed schema/table lookup, alias comparison, catalog collation, physical
  table naming, or SQLite object naming;
- a relationship where `lower_case_file_system` changes
  `lower_case_table_names`;
- SQLite fork patches.

## Ownership Boundary

- Public API remains unchanged. `mylite_execute()` returns existing scalar or
  row-result handles and diagnostics.
- Statement context owns result classification, affected rows, previous
  diagnostics, warning count, and row-count side effects.
- Lexer/parser/AST require no new syntax. Existing system-variable scalar,
  `SET`, and `SHOW VARIABLES` nodes already represent the admitted SQL shapes.
- Runtime owns the static system-variable descriptor, scope rules, read-only
  diagnostics, scalar value formatting, and `SHOW VARIABLES` display value.
- Catalog descriptors remain authoritative for schema/table names. This
  variable is runtime metadata only.
- Result builder owns scalar and `SHOW VARIABLES` result materialization.
- Storage, VFS, file format, and SQLite physical storage are not involved.

## Syntax

No grammar expansion is needed. The feature uses the existing MyLite grammar
for scalar system-variable expressions, system-variable `SET`, and
`SHOW VARIABLES`.

The admitted SQL surface is:

```ebnf
system_variable_expr:
    @@ lower_case_file_system
  | @@ GLOBAL . lower_case_file_system
  | @@ SESSION . lower_case_file_system
  | @@ LOCAL . lower_case_file_system

set_system_variable_statement:
    SET lower_case_file_system = set_system_variable_value
  | SET SESSION lower_case_file_system = set_system_variable_value
  | SET LOCAL lower_case_file_system = set_system_variable_value
  | SET GLOBAL lower_case_file_system = set_system_variable_value
  | SET @@ lower_case_file_system = set_system_variable_value
  | SET @@ GLOBAL . lower_case_file_system = set_system_variable_value
  | SET @@ SESSION . lower_case_file_system = set_system_variable_value
  | SET @@ LOCAL . lower_case_file_system = set_system_variable_value

show_variables_statement:
    SHOW [GLOBAL | SESSION | LOCAL] VARIABLES [LIKE string_literal]
  | SHOW [GLOBAL | SESSION | LOCAL] VARIABLES WHERE show_variables_where_predicate
```

### MyLite Lemon-Syntax Snippet

The existing grammar remains sufficient:

```lemon
expr(A) ::= SYSTEM_VARIABLE(T). {
    A = mylite_sql_parser_make_system_variable_expr(state, T);
}

set_statement(A) ::=
    SET(S) set_system_variable_target(T) EQUALS(E) set_system_variable_value(V). {
    A = mylite_sql_parser_make_set_system_variable_statement(state, S, T, E, V);
}

show_variables_statement(A) ::=
    SHOW(S) show_variables_scope_opt(O) VARIABLES(V) show_like_or_where_opt(F). {
    A = mylite_sql_parser_make_show_variables_statement(state, S, O, V, F);
}
```

These snippets are independently authored for MyLite's admitted subset and are
not MySQL's full grammar.

## Semantics

`lower_case_file_system` is fixed:

```text
Scalar @@ read: 0
SHOW VARIABLES value: OFF
```

Unscoped scalar reads use MySQL's effective global value and return `0`.
`GLOBAL` scalar reads return `0`. `SESSION` and `LOCAL` scalar reads fail with
`1238 / HY000` and a message containing
`Variable 'lower_case_file_system' is a GLOBAL variable`.

`SHOW VARIABLES`, `SHOW SESSION VARIABLES`, and `SHOW LOCAL VARIABLES` include
the row because MySQL's session variable introspection shows global-only rows
that have no distinct session value. `SHOW GLOBAL VARIABLES` also includes the
row. `SHOW VARIABLES WHERE` evaluates predicates over the row using existing
`Variable_name` and `Value` text semantics.

All `SET lower_case_file_system` forms admitted by the existing grammar fail
before mutation with `1238 / HY000` and a message containing
`Variable 'lower_case_file_system' is a read only variable`.

Successful scalar and `SHOW` reads do not mutate session state, catalog
generation, SQLite schema generation, file contents, or diagnostics beyond the
normal successful-statement diagnostics snapshot.

## Diagnostics

| Condition | Diagnostic |
| --- | --- |
| Unknown variable spelling outside the descriptor registry | Existing unknown-system-variable diagnostic |
| `@@SESSION.lower_case_file_system` or `@@LOCAL.lower_case_file_system` | `1238 / HY000`, global-variable message |
| Any admitted `SET lower_case_file_system = ...` target/scope form | `1238 / HY000`, read-only-variable message |
| Unsupported quoted scope such as `@@\`global\`.lower_case_file_system` | Existing unsupported quoted system-variable scope diagnostic |
| Unsupported `SHOW VARIABLES` syntax | Existing `SHOW VARIABLES` parser diagnostics |
| Allocation failure while building results | Existing MyLite allocation failure diagnostics |
| Public API misuse | Existing public API misuse behavior |

Supported reads produce `warning_count == 0`.

## Tests

Fast C tests cover:

- scalar unscoped and global reads returning `0`;
- scalar case-insensitive and quoted variable-name reads;
- scalar session/local diagnostics;
- `SHOW` default/session/local/global visibility;
- `SHOW LIKE` and `SHOW WHERE` filtering over the new row;
- read-only `SET` diagnostics for admitted target and scope forms;
- unchanged statement row-count behavior, file preamble, catalog generation,
  SQLite schema generation, and independent handles through existing
  `SHOW VARIABLES` and fixed-variable coverage.

The MySQL expectation script records the MySQL 8.4.9 runtime behavior for the
same user-visible subset.

## Compatibility Notes

MyLite intentionally reports `OFF` rather than inspecting the host filesystem.
The current single-file format and descriptor catalog have an explicit
case-sensitive compatibility contract; exposing this variable documents that
contract for clients without introducing host-dependent behavior.
