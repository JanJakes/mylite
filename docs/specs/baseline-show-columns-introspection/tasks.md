# Baseline SHOW COLUMNS Introspection Tasks

## Goal

Add descriptor-driven table-column introspection for the narrow
`SHOW COLUMNS`/`SHOW FIELDS` and table-only `DESCRIBE`/`DESC` subset through
`mylite_execute()`, backed by authoritative MyLite catalog descriptors.

## Tasks

1. Design and documentation
   - Create `docs/specs/baseline-show-columns-introspection/specs.md`.
   - Record official MySQL 8.4 documentation sources and MySQL 8.4.9 runtime
     observations.
   - Specify parser grammar, schema/table resolution, descriptor result
     mapping, diagnostics, result behavior, row-count behavior, and unsupported
     introspection forms.
   - Update compatibility docs only for the exact partial subset after
     implementation.

2. MySQL expectations
   - Add a reproducible MySQL 8.4.9 expectation script for supported
     introspection behavior and intentionally deferred wider forms.
   - Verify result columns, integer-family display strings, nullability,
     default/key/extra values, warnings, following `ROW_COUNT()`, name
     resolution, and diagnostics.
   - Treat a missing MySQL 8.4.9 runtime as a blocker for changing
     compatibility expectations.

3. Parser and AST
   - Add AST support for `SHOW COLUMNS` introspection.
   - Add `COLUMNS`, `FIELDS`, and `DESCRIBE` parser keyword recognition without
     making `COLUMNS` or `FIELDS` reserved identifiers.
   - Extend Lemon grammar for `SHOW {COLUMNS|FIELDS} {FROM|IN} table_name`,
     explicit-schema `SHOW` variants, and table-only `DESCRIBE`/`DESC`.
   - Preserve `DESC` order-direction parsing for existing `ORDER BY` clauses.
   - Add parser tests for supported forms and unsupported wider forms.

4. Runtime execution
   - Resolve unqualified, schema-qualified, and explicit-schema targets against
     selected schema and MyLite catalog descriptors.
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
   - Add a fast C test under `packages/libmylite/tests/` and register it with a
     dotted CTest name.
   - Cover supported introspection forms, schema resolution, descriptor result
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

Full introspection, `SHOW FULL COLUMNS`, `SHOW EXTENDED COLUMNS`, `LIKE`,
`WHERE`, column-filtered `DESCRIBE`, execution-plan `EXPLAIN`, `DESCRIBE
SELECT`, temporary tables, views, indexes, defaults, generated columns,
privileges, `INFORMATION_SCHEMA`, arbitrary SQLite metadata reads, arbitrary
SQLite pass-through, and SQLite fork patches.
