# Baseline Key-Aware ALTER CHANGE/MODIFY Tasks

## Goal

Allow the existing single-action descriptor-driven
`ALTER TABLE ... CHANGE [COLUMN]` and `ALTER TABLE ... MODIFY [COLUMN]`
lifecycles to operate on keyed persistent base tables when existing key
descriptors remain valid after the replacement.

## Tasks

1. Design and evidence
   - Create the feature spec and task list.
   - Record official MySQL 8.4 sources and MySQL 8.4.9 runtime observations.
   - Capture expectation probes for keyed integer rebuilds, indexed-column
     renames, primary-key nullability, prefix-key compatibility, affected rows,
     warnings, row preservation, and introspection.

2. Planner and validation
   - Remove the broad primary-key and secondary-index table rejection from
     `CHANGE`/`MODIFY`.
   - Load existing key descriptors from the catalog.
   - Apply MySQL primary-key nullability behavior for omitted versus explicit
     `NULL` replacement attributes.
   - Validate every existing key descriptor against the resulting column
     descriptors before generated SQLite SQL.
   - Keep CHECK and foreign-key constrained target tables out of scope.

3. Runtime and physical storage
   - Preserve metadata-only and pure-rename paths.
   - Recreate non-fulltext physical indexes after descriptor-driven table
     rebuilds.
   - Keep generated SQLite SQL descriptor-built, quoted, and based on stable
     physical names.
   - Preserve `.mylite` preamble and shifted SQLite payload invariants.

4. Tests and docs
   - Add MySQL 8.4.9 expectation script.
   - Extend fast C runtime tests for keyed modify/change success, key metadata,
     physical index recreation, persistence, and diagnostics.
   - Update `COMPATIBILITY.md` and `docs/compatibility/sql-table-ddl.md` only
     for the exact supported subset.

5. Verification and review
   - Run `cmake --build --preset dev`.
   - Run focused CTest entries for parser and ALTER lifecycle coverage.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.
   - Review the final diff for MySQL behavior, catalog authority, physical
     index recreation, descriptor compatibility, affected rows, cleanup,
     file-format safety, and scope control.
