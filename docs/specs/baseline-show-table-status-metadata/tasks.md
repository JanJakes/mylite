# Baseline SHOW TABLE STATUS Metadata Tasks

## Goal

Extend the existing descriptor-driven table-status baseline with durable
creation/update timestamp metadata and descriptor-derived secondary-index
length placeholders for persistent base tables.

## Tasks

1. Design and expectations
   - Create `docs/specs/baseline-show-table-status-metadata/specs.md`.
   - Record MySQL 8.4.9 runtime observations for creation time, update time,
     session time-zone rendering, `SET timestamp`, and secondary-index
     `Index_length`.
   - Specify catalog ownership, migration behavior, DML side effects, status
     row values, and intentionally deferred full statistics behavior.

2. Catalog
   - Add persistent table descriptor fields for `created_time_utc_epoch` and
     `updated_time_utc_epoch`.
   - Bump the MyLite catalog schema version and add a one-step migration for
     existing catalogs.
   - Populate new table descriptors with the current creation timestamp.
   - Add an internal catalog update helper for row-change timestamp touches.

3. Runtime
   - Format descriptor timestamps in the session time zone for
     `SHOW TABLE STATUS` and `INFORMATION_SCHEMA.TABLES`.
   - Share table-status formatting where practical to keep those surfaces in
     sync.
   - Touch persistent base-table update time after successful row-changing
     DML, and on `TRUNCATE TABLE`.
   - Derive `Index_length` from non-primary index descriptors without row
     scans or SQLite metadata inspection.

4. Tests
   - Extend the existing fast C table-status/introspection tests or add a
     focused C test under `packages/libmylite/tests/`.
   - Cover timestamps, time-zone rendering, row-changing DML touches,
     zero-affected statements, reopen persistence, rename preservation,
     primary-only and secondary-index length, and
     `INFORMATION_SCHEMA.TABLES` consistency.
   - Add or update a MySQL 8.4.9 expectation script for the new observed
     metadata behavior.

5. Documentation
   - Update `COMPATIBILITY.md` and relevant compatibility detail docs with
     limited wording only.
   - Keep unsupported full statistics, views, temporary table status rows,
     `SHOW TABLE STATUS ... WHERE`, comments, privileges, and complete InnoDB
     timestamp semantics explicit.

6. Verification and review
   - Run `cmake --build --preset dev`.
   - Run the new/updated MySQL expectation script.
   - Run focused CTest entries for parser, table status, information schema,
     DML lifecycle, indexes, auto-increment, and truncate.
   - Run `cmake --workflow --preset check`.
   - Use a review subagent before committing; fix or amend any real gaps.
   - Commit atomically and push `main` to the remote.
