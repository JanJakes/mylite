# Baseline Transaction System Variables

## Summary

This phase exposes MySQL's `transaction_isolation` and
`transaction_read_only` system variables on top of MyLite's existing
`SET TRANSACTION`, `START TRANSACTION`, and read-only transaction state.

The variables are connection-local compatibility state. They do not add new
SQLite isolation behavior, MVCC snapshots, lock modes, wire-protocol status
flags, or server-global mutable defaults. Direct session assignments update
the same session defaults already used by `SET SESSION TRANSACTION`; direct
`@@variable` assignments update the same next-transaction characteristics
already used by `SET TRANSACTION`.

## Compatibility Authority

- Official MySQL 8.4 documentation:
  - server system variables:
    <https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html>
  - `SET TRANSACTION`:
    <https://dev.mysql.com/doc/refman/8.4/en/set-transaction.html>
  - transaction control:
    <https://dev.mysql.com/doc/refman/8.4/en/commit.html>
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_transaction_system_variables_expectations.sh`.

Runtime probes show:

- default scalar readback is `@@transaction_isolation = 'REPEATABLE-READ'`
  and `@@transaction_read_only = 0`;
- default `SHOW VARIABLES` rows are `transaction_isolation = REPEATABLE-READ`
  and `transaction_read_only = OFF`;
- `GLOBAL`, `SESSION`, and `LOCAL` scalar scopes are accepted;
- `SET transaction_isolation = ...`, `SET SESSION ...`, `SET LOCAL ...`,
  `SET @@SESSION...`, and `SET @@LOCAL...` set the session default;
- `SET @@transaction_isolation = ...` and
  `SET @@transaction_read_only = ...` set next-transaction characteristics,
  do not change scalar readback, and are rejected in an active transaction
  with error `1568 / 25001`;
- session assignment inside an active transaction is allowed and does not
  affect that active transaction;
- supported assignments return affected rows `0` and warning count `0`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## Scope

This feature adds:

- scalar reads for:
  - `@@transaction_isolation`,
    `@@GLOBAL.transaction_isolation`,
    `@@SESSION.transaction_isolation`, and
    `@@LOCAL.transaction_isolation`;
  - `@@transaction_read_only`,
    `@@GLOBAL.transaction_read_only`,
    `@@SESSION.transaction_read_only`, and
    `@@LOCAL.transaction_read_only`;
- case-insensitive variable-name and scope matching, including quoted final
  variable-name components already admitted by the system-variable parser;
- `SHOW VARIABLES`, `SHOW SESSION VARIABLES`, `SHOW LOCAL VARIABLES`, and
  `SHOW GLOBAL VARIABLES` rows for both variables;
- `SHOW VARIABLES LIKE ...` and existing limited `SHOW VARIABLES WHERE`
  filtering over the two new rows;
- session/default assignment forms:
  - `SET transaction_isolation = value`;
  - `SET SESSION transaction_isolation = value`;
  - `SET LOCAL transaction_isolation = value`;
  - `SET @@SESSION.transaction_isolation = value`;
  - `SET @@LOCAL.transaction_isolation = value`;
  - the same forms for `transaction_read_only`;
- next-transaction assignment forms:
  - `SET @@transaction_isolation = value`;
  - `SET @@transaction_read_only = value`;
- exact no-op global assignments that preserve MyLite's fixed global defaults:
  - `SET GLOBAL transaction_isolation = DEFAULT`;
  - `SET GLOBAL transaction_isolation = 'REPEATABLE-READ'`;
  - `SET @@GLOBAL.transaction_isolation = DEFAULT`;
  - `SET @@GLOBAL.transaction_isolation = 'REPEATABLE-READ'`;
  - `SET GLOBAL transaction_read_only = DEFAULT`;
  - `SET GLOBAL transaction_read_only = OFF`;
  - `SET GLOBAL transaction_read_only = 0`;
  - `SET @@GLOBAL.transaction_read_only = DEFAULT`;
  - equivalent `FALSE` no-op forms;
- deterministic MyLite-specific rejection for global assignments that would
  change the fixed global defaults;
- warning count `0`, affected rows `0`, and no result rows for supported
  assignments according to existing non-query result conventions.

## Non-Goals

This feature does not implement:

- mutable server-global transaction defaults shared by future handles;
- startup options, persisted variables, `SET PERSIST`, `SET PERSIST_ONLY`,
  privilege checks, Performance Schema variable tables, or protocol state;
- new isolation, MVCC, row-lock, gap-lock, deadlock, lock-wait, snapshot, or
  serializability semantics beyond MyLite's existing SQLite-backed execution;
- full expression evaluation for `SET` values;
- SQL-standard unquoted multi-token values such as
  `SET transaction_isolation = READ COMMITTED`, which MySQL rejects for direct
  variable assignment;
- arbitrary boolean strings beyond the tested direct `transaction_read_only`
  subset;
- SQLite fork patches.

## Ownership Boundary

- Public API remains unchanged. `mylite_execute()` returns existing scalar,
  `SHOW`, or non-query result handles and diagnostics.
- Session state owns connection-local transaction defaults, pending
  next-transaction characteristics, and active transaction access mode.
- Statement context owns statement result classification, previous diagnostics,
  warning counts, affected rows, and row-count side effects.
- Lexer/parser/AST admit one additional direct `SET` value token,
  `SERIALIZABLE`, so MyLite can represent MySQL's direct
  `SET transaction_isolation = SERIALIZABLE` form.
- Runtime owns the static system-variable descriptors, scope rules, value
  conversion, direct assignment behavior, fixed global default readback,
  no-op global assignment validation, and `SHOW VARIABLES` display values.
- Catalog descriptors remain authoritative for schema/table metadata. These
  variables are runtime session metadata only.
- Result builder owns scalar and `SHOW VARIABLES` result materialization.
- Storage, VFS, file format, and SQLite physical storage are not involved.

## Syntax

The admitted SQL surface is:

```ebnf
system_variable_expr:
    @@ transaction_isolation
  | @@ GLOBAL . transaction_isolation
  | @@ SESSION . transaction_isolation
  | @@ LOCAL . transaction_isolation
  | @@ transaction_read_only
  | @@ GLOBAL . transaction_read_only
  | @@ SESSION . transaction_read_only
  | @@ LOCAL . transaction_read_only

