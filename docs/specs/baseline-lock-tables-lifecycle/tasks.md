# Baseline LOCK TABLES Lifecycle Tasks

## Goal

Add a narrow `LOCK TABLES` / `UNLOCK TABLES` compatibility slice for common
dump, migration, and maintenance SQL: parse supported lock syntax, resolve
targets through descriptors, record connection-local lock intent, preserve the
verified MySQL transaction side effects, and document deferred lock
enforcement.

## Tasks

1. Design and documentation
   - Create `docs/specs/baseline-lock-tables-lifecycle/specs.md`.
   - Record official MySQL 8.4 documentation sources and MySQL 8.4.9 runtime
     observations.
   - Specify parser grammar, target resolution, duplicate alias handling,
     transaction effects, lock-intent ownership, result behavior, diagnostics,
     storage impact, and unsupported enforcement semantics.
   - Update compatibility docs only for the exact implemented subset.

2. MySQL expectations
   - Add a reproducible MySQL 8.4.9 expectation script covering supported lock
     syntax, `ROW_COUNT()` / warning count, resolution errors, duplicate alias
     errors, temporary table acceptance, and transaction side effects.
   - Treat a missing MySQL 8.4.9 runtime as a blocker for compatibility
     expectation changes.

3. Parser and AST
   - Add `LOCK TABLE[S]` and `UNLOCK TABLE[S]` grammar.
   - Add AST node kinds for lock/unlock statements, lock target lists, lock
     targets, aliases, and lock modes where needed.
   - Preserve existing `SELECT ... LOCK IN SHARE MODE` parsing.
   - Add parser tests for accepted and rejected syntax.

4. Runtime
   - Resolve unqualified, schema-qualified, persistent, and temporary targets
     using the existing descriptor policy.
   - Reject reserved `_mylite_*` names before SQLite SQL generation.
   - Reject missing default schema, unknown schemas, unknown tables, unsupported
     object kinds, and duplicate effective lock aliases with deterministic
     MySQL-compatible diagnostics where verified.
   - Store connection-local lock intent and release it on later `LOCK TABLES`,
     `UNLOCK TABLES`, `START TRANSACTION` / `BEGIN`, and close.
   - Apply the verified implicit commit before successful `LOCK TABLES`.
   - Preserve existing non-row result conventions with affected rows `0` and
     warning count `0`.
   - Avoid SQLite fork patches and avoid physical storage changes except
     committing an active user transaction when required.

5. Tests
   - Add a focused C runtime test under `packages/libmylite/tests/` and
     register a dotted CTest name.
   - Cover successful persistent and temporary locks, aliases, multiple targets,
     schema-qualified targets, lock replacement, unlock forms, transaction
     commit/release behavior, close/reopen, independent handles, preamble
     preservation, result status, and diagnostics.
   - Keep tests deterministic and avoid a new framework.

6. Build integration
   - Add any new tests to `packages/libmylite/CMakeLists.txt`.
   - Keep first-party warning and clang-tidy policy enabled.
   - Keep vendored SQLite warning policy unchanged.

7. Verification and review
   - Run `cmake --build --preset dev`.
   - Run the new CTest entry plus parser, transaction, table lifecycle, and
     temporary-table lifecycle entries.
   - Run
     `./packages/libmylite/tests/mysql_baseline_lock_tables_lifecycle_expectations.sh`.
   - Run `cmake --workflow --preset check`.
   - Review architecture boundaries, MySQL evidence, descriptor authority,
     transaction effects, lock-intent cleanup, file-format safety, zero-init
     cleanup, compatibility docs, and tests.

## Out Of Scope

Cross-handle blocking, access enforcement for locked tables, alias-only access
requirements, read-lock write rejection, DDL restrictions while locked, implicit
trigger/view/foreign-key locks, privileges, global read locks, backup locks,
Performance Schema metadata locks, and SQLite fork patches.
