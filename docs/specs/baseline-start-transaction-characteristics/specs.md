# Baseline START TRANSACTION Characteristics

## Status

This feature extends the existing explicit transaction lifecycle with the
common MySQL `START TRANSACTION` characteristic forms. It builds on the
implemented `SET TRANSACTION` session/next-transaction state and keeps the
scope narrow: statement-level `READ ONLY` / `READ WRITE` access mode and
`WITH CONSISTENT SNAPSHOT` warning behavior only.

This is not full MySQL transaction isolation support. MyLite records isolation
state because `SET TRANSACTION` already needs it, but this feature does not add
MVCC snapshot visibility, row locks, gap locks, protocol status flags, or
storage-engine-specific optimizations.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline transaction lifecycle:
  `docs/specs/baseline-transaction-lifecycle/specs.md`
- Baseline `SET TRANSACTION`:
  `docs/specs/baseline-set-transaction/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- SQLite connection bootstrap policy:
  `docs/specs/sqlite-connection-bootstrap-policy/specs.md`
- File-backed MyLite opening VFS:
  `docs/specs/file-backed-mylite-opening-vfs/specs.md`
- MyLite file format: `docs/specs/mylite-file-format/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `START TRANSACTION`, `COMMIT`, and
  `ROLLBACK`: https://dev.mysql.com/doc/refman/8.4/en/commit.html
- MySQL 8.4 Reference Manual, `SET TRANSACTION`:
  https://dev.mysql.com/doc/refman/8.4/en/set-transaction.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes were run against local container `mylite-mysql-849` using
`mysql:8.4.9`. The expectations are captured in:

`packages/libmylite/tests/mysql_baseline_start_transaction_characteristics_expectations.sh`

Observed behavior for this slice:

- `START TRANSACTION READ ONLY`, `READ WRITE`, and
  `WITH CONSISTENT SNAPSHOT` succeed with `ROW_COUNT() = 0`.
- `READ ONLY`, `READ WRITE`, and `WITH CONSISTENT SNAPSHOT` can appear in
  either order when the access mode is not contradictory.
- Repeating the same access mode or repeating `WITH CONSISTENT SNAPSHOT`
  succeeds with no warnings.
- Combining `READ ONLY` and `READ WRITE` in one `START TRANSACTION` statement
  is a syntax error.
- `START TRANSACTION ISOLATION LEVEL ...` is a syntax error; isolation remains
  controlled by `SET TRANSACTION`.
- `BEGIN READ ONLY` is a syntax error. `BEGIN` remains only the already
  implemented alias for `START TRANSACTION` without characteristics.
- `START TRANSACTION WITH CONSISTENT SNAPSHOT` under `REPEATABLE READ` succeeds
  with no warnings.
- The same statement under `READ COMMITTED`, `READ UNCOMMITTED`, or
  `SERIALIZABLE` succeeds and emits warning `138`, SQLSTATE `HY000`, message
  `InnoDB: WITH CONSISTENT SNAPSHOT was ignored because this phrase can only
  be used with REPEATABLE READ isolation level.`
- `START TRANSACTION READ ONLY` blocks persistent-table DML with error `1792`,
  SQLSTATE `25006`, message
  `Cannot execute statement in a READ ONLY transaction.`
- DML against temporary tables remains allowed inside a read-only transaction.
- Statement-level `READ WRITE` overrides pending or session read-only state
  for the started transaction. A session read-only default remains in effect
  for later transactions after that transaction commits.
- If `START TRANSACTION` has no statement-level access mode, a pending
  `SET TRANSACTION READ ONLY` value applies to the started transaction.
- Pending isolation participates in `WITH CONSISTENT SNAPSHOT` warning
  behavior and is consumed by the successful `START TRANSACTION`.
- DDL inside an active read-only transaction follows MySQL's implicit-commit
  path for this baseline; observed `CREATE TABLE` succeeds, clears the active
  transaction, and leaves no read-only restriction on following statements.
- A nested `START TRANSACTION READ ONLY` commits prior transaction work before
  starting the new read-only transaction.

## Scope

Supported SQL forms:

```sql
START TRANSACTION READ ONLY
START TRANSACTION READ WRITE
START TRANSACTION WITH CONSISTENT SNAPSHOT
START TRANSACTION READ ONLY, WITH CONSISTENT SNAPSHOT
START TRANSACTION WITH CONSISTENT SNAPSHOT, READ WRITE
```

