# Transaction Statements

## Scope

This feature specifies Task 21, MySQL-compatible transaction-control
statements for ordinary SQL execution:

- `START TRANSACTION`
- `BEGIN` and `BEGIN WORK` as transaction starters
- `COMMIT` and `COMMIT WORK`
- `ROLLBACK` and `ROLLBACK WORK`
- `READ ONLY`, `READ WRITE`, and `WITH CONSISTENT SNAPSHOT` transaction
  characteristics on `START TRANSACTION`
- `AND CHAIN`, `AND NO CHAIN`, `RELEASE`, and `NO RELEASE` completion options
- default autocommit behavior, statement-owned rollback, diagnostics, affected
  rows, and result metadata
- design hooks for `completion_type`, `autocommit`, `transaction_isolation`,
  and `transaction_read_only` session variables

Task 21's parser, AST, runtime state, DML transaction atomicity, read-only
write rejection, chaining, no-chain, release, affected-row behavior, the first
`CREATE TABLE` implicit-commit retrofit, and C API tests are implemented for
the subset described here. Remaining DDL implicit-commit retrofits and direct
transaction system variables remain deferred as documented below.

Out of scope for Task 21:

- `SAVEPOINT`, `ROLLBACK TO SAVEPOINT`, and `RELEASE SAVEPOINT`; these are
  Task 22
- stored-program `BEGIN ... END`; this is a compound-statement feature, not a
  transaction starter
- `SET TRANSACTION` isolation/access-mode assignment
- general `SET autocommit`, `SET completion_type`, `SET transaction_isolation`,
  `SET transaction_read_only`, and system-variable exposure
- `LOCK TABLES`, `UNLOCK TABLES`, metadata locks, table locks, backup locks,
  and global read locks
- XA transaction statements
- binary logging, replication, group replication, GTID, privileges, and
  Performance Schema transaction tables
- exact wire-protocol OK packet text and disconnect handling, until protocol
  support exists

## Sources

- MySQL 8.4 Reference Manual, `START TRANSACTION`, `COMMIT`, and `ROLLBACK`
  statements:
  https://dev.mysql.com/doc/refman/8.4/en/commit.html
- MySQL 8.4 Reference Manual, statements that cause implicit commit:
  https://dev.mysql.com/doc/refman/8.4/en/implicit-commit.html
- MySQL 8.4 Reference Manual, InnoDB autocommit, commit, and rollback:
  https://dev.mysql.com/doc/refman/8.4/en/innodb-autocommit-commit-rollback.html
- MySQL 8.4 Reference Manual, `SET TRANSACTION` statement:
  https://dev.mysql.com/doc/refman/8.4/en/set-transaction.html
- MySQL 8.4 Reference Manual, server system variables:
  https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html
- Existing MyLite specs:
  - `docs/specs/create-table-base-execution/specs.md`
  - `docs/specs/drop-table/specs.md`
  - `docs/specs/insert-values/specs.md`
  - `docs/specs/insert-set/specs.md`
  - `docs/specs/update-single-table/specs.md`
  - `docs/specs/delete-single-table/specs.md`
- Observed MySQL 8.4.9 runtime behavior from Docker container
  `mylite-mysql-849`, using `docker exec -i mylite-mysql-849 mysql -uroot`
  under the default SQL mode unless noted.

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar,
documentation prose, or implementation sources.

## MySQL 8.4.9 Behavior Summary

Runtime probes used this setup:

```sql
DROP DATABASE IF EXISTS mylite_task21_tx;
CREATE DATABASE mylite_task21_tx;
USE mylite_task21_tx;
CREATE TABLE t (id INT PRIMARY KEY, v INT) ENGINE=InnoDB;
INSERT INTO t VALUES (1, 10);
```

### Autocommit Default

New MySQL sessions start with `@@autocommit = 1`. When no explicit
transaction is active, each successful DML statement commits independently and
cannot be undone by a later `ROLLBACK`. A statement that fails still rolls back
its own partial effects.

`START TRANSACTION` and `BEGIN` begin a multi-statement transaction while the
session variable `@@autocommit` remains `1`. The transaction ends at `COMMIT`
or `ROLLBACK`, after which ordinary autocommit behavior resumes.

Observed expectations:

