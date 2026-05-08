# Baseline SHOW EVENTS Empty Introspection Tasks

## Goal

Add the next narrow public introspection slice: `SHOW EVENTS` for schemas that
currently have no supported event descriptors, returning MySQL's result columns
with zero rows.

## Tasks

1. Design and documentation
   - Create `docs/specs/baseline-show-events-empty-introspection/specs.md`.
   - Record official MySQL 8.4 documentation sources and MySQL 8.4.9 runtime
     observations.
   - Specify parser grammar, selected/explicit schema behavior, result shape,
     diagnostics, physical SQLite policy, and unsupported behavior.
   - Update `COMPATIBILITY.md` and relevant detailed compatibility docs only
     for the exact partial subset.

2. MySQL expectations
   - Add a reproducible MySQL 8.4.9 expectation script for the supported
     user-visible SQL behavior and intentionally deferred wider forms.
   - Verify schema behavior, zero-row no-event results, result headers,
     `LIKE`, warnings, row-count behavior, and diagnostics.
   - Treat a missing MySQL 8.4.9 runtime as a blocker for changing
     compatibility expectations.

3. Parser and AST
   - Extend Lemon grammar for
     `SHOW EVENTS [{FROM|IN} schema] [LIKE 'pattern']`.
   - Add an AST node kind for `SHOW EVENTS` statements.
   - Map `EVENTS` in the parser adapter.
   - Keep parser code independent from runtime, catalog, storage, and SQLite.
   - Add parser tests for supported and rejected show-events syntax.

4. Analyzer/planner/runtime execution
   - Add a statement execution path through the existing statement context.
   - Resolve selected/default schema for unqualified `SHOW EVENTS`.
   - Accept known and unknown explicit schema names as empty successes, except
     reserved `_mylite_*` names.
   - Return a result set with the 15 MySQL 8.4.9 `SHOW EVENTS` column labels
     and zero rows.

5. Catalog and physical policy
   - Read only the selected schema descriptor when no explicit schema is
     provided.
   - Do not inspect SQLite schema/event metadata or `mysql.event`.
   - Do not mutate catalog rows, descriptor versions, descriptor caches,
     catalog generation, physical SQLite objects, or
     `sqlite_schema_generation`.
   - Do not add SQLite fork patches.

6. Tests
   - Add a fast C test under `packages/libmylite/tests/` and register it with a
     dotted CTest name.
   - Cover all admitted forms, exact result columns, zero rows, row-count
     behavior, unknown explicit schema success, selected-schema diagnostics,
     reserved names, reopen/drop behavior, preamble preservation, generation
     stability, independent handles, and unsupported syntax.
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
     independently authored grammar/spec text, MySQL 8.4.9 evidence, result
     shape accuracy, unknown-schema behavior, file-format safety, VFS
     preservation, zero-init safety, cleanup on failure, scope control,
     compatibility-matrix accuracy, and test relevance.

## Out Of Scope

- Event DDL, event descriptors, event scheduling, event execution, definers,
  privileges, stored-program SQL modes, time zones, replication originators,
  and `INFORMATION_SCHEMA.EVENTS`.
- Rows for any event kind.
- `SHOW CREATE EVENT`.
- `SHOW EVENTS ... WHERE`, `FULL`, `EXTENDED`, `ORDER BY`, `LIMIT`, singular
  `SHOW EVENT`, and combined `LIKE ... WHERE`.
- SQLite event metadata, `mysql.event`, or SQLite fork patches.
