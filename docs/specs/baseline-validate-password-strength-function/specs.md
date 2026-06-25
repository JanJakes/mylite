# Baseline VALIDATE_PASSWORD_STRENGTH function

## Scope

This slice implements the MySQL 8.4.9-compatible embedded baseline for
`VALIDATE_PASSWORD_STRENGTH(password)` when the optional `validate_password`
component is not installed.

The supported execution envelope is:

- tableless `SELECT`, `SELECT ... FROM DUAL`, and `DO`;
- single-table row-scalar projection and simple predicates through the existing
  generic system-function row-scalar path;
- source-free DML values and compatible scalar assignment contexts that already
  admit generic scalar functions.

MyLite does not install or emulate the `validate_password` component policy in
this slice.

## MySQL 8.4.9 behavior used for this baseline

Official reference: MySQL 8.4 Reference Manual, `The Password Validation
Component`.

The manual states that `VALIDATE_PASSWORD_STRENGTH()` takes one password
argument and returns an integer from 0 to 100 when the component is installed.
It also states that when `validate_password` is not installed, the function
always returns `0`.

Runtime probes were run against the local `mylite-mysql-849` container and
confirmed for the component-absent target runtime:

- non-`NULL` string, numeric, Boolean, and binary inputs return `0`;
- SQL `NULL` input returns SQL `NULL`;
- wrong argument count raises `1582 / 42000` with the native-function parameter
  count diagnostic;
- the result metadata is `LONGLONG`, binary collation, display length `10`,
  decimals `0`, and numeric/binary flags;
- function lookup is case-insensitive;
- table-backed predicates compare the returned integer normally, so
  `VALIDATE_PASSWORD_STRENGTH(col) = 0` matches non-`NULL` rows and excludes
  `NULL` rows.

## Syntax

MyLite Lemon-syntax intent:

```lemon
expression ::= IDENTIFIER LPAREN function_argument_list RPAREN.
expression ::= IDENTIFIER LPAREN RPAREN.
```

The function uses MyLite's existing `MYLITE_SQL_AST_GENERIC_FUNCTION` surface
rather than a dedicated lexer keyword. This matches MySQL's unreserved function
name behavior and avoids expanding the lexer keyword table for a function that
does not need special grammar.

## Semantics

For this baseline:

- exactly one argument is required;
- if the argument is SQL `NULL`, the result is SQL `NULL`;
- otherwise the result is integer `0`;
- no warnings are emitted for successful calls.

The function accepts the scalar value types admitted by the existing generic
system-function machinery: string, integer, decimal/float text, binary, Boolean,
and `NULL` values in scalar and row-scalar contexts.

## Errors and warnings

The baseline emits MySQL-shaped diagnostics for wrong argument count:

- `1582 / 42000`: incorrect parameter count in the call to native function
  `VALIDATE_PASSWORD_STRENGTH`.

No warning is emitted for successful component-absent calls.

## MyLite implementation plan

- Register `VALIDATE_PASSWORD_STRENGTH` in `mylite_sys_functions` as an
  unqualified native-style generic scalar function.
- Evaluate it by returning `NULL` for SQL `NULL` input and `0` otherwise.
- Register a private SQLite callback for row-scalar execution through the
  existing sys-function registry.
- Override scalar and row-scalar metadata for this sys-function to the MySQL
  integer result shape.
- Add parser coverage proving the function parses through the generic-function
  path.
- Add MySQL-runtime expectation and C runtime tests for scalar, DUAL, DO,
  row-backed projection, predicate, DML value, and arity behavior.

## Known gaps

- Installed-component password policy scoring is not implemented.
- `validate_password.*` system variables and status variables remain absent in
  the target runtime, consistent with the current optional-absence baseline.
- Password assignment enforcement for `CREATE USER`, `ALTER USER`, and
  `SET PASSWORD` remains outside this slice.
- Full character-set and collation-sensitive policy behavior is future work
  because the component policy is not implemented.
