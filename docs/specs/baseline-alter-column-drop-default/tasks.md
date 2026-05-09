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
- [x] Extend AST/parser support for the admitted `ALTER TABLE ... ALTER
      [COLUMN] ... DROP DEFAULT` subset.
- [x] Add durable dropped-default descriptor state and catalog migration.
- [x] Add catalog-only runtime mutation preserving row storage and SQLite schema
      generation.
- [x] Preserve descriptor authority for later inserts and introspection.
- [x] Add parser, catalog, runtime, persistence, migration, metadata,
      unsupported syntax, and file-format safety tests.
- [x] Update compatibility docs without overclaiming full `ALTER COLUMN`
      default support.
- [x] Run focused parser/runtime/MySQL checks and `cmake --workflow --preset check`.
- [x] Review the final diff, fix findings, and commit atomically.
