# Baseline ALTER COLUMN SET DEFAULT Tasks

- [x] Read project architecture, engineering standards, compatibility docs,
      existing default, integer-family, catalog, alter-table, introspection,
      parser, and runtime sources and tests.
- [x] Verify MySQL 8.4.9 behavior for supported `ALTER COLUMN SET DEFAULT`
      forms, metadata, omitted inserts, diagnostics, and intentionally deferred
      wider syntax.
- [x] Write independently authored feature spec with MyLite Lemon-style grammar
      snippets and exact non-goals.
- [x] Add MySQL-runtime expectation script for this slice.
- [x] Extend AST/parser support for the admitted `ALTER TABLE ... ALTER
      [COLUMN] ... SET DEFAULT ...` subset.
- [x] Convert and validate replacement defaults through MyLite-owned
      descriptor default logic.
- [x] Add catalog-only runtime mutation preserving row storage and SQLite schema
      generation.
- [x] Preserve descriptor authority for later inserts and introspection.
- [x] Add parser and runtime tests for success, diagnostics, persistence,
      metadata, unsupported syntax, and file-format safety.
- [x] Update compatibility docs without overclaiming full `ALTER COLUMN`
      default support.
- [x] Run focused parser/runtime/MySQL checks and `cmake --workflow --preset check`.
- [x] Review the final diff, fix findings, and commit atomically.
