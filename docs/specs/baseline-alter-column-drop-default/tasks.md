# Baseline ALTER COLUMN DROP DEFAULT Tasks

- [x] Read project architecture, engineering standards, compatibility docs,
      existing default, catalog, alter-table, introspection, parser, and runtime
      sources and tests.
- [x] Verify MySQL 8.4.9 behavior for supported `ALTER COLUMN DROP DEFAULT`
      forms, metadata, omitted inserts, diagnostics, and intentionally deferred
      wider syntax.
- [x] Write independently authored feature spec with MyLite Lemon-style grammar
      snippets and exact non-goals.
- [x] Add MySQL-runtime expectation script for this slice.
- [ ] Extend AST/parser support for the admitted `ALTER TABLE ... ALTER
      [COLUMN] ... DROP DEFAULT` subset.
- [ ] Add durable dropped-default descriptor state and catalog migration.
- [ ] Add catalog-only runtime mutation preserving row storage and SQLite schema
      generation.
- [ ] Preserve descriptor authority for later inserts and introspection.
- [ ] Add parser, catalog, runtime, persistence, migration, metadata,
      unsupported syntax, and file-format safety tests.
- [ ] Update compatibility docs without overclaiming full `ALTER COLUMN`
      default support.
- [ ] Run focused parser/runtime/MySQL checks and `cmake --workflow --preset check`.
- [ ] Review the final diff, fix findings, and commit atomically.
