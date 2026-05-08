# Baseline Current Role Function Tasks

Add the next narrow scalar system-function slice: `CURRENT_ROLE()` in one-row
scalar `SELECT` statements.

## Checklist

1. MySQL research and design
   - Verify MySQL 8.4.9 behavior for `CURRENT_ROLE()`, `FROM DUAL`, result
     column labels, warning counts, selected-database interaction, unsupported
     arguments, bare `CURRENT_ROLE`, and wider scalar forms.
   - Specify MyLite's intentional no-role return value as `NONE`.
   - Record the supported MyLite subset in `specs.md`.

2. Parser and AST
   - Add lexer keyword recognition for `CURRENT_ROLE`.
   - Add an AST node kind for `CURRENT_ROLE()`.
   - Add parser support for zero-argument `CURRENT_ROLE()`.
   - Add a parsed nonzero-argument diagnostic form for `CURRENT_ROLE(...)` so
     runtime can return MySQL error `1582` for currently parsed argument lists.
   - Preserve source spans for default result labels.
   - Preserve nonreserved `CURRENT_ROLE` identifier use where identifier
     grammar already admits it.
   - Add parser tests for supported syntax and unsupported argument/bare-name
     cases.

3. Runtime execution
   - Extend the scalar session/system select execution path so it can evaluate
     `CURRENT_ROLE()` with existing supported scalar functions and variables
     in the same select list.
   - Return `NONE`.
   - Keep catalog, role metadata, session mutation, SQLite schema, VFS,
     storage, and public ABI untouched.

4. Runtime tests
   - Add or extend a focused runtime C test.
   - Cover result values, labels, warning count, affected rows, `ROW_COUNT()`,
     `FROM DUAL`, parenthesized expressions, mixed scalar functions/variables,
     schema lifecycle interactions, reopen behavior, independent handles,
     unsupported forms, deterministic diagnostics, file preamble preservation,
     and unchanged catalog/schema generations.
   - Keep tests deterministic and avoid a new test framework.

5. MySQL expectation artifact
   - Add a shell script that checks MySQL 8.4.9 result shapes, labels, warning
     count, argument-count errors, bare-name error, and wider forms relevant
     to this slice.

6. Compatibility docs
   - Update `COMPATIBILITY.md` for limited `CURRENT_ROLE()` support.
   - Update `docs/compatibility/functions-system.md`.
   - Update `docs/compatibility/sql-users-privileges.md`.
   - Do not claim roles, role grants, `SET ROLE`, default roles, privilege
     metadata, aliases, table-backed evaluation, or general expression support.

7. Build integration and verification
   - Register any new test binary in `packages/libmylite/CMakeLists.txt`.
   - Run the new CTest entry and relevant parser/runtime scalar tests.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.

## Non-Goals

- Do not implement role catalogs, role grants, role activation, `SET ROLE`,
  `SET DEFAULT ROLE`, accounts, authentication, privilege checks, bare
  `CURRENT_ROLE`, aliases, table-backed scalar evaluation, `LIMIT`, general
  scalar expressions, function registration, arbitrary SQLite pass-through, or
  SQLite fork patches.
