# Baseline INFORMATION_SCHEMA PROCESSLIST Tasks

## Goal

Add a queryable `INFORMATION_SCHEMA.PROCESSLIST` system view with MySQL 8.4.9
column metadata and one current embedded-handle row, building on the existing
`SHOW [FULL] PROCESSLIST` behavior without implementing server-wide thread
monitoring.

## Tasks

1. Design and documentation
   - Create `docs/specs/baseline-information-schema-processlist/specs.md`.
   - Record official MySQL 8.4 documentation sources and MySQL 8.4.9 runtime
     observations.
   - Specify query surface, current-handle row semantics, system metadata,
     diagnostics, ownership boundaries, and unsupported process-list behavior.
   - Update compatibility docs only for the exact partial subset.

2. MySQL expectations
   - Add a reproducible MySQL 8.4.9 expectation script.
   - Verify `PROCESSLIST` column labels, current query row values, selected
     schema behavior, untruncated `INFO`, deprecation warning, system `TABLES`
     metadata, system `COLUMNS` metadata, row-count behavior, and unknown
     projection/predicate/order-column diagnostics.
   - Treat missing MySQL 8.4.9 runtime as a blocker.

3. Runtime metadata registry
   - Add `INFORMATION_SCHEMA.PROCESSLIST` to the synthetic table registry.
   - Add the 8 observed column definitions.
   - Ensure `INFORMATION_SCHEMA.TABLES` and `INFORMATION_SCHEMA.COLUMNS` emit
     system-view metadata for `PROCESSLIST`.
   - Emit one current embedded-handle row with full statement text and
     `STATE = executing`.
   - Append the MySQL-compatible deprecation warning for successful selects.

4. Tests
   - Add fast C tests under `packages/libmylite/tests/`.
   - Cover wildcard and explicit projection, `COUNT(*)`, aliases, predicates,
     order/limit, lowercase table-name resolution, current-row values,
     untruncated `INFO`, system metadata, unknown names, independent handles,
     reopen safety, warning count, affected rows, and `ROW_COUNT()`.
   - Keep tests deterministic and avoid a new framework.

5. Build integration
   - Add any new test binary to `packages/libmylite/CMakeLists.txt`.
   - Keep first-party warning and clang-tidy policy enabled.
   - Keep vendored SQLite warning policy unchanged.

6. Verification and review
   - Run `cmake --build --preset dev`.
   - Run focused information-schema and processlist CTest entries.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.
   - Review the final diff for scope control, metadata accuracy, docs,
     statement-context ownership, diagnostics, warning behavior, and absence of
     storage or SQLite changes.

## Out Of Scope

Server-wide thread enumeration, sleeping/background rows, another connection
list, privilege filtering, anonymous-user behavior, `PROCESS` privilege
semantics, `KILL`, Performance Schema process-list tables, sys-schema
process-list views, host TCP port fidelity, status variables for deprecated
process-list access, physical information-schema tables, storage-format
changes, and SQLite fork patches.
