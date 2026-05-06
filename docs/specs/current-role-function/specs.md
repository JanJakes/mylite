# CURRENT_ROLE() function

## Scope

This slice implements MySQL's `CURRENT_ROLE()` information function for
MyLite's embedded session model.

Implemented behavior:

- `CURRENT_ROLE()` parses and executes as a zero-argument scalar function.
- The function returns `NONE` because MyLite does not yet have grant tables,
  assigned roles, default roles, or `SET ROLE` state.
- The returned expression is a session-dependent system constant with
  `utf8mb3_general_ci` collation introspection and coercibility `3`.
- Result-set metadata follows MySQL's function metadata shape for the verified
  connection character set.
- The function works in supported scalar call sites: no-table `SELECT`,
  table projection, `WHERE`, `ORDER BY`, single-table `UPDATE`, and
  single-table `DELETE`.

Deferred behavior:

- role grants and active role sets
- `SET ROLE`, `SET DEFAULT ROLE`, and mandatory roles
- `sql_quote_show_create` formatting for active role names
- privilege-visible metadata, definer/invoker execution context, and grant
  table authentication

## MySQL 8.4.9 Behavior

Official MySQL documentation describes `CURRENT_ROLE()` as an information
function that returns the current active session roles as a `utf8mb3` string,
or `NONE` when the session has no active roles:

- <https://dev.mysql.com/doc/refman/8.4/en/information-functions.html>
- <https://dev.mysql.com/doc/refman/8.4/en/roles.html>

MySQL-runtime verification was performed against the local `mysql:8.4.9`
container.

Verified results:

| SQL | Result |
| --- | --- |
| `SELECT CURRENT_ROLE()` | `NONE` |
| `SELECT CURRENT_ROLE() = 'NONE'` | `1` |
| `SELECT CHARSET(CURRENT_ROLE())` | `utf8mb3` |
| `SELECT COLLATION(CURRENT_ROLE())` | `utf8mb3_general_ci` |
| `SELECT COERCIBILITY(CURRENT_ROLE())` | `3` |
| `SELECT CURRENT_ROLE` | error 1054, unknown column |
| `SELECT CURRENT_ROLE(1)` | error 1582, incorrect parameter count |

Metadata under the default `latin1` connection:

| Type | Length | Max length | Decimals | Collation | Flags |
| --- | ---: | ---: | ---: | --- | --- |
| `LONG_BLOB` | `50331648` | `4` | `31` | `latin1_swedish_ci` | none |

Metadata after `SET NAMES utf8mb4`:

| Type | Length | Max length | Decimals | Collation | Flags |
| --- | ---: | ---: | ---: | --- | --- |
| `LONG_BLOB` | `201326592` | `4` | `31` | `utf8mb4_0900_ai_ci` | none |

The descriptor collation follows the connection collation, while expression
collation introspection reports the system-constant `utf8mb3_general_ci`
collation. MyLite must preserve that split.

## Syntax

`CURRENT_ROLE()` is a function-call form only. `CURRENT_ROLE` without
parentheses remains an ordinary identifier and is resolved as a column name.

Independent MyLite Lemon-style grammar shape:

```lemon
scalar_function_call ::= function_name LPAREN RPAREN.
function_name ::= identifier. /* accepts CURRENT_ROLE */
```

No special bare-function grammar should be added for `CURRENT_ROLE`.

## Runtime Semantics

MyLite currently has no role manager. `CURRENT_ROLE()` therefore returns the
same deterministic no-active-role value for every session:

```text
NONE
```

The value is session-dependent rather than cacheable across prepared statement
execution, so prepared statements observe future role state when role support is
added.

`CURRENT_ROLE()` has no side effects and emits no warnings.

## Metadata

The result descriptor is:

- field type: `MYLITE_FIELD_TYPE_BLOB`
- flags: none
- declared length: `50331648 * connection_max_bytes_per_character`
- decimals: `31`
- charset/collation id: the current connection collation id
- nullable: yes

The large display length matches MySQL's observed metadata for this function.
It is intentionally not derived from the maximum possible role list size in
MyLite, because role storage does not exist yet and clients compare metadata.

## Errors and Warnings

MyLite currently reports unsupported function arity as `MYLITE_UNSUPPORTED`.
`CURRENT_ROLE(1)` should follow the existing scalar-function arity path.

Bare `CURRENT_ROLE` must not be rewritten into a function call. In no-table
`SELECT`, it should fail with an unknown-column diagnostic.

## Tests

Implementation tests should cover:

- parser acceptance for `CURRENT_ROLE()`
- parser treatment of bare `CURRENT_ROLE` as an identifier
- value result `NONE`
- equality comparisons in projection and predicates
- use in `WHERE`, `ORDER BY`, `UPDATE`, and `DELETE`
- `CHARSET`, `COLLATION`, and `COERCIBILITY` introspection
- result metadata under `utf8mb4` and `latin1`
- `CURRENT_ROLE(1)` arity rejection
- bare `CURRENT_ROLE` unknown-column diagnostics

## Compatibility Decision

Returning `NONE` is a deliberate embedded-runtime placeholder. It matches a
MySQL session with no active roles and keeps applications that introspect active
roles from failing. Full role and privilege state remains tracked under the
role and privilege compatibility area.
