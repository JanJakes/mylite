# `group_concat_max_len` System Variable

## Scope

This feature implements a focused executable slice of the MySQL
`group_concat_max_len` system variable:

- `SHOW [SESSION|LOCAL] VARIABLES LIKE 'group_concat_max_len'`
- `SHOW GLOBAL VARIABLES LIKE 'group_concat_max_len'`
- `SET group_concat_max_len = unsigned_integer`
- `SET SESSION group_concat_max_len = unsigned_integer`
- `SET LOCAL group_concat_max_len = unsigned_integer`
- `SET @@group_concat_max_len = unsigned_integer`
- `SET @@SESSION.group_concat_max_len = unsigned_integer`
- `SET @@LOCAL.group_concat_max_len = unsigned_integer`
- `SET ... group_concat_max_len = DEFAULT`
- `GROUP_CONCAT()` truncation and metadata derived from the current session
  value

Deferred surfaces:

- mutable process-global `group_concat_max_len`
- persisted variables and startup options
- `SELECT @@group_concat_max_len` and related system-variable expressions
- arbitrary assignment expressions beyond signed integer literals and `DEFAULT`
- `SET_VAR(group_concat_max_len=...)` optimizer hints
- `max_allowed_packet` interaction
- exact binary/nonbinary result-type selection for binary arguments

## Compatibility Sources

- MySQL 8.4 Reference Manual, Server System Variables:
  https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html
- MySQL 8.4 Reference Manual, Aggregate Function Descriptions:
  https://dev.mysql.com/doc/refman/8.4/en/aggregate-functions.html
- Runtime observations verified against `mylite-mysql-849`, MySQL `8.4.9`.

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar or
implementation sources.

## MySQL 8.4.9 Runtime Observations

| SQL | Result |
| --- | --- |
| `SHOW VARIABLES LIKE 'group_concat_max_len'` | Returns `group_concat_max_len`, `1024` by default. |
| `SHOW GLOBAL VARIABLES LIKE 'group_concat_max_len'` | Returns the global value, `1024` by default. |
| `SET SESSION group_concat_max_len = 4` | Succeeds and changes the current session value to `4`. |
| `SET @@SESSION.group_concat_max_len = 5` | Succeeds and changes the current session value to `5`. |
| `SET @@LOCAL.group_concat_max_len = 6` | Succeeds and changes the current session value to `6`. |
| `SET group_concat_max_len = DEFAULT` | Restores the current session value to `1024`. |
| `SET group_concat_max_len = 0` | Succeeds, stores `4`, and emits warning 1292: `Truncated incorrect group_concat_max_len value: '0'`. |
| `SET group_concat_max_len = 3` | Succeeds, stores `4`, and emits warning 1292: `Truncated incorrect group_concat_max_len value: '3'`. |
| `SET group_concat_max_len = -1` | Succeeds, stores `4`, and emits warning 1292: `Truncated incorrect group_concat_max_len value: '-1'`. |
| `SET group_concat_max_len = '7'` | Fails with error 1232: `Incorrect argument type to variable 'group_concat_max_len'`. |
| `SET group_concat_max_len = 4.9` | Fails with error 1232. |
| `SET group_concat_max_len = NULL` | Fails with error 1232. |
| `SET SESSION group_concat_max_len = 4; SELECT GROUP_CONCAT(x ORDER BY id SEPARATOR ',') ...` | Returns the first four bytes and emits warning 1260: `Row 1 was cut by GROUP_CONCAT()`. |
| `SET GLOBAL group_concat_max_len = 8` | Changes the global value but does not change the current session value. |

## Syntax

MyLite owns the grammar below; it is intentionally authored for MyLite's Lemon
parser rather than copied from MySQL sources:

```lemon
statement ::= set_system_variable_statement.

set_system_variable_statement ::= SET opt_set_system_variable_scope
                                  set_system_variable_name EQ
                                  set_system_variable_value.

opt_set_system_variable_scope ::= .
opt_set_system_variable_scope ::= SESSION.
opt_set_system_variable_scope ::= LOCAL.
opt_set_system_variable_scope ::= GLOBAL.

set_system_variable_name ::= IDENTIFIER.
set_system_variable_name ::= QUOTED_IDENTIFIER.
set_system_variable_name ::= SYSTEM_VARIABLE.

set_system_variable_value ::= literal.
set_system_variable_value ::= PLUS numeric_literal.
set_system_variable_value ::= MINUS numeric_literal.
set_system_variable_value ::= DEFAULT.
set_system_variable_value ::= REPLACE LPAREN set_system_variable_name COMMA STRING COMMA STRING RPAREN.
```

