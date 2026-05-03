# Session identity information functions

## Scope

This slice implements the MySQL session identity information functions that
fit MyLite's embedded runtime model:

- `CONNECTION_ID()`
- `USER()`
- `SESSION_USER()`
- `SYSTEM_USER()`
- `CURRENT_USER()`
- `CURRENT_USER`

The functions are available anywhere MyLite already evaluates supported scalar
function calls: no-table `SELECT`, table-backed `SELECT` projection, `WHERE`,
`ORDER BY`, and the supported single-table `UPDATE` and `DELETE` expression
paths. `CURRENT_USER` without parentheses is accepted where an ordinary scalar
primary expression is accepted.

Out of scope:

- Server authentication, grant-table lookup, anonymous-account matching, proxy
  users, roles, privileges, definers, and invoker/definer distinctions for
  stored objects. MyLite does not yet implement those server surfaces.
- `pseudo_thread_id` system-variable compatibility for `CONNECTION_ID()`.
- MySQL protocol connection identifiers beyond the embedded handle value.
- Exact MySQL numeric diagnostics for every unsupported arity. MyLite rejects
  unsupported forms deterministically through the current parser/binder status
  model.

## Sources

- MySQL 8.4 Reference Manual, Information Functions:
  https://dev.mysql.com/doc/refman/8.4/en/information-functions.html
- Observed MySQL 8.4.9 runtime behavior from Docker container
  `mylite-mysql-849`.

This specification is independently authored from official MySQL documentation
and observed MySQL 8.4.9 behavior. It does not copy MySQL grammar text,
documentation prose, or implementation sources.

## MySQL 8.4.9 behavior summary

The official information-functions documentation identifies
`SESSION_USER()` and `SYSTEM_USER()` as synonyms for `USER()`. `CURRENT_USER()`
and bare `CURRENT_USER` return the authenticated account, which can differ from
`USER()` on a server with account matching, stored programs, views, triggers, or
events.

Runtime probes against the local MySQL 8.4.9 container showed:

- `CONNECTION_ID()` returns an unsigned integer that is stable for a connection.
- `USER()`, `SESSION_USER()`, and `SYSTEM_USER()` return the client user and
  host string; in the local root probe that value was `root@localhost`.
- `CURRENT_USER()` and bare `CURRENT_USER` returned `root@localhost` in the
  local root probe.
- Function names are case-insensitive.
- `CURRENT_USER ()` with whitespace before parentheses is accepted.
- Table projection, `WHERE`, `ORDER BY`, `UPDATE`, and `DELETE` expression
  contexts accepted these functions in the probed MySQL runtime.
- `CONNECTION_ID(1)` raises error 1582, incorrect parameter count.
- `USER(1)`, `SESSION_USER(1)`, `SYSTEM_USER(1)`, `CURRENT_USER(1)`, and
  `CURRENT_USER(1,2)` are syntax errors in MySQL.
- `CHARSET(USER())`, `CHARSET(CURRENT_USER())`, and
  `CHARSET(SESSION_USER())` return `utf8mb3`; `COLLATION(...)` returns
  `utf8mb3_general_ci`; `COERCIBILITY(...)` returns `3`, under both
  `SET NAMES utf8mb4` and `SET NAMES latin1`.

Metadata observed with `SET NAMES utf8mb4`:

| Expression | Type | Charset/collation | Length | Decimals | Nullability | Flags |
| --- | --- | --- | --- | --- | --- | --- |
| `CONNECTION_ID()` | `LONGLONG` | binary numeric | 21 | 0 | not null | `NOT_NULL`, `UNSIGNED`, `BINARY`, `NUM` |
| `USER()` | `VAR_STRING` | connection result charset | 1152 | 31 | nullable | none |
| `SESSION_USER()` | `VAR_STRING` | connection result charset | 1152 | 31 | nullable | none |
| `SYSTEM_USER()` | `VAR_STRING` | connection result charset | 1152 | 31 | nullable | none |
| `CURRENT_USER()` | `VAR_STRING` | connection result charset | 1152 | 31 | nullable | none |
| `CURRENT_USER` | `VAR_STRING` | connection result charset | 1152 | 31 | nullable | none |

With `SET NAMES latin1`, the text functions reported length `288` and the
active latin1 connection collation.

## MyLite compatibility decision

MyLite is embedded and has no server authentication handshake. It therefore
uses one deterministic handle-owned identity string for both the client identity
and authenticated identity:

```text
mylite@localhost
```

