# Baseline AUTO_INCREMENT Lifecycle Tasks

## Goal

Add the first descriptor-owned auto-increment slice for persistent base tables:
single-column integer primary-key auto-increment descriptors, durable counters,
generated insert values, `LAST_INSERT_ID()` updates, metadata rendering,
persistence, and deterministic rejection of wider allocation forms.

## Tasks

1. Design and documentation
   - Create `docs/specs/baseline-auto-increment-lifecycle/specs.md`.
   - Record official MySQL 8.4 documentation sources and MySQL 8.4.9 runtime
     observations.
   - Specify grammar, descriptor ownership, catalog migration, generated-value
     semantics, counter persistence, `LAST_INSERT_ID()`, metadata rendering,
     diagnostics, physical SQLite handling, and unsupported behavior.
   - Update compatibility docs only for the exact implemented subset.

2. MySQL expectations
   - Add a reproducible MySQL 8.4.9 expectation script for supported
     auto-increment behavior and deliberately deferred wider forms.
   - Verify DDL metadata, generated values, explicit values, table options,
     `LAST_INSERT_ID()`, duplicate diagnostics, `CREATE TABLE ... LIKE`,
     `TRUNCATE`, and counter exhaustion.
   - Treat a missing MySQL 8.4.9 runtime as a blocker for changing
     compatibility expectations.

3. Parser and AST
   - Add `AUTO_INCREMENT` as a column attribute for the limited create-table
     grammar.
   - Add the `AUTO_INCREMENT=N` table option for nonnegative integer literals.
   - Preserve parser independence from runtime, catalog, storage, and SQLite.
   - Add parser tests for supported attribute orders and rejected unsupported
     forms.

4. Catalog descriptors
   - Advance the catalog schema version to version `6`.
   - Add durable column-level `is_auto_increment` metadata.
   - Add durable table-level `auto_increment_next` metadata.
   - Add migration from version `5` to `6` and update fresh bootstrap DDL.
   - Add catalog APIs to read/write cloned descriptors and update/reset
     counters inside mutations.

5. Analyzer and DDL execution
   - Resolve auto-increment attributes against planned columns and primary-key
     definitions.
   - Admit exactly one auto-increment column, and only when it is the current
     single-column integer-family primary key.
   - Reject unsupported types, missing keys, duplicate auto-increment columns,
     explicit defaults, explicit nullable primary-key parts, and unsupported
     table-option values.
   - Clone the attribute but reset counters for `CREATE TABLE ... LIKE`.
   - Preserve `CREATE TABLE ... SELECT` no-key/no-auto target behavior.

6. DML behavior
   - Generate auto-increment values for omitted, `NULL`, `0`, and `DEFAULT`
     insert targets.
   - Advance counters for generated values and successful explicit values above
     the current next counter.
   - Update `LAST_INSERT_ID()` only after successful generated insert
     statements.
   - Preserve statement atomicity for rows, counters, and session insert-id
     state.
   - Reject mixed-mode multi-row explicit/generated auto-increment inserts.
   - Preserve existing duplicate-key and `INSERT IGNORE` warning-demotion
     behavior.
   - Advance counters for successful explicit `UPDATE` assignments to the
     auto-increment primary-key column when needed.

7. Metadata rendering
   - Render `auto_increment` in `SHOW COLUMNS` `Extra`.
   - Render column and table-level auto-increment clauses in
     `SHOW CREATE TABLE`.
   - Render `SHOW TABLE STATUS` `Auto_increment` from the durable counter.
   - Keep no-auto table metadata unchanged.

8. Tests
   - Add fast plain C runtime tests under `packages/libmylite/tests/` and
     register dotted CTest names.
   - Cover supported DDL, metadata, generated inserts, explicit inserts,
     `INSERT ... SET`, `INSERT IGNORE`, `LAST_INSERT_ID()`, table options,
     counter exhaustion, `CREATE TABLE ... LIKE`, `DELETE`, `TRUNCATE`,
     updates, persistence, independent handles, preamble safety, and
     diagnostics.
   - Extend parser tests for new grammar.
   - Keep tests deterministic and avoid adding a new test framework.

9. Build integration
   - Add any new sources or tests to `packages/libmylite/CMakeLists.txt`.
   - Keep first-party warning and clang-tidy policy enabled.
   - Keep vendored SQLite warning policy unchanged.

10. Verification and review
   - Run `cmake --build --preset dev`.
   - Run the new CTest entry plus relevant parser/runtime lifecycle entries.
   - Run
     `./packages/libmylite/tests/mysql_baseline_auto_increment_lifecycle_expectations.sh`.
   - Run `cmake --workflow --preset check`.
   - Review architecture boundaries, MySQL evidence, catalog authority,
     counter atomicity, `LAST_INSERT_ID()` semantics, diagnostics, file-format
     safety, zero-init cleanup, compatibility docs, and test relevance.

## Out Of Scope

- Auto-increment without the current primary-key descriptor, secondary-index
  auto-increment, composite keys, `VARCHAR` keys, `ALTER TABLE ...
  AUTO_INCREMENT`, mixed-mode multi-row allocation gaps, `INSERT ... SELECT`
  into auto-increment tables, key-aware `REPLACE`, `LAST_INSERT_ID(expr)`,
  `NO_AUTO_VALUE_ON_ZERO`, `auto_increment_increment`,
  `auto_increment_offset`, `innodb_autoinc_lock_mode`, information-schema
  metadata, protocol insert-id metadata, replication behavior, generated
  invisible primary keys, and SQLite fork patches.
