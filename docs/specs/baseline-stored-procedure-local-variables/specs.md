# Baseline Stored Procedure Local Variables

## Scope

This slice extends MyLite's session-local stored procedure bridge from a
no-argument single-`SELECT` body to a no-argument `BEGIN ... END` body with a
leading local-variable declaration block, optional local assignments, and one
final supported `SELECT`.

Supported examples:

```sql
CREATE PROCEDURE p()
BEGIN
  DECLARE a INT;
  DECLARE b INT DEFAULT 7;
  DECLARE c VARCHAR(10) DEFAULT 'hi';
  SET a = b + 1;
  SELECT a, b, c;
END;

CALL p();
```

The existing no-local form remains supported:

```sql
CREATE PROCEDURE p()
BEGIN
  SELECT id FROM posts LIMIT 1;
END;
```

This is a MyLite wrapper/translation feature. It does not require a public
SQLite extension API, a targeted SQLite fork hook, or a private stored-program
interpreter. MyLite stores the bounded body descriptor in connection-local
state, evaluates local defaults and assignments through its existing scalar
`SELECT` execution path, substitutes local values into the final body `SELECT`,
and then runs that final statement through the normal MyLite/SQLite path.

Implementation note: the executable subset is recognized from MyLite's
stored-program placeholder AST using the MyLite lexer and raw source spans. The
main Lemon grammar remains on the existing no-local single-`SELECT` production
because expanding the full routine body there currently makes parser generation
too expensive. The Lemon snippets below document the intended MyLite grammar
shape for this bounded subset rather than the current parser entry point.

## Compatibility Authority

Official MySQL 8.4 reference pages used for the feature shape:

- <https://dev.mysql.com/doc/refman/8.4/en/declare-local-variable.html>
- <https://dev.mysql.com/doc/refman/8.4/en/sql-compound-statements.html>
- <https://dev.mysql.com/doc/refman/8.4/en/call.html>
- <https://dev.mysql.com/doc/refman/8.4/en/create-procedure.html>

MySQL 8.4.9 runtime probes verified these observations:

- `DECLARE a INT` initializes `a` to `NULL`.
- `DECLARE a, b INT DEFAULT 3` declares both variables with the same default.
- Local variable names are case-insensitive.
- Duplicate local variable names fail with error 1331, SQLSTATE `42000`.
- A `DECLARE` after an executable statement fails with parse error 1064,
  SQLSTATE `42000`.
- `SELECT missing` inside a procedure fails at `CALL` with unknown-column error
  1054, SQLSTATE `42S22`.
- `SET missing = 1` inside a procedure fails during routine creation with
  unknown-system-variable error 1193, SQLSTATE `HY000`.

The matching probe script lives in
`packages/libmylite/tests/mysql_baseline_stored_procedure_local_variables_expectations.sh`.

## Syntax

The supported MyLite Lemon grammar shape is independently authored for this
bounded subset:

```lemon
create_procedure_statement ::=
    CREATE PROCEDURE table_name LPAREN RPAREN stored_procedure_body.

stored_procedure_body ::=
    BEGIN select_statement SEMICOLON END.
stored_procedure_body ::=
    BEGIN stored_procedure_local_declaration_list
          select_statement SEMICOLON END.
stored_procedure_body ::=
    BEGIN stored_procedure_local_declaration_list
          stored_procedure_local_assignment_list
          select_statement SEMICOLON END.

stored_procedure_local_declaration_list ::=
    stored_procedure_local_declaration.
stored_procedure_local_declaration_list ::=
    stored_procedure_local_declaration_list stored_procedure_local_declaration.

stored_procedure_local_declaration ::=
    DECLARE identifier_list stored_procedure_local_type
            stored_procedure_local_default_opt SEMICOLON.

stored_procedure_local_type ::=
    integer_type | varchar_type | char_type | text_type | decimal_type |
    approximate_type | bit_type | year_type | date_type | datetime_type |
    timestamp_type | time_type | binary_string_type | enum_type | set_type |
    json_type.

stored_procedure_local_default_opt ::= .
stored_procedure_local_default_opt ::= DEFAULT expression.

stored_procedure_local_assignment_list ::=
    stored_procedure_local_assignment.
stored_procedure_local_assignment_list ::=
    stored_procedure_local_assignment_list stored_procedure_local_assignment.

stored_procedure_local_assignment ::=
    SET identifier EQUAL expression SEMICOLON.
```

The grammar intentionally keeps parameter lists, routine characteristics,
nested blocks, handlers, cursors, flow-control statements, multiple result
sets, DML statements in procedure bodies, and `SELECT ... INTO` outside this
slice.

## Semantics

- Procedures are no-argument and session-local, matching the existing bridge.
- Declarations must precede local assignments and the final `SELECT`.
- Supported local declaration types are integer-family types and character text
  family types (`CHAR`, `VARCHAR`, and `TEXT` family nodes).
- Unsupported local declaration types fail with a stored-program unsupported
  diagnostic instead of silently coercing values.
- A declaration without `DEFAULT` initializes to SQL `NULL`.
- A declaration `DEFAULT` expression is evaluated when `CALL` runs, not when the
  routine is created.
- Assignment expressions are evaluated in body order when `CALL` runs.
- Defaults and assignments can reference previously declared local variables
  from this supported body.
- Local names are matched case-insensitively.
- Duplicate local names are rejected at create time with MySQL-shaped duplicate
  local variable diagnostics.
- Unknown assignment targets are treated like MySQL's fallback to a session
  system variable assignment and fail with unknown-system-variable diagnostics.
- The final `SELECT` can reference local variables in projections, predicates,
  function arguments, and other expression positions handled by MyLite once the
  local value is substituted.
- Bare local-variable projections are substituted through `NAME_CONST()` to
  preserve MySQL-shaped output labels. Local references in defaults,
  assignments, function arguments, and expression contexts are substituted as
  typed SQL literals.
- Local values are substituted only outside string literals, quoted
  identifiers, comments, and user-variable tokens.
- `SHOW CREATE PROCEDURE` keeps returning the existing six MySQL-shaped columns
  and stores a MySQL-shaped body text for the supported subset.

## Known Incompatibilities

This slice is not full stored-program support. It still does not implement:

- routine parameters, OUT/INOUT values, or parameter metadata rows;
- persistent routine storage or `INFORMATION_SCHEMA.ROUTINES` rows for the
  session-local bridge;
- nested blocks and local shadowing;
- condition declarations, cursors, handlers, labels, or flow control;
- multiple statements after the final `SELECT`;
- DML or DDL statements in the procedure body;
- `SELECT ... INTO`, local-variable targets in `GET DIAGNOSTICS`, or local
  variables outside stored programs;
- alias rewriting for local-variable projections that are already followed by
  explicit aliases;
- full MySQL type checking, overflow handling, or local variable collation
  semantics.

These gaps keep the broad routines coverage yellow, but the local-variable
baseline is no longer absent.

## Tests

- MySQL 8.4.9 expectation script:
  `packages/libmylite/tests/mysql_baseline_stored_procedure_local_variables_expectations.sh`
- Parser/runtime regression tests:
  `packages/libmylite/tests/runtime_stored_procedure_local_variables_test.c`
- Existing stored procedure select-call tests continue to cover the previous
  no-local body form.
