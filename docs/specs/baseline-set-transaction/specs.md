# Baseline SET TRANSACTION

## Status

This feature adds the first `SET TRANSACTION` slice on top of the existing
explicit transaction, savepoint, statement-atomic DML, and temporary-table
runtime. The goal is to admit common MySQL client setup statements for
transaction isolation and access mode while preserving MyLite's current public
API and descriptor-driven execution model.

This is not full MySQL transaction compatibility. Isolation levels are accepted
and stored as session or next-transaction characteristics, but they do not add
new concurrency semantics in this embedded baseline. Access mode is observable:
`READ ONLY` must prevent writes to persistent tables in the affected
transaction, while temporary-table DML remains allowed.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline transaction lifecycle:
  `docs/specs/baseline-transaction-lifecycle/specs.md`
- Baseline savepoint lifecycle:
  `docs/specs/baseline-savepoint-lifecycle/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- SQLite connection bootstrap policy:
  `docs/specs/sqlite-connection-bootstrap-policy/specs.md`
- File-backed MyLite opening VFS:
  `docs/specs/file-backed-mylite-opening-vfs/specs.md`
- MyLite file format: `docs/specs/mylite-file-format/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
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

`packages/libmylite/tests/mysql_baseline_set_transaction_expectations.sh`

Observed behavior for this slice:

- `SET TRANSACTION ISOLATION LEVEL READ COMMITTED`,
  `READ UNCOMMITTED`, `REPEATABLE READ`, and `SERIALIZABLE` succeed with
  `ROW_COUNT() = 0` and `@@warning_count = 0`.
- `SET TRANSACTION READ WRITE` and `SET TRANSACTION READ ONLY` succeed with
  `ROW_COUNT() = 0` and no warnings.
- One isolation characteristic and one access-mode characteristic may appear in
  either comma-separated order. Repeating isolation or repeating access mode is
  a syntax error in MySQL 8.4.9.
- `SET TRANSACTION ...` without `SESSION` or `GLOBAL` applies only to the next
  transaction. It is rejected inside an active transaction with error `1568`,
  SQLSTATE `25001`, and message
  `Transaction characteristics can't be changed while a transaction is in progress`.
- `SET SESSION TRANSACTION ...` applies to later transactions in the current
  session. It is allowed inside an active transaction and does not affect the
  active transaction.
- A later `SET SESSION TRANSACTION` between transactions overrides any pending
  next-transaction value for the named characteristic.
- `READ ONLY` prevents DML writes to persistent tables in affected
  autocommit-style and explicit transactions with error `1792`, SQLSTATE
  `25006`, and message `Cannot execute statement in a READ ONLY transaction.`
- `READ ONLY` does not prevent `SELECT`. A `SELECT` after next-transaction
  `READ ONLY` consumes that next-transaction characteristic, so a later
  autocommit write uses the session default.
- DML writes to existing `TEMPORARY` tables remain allowed in a read-only
  transaction.
- DDL may still perform its MySQL implicit-commit behavior; this feature does
  not add a new DDL rejection path.

## Scope

Supported SQL forms:

```sql
SET TRANSACTION ISOLATION LEVEL READ COMMITTED
SET TRANSACTION ISOLATION LEVEL READ UNCOMMITTED
SET TRANSACTION ISOLATION LEVEL REPEATABLE READ
SET TRANSACTION ISOLATION LEVEL SERIALIZABLE
SET TRANSACTION READ WRITE
SET TRANSACTION READ ONLY
SET TRANSACTION ISOLATION LEVEL READ COMMITTED, READ WRITE
SET TRANSACTION READ WRITE, ISOLATION LEVEL READ COMMITTED
SET SESSION TRANSACTION ...
```

The same isolation levels and access modes are admitted for `SESSION` scope.
Keyword spelling is ASCII case-insensitive. Successful statements return the
existing non-row result convention with affected rows `0` and warning count
`0`.

`GLOBAL` scope is intentionally not supported in this embedded baseline. MyLite
does not have mutable server-global transaction defaults shared by future
connections. `SET GLOBAL TRANSACTION ...` must fail deterministically with a
MyLite unsupported-syntax diagnostic until a broader global variable model is
specified.

## Non-Goals

This feature must not implement:

