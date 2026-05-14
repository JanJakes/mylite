# Baseline INFORMATION_SCHEMA COLLATION_CHARACTER_SET_APPLICABILITY Tasks

## Goal

Add a queryable static
`INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY` system view for the
currently supported MyLite `utf8mb4` collations, without widening charset or
collation behavior.

## Tasks

1. Design and documentation
   - Create
     `docs/specs/baseline-information-schema-collation-character-set-applicability/specs.md`.
   - Record official MySQL 8.4 documentation sources and MySQL 8.4.9 runtime
     observations.
   - Specify query surface, row semantics, system metadata, diagnostics,
     ownership boundaries, and unsupported charset/collation behavior.
   - Update compatibility docs only for the exact partial subset.

2. MySQL expectations
   - Add a reproducible MySQL 8.4.9 expectation script.
   - Verify row shape for the five supported MyLite `utf8mb4` collations,
     system `TABLES` metadata, system `COLUMNS` metadata, warning count, and
     row-count behavior.
   - Treat missing MySQL 8.4.9 runtime as a blocker.

3. Runtime metadata registry
   - Add `INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY` to the
     synthetic table registry.
   - Add the two observed column definitions.
   - Generate rows from the existing static `utf8mb4` collation descriptors,
     mapping every supported collation to `utf8mb4`.
   - Ensure `INFORMATION_SCHEMA.TABLES` and `INFORMATION_SCHEMA.COLUMNS` emit
     system-view metadata for the table.

4. Tests
   - Add or extend fast C tests under `packages/libmylite/tests/`.
   - Cover rows, `COUNT(*)`, aliases, predicates, order/limit, system
     `TABLES` metadata, system `COLUMNS` metadata, independent handles,
     warning count, affected rows, and `ROW_COUNT()`.
   - Keep tests deterministic and avoid a new framework.

5. Build integration
   - Add any new test binary to `packages/libmylite/CMakeLists.txt`, if needed.
   - Keep first-party warning and clang-tidy policy enabled.
   - Keep vendored SQLite warning policy unchanged.

6. Verification and review
   - Run `cmake --build --preset dev`.
   - Run focused static information-schema CTest entries.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.
   - Review the final diff for metadata accuracy, scope control, docs, and no
     overclaiming of charset/collation semantics.

## Out Of Scope

Additional character sets, additional collations, `mysql.collations`,
`mysql.character_sets`, broader data-dictionary tables, collation comparison
semantics, charset conversion, and SQLite fork changes.
