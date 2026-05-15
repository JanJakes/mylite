# Baseline Descending Index Key Parts Tasks

- [x] Review existing primary, secondary, unique, prefix, `CREATE INDEX`,
  `ALTER TABLE ADD INDEX`, `SHOW INDEX`, `SHOW CREATE TABLE`, and
  `INFORMATION_SCHEMA.STATISTICS` implementation paths.
- [x] Verify MySQL 8.4.9 behavior for key-part direction syntax, rendering,
  metadata, `CREATE TABLE ... LIKE`, and representative diagnostics.
- [x] Write the independently authored feature specification with MyLite grammar
  snippets and ownership boundaries.
- [x] Add MySQL-runtime expectation script for descending key-part behavior.
- [ ] Add parser support for optional `ASC` / `DESC` on supported key parts.
- [ ] Add durable catalog descriptor support for index-column sort direction,
  including migration and temporary catalog mirroring.
- [ ] Preserve direction through create-time, alter-time, standalone create,
  create-like, reload, and drop paths.
- [ ] Render direction through `SHOW CREATE TABLE`, `SHOW INDEX`, and limited
  `INFORMATION_SCHEMA.STATISTICS`.
- [ ] Generate descriptor-driven SQLite physical indexes with `DESC` only on
  descending index terms.
- [ ] Add focused parser and runtime C coverage for success, metadata,
  persistence, diagnostics, and unchanged duplicate enforcement.
- [ ] Update `COMPATIBILITY.md` and detailed compatibility docs for the exact
  limited descending-index subset.
- [ ] Run MySQL expectations, focused parser/runtime CTests, and
  `cmake --workflow --preset check`.
- [ ] Review the final diff for MySQL evidence, catalog authority, physical SQL
  safety, migration compatibility, performance scope, docs accuracy, and
  focused tests.