| SQL sequence | MySQL behavior |
| --- | --- |
| `START TRANSACTION; INSERT ...; ROLLBACK` | inserted row is absent |
| `BEGIN WORK; INSERT ...; COMMIT WORK` | inserted row is present |
| `COMMIT` with no active transaction | succeeds, `ROW_COUNT()` is `0`, warning count is `0` |
| `ROLLBACK` with no active transaction | succeeds, `ROW_COUNT()` is `0`, warning count is `0` |
| `SET autocommit = 0; INSERT ...; ROLLBACK` | inserted row is absent |
| `SET autocommit = 0; INSERT ...; SET autocommit = 1; ROLLBACK` | inserted row is present because enabling autocommit commits first |

`SET autocommit` is not part of Task 21, but the transaction state model must
leave a clean integration point for it.

### Auto-Increment State

`AUTO_INCREMENT` values allocated by InnoDB are not reused simply because the
transaction that allocated them rolls back. Observed expectations:

| SQL sequence | MySQL behavior |
| --- | --- |
| `CREATE TABLE ai (... AUTO_INCREMENT ...) AUTO_INCREMENT=30; START TRANSACTION; INSERT two generated rows; ROLLBACK; INSERT one generated row` | the surviving row receives id `32` |
| `START TRANSACTION; UPDATE ai SET id = 100; ROLLBACK; INSERT one generated row` | the original row id is restored, but the next generated row receives id `101` |

MyLite stores `AUTO_INCREMENT` metadata in its table catalog, so catalog writes
made inside an explicit transaction would normally roll back with the user
data. Task 21 therefore keeps handle-owned pending sequence advances while an
explicit transaction is active. `COMMIT` clears the pending list because the
catalog changes are durable. `ROLLBACK` rolls back user data, then reapplies
the largest pending sequence value for each table in autocommit mode.

### Transaction Start Statements

`START TRANSACTION` accepts a comma-separated list of transaction
characteristics. `BEGIN` and `BEGIN WORK` start a transaction but do not accept
characteristics.

Observed expectations:

| SQL | MySQL behavior |
| --- | --- |
| `START TRANSACTION` | succeeds, `ROW_COUNT()` is `0`, warning count is `0` |
| `START TRANSACTION READ WRITE` | succeeds |
| `START TRANSACTION READ ONLY` | succeeds |
| `START TRANSACTION WITH CONSISTENT SNAPSHOT` | succeeds under default `REPEATABLE-READ` isolation with no warning |
| `START TRANSACTION READ WRITE, WITH CONSISTENT SNAPSHOT` | succeeds |
| `START TRANSACTION READ ONLY, WITH CONSISTENT SNAPSHOT` | succeeds |
| `START TRANSACTION READ ONLY, READ ONLY` | succeeds with no warning |
| `START TRANSACTION READ WRITE, READ WRITE` | succeeds with no warning |
| `START TRANSACTION WITH CONSISTENT SNAPSHOT, WITH CONSISTENT SNAPSHOT` | succeeds with no warning |
| `START TRANSACTION READ WRITE, READ ONLY` | syntax error 1064, `ROW_COUNT()` becomes `-1` |
| `START TRANSACTION READ ONLY READ WRITE` | syntax error 1064 |
| `START TRANSACTION WITH CONSISTENT SNAPSHOT READ ONLY` | syntax error 1064 |
| `BEGIN` | succeeds as a transaction starter |
| `BEGIN WORK` | succeeds as a transaction starter |
| `BEGIN READ ONLY` | syntax error 1064 |
| `BEGIN WORK READ ONLY` | syntax error 1064 |

Starting a transaction while another transaction is active implicitly commits
the active transaction before beginning the next one. A probe that inserted row
`4`, issued another `START TRANSACTION`, inserted row `5`, and then rolled back
left row `4` committed and row `5` absent. The repeated start produced no
warning and `ROW_COUNT()` was `0`.

In stored programs, `BEGIN` belongs to `BEGIN ... END` block syntax rather than
transaction syntax. MyLite can parse top-level `BEGIN` as a transaction
statement now and reserve stored-program disambiguation for the stored-program
parser context.

### Access Modes

`READ WRITE` and `READ ONLY` select the access mode for the transaction being
started. The default is read/write unless future `SET TRANSACTION` or
`transaction_read_only` support changes it.

Observed expectations:

