# Baseline Qualified Wildcard SELECT Tasks

- [x] Verify MySQL 8.4.9 runtime behavior for `table.*`, `schema.table.*`,
  `alias.*`, invisible-column omission, mixed qualified wildcard projection,
  joined source expansion, alias hiding, diagnostics, warning count, and
  `ROW_COUNT()`.
- [x] Write the independently authored feature specification.
- [x] Add the MySQL expectation artifact for qualified wildcard projection.
- [x] Add parser/AST support for qualified wildcard select items.
- [x] Extend descriptor-driven projection planning to expand matched source
  visible columns.
- [x] Extend fast parser and runtime C coverage.
- [x] Update compatibility docs and older qualified-column/join specs so they
  no longer claim qualified wildcards are wholly out of scope.
- [x] Run focused build and CTest entries.
- [x] Run the MySQL 8.4.9 expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff for descriptor authority, source resolution,
  visible-column expansion, no SQLite fork changes, scope control, docs, and
  test relevance.
