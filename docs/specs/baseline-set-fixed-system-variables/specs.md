# Baseline Fixed SET System Variables

## Summary

This phase admits a deliberately small MySQL `SET` statement surface for
session-scoped system variables whose current MyLite value is fixed:

```sql
SET [SESSION | LOCAL] variable_name = fixed_value
SET @@[session. | local.]variable_name = fixed_value
```

The statement succeeds only when the assignment preserves MyLite's existing
fixed runtime state. It is intended for client bootstrap statements such as
`SET autocommit = 1`, `SET sql_notes = 1`, `SET sql_warnings = 0`, and
`SET sql_mode = DEFAULT`. It does not add mutable transaction state, mutable
SQL mode semantics, user variables, global variables, multi-assignment, or a
general expression engine.

## Compatibility Authority

- Official MySQL 8.4 documentation:
  - `SET` variable assignment: <https://dev.mysql.com/doc/refman/8.4/en/set-variable.html>
  - server system variables: <https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html>
  - SQL modes: <https://dev.mysql.com/doc/refman/8.4/en/sql-mode.html>
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_set_fixed_system_variables_expectations.sh`.

Runtime probes against MySQL 8.4.9 establish these expectations for the
admitted subset:

- successful `SET` variable assignment reports no result columns or rows;
- successful `SET` makes a following `ROW_COUNT()` return `0`;
- successful in-range assignments report `@@warning_count = 0` and
  `@@error_count = 0`;
- `SET autocommit = 1`, `SET SESSION autocommit = 1`,
  `SET LOCAL autocommit = 1`, `SET @@autocommit = 1`,
  `SET @@session.autocommit = 1`, and `SET @@local.autocommit = 1` succeed;
- `SET autocommit = DEFAULT`, `SET autocommit = ON`, and
  `SET autocommit = TRUE` preserve enabled autocommit in MySQL;
- `SET sql_mode = DEFAULT`, `SET SESSION sql_mode = DEFAULT`,
  `SET @@sql_mode = DEFAULT`, and `SET @@session.sql_mode = DEFAULT` restore
  MySQL's default 8.4 SQL mode string;
- MySQL accepts broader mutable forms such as `SET autocommit = 0`,
  `SET sql_mode = 'ANSI_QUOTES'`, multi-assignment, `:=`, global assignment
  when privileges allow it, string coercions, and expression values. MyLite
  defers those forms in this baseline.

## Ownership Boundaries

- Public API: no ABI or public-header changes. `mylite_execute()` continues to
  own result-handle lifetime, diagnostics, and statement-boundary behavior.
- Statement context: successful fixed `SET` uses non-row statement
  conventions: zero result columns, zero result rows, affected rows `0`,
  statement warning count `0`, and previous row count `0` after success.
- Lexer/parser/AST: the parser adds a single-assignment system-variable
  statement node. It admits only `=` assignments and literal/default values
  listed below. It does not add user variables, parameters, expression values,
  `:=`, or assignment lists.
- Analyzer/runtime: runtime resolves the target against MyLite's existing
  system-variable registry and validates that the value is exactly the current
  fixed MyLite baseline for that variable.
- Catalog: not involved. The statement must not read or mutate schemas, table
  descriptors, descriptor versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation`.
- Result builder: creates an empty result object through existing non-row
  statement conventions and sets affected rows to `0`.
- Storage/VFS/file format: no storage writes, no physical table access, and no
  `.mylite` preamble changes.
- SQLite physical execution: no generated SQLite SQL and no SQLite fork patch.
  This is MyLite wrapper/runtime behavior.

## Syntax

The independent MyLite subset is:

```ebnf
set_system_variable_statement:
    SET set_system_variable_target = set_system_variable_value

set_system_variable_target:
    identifier
  | SESSION identifier
  | LOCAL identifier
  | GLOBAL identifier
  | system_variable

set_system_variable_value:
    DEFAULT
  | unsigned_decimal_integer_literal
  | TRUE
  | FALSE
  | ON
  | OFF
  | string_literal
```

`GLOBAL` and `@@global.name` are parsed only so MyLite can reject them with a
deterministic unsupported diagnostic. String literals are admitted only for
the fixed `sql_mode` value described below. The grammar intentionally excludes
multi-assignment, `:=`, user variables, qualified identifiers such as
`app.autocommit`, arbitrary expressions, function calls, parameters, and
subqueries.

### MyLite Lemon-Syntax Snippet

