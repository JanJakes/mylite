# Baseline User Variable Assignment Expressions

## Scope

This slice admits MySQL's legacy user-variable assignment expression form:

```lemon
expression ::= user_variable ASSIGN expression.
```

The expression target must be a user variable token such as `@name`, `@'quoted
name'`, or ``@`quoted-name` ``. The `ASSIGN` token is `:=` and binds below the
current scalar logical and comparison operators. The `=` operator outside `SET`
continues to be parsed and executed as comparison, not user-variable
assignment.

MyLite implements this as MyLite parser and runtime session-state behavior. It
does not require a SQLite fork hook because SQLite does not own MyLite user
variables or MySQL diagnostics.

## MySQL 8.4.9 Behavior

The MySQL 8.4 manual describes user-defined variables as session-specific
values and states that assignment outside `SET` remains supported for backward
compatibility, requires `:=`, and is deprecated. Runtime probes against MySQL
8.4.9 observed:

```sql
SELECT @a := 1, @a, @a := @a + 2, @a;
SHOW WARNINGS;
SELECT @b = 1, @b := 1, @b;
SHOW WARNINGS;
SELECT @sub := (SELECT 7), @sub, @@warning_count;
DO @d := 5;
SELECT @d, ROW_COUNT(), @@warning_count;
```

Observed result behavior:

- `@a := 1` returns `1`, stores `@a = 1`, and emits warning `1287 / HY000`.
- `@a := @a + 2` reads the current value, stores `3`, returns `3`, and emits a
  second warning.
- `@b = 1` is comparison when outside `SET`; with `@b` uninitialized it returns
  `NULL`.
- `@b := 1` stores and returns `1` and emits warning `1287 / HY000`.
- `@sub := (SELECT 7)` stores and returns `7` and emits warning `1287 /
  HY000`.
- `DO @d := 5` stores `@d = 5`, reports `ROW_COUNT() = 0`, and leaves
  `@@warning_count = 1`.

The warning message is:

```text
Setting user variables within expressions is deprecated and will be removed in a future release. Consider alternatives: 'SET variable=expression, ...', or 'SELECT expression(s) INTO variables(s)'.
```

MySQL documents expression evaluation order involving user variables as
undefined. Applications should not depend on reading and assigning the same
variable in one statement, but MyLite's current scalar projection evaluation is
left-to-right for supported no-source/`DUAL` scalar statements.

## MyLite Semantics

Supported:

- Parser support for `@name := expression` anywhere the shared expression
  grammar is used.
- Runtime support in scalar `SELECT`, `SELECT ... FROM DUAL`, and `DO`
  expressions when the RHS is already accepted by `session_scalar_value()`.
- Nested scalar assignment expressions such as `@outer := (@inner := 10)`.
- Existing user-variable naming, case folding, 1..64 character validation, and
  text/`NULL` storage rules.
- Existing scalar expression conversion for the RHS, including supported
  literals, user-variable reads, system-variable reads, arithmetic, comparison,
  logical, control-flow, scalar subqueries, and supported scalar functions.
- One deprecation warning `1287 / HY000` per assignment expression that is
  evaluated successfully.

Not implemented in this slice:

- Table-row assignment-expression execution such as `SELECT @v := col FROM t`
  beyond parser admission. Row-backed planning remains a separate feature.
- Assignment expressions in DML value/default/generated-column contexts unless
  the existing runtime path already evaluates them through `session_scalar_value()`.
- MySQL's prepared-statement first-execution result-type pinning for variables.
- Charset/collation propagation beyond existing user-variable text storage.
- Any new privilege, metadata-table, Performance Schema, or durable storage
  behavior.

## Tests

MySQL-runtime expectations live in:

- `packages/libmylite/tests/mysql_baseline_user_variables_expectations.sh`

MyLite parser/runtime coverage lives in:

- `packages/libmylite/tests/parser_expression_aggregate_test.c`
- `packages/libmylite/tests/runtime_user_variables_test.c`

The focused verification commands are:

```sh
sh -n packages/libmylite/tests/mysql_baseline_user_variables_expectations.sh
cmake --build --preset dev --target mylite_parser_expression_aggregate_test mylite_runtime_user_variables_test
ctest --preset dev -R '^libmylite\.(parser\.expression_aggregate|runtime\.user_variables)$' --output-on-failure
packages/libmylite/tests/mysql_baseline_user_variables_expectations.sh
```

## References

- MySQL 8.4 Reference Manual, User-Defined Variables:
  `https://dev.mysql.com/doc/refman/8.4/en/user-variables.html`
