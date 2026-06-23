# Baseline Savepoint Lifecycle

## Status

This feature adds the first user-visible savepoint slice on top of the existing
baseline transaction lifecycle. The goal is to admit the common MySQL
`SAVEPOINT`, `ROLLBACK TO SAVEPOINT`, and `RELEASE SAVEPOINT` forms for current
MyLite user transactions without changing the public API or widening the DML
surface.

This is not full transaction compatibility. It does not add mutable
`autocommit`, isolation levels, access modes, lock behavior, XA, stored program
savepoint levels, protocol status flags, or full implicit-commit parity for
future statements. It extends the current explicit user transaction model with
MySQL-compatible savepoint creation, replacement, partial rollback, release,
and cleanup for the already supported DDL/DML lifecycle.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline transaction lifecycle:
  `docs/specs/baseline-transaction-lifecycle/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- SQLite connection bootstrap policy:
  `docs/specs/sqlite-connection-bootstrap-policy/specs.md`
- File-backed MyLite opening VFS:
  `docs/specs/file-backed-mylite-opening-vfs/specs.md`
- MyLite file format: `docs/specs/mylite-file-format/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, savepoints:
  https://dev.mysql.com/doc/refman/8.4/en/savepoint.html
- MySQL 8.4 Reference Manual, transaction control:
  https://dev.mysql.com/doc/refman/8.4/en/commit.html
- MySQL 8.4 Reference Manual, implicit commit:
  https://dev.mysql.com/doc/refman/8.4/en/implicit-commit.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

`packages/libmylite/tests/mysql_baseline_savepoint_lifecycle_expectations.sh`
records the MySQL 8.4.9 expectations for this slice:

- `SAVEPOINT name` succeeds outside an explicit transaction under the current
  default autocommit mode, reports `ROW_COUNT() = 0`, produces no warnings, and
  does not leave a later usable savepoint.
- `ROLLBACK TO SAVEPOINT name` and `RELEASE SAVEPOINT name` fail with error
  `1305`, SQLSTATE `42000`, and message text `SAVEPOINT name does not exist`
  when no matching savepoint exists.
- Inside an active transaction, `SAVEPOINT name` creates a savepoint and
  replaces any existing same-name savepoint without deleting differently named
  later savepoints.
- Savepoint names compare case-insensitively, including backtick-quoted names.
- `ROLLBACK TO name`, `ROLLBACK TO SAVEPOINT name`, and
  `ROLLBACK WORK TO name` are accepted forms for partial rollback.
- `ROLLBACK TO name` rolls back changes after the named savepoint, removes
  savepoints created after it, and keeps the named savepoint available.
- `RELEASE SAVEPOINT name` removes the named savepoint and savepoints created
  after it without rolling back row changes.
- Reusing a savepoint name replaces the older savepoint. After
  `SAVEPOINT a; SAVEPOINT a; RELEASE SAVEPOINT a`, a later
  `ROLLBACK TO SAVEPOINT a` fails.
- `COMMIT`, full `ROLLBACK`, starting a new transaction while one is active,
  and supported DDL implicit commits clear active savepoints.
- `ROW_COUNT()` is `0` and `@@warning_count` is `0` after successful savepoint
  statements. A missing savepoint error makes `ROW_COUNT()` return `-1` and
  records one diagnostic condition.

## Scope

The implementation must add:

- parser and AST nodes for:
  - `SAVEPOINT identifier`;
  - `ROLLBACK TO identifier`;
  - `ROLLBACK TO SAVEPOINT identifier`;
  - `ROLLBACK WORK TO identifier`;
  - `ROLLBACK WORK TO SAVEPOINT identifier`;
  - `RELEASE SAVEPOINT identifier`;
- connection-local savepoint state for each `mylite_db` handle;
- MySQL-compatible case-insensitive savepoint lookup and replacement;
- runtime handlers using existing non-row statement result conventions;
- generated SQLite savepoint control SQL built from MyLite-owned state;
- cleanup of savepoint state on `COMMIT`, full `ROLLBACK`,
  `START TRANSACTION` implicit commit, supported DDL implicit commit, and
  `mylite_close()`;
