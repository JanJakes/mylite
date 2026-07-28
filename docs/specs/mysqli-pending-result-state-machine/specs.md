# mysqli Pending-Result State Machine

## Purpose

The mysqli adapter must preserve MySQL's connection-busy protocol semantics.
A row-producing `real_query()` whose result has not been acquired, a direct
unbuffered result that has not reached end-of-data, or an unbuffered prepared
result owns the connection. A command that is not part of releasing or
buffering that owner fails without discarding it.

This specification closes follow-up finding `API-01`. The original adapter
called `mylite_mysqli_link_clear_pending_result()` before a new command and
therefore silently finalized unread results.

## Authorities

- MySQL 8.4 commands-out-of-sync behavior:
  <https://dev.mysql.com/doc/refman/8.4/en/commands-out-of-sync.html>
- MySQL 8.4 C API `mysql_use_result()`:
  <https://dev.mysql.com/doc/c-api/8.4/en/mysql-use-result.html>
- MySQL 8.4 C API `mysql_stmt_fetch()`:
  <https://dev.mysql.com/doc/c-api/8.4/en/mysql-stmt-fetch.html>
- MySQL 8.4 C API `mysql_stmt_store_result()`:
  <https://dev.mysql.com/doc/c-api/8.4/en/mysql-stmt-store-result.html>
- PHP `mysqli::real_query()`:
  <https://www.php.net/manual/en/mysqli.real-query.php>
- PHP `mysqli::query()`:
  <https://www.php.net/manual/en/mysqli.query.php>
- PHP prepared-statement quickstart:
  <https://www.php.net/manual/en/mysqli.quickstart.prepared-statements.php>
- PHP `mysqli_stmt::reset()`:
  <https://www.php.net/manual/en/mysqli-stmt.reset.php>
- PHP `mysqli_stmt::close()`:
  <https://www.php.net/manual/en/mysqli-stmt.close.php>
- MySQL 8.4.9 runtime fixture:
  `packages/libmylite/tests/mysql_mysqli_pending_result_state_expectations.sh`

The design is independently authored from public documentation, observed
MySQL 8.4.9 behavior, and MyLite's existing native statement API.

## MySQL 8.4.9 observations

The pinned MySQL 8.4.9 runtime was probed through stock mysqli with mysqlnd.
The exact busy-state diagnostic is error `2014`, SQLSTATE `HY000`, and message
`Commands out of sync; you can't run this command now`.

Observed direct-query transitions:

- after a row-producing `real_query()`, another query failed until
  `store_result()` or `use_result()` acquired the result;
- acquiring with `store_result()` released the connection immediately, even
  when the buffered result remained unread;
- acquiring with `use_result()` kept the connection busy before the first
  fetch and after a partial fetch;
- fetching the last row did not by itself release the connection; a following
  fetch had to observe end-of-data;
- exhausting or freeing the unbuffered result released the connection;
- `commit()` and `autocommit()` failed while an unbuffered result was active;
- the failed command did not destroy the pending result, which could still be
  buffered, fetched, or freed before a successful recovery command;
- ordinary `query()` with its default buffered mode allowed a following
  command while its result object remained unread.

Observed prepared-statement transitions:

- a row-producing `execute()` created an unbuffered result by default;
- a command on the connection or execution of a different statement failed
  until the result was exhausted, buffered, reset, freed, or closed;
- `store_result()` and `get_result()` buffered the complete result and released
  the connection immediately;
- `free_result()`, `reset()`, and `close()` canceled unread rows and released
  the connection;
- re-executing the same prepared statement was legal and replaced its unread
  result;
- preparing a new statement while another direct or prepared result owned the
  connection failed with the same 2014 diagnostic.

## Connection state

Each connected mysqli object has exactly one protocol state:

| State | Owner | Commands allowed without 2014 |
| --- | --- | --- |
| `READY` | None | All supported operations |
| `DIRECT_PENDING` | The connection's last successful row-producing `real_query()` | Result acquisition, diagnostics, and close |
| `DIRECT_UNBUFFERED` | One `mysqli_result` | Fetch, free/close result, diagnostics, and connection close |
| `PREPARED_UNBUFFERED` | One `mysqli_stmt` | Fetch/bind-result operations, buffer/get/free/reset/close that owner, same-owner re-execute, diagnostics, and connection close |
| `CLOSED` | None | Only operations already documented for a closed mysqli object |

Buffered result objects do not own the connection. Their rows are
adapter-owned memory and remain readable independently of later commands.

The state is explicit. It must not be reconstructed from SQL text, a retained
`last_result` reference, native cursor internals, or result column count.
Owner identity is recorded so only the owning result or statement can release
the busy state. A stale result or statement must not release a newer owner.

## Direct-query transitions

### `real_query()`

In `READY`, a successful non-row statement returns to `READY`. A successful
row-producing statement enters `DIRECT_PENDING`, including `SELECT`, `SHOW`,
`DESCRIBE`, `EXPLAIN`, and other statements with result columns.

The adapter may internally use a streaming native statement or an already
materialized MyLite result, but the observable state remains
`DIRECT_PENDING` until mysqli result acquisition.

