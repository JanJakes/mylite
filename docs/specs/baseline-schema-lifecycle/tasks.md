# Baseline Schema Lifecycle Tasks

## Goal

Add the next narrow schema lifecycle slice: descriptor-driven public
`CREATE/DROP DATABASE|SCHEMA`, public `USE` over created schema descriptors,
and unfiltered `SHOW DATABASES|SCHEMAS` through `mylite_execute()`.

## Tasks

1. Design and documentation
   - Create `docs/specs/baseline-schema-lifecycle/specs.md`.
   - Record official MySQL 8.4 documentation sources and MySQL 8.4.9 runtime
     observations.
   - Specify parser grammar, schema resolution, catalog mutation, physical
     SQLite cleanup, diagnostics, result behavior, and unsupported behavior.
   - Update `COMPATIBILITY.md`, `docs/compatibility/sql-schemas.md`, and
     `docs/compatibility/sql-show-statements.md` only for the exact partial
     subset after implementation.

2. MySQL expectations
   - Add a reproducible MySQL 8.4.9 expectation script for supported schema
     lifecycle behavior and intentionally rejected wider forms.
   - Verify result rows, column names, errors, warnings, affected rows, selected
     schema side effects, and drop table-count behavior.
   - Treat a missing MySQL 8.4.9 runtime as a blocker for changing
     compatibility expectations.

3. Parser and AST
   - Extend Lemon grammar for `CREATE DATABASE identifier`,
     `CREATE SCHEMA identifier`, `DROP DATABASE identifier`,
     `DROP SCHEMA identifier`, `SHOW DATABASES`, and `SHOW SCHEMAS`.
   - Add AST node kinds for schema create, schema drop, and database listing.
   - Map the needed schema keywords in the parser adapter.
   - Keep parser code independent from runtime, catalog, storage, and SQLite.
   - Add parser tests for supported and rejected schema lifecycle syntax.

4. Catalog APIs
   - Add schema iteration for descriptor-driven `SHOW DATABASES`.
   - Add composable schema deletion inside an active catalog mutation.
   - Keep descriptor versions, catalog generation, and descriptor-cache
     invalidation owned by the catalog module.
   - Keep public headers unchanged unless an internal header addition is
     required.

5. Analyzer/planner/runtime execution
   - Add statement execution paths using the existing statement context.
   - Resolve schema names against MyLite catalog descriptors.
   - Reject reserved `_mylite_*` schema names before generated SQLite SQL.
   - Match MySQL diagnostics for duplicate create, missing drop, and missing
     use.
   - Clear the selected schema when the selected schema is dropped.
   - Return empty non-row results for schema create/drop/use and a one-column
     text result for database listing.

6. Physical SQLite handling
   - Collect physical table names from MyLite descriptors before dropping a
     schema.
   - Generate `DROP TABLE` SQL only from stable descriptor physical names.
   - Quote every generated SQLite identifier.
   - Execute descriptor deletion and physical drops inside one catalog mutation
     transaction.
   - Increment `sqlite_schema_generation` only after successful drops that
     removed physical table objects.
   - Preserve the `.mylite` preamble and shifted SQLite payload boundary.

7. Atomicity and cleanup
   - Roll back if descriptor iteration, descriptor deletion, physical drop,
     allocation, prepare, step, or commit fails.
   - Make new helper cleanup functions tolerate zero-initialized state.
   - Preserve existing public API misuse behavior.

8. Tests
   - Add a fast C test under `packages/libmylite/tests/` and register it with a
     dotted CTest name.
   - Cover successful create/use/show/drop paths, aliases, diagnostics,
     unsupported syntax, selected-schema clearing, schema drop with tables,
     empty schema drop, affected rows, warning counts, result shape, reopen
     persistence, independent handles, preamble safety, and cleanup.
   - Keep tests deterministic and avoid adding a new test framework.

9. Build integration
   - Add new tests to `packages/libmylite/CMakeLists.txt`.
   - Keep first-party warning and clang-tidy policy enabled.
   - Keep vendored SQLite warning policy unchanged.

10. Verification and review
    - Run `cmake --build --preset dev`.
    - Run the new CTest entry and existing parser/basic/rename/row-values/
      select-where/select-order-limit/delete/update/truncate lifecycle entries.
    - Run the MySQL expectation script.
    - Run `cmake --workflow --preset check`.
    - Review the final diff for architecture boundaries, public ABI stability,
      independently authored grammar/spec text, MySQL 8.4.9 evidence, catalog
      authority, transactional schema drop cleanup, exact affected-row
      semantics, file-format safety, VFS preservation, zero-init safety, scope
      control, compatibility-matrix accuracy, and test relevance.

## Out Of Scope

- Full schema options, `IF EXISTS`, `IF NOT EXISTS`, `ALTER DATABASE`, `SHOW
  CREATE DATABASE`, `SHOW DATABASES LIKE`, `SHOW DATABASES WHERE`, privileges,
  system schemas, information-schema tables, `DATABASE()`, locks, implicit
  commit behavior, replication, filesystem directories, temporary tables,
  views, triggers, cascades, foreign keys, arbitrary SQLite pass-through, and
  SQLite fork patches.
