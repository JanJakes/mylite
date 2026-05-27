# Baseline MySQL Charset And Collation Catalog Tasks

## Goal

Expose MySQL 8.4.9 character-set and collation catalog metadata through the
existing `SHOW` and `INFORMATION_SCHEMA` surfaces without broadening MyLite's
actual charset/collation semantics.

## Tasks

1. Design and expectations
   - [x] Verify MySQL 8.4.9 row counts, representative rows, and catalog
         hashes for character sets, collations, and applicability mappings.
   - [x] Specify the metadata-only catalog boundary and the separate semantic
         support boundary.
   - [x] Add a reproducible MySQL expectation script for the expanded catalog.

2. Runtime metadata
   - [x] Add full MySQL 8.4.9 catalog row arrays for character sets and
         collations.
   - [x] Use the full catalog arrays for `SHOW CHARACTER SET`,
         `SHOW COLLATION`, `INFORMATION_SCHEMA.CHARACTER_SETS`,
         `INFORMATION_SCHEMA.COLLATIONS`, and
         `INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY`.
   - [x] Keep DDL/runtime validation on the existing narrow supported
         charset/collation arrays.

3. Tests
   - [x] Update `runtime_show_character_set_collation` for full counts and
         representative rows.
   - [x] Update `runtime_information_schema_static_catalogs` for full counts,
         representative predicates/order, and applicability count.
   - [x] Add DDL boundary checks proving catalog-only rows are still rejected.
   - [x] Update or add MySQL expectation scripts.

4. Documentation
   - [x] Update `COMPATIBILITY.md`.
   - [x] Update `docs/compatibility/sql-show-statements.md`.
   - [x] Update `docs/compatibility/metadata-information-schema.md`.
   - [x] Update `docs/compatibility/character-sets.md` and
         `docs/compatibility/collations.md` without overclaiming semantic
         support.

5. Verification and review
   - [x] Run `cmake --build --preset dev`.
   - [x] Run focused charset/collation and information-schema CTests.
   - [x] Run the MySQL expectation script.
   - [x] Run `cmake --workflow --preset check`.
   - [x] Use a review subagent and fix real findings.
   - [x] Commit and push to `origin/main`.
