# Baseline Composite Foreign Key Constraints Tasks

## Design

- [x] Read the existing one-column FK, composite primary-key, composite
  unique-index, secondary-index, DML, metadata, catalog, storage, and SQLite
  integration guidance.
- [x] Verify MySQL 8.4.9 behavior for composite FK DDL, metadata, NULL tuple
  semantics, child index creation/reuse, enforcement, and diagnostics.
- [x] Specify the independently authored MyLite grammar, descriptor semantics,
  enforcement strategy, diagnostics, compatibility gaps, and test plan.
- [x] Add the MySQL 8.4.9 expectation script for the design surface.

## Implementation

- [ ] Admit composite integer-family FK column lists in create-table planning
  and named single-action `ALTER TABLE ... ADD CONSTRAINT ... FOREIGN KEY`.
- [ ] Reuse existing child indexes whose leftmost full key parts match the FK
  child columns; otherwise auto-create a nonunique child index with MySQL-like
  naming.
- [ ] Resolve parent composite primary/unique descriptors in order and reject
  count mismatches, missing keys, incompatible descriptors, duplicate names,
  reserved names, unknown tables, and unknown columns deterministically.
- [ ] Store one foreign-key-column descriptor per key part using the existing
  catalog tables, preserving ordinal and parent-key positions.
- [ ] Enforce child-side and parent-side checks with descriptor-built,
  parameterized SQLite `EXISTS` / `NOT EXISTS` probes that skip child tuples
  containing any `NULL` part.
- [ ] Preserve one-column FK behavior, drop-FK behavior, descriptor authority,
  catalog generation, `.mylite` preamble safety, reopen persistence, and
  independent handle behavior.
- [ ] Render composite FK metadata in `SHOW CREATE TABLE`, `SHOW INDEX`,
  `INFORMATION_SCHEMA.STATISTICS`, `TABLE_CONSTRAINTS`,
  `KEY_COLUMN_USAGE`, and `REFERENTIAL_CONSTRAINTS`.

## Tests and Docs

- [ ] Add focused C parser/runtime tests for supported and rejected behavior.
- [ ] Update `COMPATIBILITY.md` and detailed compatibility docs only for the
  implemented subset.
- [ ] Run the new MySQL expectation script and focused CTest entries.
- [ ] Run `cmake --workflow --preset check`.
- [ ] Review the final diff for architecture boundaries, MySQL behavior,
  descriptor authority, set-based enforcement, metadata accuracy, file-format
  safety, performance, and scope control.
