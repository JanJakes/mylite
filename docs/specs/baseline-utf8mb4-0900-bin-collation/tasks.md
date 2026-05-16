# Baseline utf8mb4 0900 Binary Collation Tasks

- [x] Inspect the existing charset/collation catalog, table/column metadata,
      `SET NAMES`, result metadata, and static `INFORMATION_SCHEMA` paths.
- [x] Verify MySQL 8.4.9 behavior for `utf8mb4_0900_bin` catalog rows,
      session readback, DDL metadata, and incompatible charset diagnostics.
- [x] Write the independently authored feature specification.
- [x] Add a MySQL-runtime expectation script for the admitted behavior.
- [x] Admit `utf8mb4_0900_bin` in the central `utf8mb4` collation descriptor
      table.
- [x] Preserve and render table and column metadata through existing
      descriptor paths.
- [x] Expose public result metadata collation id `309` and binary collation
      flag behavior.
- [x] Update compatibility documentation with limited wording.
- [x] Extend fast C tests for `SHOW COLLATION`, `INFORMATION_SCHEMA`,
      `SET NAMES`, table/column metadata, result metadata, persistence, and
      diagnostics.
- [x] Run focused MySQL expectations, focused CTests, `cmake --build --preset
      dev`, and `cmake --workflow --preset check`.
- [x] Review with a subagent, amend if needed, commit, push to `origin/main`,
      and continue to the next priority baseline slice.
