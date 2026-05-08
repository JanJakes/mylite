# Baseline SHOW CREATE TABLE Tasks

## Goal

Add descriptor-driven `SHOW CREATE TABLE table_name` for the narrow persistent
base-table subset through `mylite_execute()`, backed by authoritative MyLite
catalog descriptors and independent MySQL 8.4.9 expectations.

## Tasks

1. Design and documentation
   - Create `docs/specs/baseline-show-create-table/specs.md`.
   - Record official MySQL 8.4 documentation sources and MySQL 8.4.9 runtime
     observations.
   - Specify parser grammar, schema/table resolution, descriptor DDL
     rendering, identifier quoting, diagnostics, result behavior, row-count
     behavior, physical storage safety, and unsupported wider forms.
   - Update compatibility docs only for the exact partial subset after
     implementation.

2. MySQL expectations
   - Add a reproducible MySQL 8.4.9 expectation script for supported
     `SHOW CREATE TABLE` behavior and intentionally deferred wider forms.
   - Verify result columns, rendered DDL text, integer-family display strings,
     nullability/default rendering, identifier quoting, warnings, following
     `ROW_COUNT()`, name resolution, diagnostics, and syntax rejections.
   - Treat a missing MySQL 8.4.9 runtime as a blocker for changing
     compatibility expectations.

3. Parser and AST
   - Extend Lemon grammar for `SHOW CREATE TABLE table_name`.
   - Add a dedicated AST node for `SHOW CREATE TABLE`.
   - Add parser tests for supported forms and unsupported modifiers.

4. Runtime execution
   - Resolve unqualified and schema-qualified targets against selected schema
     and MyLite catalog descriptors.
   - Reject reserved `_mylite_*` schema/table names before descriptor lookup.
   - Reject unknown schemas, unknown tables, and unsupported object kinds with
     deterministic diagnostics.
   - Iterate column descriptors in ordinal order.
   - Render current integer-family logical descriptor types and nullability
     using MySQL-observed default output.
   - Quote logical identifiers with MySQL backticks and doubled embedded
     backticks.
   - Return the two-column result set with warning count `0`, affected rows
     `0`, and result-set row-count state.

5. Physical storage safety
   - Do not query SQLite metadata or generate user SQL.
   - Do not mutate descriptor rows, descriptor versions, descriptor caches,
     catalog generation, `sqlite_schema_generation`, physical user tables, or
     the MyLite preamble.
   - Avoid SQLite fork patches and custom SQLite functions.

6. Tests
   - Add a fast C test under `packages/libmylite/tests/` and register it with
     a dotted CTest name.
   - Cover supported rendering, schema resolution, quoted identifiers,
     warning/row-count behavior, diagnostics, reopen persistence, rename and
     drop behavior, independent handles, preamble safety, and unsupported
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
     rendering fidelity, result semantics, row-count semantics, file-format
     safety, cleanup on failure, compatibility docs, and scope control.

## Out Of Scope

Views, temporary tables, `SHOW CREATE VIEW`, `SHOW CREATE DATABASE`, full table
options, row formats, charsets, collations, comments, indexes, constraints,
defaults, generated columns, auto-increment, privileges, SQL modes,
`sql_quote_show_create`, arbitrary SQLite metadata reads, arbitrary SQLite
pass-through, and SQLite fork patches.
