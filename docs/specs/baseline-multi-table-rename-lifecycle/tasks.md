# Baseline Multi-Table Rename Lifecycle Tasks

## Goal

Add the next narrow table-lifecycle slice: descriptor-driven multi-pair
`RENAME TABLE` for persistent base tables through `mylite_execute()`, preserving
MySQL 8.4.9 left-to-right behavior and statement atomicity.

## Tasks

1. Design and documentation
   - Create `docs/specs/baseline-multi-table-rename-lifecycle/specs.md`.
   - Record official MySQL 8.4 documentation sources and MySQL 8.4.9 runtime
     observations.
   - Specify grammar, schema resolution, left-to-right semantics, atomic
     rollback, descriptor mutation, diagnostics, physical SQLite policy, and
     unsupported behavior.
   - Update compatibility docs only after implementation, and only for the
     exact supported subset.

2. MySQL expectations
   - Add a reproducible MySQL 8.4.9 expectation script for supported
     multi-pair renames and intentionally rejected/erroring forms.
   - Verify success status, `ROW_COUNT()`, warning count, table visibility,
     row preservation, cross-schema moves, left-to-right swap, failure
     rollback, no-default-schema behavior, unknown schemas, duplicate targets,
     repeated sources, repeated targets, and case-distinct names.
   - Treat a missing MySQL 8.4.9 runtime as a blocker for implementation.

3. Parser and AST
   - Extend Lemon grammar for comma-separated rename pairs.
   - Add or reuse AST node kinds for a rename-pair list and rename-pair nodes.
   - Keep the existing single-pair AST contract stable where practical, or
     update all callers/tests deliberately.
   - Keep parser code independent from runtime, catalog, storage, and SQLite.
   - Add parser tests for supported and rejected multi-pair syntax.

4. Analyzer/planner/runtime execution
   - Extend the existing rename execution path using statement context.
   - Resolve source and target schemas using the existing selected/default
     schema policy.
   - Reject reserved `_mylite_*` source and target schema/table names before
     catalog mutation.
   - Execute table existence and target availability checks left to right
     inside one catalog mutation.
   - Reject unsupported object kinds once non-base-table descriptors exist.
   - Preserve existing single-pair diagnostics and result conventions.

5. Catalog and physical policy
   - Reuse or extend the catalog table-rename mutation helper.
   - Update each descriptor's `schema_id`, `name`, `descriptor_version`, and
     `updated_catalog_generation` in the active mutation.
   - Commit one catalog generation for the statement.
   - Preserve physical SQLite table names, column descriptors, row data, and
     `sqlite_schema_generation`.
   - Avoid SQLite physical DDL and SQLite fork patches.

6. Atomicity and cleanup
   - Roll back if any pair fails after earlier pairs have updated descriptors.
   - Make new plan cleanup functions tolerate zero-initialized state.
   - Preserve `.mylite` preamble bytes and shifted SQLite payload invariants.

7. Tests
   - Add or extend fast C tests under `packages/libmylite/tests/`.
   - Cover parser shape, success paths, left-to-right swap, cross-schema moves,
     failure rollback, diagnostics, case-distinct names, descriptor/physical
     invariants, reopen persistence, independent handles, and unsupported
     forms.
   - Keep tests deterministic and avoid a new framework.

8. Build integration
   - Add new tests to `packages/libmylite/CMakeLists.txt` only if a new binary
     is needed.
   - Keep first-party warning and clang-tidy policy enabled.
   - Keep vendored SQLite warning policy unchanged.

9. Verification and review
   - Run `cmake --build --preset dev`.
   - Run the MySQL expectation script.
   - Run parser and rename/basic/schema/row-values/select/delete/update
     lifecycle CTests.
   - Run `cmake --workflow --preset check`.
   - Review the final diff for MySQL 8.4.9 evidence, left-to-right behavior,
     rollback, descriptor authority, catalog generation, physical SQLite
     safety, file-format preservation, scope control, docs accuracy, and test
     relevance.

## Out Of Scope

- Temporary tables, views, triggers, routines, events, foreign keys, privilege
  semantics, metadata locks, `INFORMATION_SCHEMA` completeness, ALTER rename
  forms, physical SQLite renames, table rebuilds, arbitrary SQLite pass-through,
  and SQLite fork patches.