- parser and runtime tests for success paths, missing-savepoint diagnostics,
  duplicate names, case-insensitive names, DDL cleanup, file-backed persistence,
  and independent handles;
- compatibility documentation for the exact supported subset.

## Non-Goals

This feature must not implement:

- `SAVEPOINT` modifiers or non-identifier savepoint names;
- `ROLLBACK` completion modifiers such as `AND CHAIN`, `AND NO CHAIN`,
  `RELEASE`, or `NO RELEASE`;
- `ROLLBACK TO` with arbitrary expressions, parameters, string literals, or
  user variables as the savepoint name;
- stored program savepoint-level scoping;
- protocol transaction status flags;
- broader autocommit transaction behavior outside the documented
  `baseline-autocommit-system-variable` subset;
- isolation levels, `SET TRANSACTION`, consistent snapshots, row locks, gap
  locks, XA, binary logging, replication behavior, or privilege checks;
- SQLite fork patches.

Unsupported forms must not be silently accepted.

## Ownership Boundary

- Public API remains unchanged. `mylite_execute()` continues to own public
  misuse handling, result-handle lifetime, diagnostics, and previous result
  state.
- Session state owns the connection-local active-user-transaction flag and the
  user-visible savepoint registry. These fields are internal and not ABI.
- Statement context owns each top-level statement boundary and records wrapper
  transaction information. It does not own long-lived user savepoint state.
- Lexer/parser/AST own syntax admission, node kinds, child relationships, and
  source spans. They remain independent of runtime, catalog, storage, and
  SQLite.
- Runtime dispatch owns savepoint statement execution and cleanup during
  transaction boundaries and implicit commits.
- Analyzer/planner code for DML remains descriptor-driven and unchanged by this
  feature.
- Catalog remains the descriptor authority. Savepoints must not mutate catalog
  rows, descriptor versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation`.
- SQLite owns physical b-tree storage, durability, and the low-level savepoint
  stack. MyLite uses public SQLite SQL control statements and a MyLite-owned
  registry to map MySQL visible names to internal SQLite savepoint names.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload. Savepoint
  control must not write the preamble except through ordinary SQLite payload
  changes caused by committed DML.

## Supported SQL Grammar

Supported statements:

```sql
SAVEPOINT sp
ROLLBACK TO sp
ROLLBACK TO SAVEPOINT sp
ROLLBACK WORK TO sp
ROLLBACK WORK TO SAVEPOINT sp
RELEASE SAVEPOINT sp
```

`identifier` follows the existing MyLite identifier scanner and parser rules.
Unquoted and backtick-quoted savepoint names are admitted. Case-insensitive
matching uses MyLite's current ASCII identifier folding policy for this slice.

Unsupported statements:

```sql
ROLLBACK AND CHAIN
ROLLBACK RELEASE
ROLLBACK TO 'sp'
ROLLBACK TO @sp
SAVEPOINT ?
RELEASE sp
```

Unsupported forms may be parser syntax errors until a later phase specifies a
broader deterministic diagnostic.

### MyLite Lemon-Syntax Snippet

This snippet describes MyLite's intended grammar extension, not MySQL's full
grammar:

```lemon
statement ::= transaction_control_statement.

transaction_control_statement ::= SAVEPOINT identifier.
transaction_control_statement ::= ROLLBACK rollback_work_opt TO rollback_savepoint_opt identifier.
transaction_control_statement ::= RELEASE SAVEPOINT identifier.

rollback_work_opt ::= .
rollback_work_opt ::= WORK.

