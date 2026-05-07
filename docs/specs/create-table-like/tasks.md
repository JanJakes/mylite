# CREATE TABLE ... LIKE tasks

- [x] Verify MySQL 8.4.9 runtime behavior for metadata cloning, temporary
      source/destination handling, foreign-key omission, check-name generation,
      auto-increment state, duplicate targets, same-name aliases, and missing
      schema/table errors.
- [x] Add parser and AST support for `CREATE [TEMPORARY] TABLE
      [IF NOT EXISTS] target LIKE source`.
- [x] Add execution support for catalog-backed persistent and temporary table
      cloning.
- [x] Clone table, column, index, and check metadata while omitting foreign-key
      metadata.
- [x] Add runtime tests comparing MySQL-derived result sets, metadata, errors,
      warnings, and side effects.
- [x] Update `COMPATIBILITY.md` and related specs.
- [x] Run focused tests, full tests, format checks, and static checks; fix
      root causes for failures.
