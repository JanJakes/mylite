# Baseline SHOW Processlist Introspection Tasks

## Goal

Add the next narrow public introspection slice: `SHOW PROCESSLIST` and
`SHOW FULL PROCESSLIST` as current embedded-handle process-list rows with
MySQL's result columns.

## Tasks

1. Design and documentation
   - Create `docs/specs/baseline-show-processlist-introspection/specs.md`.
   - Record official MySQL 8.4 documentation sources and MySQL 8.4.9 runtime
     observations.
   - Specify parser grammar, session/default-schema behavior, result shape,
     `Info` truncation, warning behavior, diagnostics, physical SQLite policy,
     and unsupported behavior.
   - Update `COMPATIBILITY.md` and relevant detailed compatibility docs only
     for the exact partial subset.

2. MySQL expectations
   - Add a reproducible MySQL 8.4.9 expectation script for the supported
     user-visible SQL behavior and intentionally deferred wider forms.
   - Verify result headers, no-default-schema behavior, selected-schema
     output, `FULL` and non-`FULL` `Info`, warnings, row-count behavior, and
     diagnostics.
   - Treat a missing MySQL 8.4.9 runtime, or a different
     `@@performance_schema_show_processlist` mode, as a blocker for changing
     compatibility expectations.

3. Parser and AST
   - Extend Lemon grammar for `SHOW PROCESSLIST` and
     `SHOW FULL PROCESSLIST`.
   - Add AST node kinds for process-list statements.
   - Map `PROCESSLIST` in the parser adapter while preserving existing lexer
     behavior.
   - Keep parser code independent from runtime, catalog, storage, and SQLite.
   - Add parser tests for supported and rejected process-list syntax.

4. Analyzer/planner/runtime execution
   - Add statement execution paths through the existing statement context.
   - Do not resolve a target schema or table.
   - Return a result set with MySQL 8.4.9 process-list column labels and one
     current-handle row.
   - Format `Id`, `User`, `Host`, `db`, `Command`, `Time`, `State`, and
     `Info` from MyLite-owned session and statement-context state.
   - Append the observed MySQL 8.4.9 default process-list deprecation warning
     so the public warning count is one.

5. Catalog and physical policy
   - Do not iterate schema, table, trigger, event, routine, thread, or process
     descriptors.
   - Do not inspect SQLite schema, process, information-schema, or
     performance-schema metadata.
   - Do not mutate catalog rows, descriptor versions, descriptor caches,
     catalog generation, physical SQLite objects, or
     `sqlite_schema_generation`.
   - Do not add SQLite fork patches.

6. Tests
   - Add a fast C test under `packages/libmylite/tests/` and register it with
     a dotted CTest name.
   - Cover all admitted forms, exact result columns, one current-handle row,
     warning count, row-count behavior, no-selected-default behavior,
     selected-schema output, dropped-selected-schema output, `Info`
     truncation, reopen/preamble behavior, generation stability, independent
     handles, and unsupported syntax.
   - Keep tests deterministic and avoid adding a new test framework.

7. Build integration
   - Add new sources or tests to `packages/libmylite/CMakeLists.txt`.
   - Keep first-party warning and clang-tidy policy enabled.
   - Keep vendored SQLite warning policy unchanged.

8. Verification and review
   - Run `cmake --build --preset dev`.
   - Run the new CTest entry and existing parser/show/schema lifecycle
     entries.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.
   - Review the final diff for architecture boundaries, public ABI stability,
     independently authored grammar/spec text, MySQL 8.4.9 evidence, session
     ownership, result shape accuracy, no-target-schema behavior, warning
     behavior, file-format safety, VFS preservation, zero-init safety, cleanup
     on failure, scope control, compatibility-matrix accuracy, and test
     relevance.

## Out Of Scope

- Server-wide thread enumeration, background rows, idle sessions, process
  state telemetry, process-list status counters, or thread management.
- Accounts, authentication, privileges, TCP host/port reporting, and
  server-grade user visibility.
- Performance Schema, `INFORMATION_SCHEMA.PROCESSLIST`, sys-schema views,
  `mysqladmin processlist`, and `KILL`.
- `LIKE`, `WHERE`, `ORDER BY`, `LIMIT`, `FROM`, `IN`, `EXTENDED`, filters,
  projections, and expression forms.
- SQLite process metadata or SQLite fork patches.