rollback_savepoint_opt ::= .
rollback_savepoint_opt ::= SAVEPOINT.
```

The AST stores the savepoint name as child `0` on each savepoint statement
node. The node span covers the full statement text.

## Semantics

### Savepoint Names

Savepoint names are connection-local and live only within the active user
transaction. MyLite resolves names case-insensitively using the same current
ASCII identifier folding expectations as descriptor name lookup. The original
decoded identifier text is retained for diagnostics and for generated internal
SQLite savepoint naming when needed.

If a later phase expands identifier character set, collation, or case folding,
the savepoint registry must be updated together with that parser behavior.

### `SAVEPOINT`

If no user transaction is active and session autocommit is enabled,
`SAVEPOINT name` succeeds as a no-op. It returns a non-row result with affected
rows `0`, warning count `0`, and does not create a later usable savepoint. If
session autocommit is disabled, `SAVEPOINT name` starts the current
autocommit-disabled transaction and creates a usable savepoint.

If a user transaction is active, `SAVEPOINT name` creates a new savepoint. If a
case-insensitive same-name savepoint already exists, MyLite removes that
savepoint from the user-visible registry, preserves differently named later
savepoints, and creates a fresh savepoint at the top of the visible savepoint
stack. This matches MySQL replacement semantics and avoids SQLite's different
duplicate-name behavior.

### `ROLLBACK TO`

`ROLLBACK TO name`, `ROLLBACK TO SAVEPOINT name`, `ROLLBACK WORK TO name`, and
`ROLLBACK WORK TO SAVEPOINT name` are equivalent in this slice.

If no active user transaction has a matching savepoint, MyLite returns error
`1305`, SQLSTATE `42000`, and message `SAVEPOINT name does not exist`. The
previous-statement diagnostics must make `SHOW WARNINGS`, `SHOW ERRORS`,
`SHOW COUNT(*) WARNINGS`, `SHOW COUNT(*) ERRORS`, `@@warning_count`, and
`@@error_count` follow the existing previous-error conventions.

If the savepoint exists, MyLite rolls back to it, removes savepoints created
after it, keeps the target savepoint available, returns a non-row result with
affected rows `0`, and produces no warnings.

### `RELEASE SAVEPOINT`

If no active user transaction has a matching savepoint, MyLite returns the same
missing-savepoint diagnostic as `ROLLBACK TO`.

If the savepoint exists, MyLite releases it and all later savepoints from the
MyLite registry and SQLite savepoint stack. Row changes remain part of the
active transaction. The statement returns a non-row result with affected rows
`0` and no warnings.

### Transaction Boundaries

The following events clear all user savepoint state:

- successful `COMMIT`;
- successful full `ROLLBACK`;
- `START TRANSACTION` while a user transaction is active, after the old
  transaction is committed;
- supported DDL/object lifecycle implicit commits;
- `mylite_close()` rollback of an active transaction.

`COMMIT` and full `ROLLBACK` outside a transaction continue to succeed as
no-ops and leave the savepoint registry empty.

### Statement Atomicity Interaction

Existing DML statement atomicity inside active user transactions uses a private
SQLite savepoint around each write statement. That internal savepoint is not
recorded in the user savepoint registry. User-visible savepoint operations run
only as top-level statements, so they cannot interleave with the internal
statement savepoint.

If a user creates a savepoint named like MyLite's internal statement savepoint,
the user savepoint remains user-visible state. The internal statement wrapper
is nested and released before control returns, so it must not delete or expose
the user's same-spelled savepoint.

## Result And Diagnostics

Successful supported savepoint statements:

- return `MYLITE_OK`;
- return a non-row result object;
- set `affected_rows == 0`;
- set `warning_count == 0`;
- make the next `ROW_COUNT()` return `0`;
- do not return result-set rows or columns.

Diagnostics:

- syntax errors use existing parser diagnostics;
- missing savepoints use MySQL-compatible error `1305`, SQLSTATE `42000`, and
  message `SAVEPOINT name does not exist`;
- SQLite begin/savepoint/release/rollback failures use the existing physical
  SQLite diagnostic surface unless they can be normalized to the specified
  missing-savepoint diagnostic;
- allocation failures return `MYLITE_NOMEM`;
- public API misuse is unchanged.

## SQLite Handling

MyLite uses public SQLite SQL control statements. It must not add a SQLite fork
patch for this feature.

The implementation must not rely on SQLite's duplicate savepoint-name behavior
matching MySQL. MyLite owns a savepoint stack and emits SQLite control
statements that preserve the MySQL-visible state:

- `SAVEPOINT <quoted-internal-name>` for new active-transaction savepoints;
- `ROLLBACK TO SAVEPOINT <quoted-internal-name>` for partial rollback;
- `RELEASE SAVEPOINT <quoted-internal-name>` for user-visible release.

When replacing a same-name savepoint, MyLite leaves the older internal SQLite
savepoint unreachable and removes only the old user-visible registry entry.
This is intentional: SQLite has no public operation to delete a non-top
savepoint while preserving later savepoints, and using one replacement policy
for top and non-top savepoints keeps failure behavior consistent. The
unreachable savepoint is cleared by later rollback/release of a lower savepoint
or by the transaction boundary, and it is never exposed through MyLite's
user-visible savepoint registry.

Generated SQLite identifiers must be quoted with the existing runtime dynamic
string identifier quoting helper. Savepoint names are identifiers, not values,
so there are no bound parameters for the control statements. Generated internal
names must be stable within a connection, collision-free against user-visible
names, and not written to catalog descriptors.

## Storage And File Format

Savepoint statements do not affect the MyLite file preamble. File-backed tests
must verify that rollback-to-savepoint, release, commit, full rollback, and
close-time rollback preserve the preamble and produce durable row state only
through the shifted SQLite payload.

## Tests

Add fast plain C tests under `packages/libmylite/tests/`, preferably by
extending `runtime_transaction_lifecycle_test.c` unless the feature becomes
clearer as a separate `runtime_savepoint_lifecycle` binary.

Coverage must include:

- parser coverage for supported savepoint forms and rejected unsupported forms;
- `SAVEPOINT` outside a transaction as a no-op success;
- `ROLLBACK TO` and `RELEASE` outside a transaction or without a matching
  savepoint as error `1305` / SQLSTATE `42000`;
- successful savepoint, rollback-to, release, and commit within a transaction;
- `ROLLBACK TO` keeps the target savepoint and removes later savepoints;
- `RELEASE` removes the target savepoint and later savepoints;
- duplicate savepoint replacement and release-after-replacement behavior;
- case-insensitive and backtick-quoted savepoint names;
- `ROLLBACK WORK TO` accepted forms;
- `COMMIT`, full `ROLLBACK`, nested `START TRANSACTION`, DDL implicit commit,
  and `mylite_close()` clear savepoints;
- statement-error rollback inside a transaction preserves user savepoints;
- successful statements report affected rows `0`, warning count `0`, no result
  rows, and no columns;
- missing-savepoint diagnostics appear through existing previous-diagnostics
  surfaces;
- file-backed reopen persistence after committed savepoint-controlled changes;
- independent file-backed handles keep independent savepoint state;
- zero-initialized cleanup for connection savepoint state;
- existing lexer, parser, runtime transaction, row values, update, delete, and
  DDL lifecycle tests still pass.

Run:

1. `packages/libmylite/tests/mysql_baseline_savepoint_lifecycle_expectations.sh`
2. `cmake --build --preset dev`
3. `ctest --preset dev --output-on-failure -R 'parser|runtime\\.transaction_lifecycle|runtime\\.savepoint_lifecycle|runtime\\.update_lifecycle|runtime\\.delete_lifecycle|runtime\\.row_values_lifecycle'`
4. `cmake --workflow --preset check`

## Compatibility Documentation

Update:

- `COMPATIBILITY.md`
- `docs/compatibility/sql-transactions.md`

The docs must say that user-visible savepoints are partially supported for
current explicit MyLite user transactions and the documented
autocommit-disabled transaction subset, including replacement, partial
rollback, release, and cleanup across current transaction boundaries. They must
not claim stored-program savepoint scoping, protocol transaction flags,
isolation/access modes, lock semantics, or full transaction compatibility.