| SQL sequence | MySQL behavior |
| --- | --- |
| `START TRANSACTION READ ONLY; SELECT COUNT(*) FROM t; COMMIT` | read succeeds |
| `START TRANSACTION READ ONLY; INSERT INTO t VALUES (6,60)` | error 1792 / `25006`, cannot execute statement in a read-only transaction; `ROW_COUNT()` is `-1` |
| `START TRANSACTION READ WRITE; INSERT INTO t VALUES (6,60); ROLLBACK` | insert succeeds and is rolled back |

DDL has special interaction with transaction state. Non-temporary DDL causes an
implicit commit before execution, so Task 21 must not simply reject every DDL
statement after `START TRANSACTION READ ONLY`. Observed behavior:

```sql
START TRANSACTION READ ONLY;
CREATE TABLE ro_ddl_probe (id INT PRIMARY KEY) ENGINE=InnoDB;
ROLLBACK;
```

The table existed afterward. This is an implicit-commit interaction, not proof
that ordinary writes are allowed in read-only transactions.

### Consistent Snapshot

`WITH CONSISTENT SNAPSHOT` is accepted by `START TRANSACTION`. Under MySQL's
default `REPEATABLE-READ` isolation it produced no warning in the probes. Under
`READ COMMITTED`, MySQL accepted the statement but emitted warning code `138`
that the clause was ignored. `ROW_COUNT()` after that warning-producing
statement was `-1` and warning count was `1`.

MyLite does not yet implement isolation levels. The Task 21 implementation
should accept `WITH CONSISTENT SNAPSHOT`, record that the transaction requested
a snapshot, and emit no warning while MyLite exposes the default
`REPEATABLE-READ` compatibility state. When `SET TRANSACTION` or isolation
system variables land, this clause must use the current isolation level to
decide whether warning `138` is required.

### Commit And Rollback

`COMMIT` makes the active transaction durable. `ROLLBACK` cancels the active
transaction. Both succeed even when no explicit transaction is active.

Completion options:

| SQL | MySQL behavior |
| --- | --- |
| `COMMIT` | succeeds, `ROW_COUNT()` is `0` |
| `COMMIT WORK` | succeeds, `ROW_COUNT()` is `0` |
| `ROLLBACK` | succeeds, `ROW_COUNT()` is `0` |
| `ROLLBACK WORK` | succeeds, `ROW_COUNT()` is `0` |
| `COMMIT AND CHAIN` | commits and immediately starts a new transaction |
| `ROLLBACK AND CHAIN` | rolls back and immediately starts a new transaction |
| `COMMIT AND NO CHAIN NO RELEASE` | commits and does not start a new transaction |
| `ROLLBACK AND NO CHAIN NO RELEASE` | rolls back and does not start a new transaction |
| `COMMIT NO RELEASE` | accepted |
| `ROLLBACK NO RELEASE` | accepted |
| `COMMIT AND CHAIN NO RELEASE` | accepted |
| `ROLLBACK AND CHAIN NO RELEASE` | accepted |
| `COMMIT AND CHAIN RELEASE` | syntax error 1064 |
| `ROLLBACK AND CHAIN RELEASE` | syntax error 1064 |
| `COMMIT CHAIN` | syntax error 1064 |
| `COMMIT AND` | syntax error 1064 |
| `COMMIT AND NO` | syntax error 1064 |

`AND CHAIN` was verified with DML: insert row `2`, `COMMIT AND CHAIN`, insert
row `3`, then `ROLLBACK` left row `2` present and row `3` absent. `AND NO
CHAIN` was verified by inserting row `5` after `COMMIT AND NO CHAIN`; the later
`ROLLBACK` did not remove row `5` because autocommit had resumed.

The session variable `completion_type` changes the default completion behavior
for bare `COMMIT` and `ROLLBACK`. With `completion_type = CHAIN`, a bare
`COMMIT` behaved like `COMMIT AND CHAIN`: the following insert was undone by a
later rollback. `completion_type` defaulted to `NO_CHAIN`. Numeric value `2`
sets the variable to `RELEASE`; the unquoted word `RELEASE` in `SET SESSION
completion_type = RELEASE` is a syntax error because `RELEASE` is parsed as a
keyword.

### Release Completion

