# Baseline max_allowed_packet System Variable

## Summary

This phase exposes MySQL's `max_allowed_packet` system variable as a fixed
MyLite runtime capability probe.

MyLite reports the MySQL 8.4.9 default value `67108864` for scalar `@@` reads
and `SHOW VARIABLES`. The value is metadata only: it does not enforce protocol
packet sizes, string length caps, generated intermediate string limits, C API
long-data limits, or result buffering limits.

## Compatibility Authority

- Official MySQL 8.4 documentation:
  - server system variables:
    <https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html>
  - using system variables:
    <https://dev.mysql.com/doc/refman/8.4/en/using-system-variables.html>
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_max_allowed_packet_system_variable_expectations.sh`.

The MySQL 8.4 manual defines `max_allowed_packet` as an integer variable with
global and session scope, default value `67108864`, minimum value `1024`,
maximum value `1073741824`, unit bytes, and block size `1024`. Runtime probes
show that scalar reads for no scope, `GLOBAL`, `SESSION`, and `LOCAL` return
the current session/global values; `SHOW VARIABLES` and `SHOW GLOBAL
VARIABLES` display decimal text; session/local/no-scope assignment is rejected
as a read-only session variable with error `1621`; and global assignment is
mutable upstream.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## Scope

This feature adds:

- scalar reads for `@@max_allowed_packet`,
  `@@GLOBAL.max_allowed_packet`, `@@SESSION.max_allowed_packet`, and
  `@@LOCAL.max_allowed_packet`, returning `67108864`;
- case-insensitive variable-name and scope matching for scalar reads, including
  quoted final variable-name components already admitted by existing
  system-variable parsing;
- `SHOW VARIABLES`, `SHOW SESSION VARIABLES`, `SHOW LOCAL VARIABLES`, and
  `SHOW GLOBAL VARIABLES` rows for `max_allowed_packet`, displaying
  `67108864`;
- `SHOW VARIABLES LIKE 'max_allowed_packet'` and existing limited
  `SHOW VARIABLES WHERE` filtering over the new row;
- MySQL-compatible read-only diagnostics for no-scope, session, local,
  `@@`, `@@SESSION`, and `@@LOCAL` assignment targets;
- limited no-op `SET GLOBAL max_allowed_packet = 67108864`,
  `SET @@GLOBAL.max_allowed_packet = 67108864`, and `DEFAULT` global
  assignments that preserve MyLite's fixed value;
- deterministic MyLite-specific rejection for global assignments that would
  change the fixed value;
- warning count `0`, affected rows `0`, and no result rows for supported
  no-op global assignments according to existing non-query result conventions.

## Non-Goals

This feature does not implement:

- mutable global or per-session `max_allowed_packet` state, startup options,
  persisted variables, privilege checks, `SET_VAR` hints, or Performance
  Schema variable tables;
- packet-size, generated-string, result-buffer, parameter-size, BLOB/TEXT, or
  long-data enforcement;
- value rounding to `1024` blocks for mutable assignment, warnings for rounded
  global values, or runtime maximum/minimum clamping;
- numeric expressions in `SET` values, suffix forms such as `16M`, string,
  decimal, float, hex, bit, boolean, or `NULL` assignment values;
- SQLite fork patches.

## Ownership Boundary

- Public API remains unchanged. `mylite_execute()` returns existing scalar,
  row, or non-query result handles and diagnostics.
- Statement context owns result classification, affected rows, previous
  diagnostics, warning count, and row-count side effects.
- Lexer/parser/AST require no new syntax. Existing system-variable scalar,
  `SET`, and `SHOW VARIABLES` nodes already represent the admitted SQL shapes.
- Runtime owns the static system-variable descriptor, scope rules, fixed value
  formatting, session-read-only diagnostics, no-op global assignment
  validation, and `SHOW VARIABLES` display value.
- Catalog descriptors remain authoritative for schema/table metadata. This
  variable is runtime metadata only.
- Result builder owns scalar and `SHOW VARIABLES` result materialization.
- Storage, VFS, file format, and SQLite physical storage are not involved.

## Syntax

No grammar expansion is needed. The feature uses the existing MyLite grammar
for scalar system-variable expressions, system-variable `SET`, and
`SHOW VARIABLES`.

The admitted SQL surface is:

```ebnf
system_variable_expr:
    @@ max_allowed_packet
  | @@ GLOBAL . max_allowed_packet
  | @@ SESSION . max_allowed_packet
  | @@ LOCAL . max_allowed_packet

set_system_variable_statement:
    SET max_allowed_packet = set_system_variable_value
  | SET SESSION max_allowed_packet = set_system_variable_value
  | SET LOCAL max_allowed_packet = set_system_variable_value
  | SET GLOBAL max_allowed_packet = set_system_variable_value
  | SET @@ max_allowed_packet = set_system_variable_value
  | SET @@ GLOBAL . max_allowed_packet = set_system_variable_value
  | SET @@ SESSION . max_allowed_packet = set_system_variable_value
  | SET @@ LOCAL . max_allowed_packet = set_system_variable_value

