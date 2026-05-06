# User Variable Foundation

## Scope

This slice implements session-scoped MySQL user variables for common application
SQL:

- `SET @var = expr [, @var := expr ...]`
- scalar reads such as `SELECT @var`, table predicates, ordering, grouping, and
  DML source expressions where the current expression engine already runs
- quoted variable names such as `@'dash-name'`, `@"dash-name"`, and
  ``@`dash-name```
- case-insensitive variable names
- unset variable reads as `NULL`

Stored-program local variables, `SELECT ... INTO`, assignment outside `SET`,
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

primary_expression ::= USER_VARIABLE.
```

`ASSIGN` is the lexer token for `:=`.

## Diagnostics

Allocation failure returns `MYLITE_NOMEM`. Unsupported expression shapes in the
right-hand side return the same unsupported diagnostics as the expression engine.
Expression warnings are preserved in the session diagnostics area. Fatal
expression errors prevent assignment side effects.

## Tests

Runtime tests cover:

- unset variable reads
- `SET` with both assignment operators
- case-insensitive variable names
- quoted variable names
- statement-start visibility for multi-assignment `SET`
- user variables in scalar SELECT, table SELECT predicates/order/grouping, and
  DML expressions
- metadata for representative unset, integer, text, and assigned-`NULL` values
