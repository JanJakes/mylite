# Baseline SET Value Syntax

## Scope

This slice broadens MyLite's `SET` parser and runtime surface for common
MySQL 8.4.9 bootstrap statements that currently fail before reaching existing
runtime handling.

Supported:

- bare identifier values for system-variable assignments, such as
  `SET sql_mode = TRADITIONAL` and
  `SET collation_connection = utf8mb4_bin`;
- `SET NAMES binary` and `SET CHARACTER SET binary` using the existing
  connection-character-set runtime validation;
- comma-separated assignments after `SET NAMES` / `SET CHARACTER SET`, such as
  `SET NAMES utf8mb4, collation_connection = utf8mb4_bin`;
- broader parser admission for user-variable assignment value atoms, including
  floating-point, hex, bit, and charset-introduced literals;
- charset-introduced string, hexadecimal, and bit literals in supported SET
  value positions, with optional `COLLATE` clauses, interpreted with MyLite's
  existing literal value semantics.

Out of scope:

- real character-set conversion, collation-aware comparison changes, or
  protocol character-set negotiation;
- `SET PERSIST`, `PERSIST_ONLY`, stored-program local variables, and parameter
  assignment;
- new system-variable kinds beyond the existing MyLite variable catalog;
- arbitrary user-variable assignment expressions, functions, subqueries, or
  non-SET assignment expressions such as `@v := @v + 1`;
- spatial, JSON_TABLE, full-text, or window-function semantics.

## References

- Official MySQL 8.4 Reference Manual, `SET` variable assignment:
  <https://dev.mysql.com/doc/refman/8.4/en/set-variable.html>
- Official MySQL 8.4 Reference Manual, `SET NAMES`:
  <https://dev.mysql.com/doc/refman/8.4/en/set-names.html>
- Official MySQL 8.4 Reference Manual, user variables:
  <https://dev.mysql.com/doc/refman/8.4/en/user-variables.html>
- Official MySQL 8.4 Reference Manual, character-set introducers:
  <https://dev.mysql.com/doc/refman/8.4/en/charset-introducer.html>

Runtime expectations were verified against the local `mysql:8.4.9` comparison
container `mylite-mysql-849` with direct `mysql -uroot` probes.

Observed MySQL 8.4.9 behavior for this slice:

- `SET NAMES binary` sets `character_set_client`,
  `character_set_connection`, `character_set_results`, and
  `collation_connection` to `binary`;
- `SET NAMES utf8mb4, collation_connection = utf8mb4_bin` succeeds and leaves
  the three character-set variables at `utf8mb4` while setting
  `collation_connection` to `utf8mb4_bin`;
- assignments after `SET NAMES` may include ordinary user-variable or
  system-variable assignments;
- `SET sql_mode = TRADITIONAL` succeeds and expands to the MySQL mode set
  containing `TRADITIONAL` and its component strict modes;
- `SET @v = 1e18`, `SET @v = X'41'`, and
  `SET @v = _latin1 X'41' COLLATE latin1_swedish_ci` are accepted.

## MyLite Grammar

Independent Lemon-shape grammar for this slice:

```lemon
set_connection_charset_statement ::=
    SET NAMES set_connection_charset_target set_names_collate_opt set_tail_opt
set_connection_charset_statement ::=
    SET CHARACTER SET set_connection_charset_target set_tail_opt
set_connection_charset_statement ::=
    SET CHARSET set_connection_charset_target set_tail_opt

set_connection_charset_target ::= option_name
set_connection_charset_target ::= BINARY
set_connection_charset_target ::= DEFAULT

set_tail_opt ::= .
set_tail_opt ::= COMMA set_assignment_list

set_system_variable_value ::= identifier
set_system_variable_value ::= BINARY
set_system_variable_value ::= charset_introducer STRING
set_system_variable_value ::= charset_introducer STRING COLLATE option_name
set_system_variable_value ::= charset_introducer HEX_LITERAL
set_system_variable_value ::= charset_introducer HEX_LITERAL COLLATE option_name
set_system_variable_value ::= charset_introducer BIT_LITERAL
set_system_variable_value ::= charset_introducer BIT_LITERAL COLLATE option_name

user_variable_set_value ::= FLOAT
user_variable_set_value ::= HEX_LITERAL
user_variable_set_value ::= BIT_LITERAL
user_variable_set_value ::= charset_introducer STRING
user_variable_set_value ::= charset_introducer STRING COLLATE option_name
user_variable_set_value ::= charset_introducer HEX_LITERAL
user_variable_set_value ::= charset_introducer HEX_LITERAL COLLATE option_name
user_variable_set_value ::= charset_introducer BIT_LITERAL
user_variable_set_value ::= charset_introducer BIT_LITERAL COLLATE option_name
```

The `SET NAMES` / `SET CHARACTER SET` AST keeps the character-set target as the
first child, keeps an optional `COLLATE` value as the next non-assignment child,
and appends an optional `MYLITE_SQL_AST_SET_ASSIGNMENT_LIST` for tail
assignments.

## Runtime Semantics

`SET NAMES` and `SET CHARACTER SET` first apply the connection character-set
statement through the existing MyLite runtime, then apply any tail assignment
list left-to-right through the existing `SET` assignment executor.

The combined operation uses the same handle-local session snapshot semantics as
ordinary comma-separated `SET`: if a later assignment fails, user variables,
system-variable overrides, SQL mode, time zone, timestamp overrides, and
connection character-set state are restored to their pre-statement values.

Bare identifier values are copied as text before existing system-variable
validation. This makes currently supported text variables and placeholder
session variables accept the same unquoted MySQL forms as their quoted forms.
`sql_mode` additionally treats an identifier value as the corresponding mode
string before existing SQL mode parsing and warning handling.

User-variable assignments use MyLite's current scalar evaluator for the
admitted value atoms. Floating-point literals are stored as source text in the
same string-backed user-variable value family used for other text-preserving
values. Supported literal and variable values are stored as the same
session-owned scalar cells already used by baseline user variables. Arbitrary
expressions remain syntax errors in this slice rather than being silently
no-oped.

Charset introducers and optional `COLLATE` clauses are parser metadata only in
this slice. MyLite preserves the underlying string, hex, or bit literal value
through existing literal evaluation and documents that full charset conversion
and collation behavior remain unsupported.

## Tests

Focused tests should cover:

- parser acceptance for `SET NAMES binary`, `SET CHARACTER SET binary`, and
  tail assignments after connection-character-set statements;
- runtime charset readback and rollback when a tail assignment fails;
- bare identifier system-variable values for `sql_mode` and text charset /
  collation variables;
- user-variable scientific notation, hex/bit, existing variable, and
  charset-introduced literal values where current scalar evaluation supports
  them;
- predictable syntax rejection for arbitrary user-variable assignment
  expressions that are still out of scope.