- `START TRANSACTION READ WRITE`, `START TRANSACTION READ ONLY`, or
  `WITH CONSISTENT SNAPSHOT`;
- `SET GLOBAL TRANSACTION` mutable global state or privilege checks;
- direct `SET transaction_isolation = ...`,
  `SET @@transaction_isolation = ...`, `SET transaction_read_only = ...`, or
  persisted variable forms;
- system-variable readback for `@@transaction_isolation` or
  `@@transaction_read_only`;
- protocol transaction status flags;
- row locks, gap locks, deadlock handling, lock wait timeouts, or isolation
  visibility semantics beyond the current SQLite-backed statement behavior;
- stored programs, XA, replication, binary logging, Performance Schema, or
  InnoDB transaction introspection;
- SQLite fork patches.

Unsupported forms must not be silently accepted.

## Ownership Boundary

- Public API remains unchanged. `mylite_execute()` continues to own public
  misuse handling, result lifetime, previous-result state, diagnostics, and
  statement-context setup.
- Session state owns connection-local transaction characteristic defaults,
  pending next-transaction characteristics, and active transaction access mode.
  These fields are internal and are not part of the public ABI.
- Statement context owns a top-level statement boundary and existing wrapper
  transaction state. It does not own long-lived session characteristics.
- Lexer/parser/AST own syntax admission, source spans, and characteristic node
  kinds. They remain independent of runtime, catalog, storage, and SQLite.
- Runtime dispatch owns `SET TRANSACTION` semantics, active-transaction
  diagnostics, and consuming pending next-transaction characteristics.
- DML planners remain descriptor-driven. Write rejection in read-only
  transactions is checked after target resolution so MyLite can distinguish
  persistent and temporary table descriptors without consulting SQLite schema
  text.
- Catalog remains authoritative for persistent descriptors. `SET TRANSACTION`
  must not mutate catalog rows, descriptor versions, descriptor caches,
  catalog generation, or `sqlite_schema_generation`.
- Temporary catalog remains authoritative for temporary descriptors. Its DML
  writes are allowed in read-only transactions, matching the verified MySQL
  behavior.
- SQLite owns physical row storage and transaction mechanics. MyLite uses
  existing public SQLite control statements and does not add a fork hook.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload.
  Transaction characteristics must not touch the preamble.

## Supported Grammar

This snippet describes MyLite's intended grammar extension, not MySQL's full
grammar:

```lemon
statement ::= set_transaction_statement.

set_transaction_statement ::=
    SET set_transaction_scope_opt TRANSACTION set_transaction_characteristics.

set_transaction_scope_opt ::= .
set_transaction_scope_opt ::= SESSION.
set_transaction_scope_opt ::= GLOBAL.

set_transaction_characteristics ::= set_transaction_isolation.
set_transaction_characteristics ::= set_transaction_access_mode.
set_transaction_characteristics ::= set_transaction_isolation COMMA set_transaction_access_mode.
set_transaction_characteristics ::= set_transaction_access_mode COMMA set_transaction_isolation.

set_transaction_isolation ::= ISOLATION LEVEL transaction_isolation_level.

transaction_isolation_level ::= REPEATABLE READ.
transaction_isolation_level ::= READ COMMITTED.
transaction_isolation_level ::= READ UNCOMMITTED.
transaction_isolation_level ::= SERIALIZABLE.

set_transaction_access_mode ::= READ WRITE.
set_transaction_access_mode ::= READ ONLY.
```

The grammar intentionally rejects duplicate isolation or duplicate access-mode
characteristics, missing characteristics, `LOCAL`, `@@` variable assignment
forms, arbitrary expressions, parameters, and startup-option spellings such as
`READ-COMMITTED`.

## Semantics

Each connection starts with session defaults equivalent to
`REPEATABLE READ` and `READ WRITE`. The default is stored internally even though
system-variable readback remains unsupported.

`SET SESSION TRANSACTION` updates the connection-local session default for the
named characteristics. If no explicit user transaction is active, setting a
session characteristic also clears any pending next-transaction value for that
same characteristic.

`SET TRANSACTION` updates pending next-transaction characteristics and is
valid only when no explicit user transaction is active. Pending characteristics
are consumed by the next successful transaction-consuming statement. For this
baseline that includes:

