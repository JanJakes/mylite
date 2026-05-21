# Baseline Spatial Index Type Options Tasks

- [x] Verify official MySQL 8.4 documentation context and MySQL 8.4.9 runtime
  behavior for ordinary `USING RTREE`, `USING BTREE`, and `USING HASH` around
  spatial indexes.
- [x] Specify the limited grammar, descriptor ownership, diagnostics, warning
  behavior, catalog semantics, physical SQLite boundary, and unsupported
  surfaces.
- [x] Extend the MySQL-runtime expectation script for RTREE normalization and
  BTREE/HASH/RTREE diagnostics.
- [x] Extend analyzer/planner/runtime option handling for RTREE and spatial
  type-option diagnostics without broadening explicit `SPATIAL ... USING`.
- [x] Add fast C runtime coverage for success paths, diagnostics, metadata,
  warning counts, and physical SQLite index preservation.
- [x] Update `COMPATIBILITY.md` and index compatibility docs with exact limited
  wording.
- [x] Run focused parser/runtime tests and the MySQL expectation script.
- [x] Run `cmake --build --preset dev` and `cmake --workflow --preset check`.
- [x] Review the final diff for descriptor authority, MySQL behavior, warning
  counts, physical SQLite boundaries, scope control, docs, and test relevance.
