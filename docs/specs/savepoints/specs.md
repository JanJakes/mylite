# Savepoints

## Scope

This feature specifies Task 22, MySQL-compatible savepoint statements for
ordinary SQL execution:

- `SAVEPOINT identifier`
- `ROLLBACK [WORK] TO [SAVEPOINT] identifier`
- `RELEASE SAVEPOINT identifier`
- savepoint replacement by name
- nested savepoint behavior
- missing-savepoint diagnostics
- interaction with Task 21 explicit transactions
- interaction with Task 21 statement-owned savepoint atomicity
- `AUTO_INCREMENT` preservation across partial rollback

Task 22 implements the parser, AST, runtime, and test coverage for ordinary
top-level savepoint statements in explicit MyLite transactions. The remaining
deferred items are listed below where they depend on future autocommit,
stored-program, protocol, status-variable, or DDL implicit-commit work.

Out of scope for Task 22:

- `SET autocommit`, except for designing the future integration point
- stored procedures, stored functions, triggers, and events, except for
  documenting savepoint-level requirements
- XA transaction behavior
- nontransactional storage engines
- exact wire-protocol OK packet text and status-variable exposure
- DDL implicit-commit retrofits beyond the integration contract already
  documented by Task 21

## Sources

- MySQL 8.4 Reference Manual, `SAVEPOINT`, `ROLLBACK TO SAVEPOINT`, and
  `RELEASE SAVEPOINT` statements:
  https://dev.mysql.com/doc/refman/8.4/en/savepoint.html
- MySQL 8.4 Reference Manual, `START TRANSACTION`, `COMMIT`, and `ROLLBACK`
  statements:
  https://dev.mysql.com/doc/refman/8.4/en/commit.html
- MySQL 8.4 Reference Manual, statements that cause implicit commit:
  https://dev.mysql.com/doc/refman/8.4/en/implicit-commit.html
- MySQL 8.4 Reference Manual, schema object names:
  https://dev.mysql.com/doc/refman/8.4/en/identifiers.html
- MySQL 8.4 Reference Manual, identifier length limits:
  https://dev.mysql.com/doc/refman/8.4/en/identifier-length.html
- MySQL 8.4 Reference Manual, `BEGIN ... END` compound statement:
  https://dev.mysql.com/doc/refman/8.4/en/begin-end.html
- Existing MyLite specs:
  - `docs/specs/transaction-statements/specs.md`
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

Runtime probes used this setup unless the individual probe created additional
tables:

```sql
DROP DATABASE IF EXISTS mylite_task22_savepoints;
CREATE DATABASE mylite_task22_savepoints;
USE mylite_task22_savepoints;
CREATE TABLE t (id INT PRIMARY KEY, v INT) ENGINE=InnoDB;
CREATE TABLE ai (
    id INT PRIMARY KEY AUTO_INCREMENT,
    v INT
) ENGINE=InnoDB AUTO_INCREMENT=10;
```

The probed server reported:

```text
VERSION() = 8.4.9
@@autocommit = 1
@@sql_mode = ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,
             NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION
```

### Basic Savepoint Semantics

`SAVEPOINT name` establishes a named mark inside the current transaction.
`ROLLBACK TO name` rolls back changes made after that mark and keeps the
transaction active. `ROLLBACK TO` also keeps the named savepoint available for
another rollback. `RELEASE SAVEPOINT name` removes a savepoint without rolling
back data and without committing the transaction.

Observed expectations:

| SQL sequence | MySQL behavior |
| --- | --- |
| `START TRANSACTION; INSERT 2; SAVEPOINT a; INSERT 3; ROLLBACK TO a` | row `2` remains visible in the transaction; row `3` is gone |
| `ROLLBACK TO a; INSERT 4; ROLLBACK TO a` | the second rollback to `a` succeeds because the target is retained |
| `RELEASE SAVEPOINT a` | succeeds with affected rows `0`, no warning, no data change |
| `ROLLBACK TO a` after `RELEASE SAVEPOINT a` | error 1305 / `42000`; `ROW_COUNT()` becomes `-1` |
| `COMMIT` after savepoints exist | commits data and clears all savepoints |
| full `ROLLBACK` after savepoints exist | rolls back data and clears all savepoints |

