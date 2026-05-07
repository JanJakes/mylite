# Baseline EXPLAIN Table Introspection Tasks

## Goal

Add descriptor-driven table-column introspection for the narrow
`EXPLAIN table_name` subset through `mylite_execute()`, backed by authoritative
MyLite catalog descriptors and the existing baseline `SHOW COLUMNS` result
path.

## Tasks

1. Design and documentation
   - Create `docs/specs/baseline-explain-table-introspection/specs.md`.
   - Record official MySQL 8.4 documentation sources and MySQL 8.4.9 runtime
     observations.
   - Specify parser grammar, schema/table resolution, descriptor result
     mapping, diagnostics, result behavior, row-count behavior, physical
     storage safety, and unsupported `EXPLAIN` forms.
   - Update compatibility docs only for the exact partial subset after
     implementation.

2. MySQL expectations
   - Add a reproducible MySQL 8.4.9 expectation script for supported
     table-form `EXPLAIN` behavior and intentionally deferred wider forms.
   - Verify result columns, integer-family display strings, nullability,
     default/key/extra values, warnings, following `ROW_COUNT()`, name
     resolution, and diagnostics.
   - Treat a missing MySQL 8.4.9 runtime as a blocker for changing
     compatibility expectations.

3. Parser and AST
   - Add parser keyword recognition for `EXPLAIN`.
   - Extend Lemon grammar for `EXPLAIN table_name`.
   - Reuse the existing table-column introspection AST/result path if that
     keeps the implementation smaller and preserves clear ownership.
   - Add parser tests for supported forms and unsupported wider forms.

4. Runtime execution
   - Resolve unqualified and schema-qualified targets against selected schema
     and MyLite catalog descriptors.
   - Reject reserved `_mylite_*` schema/table names before descriptor lookup.
   - Reject unknown schemas, unknown tables, and unsupported object kinds with
     deterministic diagnostics.
   - Iterate column descriptors in ordinal order.
   - Map current integer-family logical descriptor types to MySQL display
     strings.
   - Return the six-column result set with warning count `0`, affected rows
     `0`, and result-set row-count state.

5. Physical storage safety
   - Do not query SQLite metadata or generate user SQL.
   - Do not mutate descriptor rows, descriptor versions, descriptor caches,
     catalog generation, `sqlite_schema_generation`, physical user tables, or
     the MyLite preamble.
   - Avoid SQLite fork patches and custom SQLite functions.

6. Tests
   - Add or extend a fast C test under `packages/libmylite/tests/` and
     register any new binary with a dotted CTest name.
   - Cover supported `EXPLAIN` forms, schema resolution, descriptor result
     rows, warning/row-count behavior, diagnostics, reopen persistence, rename
     and drop behavior, independent handles, preamble safety, and unsupported
     syntax.
   - Keep tests deterministic and avoid adding a new test framework.

7. Build integration
   - Add any new test binary to `packages/libmylite/CMakeLists.txt`.
   - Keep first-party warning and clang-tidy policy enabled.
   - Keep vendored SQLite warning policy unchanged.

8. Verification and review
   - Run `cmake --build --preset dev`.
   - Run the new CTest entry and relevant parser/runtime lifecycle entries.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.
   - Review the final diff for parser independence, descriptor authority,
     result semantics, row-count semantics, file-format safety, cleanup on
     failure, compatibility docs, and scope control.

## Out Of Scope

Execution-plan `EXPLAIN`, `EXPLAIN ANALYZE`, `FORMAT`, `INTO`,
`FOR SCHEMA`, `FOR DATABASE`, `FOR CONNECTION`, `EXTENDED`, `PARTITIONS`,
table column filters, wildcard filters, optimizer metadata, temporary tables,
views, indexes, defaults, generated columns, privileges, arbitrary SQLite
metadata reads, arbitrary SQLite pass-through, and SQLite fork patches.
