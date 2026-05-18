# Baseline Insert Select Keyed Targets Tasks

- [x] Read project architecture, existing insert-select, primary-key,
  unique-index, foreign-key, auto-increment, compatibility, parser, runtime,
  and test context.
- [x] Research official MySQL 8.4 `INSERT`, `INSERT ... SELECT`, and
  `AUTO_INCREMENT` documentation.
- [x] Probe MySQL 8.4.9 runtime behavior for keyed table-backed
  `INSERT ... SELECT`, duplicate rollback, `IGNORE` demotion, foreign-key child
  checks, same-table sources, and auto-increment state.
- [x] Write the independently authored feature spec with ownership boundaries,
  runtime semantics, diagnostics, performance notes, and test plan.
- [x] Add a MySQL-runtime expectation script for the newly admitted behavior.
- [x] Update compatibility documentation for the exact limited subset.
- [ ] Reuse the normal insert target metadata in table-backed
  `INSERT ... SELECT` planning.
- [ ] Route streamed table-backed selected rows through the existing
  constraint-aware insert row path.
- [ ] Preserve statement-local auto-increment allocation, durable counter
  update, and `LAST_INSERT_ID()` semantics for generated rows.
- [ ] Add focused runtime C tests for keyed targets, `IGNORE`, auto-increment,
  persistence, preamble safety, and independent handles.
- [ ] Run focused insert-select, primary-key, unique-index, foreign-key,
  auto-increment, parser, and runtime tests.
- [ ] Run the MySQL expectation script against MySQL 8.4.9.
- [ ] Run `cmake --workflow --preset check`.
- [ ] Review the final diff for MySQL behavior, descriptor authority,
  performance, cleanup on failure, file-format safety, scope control, and
  compatibility accuracy.
- [ ] Commit and push the completed feature.