```lemon
statement(A) ::= set_system_variable_statement(B). {
    A = B;
}

set_system_variable_statement(A) ::=
    SET(S) set_system_variable_target(T) EQUAL set_system_variable_value(V). {
    A = mylite_sql_parser_make_set_system_variable_statement(state, S, T, V);
}

set_system_variable_target(A) ::= identifier(N). {
    A = mylite_sql_parser_make_set_system_variable_target(state, NULL, N);
}
set_system_variable_target(A) ::= SESSION(S) identifier(N). {
    A = mylite_sql_parser_make_set_system_variable_target(
        state,
        mylite_sql_parser_make_identifier(state, S),
        N);
}
set_system_variable_target(A) ::= LOCAL(L) identifier(N). {
    A = mylite_sql_parser_make_set_system_variable_target(
        state,
        mylite_sql_parser_make_identifier(state, L),
        N);
}
set_system_variable_target(A) ::= GLOBAL(G) identifier(N). {
    A = mylite_sql_parser_make_set_system_variable_target(
        state,
        mylite_sql_parser_make_identifier(state, G),
        N);
}
set_system_variable_target(A) ::= SYSTEM_VARIABLE(T). {
    A = mylite_sql_parser_make_set_system_variable_target(
        state,
        NULL,
        mylite_sql_parser_make_system_variable(state, T));
}

set_system_variable_value(A) ::= DEFAULT(T). {
    A = mylite_sql_parser_make_set_default_value(state, T);
}
set_system_variable_value(A) ::= INTEGER(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER);
}
set_system_variable_value(A) ::= TRUE(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_TRUE);
}
set_system_variable_value(A) ::= FALSE(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_FALSE);
}
set_system_variable_value(A) ::= ON(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_TRUE);
}
set_system_variable_value(A) ::= OFF(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_FALSE);
}
set_system_variable_value(A) ::= STRING(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}
```

These snippets are independently authored for MyLite's admitted subset and are
not MySQL's full grammar.

## Target Resolution

Unqualified targets resolve as session-scoped system variables. `SESSION` and
`LOCAL` are synonyms for this baseline. `@@name`, `@@session.name`, and
`@@local.name` resolve through the same system-variable registry used by
scalar `@@` reads.

Names are matched case-insensitively with the current descriptor/runtime name
policy. Quoted variable names such as ``SET `autocommit` = 1`` are accepted
for the variable name. Quoted scopes inside `@@` tokens, such as
`@@`session`.autocommit`, remain unsupported because existing MyLite `@@`
scope parsing intentionally rejects quoted scopes.

Unknown variables use the existing MySQL-compatible unknown-system-variable
diagnostic. Variables that MyLite exposes only as fixed reads and does not
admit in this write slice are rejected as read-only or unsupported. Global
assignment is rejected before any mutable state is changed.

## Admitted Assignments

The statement succeeds only for variables and values that preserve the current
fixed MyLite value.

| Variable | Fixed MyLite value | Admitted values |
| --- | --- | --- |
| `autocommit` | `1` | `1`, `TRUE`, `ON`, `DEFAULT` |
| `foreign_key_checks` | `1` | `1`, `TRUE`, `ON`, `DEFAULT` |
| `unique_checks` | `1` | `1`, `TRUE`, `ON`, `DEFAULT` |
| `sql_quote_show_create` | `1` | `1`, `TRUE`, `ON`, `DEFAULT` |
| `sql_notes` | `1` | `1`, `TRUE`, `ON`, `DEFAULT` |
| `sql_big_selects` | `1` | `1`, `TRUE`, `ON`, `DEFAULT` |
| `sql_log_bin` | `1` | `1`, `TRUE`, `ON`, `DEFAULT` |
| `sql_safe_updates` | `0` | `0`, `FALSE`, `OFF`, `DEFAULT` |
| `sql_warnings` | `0` | `0`, `FALSE`, `OFF`, `DEFAULT` |
| `sql_buffer_result` | `0` | `0`, `FALSE`, `OFF`, `DEFAULT` |
| `sql_auto_is_null` | `0` | `0`, `FALSE`, `OFF`, `DEFAULT` |
| `sql_generate_invisible_primary_key` | `0` | `0`, `FALSE`, `OFF`, `DEFAULT` |
| `sql_log_off` | `0` | `0`, `FALSE`, `OFF`, `DEFAULT` |
| `sql_require_primary_key` | `0` | `0`, `FALSE`, `OFF`, `DEFAULT` |
| `sql_mode` | MySQL 8.4 default SQL mode string | `DEFAULT` or the exact default string |

The fixed SQL mode string is:

```text
ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION
```

This baseline does not admit mutable values such as `autocommit = 0`,
`sql_warnings = 1`, or `sql_mode = 'ANSI_QUOTES'`. It also does not coerce
strings into boolean variables or evaluate arithmetic/string expressions.

