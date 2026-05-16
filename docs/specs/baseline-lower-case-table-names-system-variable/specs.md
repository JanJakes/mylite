# Baseline lower_case_table_names System Variable

## Summary

This phase exposes the MySQL `lower_case_table_names` system variable as a
fixed MyLite runtime value for common client capability probes.

MyLite reports `lower_case_table_names = 0`. This matches the current
descriptor catalog behavior: schema and table names are stored as declared and
resolved case-sensitively by the supported catalog slices. The feature does not
change identifier normalization, name comparison, physical table naming, or
catalog collation behavior.

## Compatibility Authority

- Official MySQL 8.4 documentation:
  - server system variables:
    <https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html>
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_lower_case_table_names_system_variable_expectations.sh`.

The MySQL 8.4 manual defines `lower_case_table_names` as a global, non-dynamic
integer variable with platform-dependent startup values `0`, `1`, or `2`.
MySQL runtime observations on the Linux MySQL 8.4.9 comparison container show
value `0`, global scalar reads accepted, session/local scalar reads rejected as
a global variable, `SHOW VARIABLES` and `SHOW SESSION VARIABLES` still exposing
the row, and all tested `SET` forms rejected as read-only.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## Scope

This feature adds:

- scalar reads for `@@lower_case_table_names` and
  `@@GLOBAL.lower_case_table_names`, returning `0`;
- case-insensitive variable-name matching for scalar reads, including quoted
  variable name components already admitted by the existing parser;
- MySQL-compatible global-only scalar diagnostics for
  `@@SESSION.lower_case_table_names` and `@@LOCAL.lower_case_table_names`;
- `SHOW VARIABLES`, `SHOW SESSION VARIABLES`, `SHOW LOCAL VARIABLES`, and
  `SHOW GLOBAL VARIABLES` rows for `lower_case_table_names`;
- `SHOW VARIABLES LIKE 'lower_case_table_names'` and the existing limited
  `SHOW VARIABLES WHERE` filtering over the new row;
- MySQL-compatible read-only diagnostics for no-scope, session, local, global,
  and `@@`-targeted `SET lower_case_table_names = ...` forms admitted by the
  existing `SET` grammar;
- warning count `0`, affected rows `0`, and no result rows for successful
  scalar/SHOW reads according to the existing statement-result conventions.

## Non-Goals

This feature does not implement:

- startup configuration, persisted variables, mutable global state, privilege
  checks, Performance Schema variable tables, or option-file handling;
- values `1` or `2`;
- case-insensitive schema, table, alias, trigger, or index lookup;
- catalog migrations between `lower_case_table_names` modes;
- filesystem case probing or `lower_case_file_system`;
- changes to durable descriptor collation, descriptor versioning, physical
  SQLite object naming, SQLite schema text, or `.mylite` file format;
- SQLite fork patches.

## Ownership Boundary

- Public API remains unchanged. `mylite_execute()` returns the existing scalar
  result or row-result handles and diagnostics.
- Statement context owns result classification, affected rows, previous
  diagnostics, warning count, and row-count side effects.
- Lexer/parser/AST require no new syntax. Existing system-variable scalar,
  `SET`, and `SHOW VARIABLES` nodes already represent all admitted SQL shapes.
- Runtime owns the static system-variable descriptor, scope rules, read-only
  diagnostics, scalar value formatting, and `SHOW VARIABLES` display value.
- Catalog descriptors remain authoritative for schema/table names and retain
  current case-sensitive behavior. This variable is runtime metadata only.
- Result builder owns scalar and `SHOW VARIABLES` result materialization.
- Storage, VFS, file format, and SQLite physical storage are not involved.

## Syntax

No grammar expansion is needed. The feature uses the existing MyLite grammar
for scalar system-variable expressions, system-variable `SET`, and
`SHOW VARIABLES`.

The admitted SQL surface is:

```ebnf
system_variable_expr:
    @@ lower_case_table_names
  | @@ GLOBAL . lower_case_table_names
  | @@ SESSION . lower_case_table_names
  | @@ LOCAL . lower_case_table_names

set_system_variable_statement:
    SET lower_case_table_names = set_system_variable_value
  | SET SESSION lower_case_table_names = set_system_variable_value
  | SET LOCAL lower_case_table_names = set_system_variable_value
  | SET GLOBAL lower_case_table_names = set_system_variable_value
  | SET @@ lower_case_table_names = set_system_variable_value
  | SET @@ GLOBAL . lower_case_table_names = set_system_variable_value
  | SET @@ SESSION . lower_case_table_names = set_system_variable_value
  | SET @@ LOCAL . lower_case_table_names = set_system_variable_value

show_variables_statement:
    SHOW [GLOBAL | SESSION | LOCAL] VARIABLES [LIKE string_literal]
  | SHOW [GLOBAL | SESSION | LOCAL] VARIABLES WHERE show_variables_where_predicate
```

The existing parser accepts identifier case variants and quoted variable-name
components where current system-variable support accepts them.

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

`lower_case_table_names` is a fixed integer-like text value:

```text
Scalar @@ read: 0
SHOW VARIABLES value: 0
```

Unscoped scalar reads use MySQL's effective global value and return `0`.
`GLOBAL` scalar reads return `0`. `SESSION` and `LOCAL` scalar reads fail with
`1238 / HY000` and a message containing
`Variable 'lower_case_table_names' is a GLOBAL variable`.

`SHOW VARIABLES`, `SHOW SESSION VARIABLES`, and `SHOW LOCAL VARIABLES` include
the row because MySQL's session variable introspection shows global-only rows
that have no distinct session value. `SHOW GLOBAL VARIABLES` also includes the
row. `SHOW VARIABLES WHERE` evaluates predicates over the new row using the
existing `Variable_name` and `Value` text semantics.

All `SET lower_case_table_names` forms admitted by the existing grammar fail
before mutation with `1238 / HY000` and a message containing
`Variable 'lower_case_table_names' is a read only variable`. MyLite does not
attempt to parse, range-check, or apply the right-hand value for this read-only
variable.

Successful scalar and `SHOW` reads do not mutate session state, catalog
generation, SQLite schema generation, file contents, or diagnostics beyond the
normal successful-statement diagnostics snapshot.

## Diagnostics

| Condition | Diagnostic |
| --- | --- |
| Unknown variable spelling outside the descriptor registry | Existing unknown-system-variable diagnostic |
| `@@SESSION.lower_case_table_names` or `@@LOCAL.lower_case_table_names` | `1238 / HY000`, global-variable message |
| Any admitted `SET lower_case_table_names = ...` target/scope form | `1238 / HY000`, read-only-variable message |
| Unsupported quoted scope such as `@@\`global\`.lower_case_table_names` | Existing unsupported quoted system-variable scope diagnostic |
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
- read-only `SET` diagnostics for representative target and scope forms;
- unchanged statement row-count behavior, file preamble, catalog generation,
  SQLite schema generation, and independent handles through the existing
  `SHOW VARIABLES` and fixed-variable test coverage.

The MySQL expectation script records the MySQL 8.4.9 runtime behavior for the
same user-visible subset.

## Compatibility Notes

MyLite intentionally reports `0` even on hosts where a native MySQL server
would default to `1` or `2`. MyLite's single-file catalog currently has a
case-sensitive identifier contract, and changing that contract is a larger
catalog/name-resolution feature. Exposing the fixed variable lets clients
probe the mode honestly without implying unsupported identifier behavior.