`COMMIT RELEASE` and `ROLLBACK RELEASE` terminate the client session after
ending the transaction. In a batch with a following `SELECT`, MySQL reported
lost connection error 2013 for the statement after the release. `COMMIT AND NO
CHAIN RELEASE` and `ROLLBACK AND NO CHAIN RELEASE` are accepted. `AND CHAIN
RELEASE` is rejected as syntax.

MyLite's embedded C handle should not free caller-owned memory implicitly.
The runtime should commit or roll back, mark the session as released, and let
the wire-protocol layer close the client connection after sending the OK
response. For the C API, a released handle should remain valid for
`mylite_close()` while subsequent statement preparation or execution returns a
deterministic connection-released error until the protocol/API design supplies
an exact MySQL client error mapping.

### Implicit Commit Boundaries

MySQL implicitly commits an active transaction before many DDL, account,
locking, and transaction-control statements. Most DDL also implicitly commits
after execution. Transaction-control statements such as `START TRANSACTION`
commit before they start a new transaction but do not commit again after.

Observed DDL probe:

```sql
START TRANSACTION;
INSERT INTO t VALUES (10, 100);
CREATE TABLE ddl_probe (id INT PRIMARY KEY) ENGINE=InnoDB;
ROLLBACK;
```

The inserted row and the created table both survived the later rollback.

MyLite now applies this behavior to non-temporary `CREATE TABLE` and ordinary
non-`TEMPORARY` `DROP TABLE`: an active explicit transaction is committed
before validation and execution, the DDL runs in its own statement transaction
when it reaches mutation, and no explicit transaction remains active afterward.
Temporary table DDL remains a transaction exception. Remaining DDL statements
still need the same retrofit, so the compatibility matrix continues to mark
implicit commit boundaries as incomplete.

### Diagnostics, Affected Rows, And Metadata

Transaction-control statements return no result set and no result-column
metadata. Successful statements set affected rows to `0`. Syntax errors and
runtime errors set `ROW_COUNT()` to `-1` in the observed MySQL probes.

Required diagnostics for Task 21:

- 1064 / `42000`: malformed transaction syntax, unsupported characteristics
  on `BEGIN`, missing `CHAIN` or `RELEASE` option parts, conflicting access
  modes, and `AND CHAIN RELEASE`
- 138 warning: `WITH CONSISTENT SNAPSHOT` ignored under an isolation level that
  cannot use a consistent snapshot, once isolation-level state exists
- 1792 / `25006`: writes attempted inside a read-only transaction
- 2013 / `HY000`: client-visible lost connection after release completion;
  exact exposure belongs to the protocol layer
- no warning for successful duplicate same-kind `READ ONLY`, `READ WRITE`, or
  `WITH CONSISTENT SNAPSHOT` characteristics

Current MyLite diagnostics are message-first. The implementation should still
thread structured numeric code, SQLSTATE, severity, and warning-count state
through the design so `SHOW WARNINGS`, protocol OK packets, and future client
APIs can expose MySQL-compatible details.

## MyLite Design

### Runtime State

Add handle-owned session transaction state to `mylite_db` or an equivalent
session object:

- `autocommit`, default `true`
- `transaction_active`
- `transaction_access_mode`, default read/write
- `transaction_isolation`, default repeatable read
- `completion_type`, default no-chain
- `transaction_requested_consistent_snapshot`
- `transaction_released`
- optional counters for transaction diagnostics and future status variables

`START TRANSACTION` should not change the exposed `autocommit` value. It should
set `transaction_active` until a transaction terminator finishes. If a
transaction is already active, the runtime first commits it, then starts the
new one. Any future savepoints from Task 22 must be cleared by full commit,
full rollback, and repeated transaction start.

When `autocommit` support lands:

- `autocommit = 1` and no explicit transaction means each successful DML
  statement commits independently
- `autocommit = 0` means the session always has an open transaction, and
  `COMMIT` or `ROLLBACK` immediately opens the next transaction
- changing `autocommit` from `0` to `1` commits the open transaction first

### SQLite Transaction Strategy

Current MyLite write statements use statement-owned SQLite transactions through
helpers that issue `BEGIN IMMEDIATE`, `COMMIT`, and `ROLLBACK`. That is correct
for autocommit statement atomicity but conflicts with explicit transactions,
because SQLite cannot nest `BEGIN` inside `BEGIN`.