set_system_variable_statement:
    SET transaction_isolation = transaction_isolation_value
  | SET SESSION transaction_isolation = transaction_isolation_value
  | SET LOCAL transaction_isolation = transaction_isolation_value
  | SET GLOBAL transaction_isolation = transaction_isolation_value
  | SET @@ transaction_isolation = transaction_isolation_value
  | SET @@ GLOBAL . transaction_isolation = transaction_isolation_value
  | SET @@ SESSION . transaction_isolation = transaction_isolation_value
  | SET @@ LOCAL . transaction_isolation = transaction_isolation_value
  | SET transaction_read_only = transaction_read_only_value
  | SET SESSION transaction_read_only = transaction_read_only_value
  | SET LOCAL transaction_read_only = transaction_read_only_value
  | SET GLOBAL transaction_read_only = transaction_read_only_value
  | SET @@ transaction_read_only = transaction_read_only_value
  | SET @@ GLOBAL . transaction_read_only = transaction_read_only_value
  | SET @@ SESSION . transaction_read_only = transaction_read_only_value
  | SET @@ LOCAL . transaction_read_only = transaction_read_only_value

transaction_isolation_value:
    DEFAULT
  | string_literal
  | SERIALIZABLE

transaction_read_only_value:
    DEFAULT
  | TRUE
  | FALSE
  | ON
  | OFF
  | 0
  | 1
  | string_literal

show_variables_statement:
    SHOW [GLOBAL | SESSION | LOCAL] VARIABLES [LIKE string_literal]
  | SHOW [GLOBAL | SESSION | LOCAL] VARIABLES WHERE show_variables_where_predicate
```

### MyLite Lemon-Syntax Snippet

The existing grammar remains sufficient except for admitting `SERIALIZABLE` as
a direct system-variable value:

```lemon
expr(A) ::= SYSTEM_VARIABLE(T). {
    A = mylite_sql_parser_make_system_variable_expr(state, T);
}

set_statement(A) ::=
    SET(S) set_system_variable_target(T) EQUALS(E) set_system_variable_value(V). {
    A = mylite_sql_parser_make_set_system_variable_statement(state, S, T, E, V);
}

set_system_variable_value(A) ::= SERIALIZABLE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}