Successful savepoint-control statements return no result set, expose no result
columns, set affected rows to `0`, and leave warning count at `0`.

### Autocommit

With `@@autocommit = 1` and no active explicit transaction, `SAVEPOINT name`
succeeds but does not create a usable savepoint. It also does not begin a
multi-statement transaction. A following DML statement commits independently,
and a later `ROLLBACK TO name` or `RELEASE SAVEPOINT name` returns missing
savepoint error 1305.

Observed sequence:

```sql
SAVEPOINT sp_outside;
INSERT INTO t VALUES (1, 10);
ROLLBACK TO SAVEPOINT sp_outside;
```

The `SAVEPOINT` statement had affected rows `0` and warning count `0`. The
`ROLLBACK TO` failed with error 1305 / `42000`, `ROW_COUNT()` became `-1`,
and row `1` remained present.

With `SET autocommit = 0`, a transaction is active even without
`START TRANSACTION`, so savepoints work as normal transaction savepoints:

```sql
SET autocommit = 0;
INSERT INTO t VALUES (1, 10);
SAVEPOINT ac0;
INSERT INTO t VALUES (2, 20);
ROLLBACK TO SAVEPOINT ac0;
COMMIT;
```

Only row `1` remained. MyLite does not yet implement `SET autocommit`, but the
savepoint runtime state must be compatible with the future autocommit-off
state model from Task 21.

### Replacement And Nesting

Savepoint names are unique within the active savepoint level. Creating a
savepoint with a name that already exists deletes the old mark for that name
and creates a new mark at the current transaction position. Intervening
savepoints with different names remain usable.

Observed replacement sequence:

```sql
START TRANSACTION;
INSERT INTO t VALUES (1, 10);
SAVEPOINT a;
INSERT INTO t VALUES (2, 20);
SAVEPOINT b;
INSERT INTO t VALUES (3, 30);
SAVEPOINT a;
INSERT INTO t VALUES (4, 40);
ROLLBACK TO SAVEPOINT b;
```

`ROLLBACK TO b` succeeded and left rows `1` and `2`. This means replacing `a`
did not delete intervening savepoint `b`. A later `ROLLBACK TO a` targets the
new `a`, not the old mark.

`ROLLBACK TO name` deletes active savepoints that were created after the
target savepoint. It does not delete savepoints created before the target, and
it does not delete the target itself.

`RELEASE SAVEPOINT name` deletes the target savepoint and savepoints created
after it. It does not roll back data. This is important because the official
manual describes release as removing the named savepoint, but MySQL 8.4.9 also
made later savepoints unavailable.

Observed release nesting sequences:

| SQL sequence | MySQL behavior |
| --- | --- |
| `SAVEPOINT a; SAVEPOINT b; RELEASE SAVEPOINT a; ROLLBACK TO b` | `ROLLBACK TO b` returns error 1305; data changes after `a` are still present |
| `SAVEPOINT c; SAVEPOINT d; RELEASE SAVEPOINT d; ROLLBACK TO c` | `ROLLBACK TO c` succeeds and rolls back changes after `c` |

### Names And Identifier Rules

Savepoint names use MySQL identifier syntax, but probes showed that they are
not capped by the 64-character schema-object identifier limit. Quoted
savepoint names of 512 characters succeeded. MyLite should not apply table,
column, index, or stored-program length limits to savepoint names. Practical
limits should come from SQL text length and memory allocation limits.

Observed expectations:

| SQL | MySQL behavior |
| --- | --- |
| `SAVEPOINT MixedCase; ROLLBACK TO mixedcase` | succeeds; lookup is case-insensitive |
| `SAVEPOINT \`QuotedCase\`; ROLLBACK TO \`quotedcase\`` | succeeds; quoted-name lookup is also case-insensitive |
| `SAVEPOINT \`select\`` | succeeds because the reserved word is quoted |
| `SAVEPOINT select` | syntax error 1064 / `42000` |
| `SAVEPOINT db.sp` | syntax error 1064 / `42000`; names are not schema-qualified |
| `SAVEPOINT \`db.sp\`; ROLLBACK TO \`db.sp\`` | succeeds; the dot is part of one quoted identifier |
| `SAVEPOINT` without a name | syntax error 1064 / `42000` |

The implementation should compare savepoint names case-insensitively. The
initial implementation may follow the repository's current identifier
normalization for supported identifier characters, but broad Unicode identifier
case behavior should be verified when MyLite expands non-ASCII identifier
support.

### Syntax Variants

Accepted rollback-to forms:

| SQL | MySQL behavior |
| --- | --- |
| `ROLLBACK TO s` | accepted |
| `ROLLBACK WORK TO s` | accepted |
| `ROLLBACK TO SAVEPOINT s` | accepted |
| `ROLLBACK WORK TO SAVEPOINT s` | accepted |

Rejected forms:

| SQL | MySQL behavior |
| --- | --- |
| `RELEASE s` | syntax error 1064 / `42000` |
| `ROLLBACK TO s AND CHAIN` | syntax error 1064 / `42000` |
| `ROLLBACK TO s RELEASE` | syntax error 1064 / `42000` |
| `ROLLBACK SAVEPOINT s` | syntax error 1064 / `42000` |

`AND CHAIN`, `AND NO CHAIN`, `RELEASE`, and `NO RELEASE` are full
transaction-completion options only. They are not valid on `ROLLBACK TO`.

### Diagnostics

Missing savepoints return error 1305 / `42000` with a message that names the
missing savepoint. The transaction remains active after the error.

Observed sequence:

```sql
START TRANSACTION;
INSERT INTO t VALUES (4, 40);
ROLLBACK TO SAVEPOINT missing;
INSERT INTO t VALUES (5, 50);
RELEASE SAVEPOINT missing;
COMMIT;
```

Both missing-savepoint statements returned error 1305, but rows `4` and `5`
were committed. Missing-savepoint errors therefore must not roll back the
active transaction, clear savepoints, or mark the session unusable.

Required diagnostics:

| Condition | MySQL diagnostic |
| --- | --- |
| malformed savepoint syntax | 1064 / `42000` syntax error |
| `ROLLBACK TO` unknown name | 1305 / `42000`, savepoint does not exist |
| `RELEASE SAVEPOINT` unknown name | 1305 / `42000`, savepoint does not exist |
| `SAVEPOINT` with no active transaction under autocommit-on | success; no warning; no usable savepoint |

Current MyLite diagnostics are message-first. Task 22 should still introduce a
named condition for error 1305 so future protocol, `SHOW WARNINGS`, and C API
diagnostics can expose MySQL-compatible numeric codes and SQLSTATEs.

### Read-Only Transactions

Savepoint control is permitted in read-only transactions. It does not modify
user tables and should not trip Task 21 DML read-only enforcement.

Observed sequence:

```sql
START TRANSACTION READ ONLY;
SAVEPOINT ro;
ROLLBACK TO SAVEPOINT ro;
RELEASE SAVEPOINT ro;
```

All three savepoint-control statements succeeded with affected rows `0` and
warning count `0`. A subsequent `INSERT` in the same read-only transaction
still failed with error 1792 / `25006`.

### AUTO_INCREMENT Side Effects

`ROLLBACK TO SAVEPOINT` does not make InnoDB reuse generated
`AUTO_INCREMENT` values consumed after the savepoint.

Observed sequence:

```sql
START TRANSACTION;
SAVEPOINT before_ai;
INSERT INTO ai (v) VALUES (100), (200);
ROLLBACK TO SAVEPOINT before_ai;
INSERT INTO ai (v) VALUES (300);
COMMIT;
SELECT id, v FROM ai ORDER BY id;
```