Task 21 should introduce a statement-atomicity helper with two modes:

- if no explicit or autocommit-off transaction is active, begin and finish a
  real SQLite transaction around the statement
- if a user transaction is active, use a SQLite savepoint for statement
  rollback, then release the savepoint on statement success without committing
  the surrounding user transaction

This helper is an internal statement rollback mechanism and is distinct from
Task 22 user-visible savepoints. User-visible savepoints must share the same
SQLite transaction but keep separate MySQL names, replacement rules, and
diagnostics.

For explicit `START TRANSACTION`, MyLite should prefer `BEGIN DEFERRED` or an
equivalent delayed-lock strategy where possible. `BEGIN IMMEDIATE` can take a
write lock earlier than MySQL's InnoDB behavior and can make read-only or
read-mostly transactions less compatible. Writes can upgrade the SQLite
transaction when needed, while MyLite enforces MySQL access-mode rules before
executing a write.

MyLite additionally accepts top-level `BEGIN IMMEDIATE` as a narrow
file-backed lock-probe extension for internal testing. MySQL 8.4.9 rejects this
syntax, so this extension must not be counted as MySQL SQL compatibility. It
starts a read/write explicit transaction using SQLite's immediate mode so a
pre-opened second handle can verify read surfaces such as `SELECT`,
`SHOW TABLES`, and `DESCRIBE` while the first handle owns the write lock.

### Parser And AST Needs

New AST nodes should separate syntax from execution policy:

- `MYLITE_SQL_AST_START_TRANSACTION_STATEMENT`
- `MYLITE_SQL_AST_BEGIN_TRANSACTION_STATEMENT`
- `MYLITE_SQL_AST_TRANSACTION_CHARACTERISTIC_LIST`
- `MYLITE_SQL_AST_TRANSACTION_CHARACTERISTIC`
- `MYLITE_SQL_AST_COMMIT_STATEMENT`
- `MYLITE_SQL_AST_ROLLBACK_STATEMENT`
- `MYLITE_SQL_AST_TRANSACTION_COMPLETION`

The AST should preserve source spans and option order for diagnostics, but the
analyzer should normalize duplicate same-kind characteristics and reject only
combinations that MySQL rejects.

Independently authored MyLite Lemon-style grammar sketch:

```lemon
statement(A) ::= start_transaction_statement(B). { A = B; }
statement(A) ::= begin_transaction_statement(B). { A = B; }
statement(A) ::= commit_statement(B). { A = B; }
statement(A) ::= rollback_statement(B). { A = B; }

start_transaction_statement(A) ::= START(T) TRANSACTION opt_transaction_characteristics(C). {
    A = make_start_transaction_statement(state, T, C);
}

opt_transaction_characteristics(A) ::= . { A = NULL; }
opt_transaction_characteristics(A) ::= transaction_characteristic_list(B). { A = B; }

transaction_characteristic_list(A) ::= transaction_characteristic(B). {
    A = make_transaction_characteristic_list(state, B);
}
transaction_characteristic_list(A) ::= transaction_characteristic_list(B) COMMA transaction_characteristic(C). {
    A = append_transaction_characteristic(state, B, C);
}

transaction_characteristic(A) ::= READ(T) WRITE. {
    A = make_transaction_access_mode(state, T, MYLITE_TRANSACTION_ACCESS_READ_WRITE);
}
transaction_characteristic(A) ::= READ(T) ONLY. {
    A = make_transaction_access_mode(state, T, MYLITE_TRANSACTION_ACCESS_READ_ONLY);
}
transaction_characteristic(A) ::= WITH(T) CONSISTENT SNAPSHOT. {
    A = make_transaction_consistent_snapshot(state, T);
}

begin_transaction_statement(A) ::= BEGIN(T) opt_work. {
    A = make_begin_transaction_statement(state, T);
}

commit_statement(A) ::= COMMIT(T) opt_work opt_transaction_completion(C). {
    A = make_commit_statement(state, T, C);
}

rollback_statement(A) ::= ROLLBACK(T) opt_work opt_transaction_completion(C). {
    A = make_rollback_statement(state, T, C);
}

opt_work ::= .
opt_work ::= WORK.

opt_transaction_completion(A) ::= . {
    A = make_transaction_completion_default(state);
}
opt_transaction_completion(A) ::= RELEASE. {
    A = make_transaction_completion_release_only(state, true);
}
opt_transaction_completion(A) ::= NO RELEASE. {
    A = make_transaction_completion_release_only(state, false);
}
opt_transaction_completion(A) ::= AND CHAIN opt_no_release(C). {
    A = make_transaction_completion(state, MYLITE_TRANSACTION_CHAIN_YES, C);
}
opt_transaction_completion(A) ::= AND NO CHAIN opt_release(C). {
    A = make_transaction_completion(state, MYLITE_TRANSACTION_CHAIN_NO, C);
}

opt_no_release(A) ::= . {
    A = make_transaction_release_default(state);
}
opt_no_release(A) ::= NO RELEASE. {
    A = make_transaction_release_option(state, false);
}

opt_release(A) ::= . {
    A = make_transaction_release_default(state);
}
opt_release(A) ::= RELEASE. {
    A = make_transaction_release_option(state, true);
}
opt_release(A) ::= NO RELEASE. {
    A = make_transaction_release_option(state, false);
}
```

