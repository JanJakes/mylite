# Baseline INFORMATION_SCHEMA Privileges Tasks

## Goal

Add queryable privilege metadata system views for MyLite's embedded
always-allowed identity model: global `USER_PRIVILEGES` rows for `root@%` and
empty schema/table/column privilege views until explicit grant descriptors
exist.

## Tasks

1. Design and documentation
   - Create `docs/specs/baseline-information-schema-privileges/specs.md`.
   - Record official MySQL 8.4 documentation sources and MySQL 8.4.9 runtime
     observations.
   - Specify row semantics, metadata shape, diagnostics, ownership boundaries,
     and unsupported privilege behavior.
   - Update compatibility docs only for the exact partial subset.

2. MySQL expectations
   - Add a reproducible MySQL 8.4.9 expectation script.
   - Verify root global privilege rows, system `TABLES` rows, system `COLUMNS`
     rows, empty lower-level privilege-table predicates, warning count, and
     row-count behavior.
   - Treat missing MySQL 8.4.9 runtime as a blocker.

3. Runtime metadata registry
   - Add the four privilege tables to the synthetic information-schema
     registry.
   - Add the observed column definitions.
   - Generate `USER_PRIVILEGES` rows from a static MySQL 8.4.9 global
     privilege list for `GRANTEE = '''root''@''%'''`.
   - Ensure lower-level privilege tables have no rows for this phase.
   - Ensure `INFORMATION_SCHEMA.TABLES` and `INFORMATION_SCHEMA.COLUMNS` emit
     system-view metadata for all four tables.

4. Tests
   - Add fast C tests under `packages/libmylite/tests/`.
   - Cover rows, counts, aliases, predicates, order/limit, system `TABLES`
     metadata, system `COLUMNS` metadata, independent handles, warning count,
     affected rows, and `ROW_COUNT()`.
   - Keep tests deterministic and avoid a new framework.

5. Build integration
   - Register the new test binary in `packages/libmylite/CMakeLists.txt`.
   - Keep first-party warning and clang-tidy policy enabled.
   - Keep vendored SQLite warning policy unchanged.

6. Verification and review
   - Run `cmake --build --preset dev`.
   - Run focused information-schema privilege CTest entries.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.
   - Review the final diff for metadata accuracy, scope control, docs, and no
     overclaiming of privilege semantics.

## Out Of Scope

Account storage, authentication, role state, grant descriptors, grant/revoke
DDL, `SHOW GRANTS`, lower-level privilege rows, `mysql.*` privilege tables,
privilege enforcement, definer privilege checks, privilege-dependent row
filtering, and SQLite fork changes.