The surviving row received id `12`, not `10`. MyLite already preserves pending
auto-increment advances across full transaction rollback in Task 21. Task 22
must extend that design so partial rollback cannot revert the catalog-visible
next auto-increment value that should survive a later commit.

### DDL And Implicit Commits

Non-temporary DDL causes an implicit commit before execution. That commit
clears savepoints. If a savepoint existed before a non-temporary DDL statement,
a later `ROLLBACK TO` for that name fails with error 1305. DML before the DDL
is committed.

Observed sequence:

```sql
START TRANSACTION;
INSERT INTO t VALUES (7, 70);
SAVEPOINT before_ddl;
INSERT INTO t VALUES (8, 80);
CREATE TABLE ddl_probe (id INT PRIMARY KEY) ENGINE=InnoDB;
ROLLBACK TO SAVEPOINT before_ddl;
ROLLBACK;
```

`ROLLBACK TO before_ddl` returned error 1305. Rows `7` and `8` remained
present, and `ddl_probe` existed. Task 21 documents full DDL implicit-commit
retrofits as deferred; Task 22 must integrate with that eventual helper by
clearing user-visible savepoint state whenever an implicit commit finishes.

### Stored Functions And Triggers

The official MySQL behavior creates a separate savepoint level while a stored
function or trigger runs. Savepoints in the caller's level are unavailable to
that routine or trigger, and savepoints created inside the nested level are
released when control returns.

MyLite does not yet implement stored programs or triggers. Task 22 should keep
the savepoint state model level-aware enough that a future stored-program
executor can push a new savepoint level without reworking the transaction core.

### Status Counters

MySQL exposes command counters such as `Com_savepoint`,
`Com_rollback_to_savepoint`, and `Com_release_savepoint`. A probe in one
session showed each successful savepoint-control statement incrementing the
corresponding `Com_*` counter by one. `Handler_savepoint` and
`Handler_savepoint_rollback` remained `0` in the InnoDB probes.

MyLite can defer status-variable exposure until the `SHOW STATUS` and system
variable work lands, but the runtime should use operation-specific execution
paths so future counters can be wired without parsing statement text again.

## MyLite Design

### Runtime State

Extend handle-owned transaction state with a savepoint stack. The state belongs
to the session handle, not to prepared statements.

Proposed structures:

```c
struct mylite_savepoint {
    char *original_name;
    char *normalized_name;
    char *sqlite_name;
    unsigned int level;
};

struct mylite_savepoint_state {
    struct mylite_savepoint *items;
    size_t count;
    size_t capacity;
    uint64_t next_sqlite_id;
    unsigned int current_level;
};
```

`original_name` preserves the spelling needed for diagnostics. `normalized_name`
is the case-insensitive lookup key. `sqlite_name` is an internal generated name,
never the user-visible name.

Savepoints are active only when there is an active explicit transaction or,
after future `SET autocommit = 0` support, an active autocommit-off
transaction. Under autocommit-on with no active transaction, `SAVEPOINT` is a
successful no-op and the stack remains empty.

Full transaction completion must clear the stack:

- `COMMIT`
- full `ROLLBACK`
- repeated `START TRANSACTION` or `BEGIN` after their implicit commit
- implicit commits caused by non-temporary DDL and other future
  implicit-commit statements
- connection close or handle reset

`ROLLBACK TO` must not clear the target savepoint. `RELEASE SAVEPOINT` must
delete the target and active savepoints created after it.

### SQLite Savepoint Strategy

Do not pass MySQL savepoint names directly to SQLite. Generate separate SQLite
savepoint names, such as:

```text
mylite_user_savepoint_1
mylite_user_savepoint_2
...
```

Reasons:

- MySQL lookup is case-insensitive; SQLite savepoint lookup is not the
  compatibility contract MyLite should expose.
