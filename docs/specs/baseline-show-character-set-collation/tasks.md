# Baseline SHOW CHARACTER SET and SHOW COLLATION Tasks

## Goal

Add static MyLite-owned introspection for the current default
`utf8mb4` character set and `utf8mb4_0900_ai_ci` collation through
`mylite_execute()`, with MySQL 8.4.9 result shapes and explicit partial
compatibility wording.

## Tasks

1. Design and documentation
   - Create `docs/specs/baseline-show-character-set-collation/specs.md`.
   - Record official MySQL 8.4 documentation sources and MySQL 8.4.9 runtime
     observations.
   - Specify grammar, static rows, `LIKE` matching, diagnostics, result state,
     ownership boundaries, storage non-involvement, and unsupported wider
     forms.
   - Update compatibility docs only for the exact partial subset.

2. MySQL expectations
   - Add a reproducible MySQL 8.4.9 expectation script for supported
     `SHOW CHARACTER SET`, `SHOW CHARSET`, and `SHOW COLLATION` behavior.
   - Verify exact row values, column names, case-insensitive `LIKE`,
     wildcard/escaped-wildcard matching, no-match results, warning count,
     following `ROW_COUNT()`, accepted-but-deferred `WHERE`, syntax
     rejections, and wider catalog counts.
   - Treat a missing MySQL 8.4.9 runtime as a blocker for changing
     compatibility expectations.

3. Parser and AST
   - Extend Lemon grammar for `SHOW CHARACTER SET`, `SHOW CHARSET`, and
     `SHOW COLLATION`.
   - Add dedicated AST nodes for the two statement families.
   - Reuse the existing optional `LIKE STRING` AST shape.
   - Preserve `CHARSET` and `COLLATION` as usable identifiers where MySQL
     permits them in the current identifier subset.
   - Add parser tests for supported forms and unsupported modifiers.

4. Runtime execution
   - Decode the optional `LIKE` pattern through the existing `SHOW LIKE`
     filter helper.
   - Build MySQL 8.4.9 result columns for both statements.
   - Emit only the static `utf8mb4` and `utf8mb4_0900_ai_ci` rows when they
     match the optional filter.
   - Match charset and collation names with ASCII case-insensitive `LIKE`
     semantics.
   - Preserve row-result statement state: warning count `0` and following
     `ROW_COUNT() == -1`.

5. Storage and catalog safety
   - Do not query SQLite metadata, `sqlite_schema`, PRAGMA output, catalog
     tables, or physical user rows.
   - Do not mutate descriptor rows, descriptor versions, descriptor caches,
     catalog generation, `sqlite_schema_generation`, user rows, or the MyLite
     preamble.
   - Avoid SQLite fork patches and custom SQLite functions.

6. Tests
   - Add a fast C test under `packages/libmylite/tests/` and register it with
     a dotted CTest name.
   - Cover result shape, row values, filtering, diagnostics, schema
     independence, reopen persistence, independent handles, generation
     stability, preamble safety, and unsupported syntax.
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
   - Review the final diff for parser independence, result fidelity,
     MySQL-observed matching, status semantics, file-format safety, cleanup on
     failure, compatibility docs, and scope control.

## Out Of Scope

Full character-set catalogs, full collation catalogs, unsupported
`SHOW ... WHERE` predicate forms, `INFORMATION_SCHEMA`, `mysql.collations`,
connection charset state,
`SET NAMES`, `SET CHARACTER SET`, system variables, string types, collation
coercibility, string comparison behavior, privileges, arbitrary SQLite
metadata reads, arbitrary SQLite SQL pass-through, and SQLite fork patches.