The characteristic list may contain repeated identical access-mode entries or
repeated `WITH CONSISTENT SNAPSHOT` entries because MySQL accepts them. MyLite
normalizes repeats to a single effective access mode and a boolean consistent
snapshot request.

Keyword spelling is ASCII case-insensitive. Successful statements return the
existing non-row result convention with affected rows `0`. Warning count is
`0` except for ignored `WITH CONSISTENT SNAPSHOT` under non-`REPEATABLE READ`
isolation.

## Non-Goals

This feature must not implement:

- `BEGIN` or `BEGIN WORK` modifiers;
- `START TRANSACTION ISOLATION LEVEL ...`;
- direct `SET transaction_isolation = ...`,
  `SET @@transaction_isolation = ...`, `SET transaction_read_only = ...`, or
  persisted variable forms;
- system-variable readback for `@@transaction_isolation` or
  `@@transaction_read_only`;
- protocol transaction status flags;
- row locks, gap locks, deadlock handling, lock wait timeouts, or isolation
  visibility semantics beyond the current SQLite-backed statement behavior;
- InnoDB snapshot optimization or MVCC snapshot construction;
- stored programs, XA, replication, binary logging, Performance Schema, or
  InnoDB transaction introspection;
- SQLite fork patches.

Unsupported forms must not be silently accepted.

## Ownership Boundary

- Public API remains unchanged. `mylite_execute()` owns public misuse
  handling, result lifetime, previous-result state, diagnostics, and
  statement-context setup.
- Session state owns connection-local transaction defaults, pending
  next-transaction characteristics, active transaction access mode, and the
  active transaction isolation value needed for diagnostics.
- Statement context owns only the top-level statement boundary and existing
  wrapper transaction state. It does not own long-lived transaction
  characteristics.
- Lexer/parser/AST own syntax admission, source spans, and characteristic node
  kinds. They remain independent of runtime, catalog, storage, and SQLite.
- Runtime dispatch owns applying statement-level `START TRANSACTION`
  characteristics, nested-start implicit commit behavior, warning emission, and
  consuming pending `SET TRANSACTION` characteristics.
- DML planners remain descriptor-driven. Existing read-only write rejection is
  reused after target resolution so MyLite can distinguish persistent and
  temporary table descriptors without consulting SQLite schema text.
- Catalog remains authoritative for persistent descriptors. Starting a
  transaction must not mutate catalog rows, descriptor versions, descriptor
  caches, catalog generation, or `sqlite_schema_generation`.
- Temporary catalog remains authoritative for temporary descriptors. Its DML
  writes remain allowed in read-only transactions.
- SQLite owns physical row storage and transaction mechanics. MyLite uses
  existing public SQLite control statements and does not add a fork hook.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload.
  Transaction characteristics must not touch the preamble.

## Supported Grammar

This snippet describes MyLite's intended grammar extension, not MySQL's full
grammar:

```lemon
statement ::= transaction_control_statement.

transaction_control_statement ::=
    START TRANSACTION start_transaction_characteristics_opt.

transaction_control_statement ::= BEGIN.
transaction_control_statement ::= BEGIN WORK.

start_transaction_characteristics_opt ::= .
start_transaction_characteristics_opt ::= start_transaction_characteristics.

start_transaction_characteristics ::= start_transaction_characteristic.
start_transaction_characteristics ::=
    start_transaction_characteristics COMMA start_transaction_characteristic.

start_transaction_characteristic ::= WITH CONSISTENT SNAPSHOT.
start_transaction_characteristic ::= READ WRITE.
start_transaction_characteristic ::= READ ONLY.
```

The grammar intentionally keeps `BEGIN` modifier-free and intentionally rejects
`START TRANSACTION ISOLATION LEVEL ...`. Contradictory `READ ONLY` / `READ
WRITE` combinations may be rejected during parser semantic construction or
runtime analysis as a syntax diagnostic; they must not start a transaction.

## Semantics

Each connection starts with session transaction defaults equivalent to
`REPEATABLE READ` and `READ WRITE`.

`SET TRANSACTION` continues to define pending next-transaction characteristics.
`START TRANSACTION` first combines those pending characteristics with any
statement-level characteristics. Statement-level access mode overrides a
pending or session access mode for the started transaction only. If the
statement has no access-mode characteristic, the pending next access mode is
used when present, otherwise the session default is used.

Statement-level `WITH CONSISTENT SNAPSHOT` does not change the active isolation
level. MyLite treats it as a snapshot request flag only for warning behavior:

