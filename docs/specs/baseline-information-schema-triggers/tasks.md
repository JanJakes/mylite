# Baseline INFORMATION_SCHEMA TRIGGERS Tasks

## Goal

Add a queryable empty `INFORMATION_SCHEMA.TRIGGERS` system view with MySQL
8.4.9 column metadata, without implementing trigger DDL, descriptors, or
execution.

## Tasks

1. Design and documentation
   - Create `docs/specs/baseline-information-schema-triggers/specs.md`.
   - Record official MySQL 8.4 documentation sources and MySQL 8.4.9 runtime
     observations.
   - Specify query surface, empty-row semantics, system metadata, diagnostics,
     ownership boundaries, and unsupported trigger behavior.
   - Update compatibility docs only for the exact partial subset.

2. MySQL expectations
   - Add a reproducible MySQL 8.4.9 expectation script.
   - Verify `TRIGGERS` column labels, empty user-schema results, system
     `TABLES` metadata, system `COLUMNS` metadata, warning count, and
     row-count behavior.
   - Treat missing MySQL 8.4.9 runtime as a blocker.

3. Runtime metadata registry
   - Add `INFORMATION_SCHEMA.TRIGGERS` to the synthetic table registry.
   - Add the 22 observed column definitions.
   - Ensure `INFORMATION_SCHEMA.TABLES` and `INFORMATION_SCHEMA.COLUMNS` emit
     system-view metadata for `TRIGGERS`.
   - Emit no trigger rows until trigger descriptors are designed.

4. Tests
   - Add fast C tests under `packages/libmylite/tests/`.
   - Cover wildcard and explicit projection, `COUNT(*)`, aliases, predicates,
     order/limit over the empty row set, system `TABLES` metadata, system
     `COLUMNS` metadata, independent handles, reopen safety, warning count,
     affected rows, and `ROW_COUNT()`.
   - Keep tests deterministic and avoid a new framework.

5. Build integration
   - Add any new test binary to `packages/libmylite/CMakeLists.txt`.
   - Keep first-party warning and clang-tidy policy enabled.
   - Keep vendored SQLite warning policy unchanged.

6. Verification and review
   - Run `cmake --build --preset dev`.
   - Run focused information-schema and trigger CTest entries.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.
   - Review the final diff for scope control, metadata accuracy, docs, and
     absence of trigger overclaiming.

## Out Of Scope

`CREATE TRIGGER`, `DROP TRIGGER`, `SHOW CREATE TRIGGER`, trigger execution,
trigger descriptors, trigger ordering, trigger persistence, trigger privileges,
stored-program SQL mode capture, trigger character-set metadata, real
`INFORMATION_SCHEMA.TRIGGERS` rows, and SQLite trigger reflection.
