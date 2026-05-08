# Baseline SHOW CREATE DATABASE Tasks

## Goal

Add a narrow descriptor-driven `SHOW CREATE DATABASE` / `SHOW CREATE SCHEMA`
slice for persistent MyLite schemas, returning MySQL 8.4.9-compatible columns
and fixed-default DDL text for current optionless schema descriptors.

## Tasks

1. Design and documentation
   - Create `docs/specs/baseline-show-create-database/specs.md`.
   - Record official MySQL 8.4 documentation sources and MySQL 8.4.9 runtime
     observations.
   - Specify parser grammar, schema resolution, DDL rendering, diagnostics,
     result semantics, and physical SQLite policy.
   - Update `COMPATIBILITY.md` and detailed compatibility docs only for the
     exact partial subset.

2. MySQL expectations
   - Add a reproducible MySQL 8.4.9 expectation script.
   - Verify `SHOW CREATE DATABASE`, `SHOW CREATE SCHEMA`, exact columns,
     rendered DDL, quoting, warning count, `ROW_COUNT()`, unknown schemas, and
     deferred wider forms.
   - Treat a missing MySQL 8.4.9 runtime as a blocker for changing
     compatibility expectations.

3. Parser and AST
   - Extend Lemon grammar for `SHOW CREATE {DATABASE|SCHEMA} identifier`.
   - Add an AST node kind and parser constructor.
   - Keep parser code independent from runtime, catalog, storage, and SQLite.
   - Add parser tests for supported and rejected forms.

4. Runtime execution
   - Add a statement execution path through the existing statement context.
   - Resolve schema names through MyLite catalog descriptors.
   - Reject reserved `_mylite_*` schema names before descriptor lookup.
   - Render fixed-default MySQL-style DDL with backtick quoting.
   - Return a two-column, one-row result set and result-statement row-count
     behavior.

5. Catalog and physical policy
   - Read only MyLite schema descriptors.
   - Do not inspect SQLite metadata.
   - Do not mutate catalog rows, descriptor versions, descriptor caches,
     catalog generation, physical SQLite objects, or `sqlite_schema_generation`.
   - Do not add SQLite fork patches.

6. Tests
   - Add a fast C test under `packages/libmylite/tests/` and register it with a
     dotted CTest name.
   - Cover supported aliases, exact columns, exact DDL, quoting, row-count
     behavior, diagnostics, persistence, drop behavior, preamble preservation,
     generation stability, and independent handles.
   - Keep tests deterministic and avoid adding a new test framework.

7. Build integration
   - Add new sources or tests to `packages/libmylite/CMakeLists.txt`.
   - Keep first-party warning and clang-tidy policy enabled.
   - Keep vendored SQLite warning policy unchanged.

8. Verification and review
   - Run `cmake --build --preset dev`.
   - Run the new CTest entry and existing parser/schema/show lifecycle entries.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.
   - Review the final diff for architecture boundaries, public ABI stability,
     independently authored grammar/spec text, MySQL 8.4.9 evidence, catalog
     authority, result shape accuracy, file-format safety, VFS preservation,
     zero-init safety, cleanup on failure, scope control,
     compatibility-matrix accuracy, and test relevance.

## Out Of Scope

- `SHOW CREATE DATABASE IF NOT EXISTS`.
- Schema option storage or mutation.
- System schemas, privilege filtering, and `INFORMATION_SCHEMA.SCHEMATA`.
- SQLite metadata reads or SQLite fork patches.
