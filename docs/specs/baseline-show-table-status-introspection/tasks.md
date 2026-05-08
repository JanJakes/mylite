# Baseline SHOW TABLE STATUS Introspection Tasks

## Goal

Add descriptor-driven `SHOW TABLE STATUS` for the narrow persistent base-table
subset through `mylite_execute()`, backed by authoritative MyLite catalog
descriptors and independently verified MySQL 8.4.9 expectations.

## Tasks

1. Design and documentation
   - Create `docs/specs/baseline-show-table-status-introspection/specs.md`.
   - Record official MySQL 8.4 documentation sources and MySQL 8.4.9 runtime
     observations.
   - Specify grammar, schema resolution, descriptor iteration, result row
     values, row-count reads, diagnostics, row-count state, file-format safety,
     and unsupported wider forms.
   - Update compatibility docs only for the exact partial subset.

2. MySQL expectations
   - Add a reproducible MySQL 8.4.9 expectation script for supported
     `SHOW TABLE STATUS` behavior and intentionally deferred wider forms.
   - Verify result columns, base-table fields, `FROM`/`IN` schema handling,
     `LIKE` filters, warnings, following `ROW_COUNT()`, missing default schema,
     unknown schemas, views, and syntax rejections.
   - Treat a missing MySQL 8.4.9 runtime as a blocker for changing
     compatibility expectations.

3. Parser and AST
   - Extend Lemon grammar for `SHOW TABLE STATUS`.
   - Add a dedicated AST node for `SHOW TABLE STATUS`.
   - Reuse the existing `LIKE STRING` AST shape for optional table-name
     filters.
   - Add parser tests for supported forms and unsupported modifiers.

4. Runtime execution
   - Resolve the selected or explicit schema through MyLite catalog
     descriptors.
   - Reject reserved `_mylite_*` schema names before descriptor lookup.
   - Iterate persistent base-table descriptors in catalog order and apply the
     existing `SHOW LIKE` table-name matcher.
   - Build the MySQL 8.4.9 18-column result shape.
   - Read exact physical row counts with generated, quoted SQLite `COUNT(*)`
     statements against descriptor-owned physical table names.
   - Return deterministic metadata/placeholders for the current no-index,
     no-auto-increment, no-option base-table subset.

5. Physical storage safety
   - Do not query SQLite metadata, `sqlite_schema`, or PRAGMA output.
   - Do not mutate descriptor rows, descriptor versions, descriptor caches,
     catalog generation, `sqlite_schema_generation`, physical user rows, or the
     MyLite preamble.
   - Avoid SQLite fork patches and custom SQLite functions.

6. Tests
   - Add a fast C test under `packages/libmylite/tests/` and register it with
     a dotted CTest name.
   - Cover result shape, row values, row counts after DML, filtering,
     diagnostics, reopen persistence, rename/drop behavior, independent
     handles, generation stability, preamble safety, and unsupported syntax.
   - Keep tests deterministic and avoid adding a new test framework.

7. Build integration
   - Add any new runtime/test sources to `packages/libmylite/CMakeLists.txt`.
   - Keep first-party warning and clang-tidy policy enabled.
   - Keep vendored SQLite warning policy unchanged.

8. Verification and review
   - Run `cmake --build --preset dev`.
   - Run the new CTest entry and relevant parser/runtime lifecycle entries.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.
   - Review the final diff for parser independence, descriptor authority,
     result fidelity, row-count reads, status semantics, file-format safety,
     cleanup on failure, compatibility docs, and scope control.

## Out Of Scope

Views, temporary tables, `WHERE` filters, full storage statistics, timestamps,
auto-increment metadata, table comments, table options, indexes, constraints,
partitions, privileges, information schema, arbitrary SQLite metadata reads,
arbitrary SQLite SQL pass-through, and SQLite fork patches.