- no warning when the effective isolation is `REPEATABLE READ`;
- warning `138/HY000` with the verified MySQL message for every other
  effective isolation level.

After a successful `START TRANSACTION`, pending next-transaction
characteristics are cleared. If starting the SQLite transaction fails, the
pending characteristics remain unchanged.

When `START TRANSACTION` is issued while a user transaction is active, MyLite
commits the active SQLite transaction, clears current savepoints and table lock
intent as existing lifecycle slices require, then starts a new transaction with
the new effective characteristics. This preserves MySQL's non-nested
transaction behavior.

`READ ONLY` is observable through the existing write guard:

- persistent-table `INSERT`, `REPLACE`, `UPDATE`, and `DELETE` fail with
  error `1792/25006`;
- temporary-table DML remains allowed;
- descriptor-backed `SELECT` remains allowed;
- supported DDL and table-maintenance statements keep the already implemented
  implicit-commit behavior and do not remain inside the read-only transaction.

`READ WRITE` explicitly makes the started transaction writable, overriding a
pending or session read-only default for that transaction.

## Diagnostics

Supported successful statements:

- return `MYLITE_OK`;
- create a non-row result;
- set affected rows to `0`;
- preserve warning count `0`, except for ignored `WITH CONSISTENT SNAPSHOT`;
- leave catalog and file-format metadata unchanged.

Ignored consistent snapshot:

- warning level `Warning`;
- code `138`;
- SQLSTATE `HY000`;
- message
  `InnoDB: WITH CONSISTENT SNAPSHOT was ignored because this phrase can only be used with REPEATABLE READ isolation level.`

Persistent DML in a read-only transaction:

- error `1792`;
- SQLSTATE `25006`;
- message `Cannot execute statement in a READ ONLY transaction.`

Unsupported or invalid syntax:

- `BEGIN READ ONLY`;
- `START TRANSACTION ISOLATION LEVEL ...`;
- contradictory `READ ONLY` and `READ WRITE`;
- arbitrary expressions, parameters, variables, comments-as-modifiers, or
  storage-engine clauses.

Allocation failures must use existing `MYLITE_NOMEM` behavior. Public API
misuse behavior does not change.

## SQLite And Storage Handling

No generated SQLite SQL includes transaction characteristic text. MyLite
translates a supported start request to the existing SQLite `BEGIN IMMEDIATE`
control statement after computing effective MySQL-facing characteristics.

This is a MyLite wrapper/translation feature, not a SQLite fork feature. No
SQLite extension API or fork hook is required.

The `.mylite` preamble and shifted SQLite payload invariants are unaffected.

## Performance

The feature adds constant-time AST inspection and session-state updates around
an existing SQLite transaction-control call. It does not materialize user rows,
scan catalog data, or add planner work. Read-only DML rejection remains on the
existing descriptor-resolution path and does not add extra SQLite queries.

## Test Plan

Add fast C tests under `packages/libmylite/tests/`, extending the existing
parser and transaction lifecycle coverage where practical.

Parser coverage:

- each supported `START TRANSACTION` characteristic form;
- mixed order and repeated same characteristics;
- modifier-free `BEGIN` / `BEGIN WORK` still parse as before;
- `BEGIN READ ONLY`, `START TRANSACTION ISOLATION LEVEL ...`, and
  contradictory access modes are rejected deterministically.

Runtime coverage:

- successful `READ ONLY`, `READ WRITE`, and `WITH CONSISTENT SNAPSHOT` result
  shape, affected rows, warning count, and absence of rows;
- `WITH CONSISTENT SNAPSHOT` warning behavior under non-`REPEATABLE READ`;
- no warning under `REPEATABLE READ`;
- statement-level `READ WRITE` overrides session/pending read-only state;
- statement-level `READ ONLY` overrides writable state and blocks persistent
  DML;
- temporary-table DML remains allowed in read-only transactions;
- nested `START TRANSACTION READ ONLY` commits previous work and starts a
  read-only transaction;
- savepoints are cleared by nested start, preserving existing behavior;
- DDL implicit-commit behavior does not leave the session in read-only mode;
- independent handles keep independent transaction state;
- file-backed reopen verifies the preamble is unchanged and committed rows
  persist;
- existing transaction, savepoint, lock-table, temporary-table, update, delete,
  and parser lifecycle tests still pass.

MySQL expectation coverage:

- every supported user-visible success, warning, and error introduced above;
- runtime version check against MySQL 8.4.9;
- no guessed outputs.
