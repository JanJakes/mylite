# Advisory lock functions

## Scope

This feature covers MySQL-compatible user-level advisory lock scalar functions:

- `GET_LOCK(lock_name, timeout)`
- `RELEASE_LOCK(lock_name)`
- `IS_FREE_LOCK(lock_name)`
- `IS_USED_LOCK(lock_name)`
- `RELEASE_ALL_LOCKS()`

The functions are ordinary scalar function calls; no new grammar productions are
required. The supported call sites are no-table `SELECT`, table projection,
`WHERE`, `ORDER BY`, and supported single-table `UPDATE` and `DELETE`
expression paths.

Primary reference: MySQL 8.4 locking functions documentation
<https://dev.mysql.com/doc/refman/8.4/en/locking-functions.html>. Behavioral
expectations below were also verified against MySQL 8.4.9.

## MySQL behavior verified

MySQL 8.4.9 treats advisory locks as session-owned, named locks. Names are
case-insensitive for lock ownership. A session can acquire the same name
repeatedly; every successful `GET_LOCK()` increments the ownership count, and a
matching `RELEASE_LOCK()` releases one acquisition. `RELEASE_ALL_LOCKS()`
releases every acquisition held by the session and returns the number of
acquisitions released, not just the number of distinct names.

Verified result semantics:

- `GET_LOCK('name', 0)` returns `1` when the lock is free.
- Reacquiring the same name in the same session returns `1`.
- `GET_LOCK('same', 0)`, `GET_LOCK('SAME', 0)`, `RELEASE_ALL_LOCKS()` returns
  `1`, `1`, `2`.
- When another session owns the lock, `GET_LOCK('name', 0)` returns `0`.
- `IS_FREE_LOCK('name')` returns `1` when no session owns the lock and `0`
  otherwise.
- `IS_USED_LOCK('name')` returns the owning session's `CONNECTION_ID()` when
  held and `NULL` when not held.
- `RELEASE_LOCK('name')` returns `1` when the caller releases its own lock,
  `0` when another session owns the lock, and `NULL` when the name is valid but
  not locked.
- `RELEASE_ALL_LOCKS()` returns `0` when the session holds no locks.
- Numeric lock names are coerced to text before lookup.
- A 64-byte name is valid; a 65-byte name is rejected.
- `NULL` and empty names are errors.
- Wrong arity is rejected as an unsupported native function call in MyLite's
  prepare path.

Verified metadata:

- `GET_LOCK()`, `IS_FREE_LOCK()`, and `RELEASE_LOCK()` report nullable signed
  `LONGLONG`, binary collation, display length `1`, and numeric flags.
- `IS_USED_LOCK()` reports nullable unsigned `LONGLONG`, binary collation,
  display length `21`, and numeric flags.
- `RELEASE_ALL_LOCKS()` reports non-null unsigned `LONGLONG`, binary collation,
  display length `21`, and numeric flags.

## MyLite semantics

MyLite stores advisory locks in a process-local registry keyed by a canonical
ASCII case-insensitive name. Each registry entry records the owning `mylite_db`
handle, the handle's connection id, and a reentrant acquisition count.

The registry is released on `mylite_close()` for the owning handle, matching
MySQL's session-cleanup behavior. No database file content or catalog metadata
is changed by advisory locks.

`GET_LOCK()` evaluates both arguments. The timeout value is accepted for
compatibility, but the first implementation slice does not block or sleep:

- if the lock is free, it is acquired immediately and `1` is returned;
- if the caller already owns it, the acquisition count is incremented and `1`
  is returned;
- if another handle owns it, `0` is returned.

`RELEASE_LOCK()`, `IS_FREE_LOCK()`, and `IS_USED_LOCK()` evaluate their name
argument once and apply the name validation rules before touching the registry.

## Error handling

MyLite rejects lock names that are `NULL`, empty, contain an embedded NUL byte,
or exceed 64 bytes after conversion to text. Error messages follow the MySQL
shape closely enough for application diagnostics and are recorded as expression
error conditions. Exact server-side character-set expressibility checks remain
deferred.

`GET_LOCK()` timeout conversion currently only evaluates the timeout expression;
exact MySQL warnings for every string-to-integer timeout coercion edge case are
deferred.

## Lemon syntax

No new Lemon grammar is required. The existing function-call production
continues to parse the feature:

```lemon
expr ::= identifier LP function_arguments RP.
function_arguments ::= .
function_arguments ::= expr_list.
```

The analyzer accepts only the supported arities listed in this spec.

## Tests

Runtime tests should cover:

- no-table `SELECT` acquisition, reentrant acquisition, case-insensitive
  lookup, partial release, final release, missing release, and empty
  `RELEASE_ALL_LOCKS()`;
- numeric name coercion;
- `RELEASE_ALL_LOCKS()` acquisition-count behavior;
- cross-handle nonblocking behavior for held locks;
- metadata for all five functions;
- table projection, predicates, ordering, `UPDATE`, and `DELETE` expression
  paths;
- `NULL`, empty, embedded-NUL, and overlong name errors;
- invalid arity rejection for each function.

## Deferred compatibility

The initial implementation does not provide server-wide cross-process locks,
blocking waits, timeout sleeps, Performance Schema lock visibility, exact
character-set expressibility checks, or every MySQL timeout coercion warning.
These are documented compatibility limits rather than parser gaps.
