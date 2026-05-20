# Baseline Binary Full-Column Indexes Tasks

- [x] Verify MySQL 8.4.9 behavior for full `BINARY` / `VARBINARY` key parts,
      BLOB full-key diagnostics, key-length boundaries, metadata, and duplicate
      diagnostics.
- [x] Write the independently authored feature specification.
- [x] Add MySQL-runtime expectation coverage for the admitted behavior.
- [x] Extend descriptor validation to admit full `BINARY` / `VARBINARY`
      secondary and unique key parts while preserving BLOB prefix requirements.
- [x] Preserve generated SQLite index SQL without text collation on binary key
      parts.
- [x] Adjust duplicate-key formatting for full binary key parts and fixed
      `BINARY` trailing NUL display.
- [x] Add focused C runtime coverage and CMake/CTest registration.
- [x] Update compatibility docs for the exact supported subset.
- [x] Run focused tests, MySQL expectation script, and the full check workflow.
