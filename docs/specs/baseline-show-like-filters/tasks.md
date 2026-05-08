# Baseline SHOW LIKE Filters Tasks

## Goal

Add descriptor-driven `LIKE 'pattern'` filtering for the narrow supported
`SHOW DATABASES`, `SHOW TABLES`, and `SHOW COLUMNS` subsets through
`mylite_execute()`, backed by authoritative MyLite catalog descriptors.

## Tasks

1. Design and documentation
   - Create `docs/specs/baseline-show-like-filters/specs.md`.
   - Record official MySQL 8.4 documentation sources and MySQL 8.4.9 runtime
     observations.
   - Specify parser grammar, pattern decoding, wildcard matching, case
     sensitivity, schema/table resolution, descriptor filtering, diagnostics,
     result metadata, row-count behavior, and physical storage safety.
   - Update compatibility docs only for the exact partial subset after
     implementation.

2. MySQL expectations
   - Add a reproducible MySQL 8.4.9 expectation script for supported
     `SHOW ... LIKE` behavior and intentionally deferred wider forms.
   - Verify wildcard behavior, escaped wildcard behavior, result-column names,
     result rows, warnings, following `ROW_COUNT()`, name resolution, and
     diagnostics.
   - Treat a missing MySQL 8.4.9 runtime as a blocker for changing
     compatibility expectations.

3. Parser and AST
   - Add parser keyword recognition for `LIKE`.
   - Extend Lemon grammar for optional `LIKE STRING` on supported
     `SHOW DATABASES`/`SHOW SCHEMAS`, `SHOW TABLES`, and
     `SHOW COLUMNS`/`SHOW FIELDS` forms.
   - Store the pattern literal on existing `SHOW` statement nodes without
     changing public ABI.
   - Add parser tests for supported forms and unsupported wider forms.

4. Runtime execution
   - Decode the admitted string literal pattern into MyLite-owned memory.
   - Apply MyLite-owned `SHOW LIKE` wildcard matching to descriptor names.
   - Match database and table names case-sensitively under current catalog
     policy.
   - Match column names ASCII case-insensitively for the observed MySQL 8.4.9
     behavior.
   - Preserve existing name-resolution, reserved-name, object-kind, and
     descriptor diagnostics before filtering.
   - Return MySQL-observed result-column names and result-set row-count state.

5. Physical storage safety
   - Do not query SQLite metadata or generate user SQL.
   - Do not mutate descriptor rows, descriptor versions, descriptor caches,
     catalog generation, `sqlite_schema_generation`, physical user tables, or
     the MyLite preamble.
   - Avoid SQLite fork patches and custom SQLite functions.

6. Tests
   - Add or extend fast C tests under `packages/libmylite/tests/` and register
     any new binary with a dotted CTest name.
   - Cover supported filters, wildcard semantics, escaped wildcard semantics,
     case sensitivity, empty/no-match result sets, schema resolution,
     descriptor result rows, warning/row-count behavior, diagnostics, reopen
     persistence, rename and drop behavior, independent handles, preamble
     safety, and unsupported syntax.
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
     pattern semantics, result metadata, row-count semantics, file-format
     safety, cleanup on failure, compatibility docs, and scope control.

## Out Of Scope

General `LIKE`, `NOT LIKE`, `RLIKE`, `SHOW ... WHERE`, `DESCRIBE`/`EXPLAIN`
wildcard filters, `SHOW FULL`, `SHOW EXTENDED`, hidden metadata, privileges,
views, temporary tables, collations, `INFORMATION_SCHEMA`, arbitrary SQLite
metadata reads, arbitrary SQLite pass-through, and SQLite fork patches.
