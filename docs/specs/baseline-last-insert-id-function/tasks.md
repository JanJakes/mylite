# Baseline Last Insert ID Function Tasks

Add the next narrow scalar system-function slice: zero-argument
`LAST_INSERT_ID()` in one-row scalar `SELECT` statements.

## Checklist

1. MySQL research and design
   - Verify MySQL 8.4.9 behavior for fresh `LAST_INSERT_ID()`, `FROM DUAL`,
     result column labels, warning counts, row-count interaction,
     non-auto-increment inserts, one-argument forms, multiple-argument forms,
     bare `LAST_INSERT_ID`, and wider scalar forms.
   - Specify MyLite's current no-auto-increment return value as `0`.
   - Record the supported MyLite subset in `specs.md`.

2. Parser and AST
   - Add lexer keyword recognition for `LAST_INSERT_ID`.
   - Add an AST node kind for `LAST_INSERT_ID()`.
   - Add parser support for zero-argument `LAST_INSERT_ID()`.
   - Preserve source spans for default result labels.
   - Preserve nonreserved `LAST_INSERT_ID` identifier use where identifier
     grammar already admits it.
   - Add parser tests for supported syntax and unsupported argument/bare-name
     cases.

3. Runtime execution
   - Extend the scalar session/system select execution path so it can evaluate
     `LAST_INSERT_ID()` with existing supported scalar functions and variables
     in the same select list.
   - Add or reuse connection/session state initialized to `0`.
   - Keep currently supported non-auto-increment statements from mutating the
     value.
   - Keep catalog, auto-increment metadata, SQLite schema, VFS, storage, and
     public ABI untouched.

4. Runtime tests
   - Add or extend a focused runtime C test.
   - Cover result values, labels, warning count, affected rows, `ROW_COUNT()`,
     `FROM DUAL`, parenthesized expressions, mixed scalar functions/variables,
     schema lifecycle interactions, supported DML/DDL non-mutation,
     reopen behavior, independent handles, unsupported forms, deterministic
     diagnostics, file preamble preservation, and unchanged catalog/schema
     generations.
   - Keep tests deterministic and avoid a new test framework.

5. MySQL expectation artifact
   - Add a shell script that checks MySQL 8.4.9 result shapes, labels, warning
     count, row-count interaction, non-auto-increment insert behavior,
     argument-form behavior, bare-name error, and wider forms relevant to this
     slice.

6. Compatibility docs
   - Update `COMPATIBILITY.md` for limited zero-argument
     `LAST_INSERT_ID()` support.
   - Update `docs/compatibility/functions-system.md`.
   - Do not claim auto-increment, `LAST_INSERT_ID(expr)`, sequence emulation,
     protocol insert-id metadata, C API behavior, aliases, table-backed
     evaluation, or general expression support.

7. Build integration and verification
   - Register any new test binary in `packages/libmylite/CMakeLists.txt`.
   - Run the new CTest entry and relevant parser/runtime scalar tests.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.

## Non-Goals

- Do not implement `AUTO_INCREMENT`, generated ids, `LAST_INSERT_ID(expr)`,
  sequence emulation, protocol OK-packet insert-id metadata, C API
  `mysql_insert_id()` behavior, aliases, table-backed scalar evaluation,
  `LIMIT`, general scalar expressions, function registration, arbitrary SQLite
  pass-through, or SQLite fork patches.
