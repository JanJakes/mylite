# Baseline Current User Identity Tasks

Add the next narrow runtime identity slice: `USER()`, `CURRENT_USER()`, and
bare `CURRENT_USER` in one-row scalar `SELECT` statements.

## Checklist

1. MySQL research and design
   - Verify MySQL 8.4.9 behavior for `USER()`, `CURRENT_USER()`, bare
     `CURRENT_USER`, `FROM DUAL`, result column labels, warning counts,
     selected-database interaction, unsupported arguments, bare `USER`, and
     wider scalar forms.
   - Keep `SESSION_USER()` and `SYSTEM_USER()` out of this slice.
   - Record the supported MyLite subset in `specs.md`.

2. Parser and AST
   - Add AST node kinds for the supported current-user identity expressions.
   - Add parser support for `USER()`, `CURRENT_USER()`, and bare
     `CURRENT_USER`.
   - Preserve source spans for default result labels.
   - Preserve nonreserved `USER` identifier use where identifier grammar
     already admits it.
   - Add parser tests for supported syntax and unsupported argument/bare-user
     cases.

3. Runtime execution
   - Generalize the scalar session-select execution path so it can evaluate
     the existing `DATABASE()` / `SCHEMA()` functions and the new identity
     functions in the same select list.
   - Return `session.client_user_identity` for `USER()`.
   - Return `session.current_user_identity` for `CURRENT_USER()` and bare
     `CURRENT_USER`.
   - Keep catalog, SQLite schema, VFS, storage, and public ABI untouched.

4. Runtime tests
   - Add a focused `runtime_current_user_identity` C test.
   - Cover result values, labels, warning count, affected rows, `FROM DUAL`,
     parenthesized expressions, mixed scalar functions, schema lifecycle
     interactions, reopen behavior, independent handles, unsupported forms,
     and deterministic diagnostics.
   - Keep tests deterministic and avoid a new test framework.

5. MySQL expectation artifact
   - Add a shell script that checks MySQL 8.4.9 result shapes, labels, warning
     count, syntax errors, and wider forms relevant to this slice.
   - Use runtime-discovered MySQL identity values because the test container
     identity is environment-specific.

6. Compatibility docs
   - Update `COMPATIBILITY.md` for the limited current-user identity support.
   - Update `docs/compatibility/functions-system.md`.
   - Update `docs/compatibility/sql-users-privileges.md` only for the
     MyLite-specific embedded identity exposure; do not claim account or
     privilege support.

7. Build integration and verification
   - Register any new test binary in `packages/libmylite/CMakeLists.txt`.
   - Run the new CTest entry and relevant parser/runtime scalar tests.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.

## Non-Goals

- Do not implement authentication, accounts, grants, roles, privileges, host
  matching, `SESSION_USER()`, `SYSTEM_USER()`, `CURRENT_ROLE()`, user identity
  in definers/defaults/stored programs, aliases, table-backed scalar
  evaluation, `LIMIT`, general scalar expressions, function registration,
  arbitrary SQLite pass-through, or SQLite fork patches.
