# Baseline INFORMATION_SCHEMA PARAMETERS Tasks

## Goal

Add a queryable empty `INFORMATION_SCHEMA.PARAMETERS` system view with MySQL
8.4.9 column metadata, without implementing stored routine DDL, descriptors,
parameters, return values, bodies, privileges, or execution.

## Tasks

1. Design and documentation
   - Create `docs/specs/baseline-information-schema-parameters/specs.md`.
   - Record official MySQL 8.4 documentation sources and MySQL 8.4.9 runtime
     observations.
   - Specify query surface, empty-row semantics, system metadata, diagnostics,
     ownership boundaries, and unsupported stored routine behavior.
   - Update compatibility docs only for the exact partial subset.

2. MySQL expectations
   - Add a reproducible MySQL 8.4.9 expectation script.
   - Verify `PARAMETERS` column labels, empty user-schema results, system
     `TABLES` metadata, system `COLUMNS` metadata, warning count, row-count
     behavior, unknown predicate-column diagnostics, and real MySQL parameter
     row shape after creating a procedure/function.
   - Treat missing MySQL 8.4.9 runtime as a blocker.

3. Runtime metadata registry
   - Add `INFORMATION_SCHEMA.PARAMETERS` to the synthetic table registry.
   - Add the 16 observed column definitions.
   - Ensure `INFORMATION_SCHEMA.TABLES` and `INFORMATION_SCHEMA.COLUMNS` emit
     system-view metadata for `PARAMETERS`.
   - Emit no parameter rows until routine descriptors are designed.

4. Tests
   - Add fast C tests under `packages/libmylite/tests/`.
   - Cover wildcard and explicit projection, `COUNT(*)`, aliases, predicates,
     order/limit over the empty row set, system `TABLES` metadata, system
     `COLUMNS` metadata, unknown predicate columns, independent handles,
     reopen safety, warning count, affected rows, and `ROW_COUNT()`.
   - Keep tests deterministic and avoid a new framework.

5. Build integration
   - Add any new test binary to `packages/libmylite/CMakeLists.txt`.
   - Keep first-party warning and clang-tidy policy enabled.
   - Keep vendored SQLite warning policy unchanged.

6. Verification and review
   - Run `cmake --build --preset dev`.
   - Run focused information-schema and routine CTest entries.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.
   - Review the final diff for scope control, metadata accuracy, docs, and
     absence of routine overclaiming.

## Out Of Scope

`CREATE PROCEDURE`, `CREATE FUNCTION`, `ALTER PROCEDURE`, `ALTER FUNCTION`,
`DROP PROCEDURE`, `DROP FUNCTION`, `CALL`, `SHOW CREATE PROCEDURE`,
`SHOW CREATE FUNCTION`, routine execution, routine descriptors, routine body
storage, parameter descriptors, function return metadata, persistence,
privileges, definers, real `INFORMATION_SCHEMA.PARAMETERS` rows, and SQLite
function reflection.