The `REPLACE(...)` value production exists for the already-supported
`SET sql_mode = REPLACE(@@SESSION.sql_mode, 'ONLY_FULL_GROUP_BY', '')` idiom.
For `group_concat_max_len`, only signed integer literals and `DEFAULT` mutate
state in this slice. Other literal kinds parse so execution can return the
MySQL-compatible type error.

## AST

Add a `set_system_variable_statement` AST node with:

- a scope marker:
  - omitted, `SESSION`, and `LOCAL` normalize to session scope
  - `GLOBAL` is preserved so execution can reject unsupported global mutation
- a variable-name child preserving bare identifier, quoted identifier, or
  system-variable token text
- a value child preserving a literal, unary signed numeric literal, `DEFAULT`,
  or the supported `REPLACE(...)` function call

Existing `SET sql_mode` statements move to this general node and retain their
current execution behavior.

## Runtime Semantics

Session state:

- Each `mylite_db` handle stores a session `group_concat_max_len`.
- The default value is `1024`.
- `SET ... = DEFAULT` restores the session value to `1024`.
- The minimum value is `4`.
- Values below `4`, including negative signed literals, store `4` and append
  warning 1292 with the assigned text in the message.
- Non-integer values fail with error 1232 and leave the prior value unchanged.
- Omitted scope, `SESSION`, `LOCAL`, `@@var`, `@@SESSION.var`, and
  `@@LOCAL.var` all target the session value.
- `GLOBAL` and `@@GLOBAL.var` parse but return an unsupported diagnostic in
  this slice because MyLite has no process-global mutable variable state yet.

`SHOW VARIABLES`:

- Session and local `SHOW VARIABLES` expose the current session value.
- Global `SHOW VARIABLES` exposes MyLite's fixed global default, `1024`.
- Rows retain the existing `Variable_name` / `Value` metadata and ordering.

`GROUP_CONCAT()`:

- Runtime truncation uses the current session value in bytes.
- The existing warning 1260 message is preserved when truncation occurs.
- Result metadata length is derived from the session value multiplied by the
  connection character-set maximum byte length.
- When `group_concat_max_len <= 512`, MyLite reports `VAR_STRING` metadata for
  current nonbinary `GROUP_CONCAT()` results. Larger values report `BLOB`
  metadata. Full binary-argument metadata remains deferred.

## Storage And Performance

The value is handle-owned session state and requires no file-format change.
`GROUP_CONCAT()` already accumulates result text in memory; this feature only
replaces the fixed cap with a session cap. Very large values may still exhaust
memory before reaching the configured limit; spilling large aggregates to
storage remains deferred.

## Tests

Parser coverage:

- `SET group_concat_max_len = 4`
- `SET SESSION group_concat_max_len = 4`
- `SET SESSION group_concat_max_len = DEFAULT`
- `SET LOCAL @@session.group_concat_max_len = -1`
- `SET @@LOCAL.group_concat_max_len = 5`
- existing `SET sql_mode` forms through the generalized AST node
- unsupported global forms parse for deterministic execution diagnostics

Runtime coverage:

- default `SHOW VARIABLES LIKE 'group_concat_max_len'`
- default `SHOW GLOBAL VARIABLES LIKE 'group_concat_max_len'`
- session assignment updates `SHOW VARIABLES`
- global/default values remain independent
- `DEFAULT` restores `1024`
- values below `4` clamp to `4` with warning 1292
- string, decimal, and `NULL` values fail with error 1232 and preserve the
  prior value
- table-backed `GROUP_CONCAT()` truncates to the session value and emits warning
  1260
- no-table `GROUP_CONCAT()` uses the same session value
- metadata length and type follow the current session value

## Deferred Work

- Mutable process-global state and new-handle inheritance.
- `SET GLOBAL group_concat_max_len` execution.
- `SELECT @@group_concat_max_len` expression forms.
- Arbitrary assignment expressions and system-variable references in assignment
  values.
- `SET PERSIST`, persisted configuration, and startup-option integration.
- `SET_VAR` optimizer hints.
- Exact binary-string result metadata and `max_allowed_packet` interaction.