- explicit `START TRANSACTION` / `BEGIN`;
- descriptor-backed `SELECT` statements;
- supported DML write statements;
- supported DDL and table-maintenance statements that already have MySQL
  implicit-commit behavior.

If the next transaction is read-only and a persistent-table write is attempted,
MyLite returns error `1792 / 25006` before starting or mutating physical row
storage. The pending next-transaction characteristic remains available after
that failed write, matching MySQL's observed behavior.

When `START TRANSACTION` or `BEGIN` starts an explicit transaction, MyLite
computes the active transaction access mode from pending next-transaction
access mode if present, otherwise from the session default. Pending
characteristics are cleared after the transaction starts. The active access
mode remains until `COMMIT`, full `ROLLBACK`, a nested `START TRANSACTION`
implicit commit, or an existing DDL/table-lock implicit commit clears the
active user transaction.

For autocommit-style DML outside an explicit user transaction, MyLite checks
the access mode that would apply to that statement transaction. A successful
write consumes pending next-transaction characteristics. A read-only write
failure does not consume them.

`SET SESSION TRANSACTION READ ONLY` inside an active read/write transaction
does not affect that active transaction. It affects later transactions after
the active transaction completes.

Isolation levels do not change SQLite execution in this baseline. They are
tracked so `SET SESSION TRANSACTION ...` and `SET TRANSACTION ...` have the
same scoping and overwrite behavior as access mode, and so later system
variable readback or concurrency work has an internal state owner.

## Diagnostics

Supported successful forms:

- return status `MYLITE_OK`;
- produce a non-row result with affected rows `0`;
- leave warning count `0`;
- leave `ROW_COUNT()` as `0`.

Required diagnostics:

- `SET TRANSACTION ...` during an active transaction:
  `1568 / 25001`,
  `Transaction characteristics can't be changed while a transaction is in progress`.
- Persistent-table DML in an affected read-only transaction:
  `1792 / 25006`,
  `Cannot execute statement in a READ ONLY transaction.`
- Unsupported `SET GLOBAL TRANSACTION ...`:
  deterministic MyLite unsupported diagnostic using the existing unsupported
  statement conventions.
- Unsupported grammar:
  parser syntax error using existing parser diagnostics.
- Allocation or SQLite control failures:
  existing MyLite allocation/runtime diagnostics.

## Physical SQLite Handling

No generated SQLite SQL is needed for `SET TRANSACTION` itself. It updates
connection-local MyLite session state only.

Read-only write rejection happens before descriptor-backed DML calls the
existing generated SQLite `INSERT`, `REPLACE`, `UPDATE`, or `DELETE` SQL. The
check must use MyLite descriptors and the temporary catalog, not SQLite
`sqlite_schema` text.

Existing SQLite `BEGIN IMMEDIATE`, `COMMIT`, `ROLLBACK`, and internal
statement savepoint handling remain unchanged except that an explicit active
transaction records whether it is read-only. The `.mylite` preamble and shifted
SQLite payload invariants are unchanged.

## Tests

Required tests:

- parser coverage for supported scope, isolation, access, ordering, and
  duplicate-characteristic syntax rejection;
- MySQL expectation script for successful forms, active-transaction rejection,
  read-only persistent DML rejection, next-characteristic consumption, session
  default behavior, and temporary-table DML allowance;
- runtime C tests for:
  - successful `SET TRANSACTION` and `SET SESSION TRANSACTION` result shape;
  - next read-only persistent DML rejection in autocommit-style and explicit
    transactions;
  - read-only temporary-table DML success;
  - session read-only default and next read-write override;
  - `SET SESSION TRANSACTION` inside an active transaction affecting only later
    transactions;
  - active `SET TRANSACTION` diagnostic;
  - unsupported `GLOBAL` scope;
  - cleanup on `COMMIT`, `ROLLBACK`, nested `START TRANSACTION`, and DDL
    implicit commit where the active transaction is closed;
  - independent handles with independent transaction characteristics;
  - file-backed persistence and unchanged MyLite preamble.

Existing transaction, savepoint, lock-table, DML lifecycle, parser, lexer,
runtime handle, diagnostics, statement-context, file-backed opening, VFS,
catalog, and full workflow checks must continue to pass.
