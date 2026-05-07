# Baseline Session/System User Identity Tasks

Add the next narrow runtime identity-alias slice: `SESSION_USER()` and
`SYSTEM_USER()` in one-row scalar `SELECT` statements.

## Checklist

1. MySQL research and design
   - Verify MySQL 8.4.9 behavior for `SESSION_USER()`, `SYSTEM_USER()`,
     `FROM DUAL`, result column labels, warning counts, selected-database
     interaction, unsupported arguments, bare names, whitespace-sensitive
     function parsing, and wider scalar forms.
   - Specify why `SESSION_USER ()`, `SYSTEM_USER ()`, and comment-separated
     forms remain outside this MyLite slice.
   - Record the supported MyLite subset in `specs.md`.

2. Parser and AST
   - Add AST node kinds for the supported identity-alias expressions.
   - Add parser support for no-whitespace `SESSION_USER()` and
     `SYSTEM_USER()`.
   - Preserve source spans for default result labels.
   - Preserve nonreserved `SESSION_USER` and `SYSTEM_USER` identifier use where
     identifier grammar admits function-name identifiers.
   - Add parser tests for supported syntax and unsupported argument, bare-name,
     and whitespace-sensitive cases.

3. Runtime execution
   - Extend the scalar session-select execution path so it can evaluate
     `SESSION_USER()` and `SYSTEM_USER()` with existing supported scalar
     functions in the same select list.
   - Return `session.client_user_identity` for both aliases.
   - Preserve the existing `ROW_COUNT()` result-set transition behavior.
   - Keep catalog, SQLite schema, VFS, storage, and public ABI untouched.

4. Runtime tests
   - Extend the focused `runtime_current_user_identity` C test.
   - Cover result values, labels, warning count, affected rows, `FROM DUAL`,
     parenthesized expressions, mixed scalar functions, schema lifecycle
     interactions, reopen behavior, independent handles, unsupported forms,
     whitespace/comment-sensitive rejection, and deterministic diagnostics.
   - Keep tests deterministic and avoid a new test framework.

5. MySQL expectation artifact
   - Add a shell script that checks MySQL 8.4.9 result shapes, labels, warning
     count, syntax errors, whitespace-sensitive stored-function resolution,
     and wider forms relevant to this slice.
   - Use runtime-discovered MySQL identity values because the test container
     identity is environment-specific.

6. Compatibility docs
   - Update `COMPATIBILITY.md` for the limited identity-alias support.
   - Update `docs/compatibility/functions-system.md`.
   - Update `docs/compatibility/sql-users-privileges.md` only for the
     MyLite-specific embedded identity exposure; do not claim account or
     privilege support.

7. Build integration and verification
   - Register any new test binary in `packages/libmylite/CMakeLists.txt`.
   - Run the relevant parser/runtime scalar tests.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.

## Non-Goals

- Do not implement authentication, accounts, grants, roles, privileges, host
  matching, `CURRENT_ROLE()`, `IGNORE_SPACE`, stored functions, loadable
  functions, routine namespaces, user identity in definers/defaults/stored
  programs, aliases, table-backed scalar evaluation, `LIMIT`, general scalar
  expressions, function registration, arbitrary SQLite pass-through, or SQLite
  fork patches.