- MySQL replacement semantics differ from simple raw-name forwarding.
- A user-visible savepoint named `mylite_statement_atomicity` must not collide
  with Task 21's internal statement rollback savepoint.
- Quoted MySQL identifiers can contain characters that should not be interpolated
  into raw SQLite SQL text.

The Task 21 statement-owned savepoint name `mylite_statement_atomicity` remains
an internal implementation detail. User savepoint internals must use a
different generated namespace. If Task 22 changes statement atomicity later, it
should still keep statement savepoints and user savepoints in disjoint internal
namespaces.

Replacement behavior should not try to physically delete a non-top SQLite
savepoint because doing so would release later active savepoints and break
MySQL behavior. Instead:

1. Remove the old same-name entry from MyLite's active stack.
2. If the removed entry was the top active savepoint, optionally release its
   SQLite savepoint immediately.
3. If it was not topmost, leave its SQLite savepoint hidden and unreachable.
4. Create a new generated SQLite savepoint and append a new active entry.

Hidden obsolete SQLite savepoints are harmless until the surrounding
transaction ends. The implementation should avoid unbounded work per operation,
but it may leave obsolete internal savepoints in SQLite when preserving later
active savepoints requires that.

`ROLLBACK TO` execution:

1. Find the most recent active savepoint in the current level with the
   normalized name.
2. If no match exists, return error 1305 without changing transaction state.
3. Execute `ROLLBACK TO SAVEPOINT <sqlite_name>` using the internal name.
4. Drop active stack entries after the target.
5. Keep the target entry.
6. Reapply transaction-scoped pending auto-increment maxima inside the active
   transaction.
7. Set affected rows to `0`.

`RELEASE SAVEPOINT` execution:

1. Find the most recent active savepoint in the current level with the
   normalized name.
2. If no match exists, return error 1305 without changing transaction state.
3. Execute `RELEASE SAVEPOINT <sqlite_name>` using the internal name.
4. Drop the target entry and active stack entries after it.
5. Set affected rows to `0`.

SQLite's `RELEASE` behavior of releasing the named savepoint and those after it
matches the MySQL 8.4.9 runtime probe for release nesting when internal names
are unique and ordered.

### Statement Atomicity Interaction

Task 21 uses statement-owned SQLite savepoints inside an explicit transaction
so a failing DML statement can roll back its own partial effects without
ending the user transaction. Task 22 must preserve this layering:

- Savepoint-control statements must not run inside the statement atomicity
  helper. They are transaction-control statements, not DML statements.
- DML inside an explicit transaction should continue to create
  `mylite_statement_atomicity` after any user-visible savepoint marks.
- Successful DML releases only the statement-owned savepoint.
- Failed DML rolls back only to the statement-owned savepoint and then releases
  it, leaving user-visible savepoint metadata unchanged.
- User-visible names are never forwarded to SQLite, so a user can create
  `SAVEPOINT mylite_statement_atomicity` without affecting the internal helper.

If future triggers or stored functions can create user-visible savepoints while
executing inside a DML statement, the statement atomicity helper will need
reentrant internal names or a level-aware statement-savepoint stack. That is
deferred with stored program support.

### Pending AUTO_INCREMENT State

Current Task 21 transaction support records pending `AUTO_INCREMENT` advances
so full transaction rollback does not reuse consumed values. Savepoints add a
commit-after-partial-rollback case:

```sql
START TRANSACTION;
SAVEPOINT s;
INSERT INTO ai (v) VALUES (100), (200);
ROLLBACK TO SAVEPOINT s;
INSERT INTO ai (v) VALUES (300);
COMMIT;
```

The next generated id must account for the rolled-back rows. Because MyLite's
auto-increment metadata lives in SQLite tables, `ROLLBACK TO` may undo catalog
updates made after the savepoint. The runtime should keep pending
auto-increment maxima transaction-scoped, not savepoint-scoped. After a
successful `ROLLBACK TO`, it should reapply pending maxima inside the still
active transaction and keep the pending list for future rollback or commit
handling.

