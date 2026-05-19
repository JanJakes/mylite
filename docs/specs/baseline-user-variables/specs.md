# Baseline User Variables

This slice adds the first MyLite-owned user-defined variable surface:
session-local `@name` values, `SET` assignment lists, and scalar reads. The
main practical target is MySQL dump and application prologue compatibility,
such as saving `@@sql_mode` into `@OLD_SQL_MODE`, changing session state, and
restoring that state later.

This is not SQL prepared statement support, a general expression engine, or
Performance Schema exposure. It is a small runtime feature layered above the
existing parser, statement context, diagnostics, result builder, and
handle-local session state.

## Sources

- MySQL 8.4 Reference Manual, user-defined variables:
  <https://dev.mysql.com/doc/refman/8.4/en/user-variables.html>
- MySQL 8.4 Reference Manual, `SET` variable assignment:
  <https://dev.mysql.com/doc/refman/8.4/en/set-variable.html>
- MySQL 8.4.9 runtime probes in
  `packages/libmylite/tests/mysql_baseline_user_variables_expectations.sh`
- MyLite SQLite source snapshot notes: `third_party/sqlite/README.md`

The MyLite grammar and implementation are independently authored from the
official documentation, observed MySQL 8.4.9 behavior, and existing MyLite
patterns.

## MySQL 8.4.9 Observations

Runtime probes verified the following behavior for this slice:

- `SELECT @missing` returns `NULL` without an unknown-variable diagnostic.
- User variable names are case-insensitive: `@Foo`, `@foo`, and `@FOO` resolve
  to the same session value.
- Unquoted variable-name bytes are limited to alphanumeric characters, `.`, `_`,
  and `$`; quoted forms such as ``@`dash-name``` and `@'sp ace'` are accepted.
- Names longer than 64 UTF-8 characters fail with `3061 / 42000`.
- `SET @a = 1`, `SET @b := 'x'`, and comma-separated user-variable assignments
  succeed and report `ROW_COUNT() == 0`.
- `SET @old_sql_mode = @@sql_mode, sql_mode = 'NO_ENGINE_SUBSTITUTION'`
  evaluates atomically and left-to-right for the successful observed subset.
- `SET sql_mode = @old_sql_mode` restores from a user variable containing a
  string-compatible system-variable value.
- If one assignment in a `SET` list fails, MySQL leaves prior assignments in
  the same statement unapplied.
- `DEFAULT` is not admitted as a user-variable assignment expression.
- Selecting a user variable returns a result column labeled from the source
  expression unless an alias is present.

## Supported Surface

### Names

Supported user variables:

- unquoted `@name` where `name` consists of ASCII letters, ASCII digits, `.`,
  `_`, and `$`;
- quoted ``@`name``` and `@'name'` forms decoded as ASCII/UTF-8 names without
  embedded `NUL` bytes;
- double-quoted `@"name"` only while later lexer modes still treat double
  quotes as strings, matching the existing lexer policy;
- names from 1 through 64 UTF-8 characters after removing the leading `@` and decoding the
  quoted form.

Variable names are folded ASCII case-insensitively for lookup. The original
source spelling is still used for scalar result labels.

### Values

Supported assignment values:

- `NULL`;
- decimal integer literals with optional unary `+` or `-`;
- ordinary string literals without embedded `NUL` bytes;
- boolean keywords `TRUE`, `FALSE`, `ON`, and `OFF`;
- existing scalar system variables, such as `@@sql_mode`;
- existing user variables, such as `@old_sql_mode`;
- parenthesized supported values.

The value stored in a user variable is MyLite-owned session data. Integer and
boolean values are stored in their MySQL-visible text form (`1`, `0`, `-2`,
etc.); string values are stored after MyLite string-literal decoding; `NULL`
is stored as SQL `NULL`. This keeps scalar result readback and system-variable
restoration deterministic without adding a new public value type.

The following remain unsupported: decimal/floating literals, hex/bit literals,
temporal literals, JSON values, arbitrary arithmetic assignment expressions,
function calls, subqueries, parameters, column references, assignment outside
`SET`, and binary-string values containing embedded `NUL` bytes.

### SET

Supported assignment forms:

```sql
SET @name = user_variable_value
SET @name := user_variable_value
SET @name = user_variable_value, @other := user_variable_value
SET @old_sql_mode = @@sql_mode, sql_mode = 'NO_ENGINE_SUBSTITUTION'
SET sql_mode = @old_sql_mode
```

The assignment list may mix the existing supported system-variable targets and
new user-variable targets. System-variable assignments keep their existing
scope and mutability rules. The new behavior is that a system-variable value
may also be a supported user-variable reference.

`SET NAMES`, `SET CHARACTER SET`, and `SET TRANSACTION` keep their separate
statement shapes and are not folded into the general assignment-list AST.

MyLite snapshots the relevant handle-local session state before applying the
assignment list. If any assignment fails, the snapshot is restored, so no user
variables or system variables from that `SET` statement are left changed.

### Scalar Reads

Supported scalar user-variable reads:

```sql
SELECT @name
SELECT @name AS label
SELECT @name FROM DUAL
DO @name
```

User variables participate in the existing limited no-source scalar-expression
pipeline. This means simple current scalar functions that already accept scalar
operands may accept user-variable values when their existing operand conversion
accepts the stored text/`NULL` value. This slice does not add table-backed user
variables in predicates, ordering, assignments, generated expressions, check
constraints, defaults, or descriptor-driven DML values.

