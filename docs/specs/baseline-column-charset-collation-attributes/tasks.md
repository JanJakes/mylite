# Baseline Column Charset and Collation Attributes Tasks

- [x] Audit existing table charset/collation, legacy collation, character
      alias, text/string descriptor, result metadata, `CREATE TABLE LIKE`,
      CTAS, temporary table, and ALTER column code paths.
- [x] Verify MySQL 8.4.9 behavior for admitted column charset/collation
      syntax, metadata, cloning, CTAS, ALTER, and unsupported syntax.
- [x] Write the independently authored feature specification.
- [x] Add a MySQL-runtime expectation script for the supported and explicitly
      deferred behavior.
- [x] Add parser/AST support for the narrow column charset/collation attribute
      grammar with MySQL-compatible ordering limits.
- [x] Bump the catalog schema and persist explicit column charset/collation
      descriptor fields.
- [x] Implement runtime validation, canonicalization, descriptor derivation,
      cloning, CTAS inference, and ALTER replacement behavior.
- [x] Render effective metadata in `SHOW CREATE TABLE`, `SHOW COLUMNS`,
      `SHOW FULL COLUMNS`, `INFORMATION_SCHEMA.COLUMNS`, and public result
      metadata.
- [x] Preserve physical SQLite storage and generated SQL boundaries without
      adding SQLite collation hooks.
- [x] Update compatibility docs with limited wording.
- [x] Add focused fast C parser/runtime/result metadata tests and register any
      new test binary.
- [x] Run focused MySQL expectations, build, focused CTest entries, and full
      `cmake --workflow --preset check`.
- [x] Review, amend if needed, commit, push, and continue to the next priority
      baseline slice.
