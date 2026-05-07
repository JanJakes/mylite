# User Variable Foundation

## Scope

This slice implements session-scoped MySQL user variables for common application
SQL:

- `SET @var = expr [, @var := expr ...]`
- `@var := expr` assignment expressions in scalar expression evaluation
- scalar reads such as `SELECT @var`, table predicates, ordering, grouping, and
  DML source expressions where the current expression engine already runs
- quoted variable names such as `@'dash-name'`, `@"dash-name"`, and
  ``@`dash-name```
- case-insensitive variable names
- unset variable reads as `NULL`

Stored-program local variables, `SELECT ... INTO`, broad assignment-expression
coverage in every DML, predicate, ordering, and grouped-query context,
Performance Schema exposure, and exact character-set propagation for every
expression type remain deferred.

## References

- MySQL 8.4 Reference Manual, User-Defined Variables:
  https://dev.mysql.com/doc/refman/8.4/en/user-variables.html
- MySQL 8.4 Reference Manual, SET Syntax for Variable Assignment:
  https://dev.mysql.com/doc/refman/8.4/en/set-variable.html
- MySQL 8.4.9 runtime probes run against the local `mysql:8.4.9` comparison
  container.

## MySQL Behavior

User variables are session state. They are not visible to other sessions and are
freed when the session ends. Names are case-insensitive. The ordinary form is
`@name`; quoted string or identifier forms allow characters that are not valid
in bare variable names.

`SET` accepts either `=` or `:=` for user-variable assignment and returns no
result columns with affected rows `0`. Multiple assignments in one `SET`
statement evaluate their right-hand expressions from the statement-start
variable state; assignment side effects are applied after expression evaluation.
Observed on MySQL 8.4.9:

```sql
SET @a = 10;
SET @a = 1, @b = @a + 1, @a = 5, @c = @b + @a;
SELECT @a, @b, @c, ROW_COUNT();
-- 5, 11, NULL, 0
```

Reading an unset user variable returns SQL `NULL`. MySQL reports unset variables
with string-like binary metadata. Assigned integer variables report integer
metadata; assigned nonbinary strings report string/blob metadata; assigned
`NULL` keeps assigned-variable string/blob metadata rather than reverting to an
uninitialized variable.

Observed MySQL 8.4.9 result metadata:

- unset variables: `VAR_STRING`, binary collation, length `65535`, decimals
  `31`, `BINARY` flag, nullable
- assigned signed integers: `LONGLONG`, binary collation, length `21`,
  decimals `0`, `BINARY` and `NUM` flags, nullable
- assigned nonbinary strings under `utf8mb4`: `LONG_BLOB`,
  `utf8mb4_0900_ai_ci`, length `268435440`, decimals `31`, no flags, nullable
- assigned nonbinary strings under `latin1`: `MEDIUM_BLOB`,
  `latin1_swedish_ci`, length `16777215`, decimals `31`, no flags, nullable
- assigned binary strings and assigned `NULL`: `MEDIUM_BLOB`, binary collation,
  length `16777215`, decimals `31`, `BINARY` flag, nullable

`@var := expr` is also accepted as an expression. It evaluates the right-hand
expression, stores the value immediately, returns the assigned value, and emits
warning `1287` with the MySQL deprecation message. SELECT-list evaluation is
left-to-right, so a later expression in the same select list can observe an
earlier assignment:

```sql
SET @a := 10;
SELECT @a := 1 AS assigned, @a AS after_a,
       @b := @a + 1 AS b, @a := @a + 5 AS bumped,
       @a AS final_a, @b AS final_b;
-- 1, 1, 2, 6, 6, 2 with three 1287 warnings
```

Nested assignment expressions inside a `SET` right-hand expression take effect
immediately for later right-hand expressions, while the outer `SET` assignment
values are still applied after all right-hand expressions have been evaluated:

```sql
SET @a = 10, @b = NULL, @c = NULL;
SET @a = 1, @b = (@a := 2), @c = @a;
SELECT @a, @b, @c;
-- 1, 2, 2
```

User variables are data values. They cannot supply identifiers, keywords, table
names, or database names directly. The exception is dynamic SQL text supplied to
`PREPARE`.

## MyLite Design

Each `mylite_db` owns a user-variable store. Entries are keyed by a normalized
ASCII-lowercase name after removing the leading `@` and unquoting quoted forms.
The store owns a copied expression value and a field descriptor inferred when
the value is assigned.

`SET @var = expr` is a custom statement. Preparation copies the assignment
expressions into statement-owned AST storage. Execution evaluates every
right-hand expression against the current session state, then applies all
assignment values in source order. This matches the statement-start visibility
observed in MySQL for multiple assignments.

Expression resolvers treat user variables like system variables: they bypass
column-name resolution and evaluate through session state. This applies to the
currently supported scalar/table-backed SELECT, aggregate, order, predicate, and
DML expression paths.

Expression assignment is modeled as a non-cacheable expression. Scalar SELECT
preparation therefore defers it to row execution, preserving MySQL's observable
left-to-right select-list side effects. Table-backed SELECT projection accepts
user-variable reads and `@var := expr` expressions, binding the right-hand
expression against the current table plan. The assignment expression's metadata
and collation are inferred from its right-hand expression; the stored
user-variable descriptor is inferred from the runtime assigned value.

## Grammar

MyLite Lemon-level intent:

```lemon
statement ::= set_user_variable_statement.

set_user_variable_statement ::= SET set_user_variable_assignment_list.
set_user_variable_assignment_list ::= set_user_variable_assignment.
set_user_variable_assignment_list ::= set_user_variable_assignment_list COMMA
                                      set_user_variable_assignment.
set_user_variable_assignment ::= USER_VARIABLE EQ expression.
set_user_variable_assignment ::= USER_VARIABLE ASSIGN expression.

expression ::= USER_VARIABLE ASSIGN expression.

primary_expression ::= USER_VARIABLE.
```

`ASSIGN` is the lexer token for `:=`.

## Diagnostics

Allocation failure returns `MYLITE_NOMEM`. Unsupported expression shapes in the
right-hand side return the same unsupported diagnostics as the expression engine.
Expression warnings are preserved in the session diagnostics area. Fatal
expression errors prevent assignment side effects.
Expression assignment appends warning `1287`:

```text
Setting user variables within expressions is deprecated and will be removed in a future release. Consider alternatives: 'SET variable=expression, ...', or 'SELECT expression(s) INTO variables(s)'.
```

## Tests

Runtime tests cover:

- unset variable reads
- `SET` with both assignment operators
- case-insensitive variable names
- quoted variable names
- statement-start visibility for multi-assignment `SET`
- expression-assignment `:=` side effects, result values, deprecation warnings,
  and metadata
- nested `:=` assignment inside a `SET` right-hand expression
- user-variable reads and assignment expressions in table SELECT projections
- user variables in scalar SELECT, table SELECT predicates/order/grouping, and
  DML expressions
- metadata for representative unset, integer, text, and assigned-`NULL` values