Uninitialized variables read as `NULL`.

## Grammar

The MyLite Lemon grammar is extended with new nodes for user variables and
general `SET` assignment lists:

```lemon
statement ::= set_assignment_statement.

set_assignment_statement ::= SET set_assignment_list.
set_assignment_list ::= set_assignment.
set_assignment_list ::= set_assignment_list COMMA set_assignment.

set_assignment ::= set_system_variable_target EQUAL set_system_variable_value.
set_assignment ::= user_variable EQUAL user_variable_set_value.
set_assignment ::= user_variable ASSIGN user_variable_set_value.

user_variable ::= USER_VARIABLE.

set_system_variable_value ::= DEFAULT.
set_system_variable_value ::= SYSTEM.
set_system_variable_value ::= UTC.
set_system_variable_value ::= SERIALIZABLE.
set_system_variable_value ::= literal.
set_system_variable_value ::= PLUS INTEGER.
set_system_variable_value ::= MINUS INTEGER.
set_system_variable_value ::= user_variable.
set_system_variable_value ::= SYSTEM_VARIABLE.
set_system_variable_value ::= LPAREN set_system_variable_value RPAREN.

user_variable_set_value ::= NULL.
user_variable_set_value ::= TRUE.
user_variable_set_value ::= FALSE.
user_variable_set_value ::= ON.
user_variable_set_value ::= OFF.
user_variable_set_value ::= INTEGER.
user_variable_set_value ::= PLUS INTEGER.
user_variable_set_value ::= MINUS INTEGER.
user_variable_set_value ::= STRING_LITERAL.
user_variable_set_value ::= user_variable.
user_variable_set_value ::= SYSTEM_VARIABLE.
user_variable_set_value ::= LPAREN user_variable_set_value RPAREN.

expression ::= USER_VARIABLE.
```

`DEFAULT` remains admitted only for supported system-variable assignments; it is
not a user-variable assignment value.

`set_system_variable_statement` becomes the one-assignment specialization of
`set_assignment_statement` at the AST/runtime layer. The existing public parser
API remains unchanged.

## Architecture

- Public API: no ABI or header change.
- Statement context: no new transaction, catalog, or file-format state.
- Lexer/parser/AST: lexer already tokenizes `USER_VARIABLE`; parser adds AST
  support and assignment lists.
- Runtime session state: owns an in-memory growable vector of user variables
  per `mylite_db` handle. Values are freed on close and are not durable.
- Analyzer/planner: no SQLite schema analysis is introduced. System-variable
  assignment targets continue to use the existing resolver.
- Catalog/storage/VFS: no changes. User variables do not mutate catalog rows,
  descriptors, generation counters, SQLite schema, or `.mylite` payload layout.
- SQLite physical execution: none. This is MyLite runtime/session behavior.
- Result builder: scalar `SELECT` uses the existing result object conventions;
  successful `SET`/`DO` statements return no rows with affected rows `0`.

## Diagnostics

The slice covers these diagnostics:

- syntax errors for malformed variable tokens or nonadmitted grammar;
- `3061 / 42000` for decoded user-variable names longer than 64 UTF-8 characters;
- existing unknown/read-only/unsupported system-variable diagnostics for
  system targets;
- existing value diagnostics for system variables restored from incompatible
  user-variable values;
- deterministic MyLite unsupported diagnostics for nonadmitted user-variable
  assignment expressions;
- allocation failures as `MYLITE_NOMEM`;
- public API misuse remains unchanged.

Supported assignments emit no warnings. Existing system-variable assignments
keep their current warning behavior, such as SQL-mode deprecation warnings and
auto-increment clamp warnings.

## Performance

User variables are stored in a small per-handle vector and looked up linearly.
This is acceptable for dump prologues and common session scratch variables.
The feature does not add row materialization, SQLite query interception, or
extra scans. If applications begin using many user variables, the vector can be
replaced by a small case-folded map without changing public behavior.

## Tests

Implementation must add:

- parser coverage for `@var` expressions, quoted names, `:=`, assignment lists,
  and mixed system/user assignment lists;
- runtime tests for uninitialized `NULL`, case-insensitive lookup,
  quoted names, 64-character name limit, unsupported 65-character names, assignment
  lists, `ROW_COUNT()`, warning/error counts, `DO`, scalar `SELECT`,
  independent handles, close/reopen nonpersistence, and zero-initialized
  cleanup;
- system-variable save/restore tests for `sql_mode`, `time_zone`,
  `foreign_key_checks`, `unique_checks`, and `sql_notes` where current MyLite
  assignment rules admit the value;
- atomic failure tests proving a failing assignment list leaves both user
  variables and system variables unchanged;
- MySQL 8.4.9 expectation script coverage for all user-visible behavior in
  this slice.

## Out of Scope

SQL prepared statements, `PREPARE ... FROM @var`, `EXECUTE ... USING @var`,
Performance Schema user-variable tables, assignment outside `SET`, client-side
evaluation quirks, table-backed user-variable expressions, identifier
substitution, `LIMIT @n`, binary user-variable values, full expression
assignment, subqueries, parameters, and stored-program variable interaction are
deferred.
