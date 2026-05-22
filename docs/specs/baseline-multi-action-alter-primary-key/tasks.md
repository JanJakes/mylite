# Baseline Multi-Action ALTER Primary Key Tasks

## Design and Evidence

- [x] Record MySQL 8.4.9 expectations for successful multi-action primary-key
      adds, drops, swaps, same-statement added columns, rollback, row counts,
      warnings, metadata, and auto-increment final-state behavior.
- [x] Specify parser, AST, analyzer/planner/runtime, catalog, result, SQLite,
      storage, and compatibility boundaries.
- [x] Update compatibility documentation for the exact supported subset.

## Parser and AST

- [x] Add `ADD PRIMARY KEY` as first and later actions in
      `alter_table_multi_action_list`.
- [x] Add `DROP PRIMARY KEY` as first and later actions in
      `alter_table_multi_action_list`.
- [x] Keep single-action option-tail grammar unchanged and keep trailing
      multi-action `ALGORITHM` / `LOCK` options deferred.
- [x] Add parser tests for accepted and deferred forms.

## Runtime

- [x] Reuse the existing single-action `ADD PRIMARY KEY` planner through the
      shared multi-action table view.
- [x] Split single-action add-primary execution so catalog/physical mutation
      can run inside an existing catalog mutation.
- [x] Reuse the existing single-action `DROP PRIMARY KEY` planner while
      allowing multi-action execution to defer auto-increment key preservation
      to final-state validation.
- [x] Split single-action drop-primary execution so catalog/physical mutation
      can run inside an existing catalog mutation.
- [x] Reject warning-producing multi-action primary-key adds such as
      `USING HASH`.
- [x] Validate the final descriptor state so every auto-increment column remains
      indexed by the first part of a surviving index.
- [x] Preserve multi-action result semantics with `affected_rows == 0` and
      `warning_count == 0` for successful supported forms.

## Tests

- [x] Extend runtime multi-action tests for successful primary-key adds, drops,
      swaps, metadata, DML enforcement, persistence, preamble preservation, and
      auto-increment key preservation.
- [x] Extend rollback tests for duplicate, `NULL`, missing, and same-statement
      added-column failure cases.
- [x] Extend diagnostics tests for no database selected, unknown schema,
      unknown table, unknown column, missing primary key, duplicate primary
      key, temporary tables, and warning-producing primary-key options.
- [x] Add or update MySQL expectation artifacts and run them against MySQL
      8.4.9.

## Verification

- [x] `cmake --build --preset dev`
- [x] Focused CTest entries for parser, multi-action ALTER, add/drop primary
      key, add/drop indexes, defaults, and related runtime lifecycle tests.
- [x] `packages/libmylite/tests/mysql_baseline_multi_action_alter_primary_key_expectations.sh`
- [x] `git diff --check`
- [x] `cmake --workflow --preset check`
- [x] Review the final diff for scope, descriptor authority, rollback,
      auto-increment final-state validation, physical SQLite handling,
      compatibility docs, and tests.
