# Baseline VARCHAR Length Lifecycle Tasks

- [x] Research official MySQL 8.4 documentation for `VARCHAR` length, row-size,
  storage, and key-length boundaries.
- [x] Verify MySQL 8.4.9 runtime behavior for wider `VARCHAR` descriptors,
  metadata, aliases, row-size failures, and key-boundary gaps.
- [x] Specify the narrow widened descriptor lifecycle, row-size ownership,
  diagnostics, metadata, key boundary, and non-goals.
- [ ] Add MySQL-runtime expectation script for supported and deferred behavior.
- [ ] Raise non-key `VARCHAR` descriptor support to `0..16383`.
- [ ] Add descriptor-owned row-size validation for `CREATE TABLE` and
  `ALTER TABLE ... ADD COLUMN`.
- [ ] Keep current ASCII string-key support capped at `CHAR` /
  `VARCHAR(1..255)`.
- [ ] Add fast C runtime tests for wide metadata, DML, persistence, row-size
  diagnostics, key-boundary diagnostics, and file-format safety.
- [ ] Update `COMPATIBILITY.md` and detailed compatibility docs.
- [ ] Run focused parser/runtime/MySQL expectation tests.
- [ ] Run `cmake --workflow --preset check`.
- [ ] Review the final diff for MySQL behavior, descriptor authority, row-size
  accounting, key-scope control, physical SQL safety, metadata accuracy, and
  scope control.