The grammar deliberately accepts `RELEASE` after `AND NO CHAIN` but not after
`AND CHAIN`, matching the MySQL 8.4.9 probes.

`BEGIN` is currently a restricted-label keyword in the lexer. Top-level SQL can
route it to `begin_transaction_statement`. Stored-program support must later
parse `BEGIN ... END` in a stored-program context before considering
transaction-starter syntax.

### Statement Execution

`START TRANSACTION` / `BEGIN`:

1. Clear previous diagnostics.
2. If the session is released, return the connection-released diagnostic.
3. If another transaction is active, commit it first.
4. Resolve access mode from explicit characteristics or session defaults.
5. Resolve consistent-snapshot behavior from current isolation state.
6. Start the SQLite transaction with the selected strategy.
7. Store transaction state and set affected rows to `0`.

`COMMIT`:

1. Clear previous diagnostics.
2. If no transaction is active and `autocommit = 1`, succeed with affected
   rows `0`.
3. Commit the active SQLite transaction.
4. Clear active transaction state and Task 22 savepoint state.
5. Apply explicit completion options or `completion_type`.
6. If chaining, immediately start a new transaction with the previous
   transaction's isolation level and access mode.
7. If release is requested, mark the session released after commit succeeds.

`ROLLBACK`:

1. Clear previous diagnostics.
2. If no transaction is active and `autocommit = 1`, succeed with affected
   rows `0`.
3. Roll back the active SQLite transaction.
4. Clear active transaction state and Task 22 savepoint state.
5. Apply explicit completion options or `completion_type`.
6. If chaining, immediately start a new transaction with the previous
   transaction's isolation level and access mode.
7. If release is requested, mark the session released after rollback succeeds.

Read-only enforcement belongs in the shared write-statement entry path, not in
individual table-specific code. Every DML executor that changes permanent user
tables must check the session transaction access mode before evaluating
write-side side effects. Temporary-table exceptions are deferred until
temporary tables exist.

### DDL And Implicit Commit Integration

Existing DDL helpers for `CREATE TABLE`, `DROP TABLE`, and schema lifecycle use
statement transactions for all-or-nothing execution. Task 21 should not remove
that safety. It should add explicit user-transaction awareness:

- before non-temporary DDL that MySQL classifies as implicit-commit DDL, commit
  the active user transaction
- execute the DDL in its own statement transaction
- leave no user transaction active afterward unless a later feature proves a
  statement-specific exception

Current specs for `CREATE TABLE` and `DROP TABLE` document implicit commit as
deferred. Task 21 can provide the shared helper and state model, while exact
retrofits may be completed as follow-up DDL compatibility work.

### Storage And Performance

Task 21 does not change the `.mylite` file format, schema catalog, table
catalog, column catalog, or index catalog. It adds handle/session state and
changes transaction boundaries around existing SQLite writes.

Performance considerations:

- avoid starting SQLite write transactions for read-only transaction starters
  until a write is attempted
- use statement savepoints inside explicit transactions instead of copying
  affected rows for rollback
- keep transaction state handle-owned to preserve independent MyLite
  connections
- avoid process-global state for autocommit, completion type, or transaction
  flags

## MySQL-Verified Test Expectations