## Semantics

Execution order:

1. Parse exactly one system-variable assignment.
2. Resolve the target variable and scope.
3. Reject global scope and unknown/read-only/unsupported variables.
4. Validate that the value is the exact fixed value currently exposed by
   MyLite for that variable.
5. Return an empty successful result with affected rows `0`.
6. Set the connection's previous row-count state to `0`.

No mutable session field is changed by a successful statement. The visible
state remains the same because every admitted assignment is a no-op against
the current fixed MyLite baseline.

## Diagnostics

Supported successful statements:

- return `MYLITE_OK`;
- return a result object with `column_count == 0` and `row_count == 0`;
- report `affected_rows == 0`;
- report `warning_count == 0`; and
- leave catalog, descriptor, storage, file-format, and SQLite schema state
  unchanged.

Diagnostics for this baseline:

- syntax errors use the existing MySQL-compatible parse diagnostic surface;
- unknown variables use MySQL error 1193 / SQLSTATE `HY000`;
- read-only variable writes use MySQL error 1238 / SQLSTATE `HY000` with a
  deterministic read-only variable message;
- unsupported global assignment and unsupported mutable values use
  deterministic MyLite-specific unsupported diagnostics;
- unsupported value expressions use deterministic MyLite-specific unsupported
  diagnostics when parsed, or syntax errors when the narrow grammar rejects
  them before runtime;
- allocation failure returns `MYLITE_NOMEM`; and
- public API misuse remains unchanged.

Unsupported for this slice:

- `SET` assignment lists;
- assignment operator `:=`;
- `GLOBAL`, `PERSIST`, `PERSIST_ONLY`, or `@@global` mutation;
- user variables such as `@x`;
- stored-program local variables;
- schema-qualified variable names such as `app.autocommit`;
- table, column, or expression assignment values;
- function calls, parameters, subqueries, CTEs, and arbitrary expressions;
- string/decimal/float/hex/bit/temporal values except the exact fixed
  `sql_mode` string;
- mutable transaction/autocommit behavior;
- mutable SQL mode behavior;
- changed privilege, replication, or binary-log semantics; and
- arbitrary SQLite pass-through.

## Runtime And Storage Design

The implementation stays in the MyLite parser/runtime layer:

1. Add parser tokens and AST nodes for a fixed `SET` system-variable
   statement, target wrapper, and `DEFAULT` value.
2. Dispatch the statement from `execute_parsed_statement()`.
3. Reuse the existing system-variable name registry for target resolution.
4. Validate admitted values with MyLite-owned literal checks.
5. Return a normal empty result with affected rows `0`.
6. Add the statement to completed-statement row-count rules as a non-row
   statement with row count `0`.

The implementation must not generate SQLite SQL, bind SQLite parameters,
touch descriptor catalog rows, mutate storage, or add a SQLite fork patch.

## Performance

The supported path is O(length of variable name and literal text). It does not
scan tables, materialize rows, allocate catalog descriptors, or call SQLite for
execution. This keeps client bootstrap `SET` statements cheap and avoids
adding state machinery before MyLite implements mutable session semantics.

## Test Plan

Add fast C coverage under `packages/libmylite/tests/`, preferably
`runtime_set_fixed_system_variables_test.c`, with a dotted CTest name
`libmylite.runtime.set_fixed_system_variables`.

Cover:

- `SET autocommit = 1` and scope spellings with `SESSION`, `LOCAL`, `@@`,
  `@@session`, and `@@local`;
- quoted variable names where admitted;
- fixed-true values `1`, `TRUE`, `ON`, and `DEFAULT`;
- fixed-false values `0`, `FALSE`, `OFF`, and `DEFAULT`;
- `sql_mode = DEFAULT` and the exact fixed default SQL mode string;
- result shape, affected rows `0`, warning count `0`, and `ROW_COUNT() = 0`;
- diagnostics clearing at the statement boundary;
- unknown variables;
- read-only/unsupported variables such as `version`;
- global scope rejection;
- unsupported mutable values such as `autocommit = 0`,
  `sql_warnings = 1`, and `sql_mode = 'ANSI_QUOTES'`;
- unsupported assignment lists, `:=`, user variables, schema-qualified names,
  expression values, parameters, function calls, and subqueries;
- file-backed preamble preservation and unchanged catalog/SQLite generation;
- independent handles preserving independent diagnostics/session state; and
- existing parser, runtime handle, diagnostics, statement context, charset
  `SET`, system-variable read, and lifecycle tests.

Add a MySQL expectation script that verifies the successful forms and records
broader MySQL forms deferred by this baseline.