show_variables_statement:
    SHOW [GLOBAL | SESSION | LOCAL] VARIABLES [LIKE string_literal]
  | SHOW [GLOBAL | SESSION | LOCAL] VARIABLES WHERE show_variables_where_predicate
```

### MyLite Lemon-Syntax Snippet

The existing grammar remains sufficient:

```lemon
expr(A) ::= SYSTEM_VARIABLE(T). {
    A = mylite_sql_parser_make_system_variable_expr(state, T);
}

set_statement(A) ::=
    SET(S) set_system_variable_target(T) EQUALS(E) set_system_variable_value(V). {
    A = mylite_sql_parser_make_set_system_variable_statement(state, S, T, E, V);
}

show_variables_statement(A) ::=
    SHOW(S) show_variables_scope_opt(O) VARIABLES(V) show_like_or_where_opt(F). {
    A = mylite_sql_parser_make_show_variables_statement(state, S, O, V, F);
}
```

These snippets are independently authored for MyLite's admitted subset and are
not MySQL's full grammar.

## Semantics

`max_allowed_packet` is fixed:

```text
Scalar @@ read: 67108864
SHOW VARIABLES value: 67108864
```

Unscoped, `GLOBAL`, `SESSION`, and `LOCAL` scalar reads return `67108864`.
`SHOW VARIABLES`, `SHOW SESSION VARIABLES`, `SHOW LOCAL VARIABLES`, and
`SHOW GLOBAL VARIABLES` include the row. `SHOW VARIABLES WHERE` evaluates
predicates over the row using existing `Variable_name` and `Value` text
semantics.

No-scope, `SESSION`, `LOCAL`, `@@`, `@@SESSION`, and `@@LOCAL` assignment
targets fail with `1621 / HY000` and a message containing
`SESSION variable 'max_allowed_packet' is read-only. Use SET GLOBAL to assign the value`.

`GLOBAL` and `@@GLOBAL` assignments are accepted only when the value is
`DEFAULT`, decimal integer literal `67108864`, or unary-plus decimal integer
literal `+67108864`. These assignments are no-ops: they do not create mutable
global state and do not change the value observed by existing or future
handles. Other admitted parser values fail with a deterministic unsupported
assignment diagnostic.

Supported reads and no-op global assignments do not mutate session SQL mode,
selected schema, catalog generation, SQLite schema generation, file contents,
or diagnostics beyond the normal successful-statement diagnostics snapshot.

## Diagnostics

| Condition | Diagnostic |
| --- | --- |
| Unknown variable spelling outside the descriptor registry | Existing unknown-system-variable diagnostic |
| No-scope/session/local assignment target | `1621 / HY000`, session-read-only message |
| Global assignment to `DEFAULT`, `67108864`, or `+67108864` | Success, affected rows `0`, warning count `0` |
| Global assignment to any other currently parsed value | Existing unsupported-statement diagnostic with a `max_allowed_packet` no-op message |
| Unsupported quoted scope such as ``@@`global`.max_allowed_packet`` | Existing unsupported quoted system-variable scope diagnostic |
| Unsupported `SHOW VARIABLES` syntax | Existing `SHOW VARIABLES` parser diagnostics |
| Allocation failure while building results | Existing MyLite allocation failure diagnostics |
| Public API misuse | Existing public API misuse behavior |

Supported reads and no-op global assignments produce `warning_count == 0`.

## Tests

Fast C tests cover:

- scalar unscoped, global, session, and local reads returning `67108864`;
- scalar case-insensitive and quoted variable-name reads;
- `SHOW` default/session/local/global visibility;
- `SHOW LIKE` and `SHOW WHERE` filtering over the new row;
- session-read-only diagnostics for no-scope, session, local, `@@`,
  `@@SESSION`, and `@@LOCAL` assignment targets;
- no-op global assignments with integer and `DEFAULT` values;
- deterministic rejection of non-default global assignment values;
- unchanged statement row-count behavior, file preamble, catalog generation,
  SQLite schema generation, and independent handles.

The MySQL expectation script records the MySQL 8.4.9 runtime behavior for the
same user-visible subset plus the upstream mutable-global evidence that MyLite
intentionally does not implement in this slice.

## Compatibility Notes

MyLite intentionally reports the MySQL default without enforcing packet or
intermediate string sizes. Enforcement belongs with protocol/message-size and
large value handling, not a scalar metadata probe. Until mutable global
system-variable state exists, global `SET` support is limited to exact no-op
assignments that preserve the fixed value.
