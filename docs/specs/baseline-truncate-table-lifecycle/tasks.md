# Baseline Truncate Table Lifecycle Tasks

## Goal

Add the next narrow table lifecycle slice: descriptor-driven
`TRUNCATE [TABLE] table_name` through `mylite_execute()`, backed by
authoritative MyLite catalog descriptors and stable SQLite physical table
names.

## Tasks

1. Design and documentation
   - Create `docs/specs/baseline-truncate-table-lifecycle/specs.md`.
   - Record official MySQL 8.4 documentation sources and MySQL 8.4.9 runtime
     observations.
   - Specify parser grammar, schema/table resolution, physical SQLite row
     emptying, diagnostics, result behavior, and unsupported behavior.
   - Update `COMPATIBILITY.md` and `docs/compatibility/sql-table-ddl.md` only
     for the exact partial subset after implementation.

2. MySQL expectations
   - Add a reproducible MySQL 8.4.9 expectation script for supported truncates
     and intentionally rejected wider forms.
   - Verify affected rows, warnings, remaining rows, metadata preservation,
     diagnostics, and side effects.
   - Treat a missing MySQL 8.4.9 runtime as a blocker for changing
     compatibility expectations.

3. Parser and AST
   - Extend Lemon grammar for `TRUNCATE table_name` and
     `TRUNCATE TABLE table_name`.
   - Add an AST node kind for truncate statements.
   - Map the `TRUNCATE` keyword in the parser adapter.
   - Keep parser code independent from runtime, catalog, storage, and SQLite.
   - Add parser tests for supported and rejected truncate syntax.

4. Analyzer/planner/runtime execution
   - Add a statement execution path using the existing statement context.
   - Resolve unqualified and schema-qualified targets against selected schema
     and MyLite catalog descriptors.
   - Match MySQL's table-does-not-exist diagnostic for qualified unknown
     schemas.
   - Reject reserved `_mylite_*` target schema/table names before generated
     SQLite SQL.
   - Reject unknown tables and unsupported object kinds deterministically.
   - Return an empty result with `affected_rows == 0` and no warnings.

5. Physical SQLite handling
   - Generate SQLite SQL only from descriptors and stable physical table names.
   - Quote every generated SQLite identifier.
   - Avoid relying on unsupported SQLite `TRUNCATE` syntax or optional
     `DELETE ... LIMIT` behavior.
   - Execute inside one MyLite-owned `BEGIN IMMEDIATE` transaction.
   - Keep descriptor rows, catalog generation, descriptor versions, descriptor
     caches, and SQLite schema generation unchanged for successful truncates.

6. Atomicity and cleanup
   - Roll back if prepare, step, allocation, or SQLite execution fails.
   - Make new planner cleanup functions tolerate zero-initialized state.
   - Preserve existing public API misuse behavior.

7. Tests
   - Add a fast C test under `packages/libmylite/tests/` and register it with a
     dotted CTest name.
   - Cover successful selected-schema, optional-`TABLE`, schema-qualified,
     empty-table, persistence, rename/drop, diagnostics, unsupported syntax,
     descriptor preservation, preamble safety, independent handles, and result
     behavior.
   - Keep tests deterministic and avoid adding a new test framework.

8. Build integration
   - Add new tests to `packages/libmylite/CMakeLists.txt`.
   - Keep first-party warning and clang-tidy policy enabled.
   - Keep vendored SQLite warning policy unchanged.

9. Verification and review
   - Run `cmake --build --preset dev`.
   - Run the new CTest entry and existing parser/basic/rename/row-values/
     select-where/select-order-limit/delete/update lifecycle entries.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.
   - Review the final diff for architecture boundaries, public ABI stability,
     independently authored grammar/spec text, MySQL 8.4.9 evidence, catalog
     authority, descriptor-driven physical emptying, exact affected-row
     semantics, file-format safety, VFS preservation, zero-init safety, cleanup
     on failure, scope control, compatibility-matrix accuracy, and test
     relevance.

## Out Of Scope

- Full MySQL `TRUNCATE TABLE`, implicit commits, transactions, table locks,
  privileges, temporary tables, views, partitions, Performance Schema summary
  tables, foreign keys, triggers, cascades, auto-increment reset, physical
  storage rebuilds, arbitrary SQLite pass-through, and SQLite fork patches.