Expected lifecycle:

- generated or explicit high auto-increment values update the transaction's
  pending max exactly as in Task 21
- `ROLLBACK TO` reapplies pending maxima after SQLite has rolled back to the
  target savepoint
- `ROLLBACK TO` does not clear pending maxima
- full `ROLLBACK` reapplies pending maxima outside the rolled-back transaction,
  as Task 21 already does
- `COMMIT` clears pending maxima only after the catalog state containing those
  maxima is durable

### Read-Only Enforcement

Savepoint-control statements are allowed in read-only transactions. They should
not be classified as write statements by `write_statement_kind()`.

Read-only DML rejection remains unchanged:

```sql
START TRANSACTION READ ONLY;
SAVEPOINT s;          -- success
ROLLBACK TO s;        -- success
RELEASE SAVEPOINT s;  -- success
INSERT INTO t VALUES (1, 1); -- error 1792 / 25006
```

### Parser And AST Needs

Add AST node kinds for:

- `MYLITE_SQL_AST_SAVEPOINT_STATEMENT`
- `MYLITE_SQL_AST_ROLLBACK_TO_SAVEPOINT_STATEMENT`
- `MYLITE_SQL_AST_RELEASE_SAVEPOINT_STATEMENT`

Each statement node should preserve the savepoint identifier's source span and
identifier text. If the existing identifier AST node is sufficient, the
statement can own that child. If not, add a small savepoint-name child node
rather than storing statement-specific text in parser globals.

Prepared statement copying should store:

- original savepoint name
- normalized savepoint lookup key
- operation kind: create, rollback-to, release

The parser should reject schema-qualified names such as `SAVEPOINT db.sp`.
Quoted names containing dots are single identifiers and should be accepted by
the general identifier parser.

Independently authored MyLite Lemon-style grammar sketch:

```lemon
statement(A) ::= savepoint_statement(B). { A = B; }
statement(A) ::= rollback_to_savepoint_statement(B). { A = B; }
statement(A) ::= release_savepoint_statement(B). { A = B; }

savepoint_statement(A) ::= SAVEPOINT(T) identifier_name(N). {
    A = make_savepoint_statement(state, T, N);
}

rollback_to_savepoint_statement(A) ::= ROLLBACK(T) opt_work(W) TO opt_savepoint_keyword(S) identifier_name(N). {
    A = make_rollback_to_savepoint_statement(state, T, W, S, N);
}

release_savepoint_statement(A) ::= RELEASE(T) SAVEPOINT(S) identifier_name(N). {
    A = make_release_savepoint_statement(state, T, S, N);
}

opt_savepoint_keyword(A) ::= . {
    A = empty_token();
}
opt_savepoint_keyword(A) ::= SAVEPOINT(T). {
    A = T;
}
```

The `rollback_to_savepoint_statement` production must be distinct from the
Task 21 full `rollback_statement` production:

```lemon
rollback_statement(A) ::= ROLLBACK(T) opt_work(W) opt_transaction_completion(C). {
    A = make_rollback_statement(state, T, W, C);
}
```

`TO` is not a transaction-completion token, so `ROLLBACK [WORK] TO ...` should
route to the savepoint statement. Completion clauses must remain invalid after
`ROLLBACK TO`.

`RELEASE` is already a keyword for Task 21 completion options. Top-level
`RELEASE SAVEPOINT name` should be routed before any generic unsupported
statement fallback.

### Statement Execution

`SAVEPOINT name`:

1. Clear statement diagnostics.
2. If the session is released, return the Task 21 connection-released
   diagnostic.
3. If no transaction is active and autocommit is on, succeed as a no-op with
   affected rows `0`.
4. Normalize the name for case-insensitive lookup.
5. Remove an active same-name savepoint entry in the current level if present.
6. Create a generated SQLite savepoint.
7. Append the new MyLite savepoint entry.
8. Set affected rows to `0`.

`ROLLBACK TO name`:

