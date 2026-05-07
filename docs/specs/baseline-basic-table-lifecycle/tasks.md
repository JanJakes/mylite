# Baseline Basic Table Lifecycle Tasks

## Goal

Add the first narrow public SQL execution and persistent base-table lifecycle
slice on top of file-backed `mylite_db`, statement context, parser scaffolding,
shifted `.mylite` storage, and the durable MyLite catalog.

## Tasks

1. Design and documentation
   - Create `docs/specs/baseline-basic-table-lifecycle/specs.md`.
   - Record official MySQL 8.4 documentation sources and MySQL 8.4.9 runtime
     observations.
   - Specify the public execution/result ABI and ownership rules.
   - Specify parser grammar, schema resolution, catalog descriptors, physical
     SQLite DDL generation, transaction behavior, diagnostics, and unsupported
     behavior.
   - Update `COMPATIBILITY.md` and relevant detailed compatibility docs only
     for the exact partial subset.

2. MySQL expectations
   - Add a reproducible MySQL 8.4.9 expectation script/artifact for the
     supported user-visible SQL behavior.
   - Verify success status, affected rows, diagnostics, warnings, result column
     labels, result rows, column descriptors, and side effects.
   - Treat a missing MySQL 8.4.9 runtime as a blocker for changing
     compatibility expectations.

3. Public execution/result API
   - Add an opaque `mylite_result` handle to the public header.
   - Add `mylite_execute()` and result accessors.
   - Keep SQLite types out of public headers.
   - Make cleanup and accessors tolerate `NULL`.
   - Return no result handle on failure and keep diagnostics on `mylite_db`.

4. Parser and AST
   - Extend Lemon grammar for the limited `USE`, `CREATE TABLE`, `DROP TABLE`,
     and `SHOW TABLES` surface.
   - Add AST node kinds and payloads for table DDL, show-tables, column
     definitions, integer type, unsigned flag, and nullability.
   - Keep parser code independent from runtime, catalog, storage, and SQLite.
   - Add parser tests for supported and rejected syntax.

5. Analyzer/planner/runtime execution
   - Add a statement execution path that begins and ends
     `mylite_statement_context` for every public execution call.
   - Resolve schema-qualified and unqualified table names against selected
     schema and MyLite catalog descriptors.
   - Implement `USE` for existing catalog schemas.
   - Reject reserved `_mylite_*` user names before SQLite SQL generation.
   - Reject duplicate tables, unknown tables, duplicate columns, unknown
     schemas, no selected schema, and unsupported parsed scopes with
     deterministic diagnostics.
   - Build descriptor-driven planned results for `SHOW TABLES`.

6. Catalog and physical DDL
   - Add catalog mutation helpers that can run table and column descriptor
     changes inside the caller's active statement transaction.
   - Allocate deterministic internal physical table names.
   - Generate quoted SQLite physical `CREATE TABLE` and `DROP TABLE` SQL from
     descriptors.
   - Advance catalog generation and invalidate descriptor caches only after a
     successful DDL commit.
   - Advance SQLite schema generation on successful physical schema mutation.
   - Roll back catalog and physical SQLite schema changes together on failure.

7. Tests
   - Add a fast C test under `packages/libmylite/tests/` and register it with a
     dotted CTest name.
   - Cover public API misuse, result cleanup, parser support, create/drop
     success, catalog descriptors, physical SQLite schema, preamble safety,
     reopen persistence, `SHOW TABLES`, failure unwinding, independent handles,
     reserved names, duplicate table, unknown table, duplicate columns,
     unknown schema, no selected schema, unsupported syntax, and induced
     physical/catalog failure where practical.
   - Keep tests deterministic and avoid adding a new test framework.

8. Build integration
   - Add new sources and tests to `packages/libmylite/CMakeLists.txt`.
   - Keep first-party warning and clang-tidy policy enabled.
   - Keep vendored SQLite warning policy unchanged.

9. Verification and review
   - Run `cmake --build --preset dev`.
   - Run the new CTest entry.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.
   - Review the final diff for architecture boundaries, public ABI stability,
     independently authored grammar/spec text, MySQL 8.4.9 evidence, catalog
     authority, descriptor/physical-schema atomicity, file-format safety, VFS
     preservation, zero-init safety, cleanup on failure, scope control,
     compatibility-matrix accuracy, and test relevance.

## Out Of Scope

- `CREATE DATABASE`, `DROP DATABASE`, database listing, or full dynamic schema
  management.
- General `SELECT`, DML, joins, expressions beyond table lifecycle grammar, or
  arbitrary SQLite pass-through.
- `ALTER TABLE`, `TRUNCATE TABLE`, `RENAME TABLE`, indexes, primary keys,
  foreign keys, checks, defaults, generated columns, comments, table options,
  temporary tables, views, partitions, triggers, routines, events,
  `INFORMATION_SCHEMA`, full type conversion, auto-increment semantics, and
  SQLite fork patches.
