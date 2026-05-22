# Baseline Current Date And Time Defaults Tasks

- [x] Audit existing current date/time function support, current timestamp
      defaults, date/time descriptors, generated default rendering, and default
      materialization paths.
- [x] Verify MySQL 8.4.9 behavior for current date/time defaults, metadata,
      DML materialization, `ALTER TABLE`, `CREATE TABLE ... LIKE`, and
      unsupported syntax.
- [x] Write the independently authored feature specification.
- [x] Add a MySQL-runtime expectation script for the supported subset.
- [x] Bump the catalog schema and persist current-date/current-time generated
      default kinds.
- [x] Implement parser/runtime validation for parenthesized generated date/time
      defaults without admitting nonparenthesized forms.
- [x] Implement descriptor rendering for `SHOW`, `DESCRIBE`,
      `INFORMATION_SCHEMA.COLUMNS`, and `SHOW CREATE TABLE`.
- [x] Implement omitted-column, explicit `DEFAULT`, `ALTER TABLE ADD COLUMN`
      backfill, and `ALTER TABLE ALTER COLUMN SET DEFAULT` materialization.
- [x] Update compatibility docs with limited wording.
- [x] Add focused fast C runtime tests and register any new test binary.
- [x] Run focused MySQL expectations, build, focused CTest entries, and full
      `cmake --workflow --preset check`.
- [x] Review, amend if needed, commit, push, and continue to the next priority
      baseline slice.
