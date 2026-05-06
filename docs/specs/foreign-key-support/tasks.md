# Foreign Key Support Tasks

This task list breaks roadmap Task 48 into implementation slices. Every
completed item must include MySQL 8.4.9 runtime-verified expectations,
MyLite runtime tests, compatibility documentation updates, and a focused
commit.

## Foundation

- [x] Add a persistent foreign-key catalog with one row per child column part.
- [ ] Add catalog helper APIs for FK catalog names, cleanup, rename rewrites,
      and lookup by child constraint or parent unique index.
- [x] Update `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`,
      `INFORMATION_SCHEMA.KEY_COLUMN_USAGE`, and
      `INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS` to read from the FK catalog.
- [x] Render catalog-backed foreign-key lines in `SHOW CREATE TABLE`.

## `CREATE TABLE`

- [x] Copy table-level `FOREIGN KEY` clauses into create-table plans with
      constraint name, optional supporting-index name, child columns,
      referenced table, referenced columns, match option, and referential
      actions.
- [x] Generate MySQL-style unnamed FK constraint names.
- [x] Create or reuse supporting child indexes during `CREATE TABLE`.
- [x] Validate child columns, parent table, referenced columns, and referenced
      unique/primary key metadata.
- [x] Atomically insert FK catalog rows with the table, column, and index
      catalog rows.

## `ALTER TABLE`

- [x] Implement FK-only `ALTER TABLE ... ADD [CONSTRAINT] FOREIGN KEY ...`
      over supported persistent base tables.
- [x] Validate existing child rows when `foreign_key_checks = 1`.
- [x] Skip existing-row validation when `foreign_key_checks = 0`.
- [x] Implement `ALTER TABLE ... DROP FOREIGN KEY` without removing the
      supporting child index.
- [x] Preserve mixed-action ALTER atomicity when FK actions are combined with
      supported column/index actions. Mixed FK actions remain intentionally
      unsupported, but runtime tests verify that no partial column, index, or
      foreign-key metadata mutation survives.

## DML Enforcement

- [x] Enforce child-row insert checks for `INSERT ... VALUES`, `INSERT ... SET`,
      `ON DUPLICATE KEY UPDATE`, `INSERT IGNORE`, and `REPLACE`.
- [x] Enforce child-row insert checks for `INSERT ... SELECT FROM DUAL`.
- [x] Enforce child-row update checks for single-table and joined `UPDATE`.
- [x] Enforce parent-row `ON UPDATE RESTRICT` and `NO ACTION`.
- [x] Enforce parent-row `ON DELETE RESTRICT` and `NO ACTION`.
- [x] Implement `ON UPDATE CASCADE`.
- [x] Implement direct `ON DELETE CASCADE` for supported `DELETE` paths.
- [x] Implement `ON UPDATE SET NULL`.
- [x] Implement direct `ON DELETE SET NULL` for supported `DELETE` paths.
- [x] Verify and document `SET DEFAULT` behavior before enabling or rejecting
      it with a MySQL-compatible diagnostic. MyLite preserves `SET DEFAULT`
      metadata and rejects covered parent updates/deletes with 1451, matching
      observed MySQL 8.4.9 InnoDB behavior.
- [x] Apply `foreign_key_checks` to implemented child-row insert/update and
      parent restrict checks without retroactive validation on re-enable.
- [x] Apply `foreign_key_checks` to direct delete cascade and set-null
      referential actions.

## DDL Dependencies

- [x] Reject dropping a child supporting index or parent primary/unique index
      needed by a foreign key.
- [x] Reject or correctly handle dropping parent and child tables according to
      MySQL `foreign_key_checks` behavior.
- [x] Reject or correctly rewrite `RENAME TABLE` / `ALTER TABLE ... RENAME`
      for child and parent tables.
- [x] Apply MySQL-compatible foreign-key restrictions to `TRUNCATE TABLE`.
- [x] Preserve or reject column changes that affect FK child/parent columns
      with MySQL-compatible diagnostics.

## Follow-On Compatibility

- [x] Verify temporary table foreign-key behavior against MySQL 8.4.9 and add
      explicit diagnostics or support. MyLite rejects temporary child foreign
      keys with `Cannot add foreign key constraint` and treats temporary parent
      tables as unavailable to persistent child foreign keys.
- [ ] Add field-metadata coverage for FK information-schema rows when the
      unified information-schema metadata pass lands.
- [x] Revisit `REPLACE` after FK enforcement to align delete-plus-insert
      behavior. Conflict deletes now apply covered `ON DELETE CASCADE` and
      `ON DELETE SET NULL` actions without adding child side effects to
      affected rows.
- [x] Revisit `INSERT IGNORE` and `UPDATE IGNORE` after FK enforcement to align
      warning demotion behavior. Covered child-row warnings 1452 for
      `INSERT IGNORE` and single-table `UPDATE IGNORE`, parent-row warning
      1451 for single-table `UPDATE IGNORE`, skipped-row side effects, and
      `foreign_key_checks=0` bypass behavior.
