# Baseline Version Function Tasks

Add the next narrow scalar system-function slice: `VERSION()` in one-row
scalar `SELECT` statements.

## Checklist

1. MySQL research and design
   - Verify MySQL 8.4.9 behavior for `VERSION()`, `FROM DUAL`, result column
     labels, warning counts, selected-database interaction, unsupported
     arguments, bare `VERSION`, and wider scalar forms.
   - Specify MyLite's intentional return value as `mylite_version()` rather
     than a MySQL server-version impersonation.
   - Record the supported MyLite subset in `specs.md`.

2. Parser and AST
   - Add an AST node kind for `VERSION()`.
   - Add parser support for zero-argument `VERSION()`.
   - Add a parsed nonzero-argument diagnostic form for `VERSION(...)` so the
     runtime can return MySQL error `1582` for currently parsed argument lists.
   - Preserve source spans for default result labels.
   - Preserve nonreserved `VERSION` identifier use where identifier grammar
     already admits it.
   - Add parser tests for supported syntax and unsupported argument/bare-name
     cases.

3. Runtime execution
   - Extend the scalar session/system select execution path so it can evaluate
     `VERSION()` with existing supported scalar functions in the same select
     list.
   - Return `mylite_version()`.
   - Keep catalog, session mutation, SQLite schema, VFS, storage, and public
     ABI untouched.

4. Runtime tests
   - Add a focused `runtime_version_function` C test.
   - Cover result values, labels, warning count, affected rows, `FROM DUAL`,
     parenthesized expressions, mixed scalar functions, schema lifecycle
     interactions, reopen behavior, independent handles, unsupported forms,
     and deterministic diagnostics.
   - Cover `VERSION(1)`, `VERSION(NULL)`, and `VERSION(1, 2)` returning
     MySQL-compatible native-function parameter-count diagnostics.
   - Keep tests deterministic and avoid a new test framework.

5. MySQL expectation artifact
   - Add a shell script that checks MySQL 8.4.9 result shapes, labels, warning
     count, argument-count errors, bare-name error, and wider forms relevant to
     this slice.
   - Use runtime-discovered MySQL `VERSION()` output because distribution
     suffixes are environment-specific.

6. Compatibility docs
   - Update `COMPATIBILITY.md` for limited `VERSION()` support.
   - Update `docs/compatibility/functions-system.md`.
   - Do not claim MySQL server-version impersonation, protocol handshake
     support, `@@version`, or system-variable support.

7. Build integration and verification
   - Register any new test binary in `packages/libmylite/CMakeLists.txt`.
   - Run the new CTest entry and relevant parser/runtime scalar tests.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.

## Non-Goals

- Do not implement MySQL server-version impersonation, protocol handshake
  version reporting, `@@version`, `SHOW VARIABLES`, aliases, table-backed
  scalar evaluation, `LIMIT`, general scalar expressions, function
  registration, arbitrary SQLite pass-through, or SQLite fork patches.
