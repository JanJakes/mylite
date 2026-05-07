# Baseline Connection ID Function Tasks

Add the next narrow scalar system-function slice: `CONNECTION_ID()` in one-row
scalar `SELECT` statements.

## Checklist

1. MySQL research and design
   - Verify MySQL 8.4.9 behavior for stable per-connection values,
     independent concurrent connections, `FROM DUAL`, result column labels,
     warning counts, selected-database interaction, statement failures,
     unsupported arguments, bare names, and wider scalar forms.
   - Specify MyLite's embedded handle-id policy and its limits relative to
     MySQL server thread/process-list ids.
   - Record the supported MyLite subset in `specs.md`.

2. Parser and AST
   - Add AST node kinds for `CONNECTION_ID()` and native argument-count error
     forms.
   - Add parser support for zero-argument `CONNECTION_ID()`.
   - Preserve source spans for default result labels.
   - Preserve nonreserved `CONNECTION_ID` identifier use where identifier
     grammar admits function-name identifiers.
   - Add parser tests for supported syntax and unsupported argument/bare-name
     cases.

3. Runtime execution
   - Add connection-local nonzero unsigned handle id state initialized at
     handle creation.
   - Extend the scalar session-select execution path so it can evaluate
     `CONNECTION_ID()` with existing supported scalar functions in the same
     select list.
   - Format the handle id as unsigned decimal text.
   - Preserve the existing `ROW_COUNT()` result-set transition behavior.
   - Keep catalog descriptors, SQLite schema, VFS, storage, and public ABI
     untouched.

4. Runtime tests
   - Add a focused `runtime_connection_id_function` C test.
   - Cover values, labels, warning count, affected rows, `FROM DUAL`,
     parenthesized expressions, mixed scalar functions, stability across
     schema/table/DML/failure statements, reopen behavior, independent handles,
     unsupported forms, native argument-count diagnostics, and deterministic
     wider-shape diagnostics.
   - Keep tests deterministic and avoid a new test framework.

5. MySQL expectation artifact
   - Add a shell script that checks MySQL 8.4.9 result values, labels, warning
     counts, same-connection stability, independent concurrent connection
     uniqueness, argument-count errors, bare-name error, and wider forms
     relevant to this slice.
   - Use runtime-discovered MySQL connection ids because values are
     environment-specific.

6. Compatibility docs
   - Update `COMPATIBILITY.md` for limited `CONNECTION_ID()` support.
   - Update `docs/compatibility/functions-system.md`.
   - Update process-list / Performance Schema detail docs only if needed to
     avoid overclaiming server thread or process-list metadata.

7. Build integration and verification
   - Register any new test binary in `packages/libmylite/CMakeLists.txt`.
   - Run the new CTest entry and relevant parser/runtime scalar tests.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.

## Non-Goals

- Do not add public API, protocol connection ids, server threads,
  `pseudo_thread_id`, process-list metadata, Performance Schema thread
  metadata, aliases, table-backed scalar evaluation, clauses, general scalar
  expressions, function registration, arbitrary SQLite pass-through, or SQLite
  fork patches.
