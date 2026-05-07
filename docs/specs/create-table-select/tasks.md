# CREATE TABLE ... SELECT tasks

- [x] Verify MySQL 8.4.9 runtime behavior for row counts, metadata inference,
      default handling, duplicate output names, temporary destinations,
      `IF NOT EXISTS` ordering, and transaction boundaries.
- [x] Add parser and AST support for `CREATE [TEMPORARY] TABLE
      [IF NOT EXISTS] target [AS] SELECT ...`.
- [x] Clone SELECT AST/text into the prepared CTAS statement.
- [x] Infer CTAS target columns from SELECT metadata and preserve source-column
      metadata for direct selected columns.
- [x] Execute CTAS atomically by creating the physical table, inserting SELECT
      rows, and inserting catalog rows.
- [x] Add MySQL-derived parser/runtime tests for supported CTAS behavior and
      deterministic deferred-definition diagnostics.
- [x] Update `COMPATIBILITY.md` and related roadmap/spec docs.
- [x] Run focused tests, full tests, format checks, and targeted CTAS
      clang-tidy; `cmake --workflow --preset check` still fails in the
      repository-wide tidy step because of the known local SDK header lookup
      issue plus pre-existing unrelated tidy findings outside this slice.
