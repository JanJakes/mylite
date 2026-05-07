# Baseline Row Count Function Tasks

Add the next narrow scalar system-function slice: `ROW_COUNT()` in one-row
scalar `SELECT` statements.

## Checklist

1. MySQL research and design
   - Verify MySQL 8.4.9 behavior for initial state, result-set `SELECT`,
     `CREATE/DROP DATABASE`, `USE`, table DDL, `INSERT`, `DELETE`, changed and
     no-op `UPDATE`, repeated `ROW_COUNT()` selects, warning count, unsupported
     arguments, bare `ROW_COUNT`, and wider scalar forms.
   - Specify MyLite's connection-local row-count state and statement-boundary
     update rules.
   - Record the supported MyLite subset in `specs.md`.

2. Parser and AST
   - Add an AST node kind for `ROW_COUNT()`.
   - Add parser support for zero-argument `ROW_COUNT()`.
   - Keep `ROW_COUNT(...)` with arguments as a syntax error.
   - Preserve source spans for default result labels.
   - Preserve nonreserved `ROW_COUNT` identifier use where identifier grammar
     already admits it.
   - Add parser tests for supported syntax and unsupported argument/bare-name
     cases.

3. Runtime execution
   - Add connection-local previous-row-count state initialized to `-1`.
   - Seed statement context from the connection state at statement begin.
   - Extend the scalar session/system select execution path so it can evaluate
     `ROW_COUNT()` with existing supported scalar functions in the same select
     list.
   - Format the saved row-count value as signed decimal text.
   - Update row-count state after successful supported statements and reset it
     to `-1` after failed SQL execution with a database handle.
   - Keep catalog descriptors, SQLite schema, VFS, storage, and public ABI
     untouched.

4. Runtime tests
   - Add a focused `runtime_row_count_function` C test.
   - Cover result values, labels, warning count, affected rows, `FROM DUAL`,
     parenthesized expressions, mixed scalar functions, DDL/DML/select
     transitions, changed-row update semantics, repeated row-count selects,
     close/reopen, independent handles, unsupported forms, and deterministic
     diagnostics.
   - Keep tests deterministic and avoid a new test framework.

5. MySQL expectation artifact
   - Add a shell script that checks MySQL 8.4.9 result values, labels, warning
     counts, statement transitions, changed-row semantics, argument syntax
     errors, bare-name error, and wider forms relevant to this slice.

6. Compatibility docs
   - Update `COMPATIBILITY.md` for limited `ROW_COUNT()` support.
   - Update `docs/compatibility/functions-system.md`.
   - Do not claim `CLIENT_FOUND_ROWS`, protocol OK-packet parity,
     table-backed expression evaluation, aliases, or general diagnostics area
     support.

7. Build integration and verification
   - Register any new test binary in `packages/libmylite/CMakeLists.txt`.
   - Run the new CTest entry and relevant parser/runtime lifecycle tests.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.

## Non-Goals

- Do not add public API, protocol OK-packet state, `CLIENT_FOUND_ROWS`,
  general diagnostics area behavior, aliases, table-backed scalar evaluation,
  clauses, general scalar expressions, prepared-statement behavior, function
  registration, arbitrary SQLite pass-through, or SQLite fork patches.