### Parser Tests

Accepted forms:

| SQL | Expected parser result |
| --- | --- |
| `START TRANSACTION` | start-transaction AST with no characteristics |
| `START TRANSACTION READ WRITE` | start-transaction AST with read/write mode |
| `START TRANSACTION READ ONLY` | start-transaction AST with read-only mode |
| `START TRANSACTION WITH CONSISTENT SNAPSHOT` | start-transaction AST with snapshot flag |
| `START TRANSACTION READ WRITE, WITH CONSISTENT SNAPSHOT` | two characteristics |
| `START TRANSACTION READ ONLY, READ ONLY` | accepted; analyzer normalizes duplicate read-only |
| `START TRANSACTION WITH CONSISTENT SNAPSHOT, WITH CONSISTENT SNAPSHOT` | accepted; analyzer normalizes duplicate snapshot |
| `BEGIN` | begin-transaction AST |
| `BEGIN WORK` | begin-transaction AST |
| `BEGIN IMMEDIATE` | MyLite lock-probe extension AST, not MySQL syntax |
| `COMMIT` | commit AST, default completion |
| `COMMIT WORK` | commit AST, default completion |
| `COMMIT AND CHAIN` | commit AST, chain completion |
| `COMMIT AND CHAIN NO RELEASE` | commit AST, chain and no-release completion |
| `COMMIT AND NO CHAIN` | commit AST, no-chain completion |
| `COMMIT AND NO CHAIN NO RELEASE` | commit AST, no-chain and no-release completion |
| `COMMIT AND NO CHAIN RELEASE` | commit AST, no-chain release completion |
| `COMMIT RELEASE` | commit AST, release completion |
| `COMMIT NO RELEASE` | commit AST, no-release completion |
| `ROLLBACK` | rollback AST, default completion |
| `ROLLBACK WORK` | rollback AST, default completion |
| `ROLLBACK AND CHAIN` | rollback AST, chain completion |
| `ROLLBACK AND CHAIN NO RELEASE` | rollback AST, chain and no-release completion |
| `ROLLBACK AND NO CHAIN` | rollback AST, no-chain completion |
| `ROLLBACK AND NO CHAIN NO RELEASE` | rollback AST, no-chain and no-release completion |
| `ROLLBACK AND NO CHAIN RELEASE` | rollback AST, no-chain release completion |
| `ROLLBACK RELEASE` | rollback AST, release completion |

Rejected forms:

| SQL | MySQL expectation |
| --- | --- |
| `START TRANSACTION READ WRITE, READ ONLY` | syntax error 1064 |
| `START TRANSACTION READ ONLY READ WRITE` | syntax error 1064 |
| `START TRANSACTION WITH CONSISTENT SNAPSHOT READ ONLY` | syntax error 1064 |
| `BEGIN READ ONLY` | syntax error 1064 |
| `BEGIN WORK READ ONLY` | syntax error 1064 |
| `COMMIT CHAIN` | syntax error 1064 |
| `COMMIT AND` | syntax error 1064 |
| `COMMIT AND NO` | syntax error 1064 |
| `COMMIT AND CHAIN RELEASE` | syntax error 1064 |
| `COMMIT AND CHAIN AND RELEASE` | syntax error 1064 |
| `ROLLBACK AND CHAIN RELEASE` | syntax error 1064 |
| `ROLLBACK TO SAVEPOINT s` | not Task 21; belongs to Task 22 |
| `RELEASE SAVEPOINT s` | not Task 21; belongs to Task 22 |
| `BEGIN label: SELECT 1; END` | not Task 21; stored-program parser surface |

### Runtime Tests

Runtime tests should compare against MySQL 8.4.9 for result rows, affected
rows, warning counts, errors, and side effects.

Required Task 21 runtime cases:

| Scenario | Expected behavior |
| --- | --- |
| `START TRANSACTION; INSERT; ROLLBACK` | row absent, transaction statements affected rows `0` |
| `BEGIN WORK; INSERT; COMMIT WORK` | row present |
| repeated `START TRANSACTION` after an insert | first insert committed, later rollback does not undo it |
| `COMMIT` with no active transaction | succeeds, affected rows `0`, warning count `0` |
| `ROLLBACK` with no active transaction | succeeds, affected rows `0`, warning count `0` |
| `COMMIT AND CHAIN`, then insert, then `ROLLBACK` | pre-chain work committed, post-chain work rolled back |
| `ROLLBACK AND CHAIN`, then insert, then `ROLLBACK` | pre-chain work rolled back, post-chain work rolled back |
| `COMMIT AND NO CHAIN`, then insert, then `ROLLBACK` | post-commit insert remains because autocommit resumed |
| `ROLLBACK AND NO CHAIN`, then insert, then `ROLLBACK` | post-rollback insert remains because autocommit resumed |
| `AUTO_INCREMENT` insert inside a rolled-back transaction | row is absent, but consumed ids are not reused |
| `AUTO_INCREMENT` column update to a high value inside a rolled-back transaction | row value is restored, but the high value advances the next generated id |
| `START TRANSACTION READ ONLY; SELECT ...; COMMIT` | read succeeds |
| `START TRANSACTION READ ONLY; INSERT ...` | error 1792 / `25006`, affected rows `-1`, row absent |
| `START TRANSACTION READ WRITE; INSERT ...; ROLLBACK` | insert succeeds and rolls back |
| `START TRANSACTION WITH CONSISTENT SNAPSHOT` under default isolation | succeeds without warning |
| `SET SESSION TRANSACTION ISOLATION LEVEL READ COMMITTED; START TRANSACTION WITH CONSISTENT SNAPSHOT` | warning 138 once isolation state exists |
| `START TRANSACTION; INSERT; CREATE TABLE; ROLLBACK` | inserted row and created table survive due implicit commit |
| `START TRANSACTION READ ONLY; CREATE TABLE; ROLLBACK` | created table survives due implicit commit |
| `START TRANSACTION; INSERT; DROP TABLE existing; INSERT; ROLLBACK` | both inserts and the drop survive due implicit commit |
| `START TRANSACTION; INSERT; DROP TABLE missing; INSERT; ROLLBACK` | both inserts survive because the failed drop still commits before validation |
| `BEGIN IMMEDIATE` on a file-backed handle while another handle reads | pre-opened reader can still run `SELECT`, `SHOW TABLES`, and `DESCRIBE` |
| `COMMIT RELEASE` followed by another statement in protocol tests | connection is released after OK response |
| `ROLLBACK RELEASE` followed by another statement in protocol tests | connection is released after OK response |

When MyLite does not yet expose SQL `ROW_COUNT()`, tests should verify the same
state through `mylite_affected_rows()` and public warning/error accessors, then
add SQL-level assertions when `ROW_COUNT()` and `SHOW WARNINGS` are implemented.

## Deferred Behavior

- User-visible savepoint grammar and semantics are Task 22.
- `SET TRANSACTION` and full isolation-level behavior remain deferred.
- Direct `SET autocommit`, `SET completion_type`, `SET transaction_read_only`,
  and `SET transaction_isolation` remain deferred to the system-variable task.
- Remaining DDL implicit-commit retrofits are tracked by DDL statement tasks
  and the compatibility matrix's implicit-commit row.
- Temporary-table exceptions for read-only transactions and implicit commits
  are deferred until temporary tables exist.
- Stored-program `BEGIN ... END` remains separate from top-level transaction
  `BEGIN`.
- Exact protocol disconnect, OK-packet info strings, status flags, and
  session-state tracking belong to the protocol/session-state tasks.
- XA, binary logging, replication safety, table locks, metadata locks,
  privilege checks, and Performance Schema transaction tables are out of scope.

## Implementation Handoff

1. Add parser and AST support for Task 21 statements and completion options.
2. Add handle-owned transaction state with default autocommit-on behavior.
3. Replace direct statement transaction helpers with a shared helper that uses
   real SQLite transactions outside user transactions and savepoints inside
   user transactions.
4. Implement `START TRANSACTION` / `BEGIN`, including repeated-start implicit
   commit and access-mode normalization.
5. Implement `COMMIT` and `ROLLBACK`, including chain and release decisions.
6. Add read-only checks to shared permanent-table write paths.
7. Add tests that prove existing `INSERT`, `UPDATE`, `DELETE`, and supported
   DDL statements interact correctly with explicit transaction boundaries.
8. Keep Task 22 savepoint behavior separate, but reserve the state clearing and
   statement-savepoint hooks needed by savepoints.