This means `USER()`, `SESSION_USER()`, `SYSTEM_USER()`, `CURRENT_USER()`, and
`CURRENT_USER` intentionally return the same value until MyLite grows explicit
authentication, definers, or invoker security.

`CONNECTION_ID()` returns a handle-owned unsigned integer. The first embedded
slice uses `1` for each independent handle. That preserves determinism, avoids
process-global mutable counters, and keeps the value stable for the handle's
lifetime. A later protocol server can assign externally visible connection
identifiers without changing the scalar function's handle-owned contract.

## MyLite Lemon grammar snippets

MyLite already has generic function-call grammar. The intended accepted surface
for this slice can be represented as:

```lemon
scalar_expr(A) ::= session_identity_function(A).
scalar_expr(A) ::= CURRENT_USER(T).

session_identity_function(A) ::= CONNECTION_ID LP RP.
session_identity_function(A) ::= USER LP RP.
session_identity_function(A) ::= SESSION_USER LP RP.
session_identity_function(A) ::= SYSTEM_USER LP RP.
session_identity_function(A) ::= CURRENT_USER LP RP.
```

The snippets are descriptive for MyLite's own grammar and are not copied from
MySQL. If most forms remain under generic function-call parsing, the binder
must enforce the zero-argument set. `CURRENT_USER` without parentheses must be
parsed directly as a zero-argument function call node so downstream expression
evaluation and metadata inference can share one implementation.

## Runtime semantics

### `CONNECTION_ID()`

Returns `mylite_db.connection_id` as an unsigned integer expression value. The
value is initialized when the handle is opened and never mutates during the
handle lifetime.

### `USER()`, `SESSION_USER()`, and `SYSTEM_USER()`

Return the deterministic embedded client identity string `mylite@localhost`.
All three functions are synonyms in this slice.

### `CURRENT_USER()` and `CURRENT_USER`

Return the deterministic embedded authenticated identity string
`mylite@localhost`. MyLite does not yet have stored routines, views, triggers,
events, definers, invokers, roles, or grants, so there is currently no runtime
surface where `CURRENT_USER` can diverge from `USER()`.

## Result metadata

Text identity functions use the active connection result charset/collation and
the MySQL-observed display length of 288 characters multiplied by the active
result charset's maximum bytes per character. For `utf8mb4`, the display length
is `1152`; for `latin1`, it is `288`.

The text metadata is nullable, matching MySQL's metadata even though MyLite's
embedded identity value is currently always non-`NULL`.

`CONNECTION_ID()` uses unsigned `LONGLONG` metadata with display length `21`,
binary charset id `63`, decimals `0`, and `NOT_NULL`, `UNSIGNED`, `BINARY`, and
`NUM` flags.

## Errors and warnings

- `CONNECTION_ID()` accepts exactly zero arguments.
- `USER()`, `SESSION_USER()`, `SYSTEM_USER()`, and `CURRENT_USER()` accept
  exactly zero arguments.
- Bare `CURRENT_USER` accepts no argument list because it has no parentheses.
- Unsupported arity is rejected at parse or prepare/bind time through the
  current MyLite status model.
- Successful evaluation produces no warnings.

## Storage and performance implications

No `.mylite` file-format or SQLite storage change is required.

The implementation adds one small handle-owned connection-id field and uses a
static immutable identity string. Function evaluation is O(1), allocates only
the returned expression text, and introduces no third-party dependencies.

## Test plan

Fast C tests must cover:

- parser acceptance for all function forms in no-table `SELECT`
- parser acceptance for bare `CURRENT_USER`
- case-insensitive function names
- no-table scalar results for all identity functions
- stable `CONNECTION_ID()` within a handle
- table-backed projection, `WHERE`, and `ORDER BY`
- supported `UPDATE` assignment and `DELETE` predicate expression paths
- metadata under `SET NAMES utf8mb4` and `SET NAMES latin1`
- unsupported arity/syntax for argument-bearing identity forms
- preservation of existing session information function behavior

## Implementation handoff

Implementation should:

1. Extend the scalar function registry with the identity functions and
   zero-argument arity validation.
2. Add bare `CURRENT_USER` grammar that produces the same zero-argument
   function-call AST shape as `CURRENT_USER()`.
3. Add handle-owned `connection_id` state initialized on open.
4. Extend the session-function evaluator for identity text and connection id.
5. Extend result metadata inference for identity text and connection id.
6. Update compatibility and scalar/session docs to point at this slice.
