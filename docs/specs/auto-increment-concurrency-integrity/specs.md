# AUTO_INCREMENT Concurrency Integrity

## Status

This specification tightens the existing baseline AUTO_INCREMENT lifecycle for
persistent base tables. It does not widen the admitted column, index, or insert
syntax surface.

## Compatibility Basis

MySQL 8.4 documents AUTO_INCREMENT as a server-generated identity facility and
documents that generated values can be lost when transactions roll back. A
generated identity must not be allowed to identify an unrelated row committed
by another writer while the generating statement was waiting.

Reference:

- <https://dev.mysql.com/doc/refman/8.4/en/example-auto-increment.html>
- <https://dev.mysql.com/doc/refman/8.4/en/innodb-auto-increment-handling.html>

MyLite's existing MySQL 8.4.9 expectation fixtures remain authoritative for
single-statement generated, explicit, duplicate, IGNORE, REPLACE, and
`ON DUPLICATE KEY UPDATE` behavior. This specification adds a MyLite storage
concurrency invariant that cannot be compared by sharing one physical database
file with MySQL.

## Writer Protocol

Planning may discover that a statement targets an AUTO_INCREMENT column, but a
persistent generated value is not reserved by planning. Execution must:

1. establish the statement transaction and acquire SQLite's serialized writer
   boundary;
2. read the current persistent `auto_increment_next` value directly from the
   catalog inside that transaction, bypassing descriptor caches;
3. rebase every generated row value from that locked lower bound while
   respecting `auto_increment_increment` and `auto_increment_offset`;
4. execute row mutations and duplicate handling;
5. advance the catalog counter in the same statement transaction; and
6. commit or roll back the complete row-and-counter statement unit.

Temporary tables are connection-local and continue to use their session-owned
descriptor counter. A nonzero session `insert_id` is an explicit allocation
request and remains the lower bound for that statement.

Explicit positive values are evaluated against the locked current counter.
They may advance it but never lower it. Generated multi-row statements allocate
their complete admitted sequence from the locked lower bound. LOAD DATA and
table-backed INSERT SELECT initialize their streaming row counters from the
same locked catalog value. UPDATE assignments that can advance an
AUTO_INCREMENT counter also compare against the locked current value.

## Duplicate Semantics

Duplicate processing uses only values rebased after the writer boundary.
Consequently, a generated `ON DUPLICATE KEY UPDATE` attempt cannot target a row
that another handle committed after this statement was planned. IGNORE,
REPLACE, and duplicate-update counter consumption continue to follow their
existing MySQL-verified statement semantics, but start from the locked counter.

## Rollback, Close, And Process Death

Successful statements inside an explicit user transaction retain the existing
session high-water record. Explicit rollback, rollback to savepoint followed by
commit, and clean connection close reconcile consumed persistent values before
the transaction is discarded, so a clean lifecycle does not reuse them.

SQLite permits only one writer to the database file. MyLite therefore cannot
publish an AUTO_INCREMENT high-water update in a second durable transaction
while the user's first write transaction remains uncommitted. If the process is
terminated before MyLite can reconcile that session record, SQLite crash
recovery rolls back both the rows and their catalog counter update. Reuse of
that uncommitted value after process death is an explicit compatibility
limitation, not a clean-close guarantee. Preserving crash-time gaps would
require a separately durable allocation channel and a file-format design.

Committed counters remain durable across close, reopen, and hot-journal
recovery. Failed statements do not expose partially mutated rows or counters.

## Diagnostics And Ownership

The locked catalog read returns an internal error if the target row is missing,
the counter is NULL, or the value is not positive. Plan-owned generated values
remain mutable until execution finishes and are released through existing plan
cleanup. The catalog scalar read returns no borrowed storage.

## Verification

Coverage must include:

- a deterministic two-handle schedule that pauses writer A after planning,
  commits writer B, and proves A allocates a later generated key;
- the same schedule for plain INSERT, IGNORE, REPLACE, and
  `ON DUPLICATE KEY UPDATE`;
- generated and explicit multi-row statements;
- table-backed INSERT SELECT, row-scalar INSERT SELECT, LOAD DATA, and explicit
  UPDATE counter advancement;
- explicit rollback, savepoint rollback, clean close, reopen, and a documented
  process-death recovery case;
- generated IDs, affected rows, `LAST_INSERT_ID()`, row contents, and durable
  metadata after each schedule.