1. Clear statement diagnostics.
2. If the session is released, return the Task 21 connection-released
   diagnostic.
3. If no matching active savepoint exists, set error 1305 and affected rows
   `-1`.
4. Roll back to the matched internal SQLite savepoint.
5. Drop active savepoint entries after the matched entry.
6. Reapply pending auto-increment maxima.
7. Keep the matched savepoint active.
8. Set affected rows to `0`.

`RELEASE SAVEPOINT name`:

1. Clear statement diagnostics.
2. If the session is released, return the Task 21 connection-released
   diagnostic.
3. If no matching active savepoint exists, set error 1305 and affected rows
   `-1`.
4. Release the matched internal SQLite savepoint.
5. Drop the matched entry and active entries after it.
6. Set affected rows to `0`.

Full `COMMIT`, full `ROLLBACK`, repeated `START TRANSACTION`, repeated
`BEGIN`, and implicit-commit helpers must call a shared `clear_savepoints()`
helper. `clear_savepoints()` should free strings and reset the stack without
issuing SQLite savepoint statements after SQLite has already ended the
transaction.

### Storage And Performance

Task 22 does not change the `.mylite` file format, schema catalog, table
catalog, column catalog, or index catalog. Savepoint state is session-owned
memory and SQLite transaction state.

Performance considerations:

- Savepoint lookup can be linear for the initial implementation. Common
  application usage has shallow savepoint stacks.
- Use amortized growth for the savepoint stack.
- Avoid copying SQL text beyond the identifier name and normalized key.
- Hidden obsolete SQLite savepoints from non-top replacement are acceptable,
  but replacement-heavy workloads should not cause quadratic MyLite-side work.
- Do not create SQLite transactions for autocommit-on no-op `SAVEPOINT`
  statements outside an active transaction.

## MySQL-Verified Test Expectations

### Parser Tests

Accepted forms:

| SQL | Expected parser result |
| --- | --- |
| `SAVEPOINT s` | savepoint AST with identifier `s` |
| `SAVEPOINT \`select\`` | savepoint AST with quoted identifier |
| `SAVEPOINT \`db.sp\`` | savepoint AST with one quoted identifier containing `.` |
| `ROLLBACK TO s` | rollback-to-savepoint AST |
| `ROLLBACK WORK TO s` | rollback-to-savepoint AST with optional `WORK` |
| `ROLLBACK TO SAVEPOINT s` | rollback-to-savepoint AST with optional `SAVEPOINT` |
| `ROLLBACK WORK TO SAVEPOINT s` | rollback-to-savepoint AST with both optional keywords |
| `RELEASE SAVEPOINT s` | release-savepoint AST |

Rejected forms:

| SQL | MySQL expectation |
| --- | --- |
| `SAVEPOINT` | syntax error 1064 |
| `SAVEPOINT select` | syntax error 1064 |
| `SAVEPOINT db.sp` | syntax error 1064 |
| `ROLLBACK TO` | syntax error 1064 |
| `ROLLBACK TO SAVEPOINT` | syntax error 1064 |
| `ROLLBACK TO s AND CHAIN` | syntax error 1064 |
| `ROLLBACK TO s RELEASE` | syntax error 1064 |
| `ROLLBACK SAVEPOINT s` | syntax error 1064 |
| `RELEASE s` | syntax error 1064 |
| `RELEASE SAVEPOINT` | syntax error 1064 |

### Runtime Tests

Runtime tests should compare against MySQL 8.4.9 for result rows, affected
rows, warning counts, errors, and side effects.

Required runtime cases:

