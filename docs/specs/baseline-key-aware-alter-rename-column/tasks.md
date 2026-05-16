# Baseline Key-Aware ALTER RENAME COLUMN Tasks

## Goal

Extend the existing `ALTER TABLE ... RENAME COLUMN` lifecycle so descriptor
keys and supported foreign-key dependencies follow the renamed column by
`column_id`.

## Tasks

1. Design and MySQL evidence
   - Add `docs/specs/baseline-key-aware-alter-rename-column/specs.md`.
   - Record official MySQL 8.4 documentation sources and MySQL 8.4.9 runtime
     observations.
   - Capture reproducible MySQL expectation probes for primary-key, secondary
     key, prefix key, fulltext, and foreign-key renames.

2. Runtime implementation
   - Remove the current primary-key-column rejection from rename-column
     planning.
   - Keep existing schema/table/column, duplicate-name, reserved-name,
     same-name no-op, case-only rename, CHECK rejection, and physical failure
     behavior unchanged.
   - Preserve descriptor ids and key/foreign-key descriptor rows.
   - Continue to generate one quoted SQLite `ALTER TABLE ... RENAME COLUMN`
     statement from MyLite descriptors only.

3. Tests
   - Extend the existing rename-column runtime test with keyed-table cases.
   - Cover primary-key, unique, nonunique, prefix, metadata-only fulltext, and
     supported foreign-key parent/child renames.
   - Verify descriptor metadata, `SHOW CREATE TABLE`, `SHOW INDEX`,
     `INFORMATION_SCHEMA.STATISTICS`, row readback, duplicate enforcement,
     foreign-key enforcement, reopen persistence, and physical index count.
   - Update stale rejection expectations in related tests, if any.

4. Documentation
   - Update `COMPATIBILITY.md` and `docs/compatibility/sql-table-ddl.md` only
     for the exact expanded rename-column subset.
   - Do not claim general `ALTER TABLE`, multi-action renames, algorithms,
     locks, CHECK dependency rewrites, generated columns, views, triggers, or
     broader foreign-key behavior.

5. Verification
   - Run `cmake --build --preset dev`.
   - Run the MySQL expectation script.
   - Run focused CTest entries for parser, rename-column, key/index,
     fulltext, and foreign-key lifecycles.
   - Run `cmake --workflow --preset check`.
   - Review the final diff for descriptor authority, SQLite physical safety,
     `.mylite` preamble preservation, public ABI stability, scope control,
     and test relevance.