In any busy state, a new `real_query()`, `query()`, transaction command,
autocommit change, database change, statement prepare, or other operation that
would communicate with the server fails with 2014. The current owner and its
cursor remain intact.

### `store_result()`

In `DIRECT_PENDING`, `store_result()` transfers or copies all rows into a
buffered `mysqli_result`, releases execution-owned native resources, and
enters `READY`. Zero-row rowsets still return a result object and release the
connection.

Calling `store_result()` without a pending row-producing result returns
`false` according to the existing mysqli surface. An unsupported result mode
does not consume the pending result.

### `use_result()`

In `DIRECT_PENDING`, `use_result()` returns a result owner and enters
`DIRECT_UNBUFFERED`. A streaming native cursor stays attached to that owner.
If the underlying MyLite result was materialized, the adapter still enforces
the same logical unbuffered ownership while rows are read from its storage.

The state returns to `READY` only when:

- a fetch observes end-of-data;
- the result is explicitly freed or closed;
- the result object is destroyed; or
- the connection is closed.

Returning the final row is not end-of-data. The subsequent fetch that returns
`null` releases the connection.

## Prepared-statement transitions

A successful row-producing `mysqli_stmt::execute()` enters
`PREPARED_UNBUFFERED`. Execution errors leave the connection `READY`.
Non-row statements execute to completion and remain `READY`.

The owning statement supports these transitions:

- `fetch()` reads one row; the fetch that reports no more data enters `READY`;
- `store_result()` consumes all remaining rows into the statement's buffered
  result and enters `READY`;
- `get_result()` consumes all remaining rows into a buffered
  `mysqli_result`, transfers that buffered result through the existing
  adapter surface, and enters `READY`;
- `free_result()`, `reset()`, and `close()` discard unread rows, release the
  native execution, and enter `READY`;
- `execute()` on the same statement discards its previous unread execution,
  resets/binds/executes the new one, and retains or re-enters the appropriate
  state.

Preparing or executing a different statement while a prepared owner is
active fails with 2014. The diagnostic is published on the object whose
operation failed, while the active owner remains usable.

Stored prepared results remain readable while the connection executes other
commands. Re-execution clears the previous stored result as documented by
mysqli.

## Diagnostics

Every disallowed operation reports:

```text
error code: 2014
SQLSTATE: HY000
message: Commands out of sync; you can't run this command now
```

With `MYSQLI_REPORT_OFF`, the operation returns `false` and updates the
connection or statement error properties. With
`MYSQLI_REPORT_ERROR | MYSQLI_REPORT_STRICT`, it throws
`mysqli_sql_exception` carrying the same code, SQLSTATE, and message.

The rejected command does not:

- execute any SQL;
- clear, finalize, fetch, or buffer the active result;
- change affected rows, insert id, warning count, or field count from the
  owning operation;
- commit, roll back, or change autocommit;
- replace the current owner.

After the owner is released, a successful command clears the ordinary link
error state through the existing diagnostic path.

## Ownership and cleanup

The connection owns an unclaimed direct result. A claimed direct unbuffered
result holds the connection object alive and registers itself as the active
owner. A prepared statement already holds its connection object alive and
registers itself while its unbuffered execution is active.

Release is identity-checked and idempotent. Result destruction, explicit
free, statement reset, statement close, connection close, execution failure,
and PHP shutdown must not double-finalize a native statement or leave a
dangling owner pointer. Connection close cancels any pending or active result
before closing the native database.

Buffered results contain no live native cursor and no connection-busy
ownership. They retain their existing field/value lifetime.

## Native, parser, storage, and ABI impact

This is an adapter state-machine change. It uses the existing native prepare,
step, reset, finalize, result, and diagnostic APIs. It changes no SQL grammar,
AST, SQLite fork code, `.mylite` file format, catalog format, or public MyLite
ABI.

No new dependency is permitted.

## Performance requirements

- Readiness checks are constant-time.
- Buffered query behavior and row storage remain unchanged.
- Unbuffered direct and prepared paths retain at most the current row plus
  native cursor state unless mysqli explicitly requests buffering.
- State transitions add no SQL parsing and no scan of live PHP objects.
- Owner cleanup must not introduce a reference cycle.

## Test plan

### MySQL 8.4.9 differential

- object and procedural `real_query()` before result acquisition;
- unread, partial, final-row-without-EOF, exhausted, and freed unbuffered
  results;
- buffered direct result followed by another command while unread;
- query, commit, and autocommit attempts in busy states;
- report-off return/error properties and strict exception fields;
- prepared unread and partial results;
- prepared store/get, free, reset, close, same-owner re-execute, and
  different-owner rejection;
- recovery after every release transition.

### MyLite adapter

Run the same state matrix against the replacement extension. Assert returned
rows as well as the 2014 diagnostic so a rejected command cannot pass by
silently discarding the owner. Cover object and procedural APIs and preserve
all existing buffered-query and statement tests.

Run the complete mysqli and PDO extension suites after the focused regression.
The adapter state must remain safe under ASan/UBSan where the PHP build
supports it.