| Scenario | Expected behavior |
| --- | --- |
| `SAVEPOINT` outside an active transaction with autocommit on | success, affected rows `0`, warning count `0`, no usable savepoint |
| `SAVEPOINT` outside active transaction, DML, `ROLLBACK TO` | missing savepoint error 1305; DML remains committed |
| basic `START TRANSACTION; SAVEPOINT; DML; ROLLBACK TO; COMMIT` | only pre-savepoint work commits |
| repeated `ROLLBACK TO` same target | target savepoint remains reusable |
| `RELEASE SAVEPOINT`, then `ROLLBACK TO` same name | release succeeds, later rollback-to returns 1305 |
| same-name replacement | rollback-to new name uses replacement position |
| replacement with intervening savepoint | intervening savepoint remains usable |
| `ROLLBACK TO` outer savepoint | later savepoints are deleted; target remains |
| `RELEASE` outer savepoint | target and later savepoints are deleted; data is not rolled back |
| missing `ROLLBACK TO` inside transaction | error 1305, transaction remains active, later commit succeeds |
| missing `RELEASE` inside transaction | error 1305, transaction remains active, later commit succeeds |
| case-insensitive unquoted lookup | `SAVEPOINT MixedCase; ROLLBACK TO mixedcase` succeeds |
| case-insensitive quoted lookup | `SAVEPOINT \`QuotedCase\`; ROLLBACK TO \`quotedcase\`` succeeds |
| quoted reserved-word savepoint name | accepted |
| unquoted reserved-word savepoint name | syntax error 1064 |
| quoted name containing dot | accepted and matched as one name |
| unquoted qualified-looking name | syntax error 1064 |
| quoted 512-character savepoint name | accepted |
| read-only transaction savepoint control | savepoint, rollback-to, and release succeed |
| read-only transaction DML after savepoint control | DML still fails with 1792 / `25006` |
| auto-increment insert after `ROLLBACK TO` | generated ids consumed after the savepoint are not reused |
| full `COMMIT` clears savepoints | later rollback-to returns 1305 |
| full `ROLLBACK` clears savepoints | later release returns 1305 |
| repeated `START TRANSACTION` after savepoint | implicit commit clears previous savepoints |
| non-temporary DDL after savepoint | implicit commit clears savepoints once DDL implicit-commit support exists |
| user savepoint named `mylite_statement_atomicity` | does not collide with internal statement atomicity |
| failed DML inside user savepoint | statement rollback preserves user savepoint stack |

### Future Autocommit-Off Tests

When `SET autocommit` lands, add MySQL-comparison tests:

| Scenario | Expected behavior |
| --- | --- |
| `SET autocommit = 0; SAVEPOINT; DML; ROLLBACK TO; COMMIT` | savepoint works in implicit transaction |
| `SET autocommit = 0; SAVEPOINT; SET autocommit = 1` | enabling autocommit commits and clears savepoints |
| `SET autocommit = 0; COMMIT; ROLLBACK TO old_name` | commit clears old savepoints and starts next transaction |
| `SET autocommit = 0; ROLLBACK; RELEASE old_name` | rollback clears old savepoints and starts next transaction |

### Future Stored-Program Tests

When stored functions and triggers land, add tests for savepoint levels:

| Scenario | Expected behavior |
| --- | --- |
| caller savepoint exists, stored function creates same name | no conflict between levels |
| stored function attempts `ROLLBACK TO` caller savepoint | caller savepoint is unavailable inside nested level |
| stored function returns after creating savepoint | nested savepoints are released |
| trigger creates savepoint with caller's name | no conflict with caller level |

## Deferred Behavior And Risks

- Exact non-ASCII savepoint-name folding needs MySQL-runtime verification when
  MyLite broadens identifier character support.
- Status variables and protocol diagnostics are deferred, but execution paths
  should be operation-specific to make later counters straightforward.
- DDL implicit-commit behavior is still incomplete in MyLite. Savepoint support
  must clear savepoint state through the shared implicit-commit helper when
  that retrofit lands.
- Hidden obsolete SQLite savepoints are a pragmatic way to preserve MySQL
  replacement semantics without disrupting later active savepoints. The
  implementation should watch replacement-heavy workloads for memory growth.
- Future stored-program savepoint levels require level-aware state. Task 22 can
  initialize only level `0`, but the data model should not make nested levels
  impossible.