show_variables_statement(A) ::=
    SHOW(S) show_variables_scope_opt(O) VARIABLES(V) show_like_or_where_opt(F). {
    A = mylite_sql_parser_make_show_variables_statement(state, S, O, V, F);
}
```

These snippets are independently authored for MyLite's admitted subset and are
not MySQL's full grammar.

## Semantics

Each connection starts with session defaults:

```text
transaction_isolation: REPEATABLE-READ
transaction_read_only: 0 / OFF
```

Global scalar reads and `SHOW GLOBAL VARIABLES` report fixed defaults:
`REPEATABLE-READ` and `OFF`. MyLite does not maintain mutable global
transaction defaults. Existing and future handles therefore continue to start
with the fixed defaults.

Session and local scalar reads report the connection's session default. They
do not expose pending next-transaction characteristics and do not expose the
active transaction's access mode. This matches MySQL 8.4.9 observations where
`SET @@transaction_read_only = ON` leaves `@@transaction_read_only` at the
session value until a session assignment changes it.

Session/default assignment forms update the connection's session defaults.
They are permitted inside active transactions and do not alter the active
transaction. If no explicit user transaction is active, assigning a session
default clears any pending next-transaction value for the same characteristic,
matching the existing `SET SESSION TRANSACTION` behavior.

`SET @@transaction_isolation = value` and
`SET @@transaction_read_only = value` update pending next-transaction
characteristics. They are rejected inside an active explicit user transaction
with `1568 / 25001`. They do not change scalar readback. The next successful
explicit transaction start or write statement uses and consumes the pending
characteristic. Runtime probes show that a scalar `SELECT` after direct
`@@transaction_read_only` or `@@transaction_isolation` assignment does not
consume the pending direct system-variable characteristic, unlike MyLite's
earlier verified `SET TRANSACTION READ ONLY; SELECT ...` baseline. A failed
persistent write in a pending read-only transaction does not consume the
pending characteristic.

The admitted `transaction_isolation` values are:

```text
DEFAULT          -> REPEATABLE_READ
'REPEATABLE-READ' -> REPEATABLE_READ
'READ-COMMITTED'  -> READ_COMMITTED
'READ-UNCOMMITTED' -> READ_UNCOMMITTED
'SERIALIZABLE'   -> SERIALIZABLE
SERIALIZABLE      -> SERIALIZABLE
```

Matching is ASCII case-insensitive. Space-separated string values such as
`'READ COMMITTED'`, `NULL`, numeric values, and unsupported identifiers are
rejected with MySQL-style `1231 / 42000` diagnostics.

The admitted `transaction_read_only` values are:

```text
DEFAULT -> READ WRITE
OFF, FALSE, 0, 'OFF' -> READ WRITE
ON, TRUE, 1, 'ON' -> READ ONLY
```

Matching for string values is ASCII case-insensitive. `NULL`, integers other
than `0` and `1`, and unsupported strings are rejected with MySQL-style
`1231 / 42000` diagnostics.

Supported assignments do not mutate selected schema, catalog generation,
SQLite schema generation, file contents, or diagnostics beyond normal
successful-statement diagnostics.

## Diagnostics

| Condition | Diagnostic |
| --- | --- |
| Unknown variable spelling outside the descriptor registry | Existing unknown-system-variable diagnostic |
| Supported session/default/local assignment | Success, affected rows `0`, warning count `0` |
| Supported next-transaction assignment outside active transaction | Success, affected rows `0`, warning count `0` |
| Next-transaction assignment inside active transaction | `1568 / 25001`, `Transaction characteristics can't be changed while a transaction is in progress` |
| No-op global assignment to fixed default | Success, affected rows `0`, warning count `0` |
| Global assignment that would change the fixed default | Existing unsupported-statement diagnostic with a transaction-variable no-op message |
| Invalid isolation value | `1231 / 42000`, `Variable 'transaction_isolation' can't be set to the value of '...'` |
| Invalid read-only value | `1231 / 42000`, `Variable 'transaction_read_only' can't be set to the value of '...'` |
| Unsupported quoted scope such as ``@@`global`.transaction_isolation`` | Existing unsupported quoted system-variable scope diagnostic |
| Unsupported `SHOW VARIABLES` syntax | Existing `SHOW VARIABLES` parser diagnostics |
| Allocation failure while building results | Existing MyLite allocation failure diagnostics |
| Public API misuse | Existing public API misuse behavior |

Supported reads and assignments produce `warning_count == 0`.

## Physical SQLite Handling

No generated SQLite SQL is needed for the variables themselves. Assignment
updates connection-local MyLite session state only.

Read-only write rejection and transaction start behavior continue to use the
existing `SET TRANSACTION` state paths. Isolation assignments do not change
SQLite control SQL. Storage/VFS and `.mylite` preamble invariants are
unchanged.

## Tests

Fast C tests cover:

- scalar unscoped, global, session, and local reads for both variables;
- scalar case-insensitive and quoted final variable-name reads;
- `SHOW` default/session/local/global visibility;
- `SHOW LIKE` and `SHOW WHERE` filtering over both new rows;
- session/default/local assignment for isolation and read-only values;
- direct `@@` next-transaction assignment and active-transaction diagnostics;
- session assignment inside an active transaction not affecting the active
  transaction;
- read-only persistent DML rejection through direct `@@transaction_read_only`
  and direct session `transaction_read_only`;
- direct `@@transaction_isolation` feeding existing
  `START TRANSACTION WITH CONSISTENT SNAPSHOT` warning behavior;
- invalid value diagnostics;
- exact no-op global assignments and deterministic rejected global changes;
- unchanged statement row-count behavior, file preamble, catalog generation,
  SQLite schema generation, and independent handles.

The MySQL expectation script records MySQL 8.4.9 runtime behavior for the same
user-visible subset plus upstream mutable-global evidence that MyLite
intentionally does not implement in this slice.

## Compatibility Notes

MyLite intentionally reports and stores transaction variables as runtime
compatibility state. The variables line up with existing transaction
characteristics, but they do not imply MySQL's full concurrent isolation model
until that model is designed and implemented separately.
