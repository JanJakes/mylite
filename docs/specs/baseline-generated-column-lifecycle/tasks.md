# Baseline Generated Column Lifecycle Tasks

## Design And Evidence

- [x] Verify MySQL 8.4.9 runtime is available.
- [x] Probe generated-column syntax, DML interaction, metadata, and diagnostics.
- [x] Write independently authored feature specification.
- [x] Add MySQL-runtime expectation artifact for the user-visible subset.

## Implementation

- [x] Extend parser/AST support for generated-column clauses.
- [x] Add generated-column fields to persistent catalog column descriptors.
- [x] Add catalog schema migration and descriptor materialization updates.
- [x] Plan and validate the admitted generated-column expression subset.
- [x] Render MySQL display expressions and SQLite physical expressions.
- [x] Emit physical SQLite generated columns for `CREATE TABLE`.
- [x] Enforce DML admission rules for generated-column targets and physical
      insert/update omission.
- [x] Update `SHOW COLUMNS`, `SHOW FULL COLUMNS`, `SHOW CREATE TABLE`, and
      `INFORMATION_SCHEMA.COLUMNS` metadata.
- [x] Preserve/omit generated columns correctly in `CREATE TABLE ... LIKE`,
      CTAS, rename, drop, and reopen flows.

## Tests And Docs

- [x] Add parser and runtime C tests under `packages/libmylite/tests/`.
- [x] Register new CTest entries in `packages/libmylite/CMakeLists.txt`.
- [x] Update `COMPATIBILITY.md` and relevant compatibility detail docs.
- [x] Run the MySQL expectation script.
- [x] Run targeted parser/runtime tests.
- [x] Run `cmake --workflow --preset check`.
- [x] Review, commit, and push to remote `main`.
