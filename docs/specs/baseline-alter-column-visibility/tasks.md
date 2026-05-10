# Baseline ALTER COLUMN Visibility Tasks

- [x] Read project architecture, engineering standards, compatibility docs,
      existing alter-table/default/catalog/DML/introspection specs, and related
      parser/runtime tests.
- [x] Verify MySQL 8.4.9 behavior for supported `ALTER COLUMN SET
      VISIBLE|INVISIBLE` forms, metadata, DML effects, diagnostics, and
      intentionally deferred wider syntax.
- [x] Write independently authored feature spec with MyLite Lemon-style grammar
      snippets and exact non-goals.
- [x] Add MySQL-runtime expectation script for this slice.
- [ ] Extend AST/parser support for the admitted visibility subset.
- [ ] Add durable column visibility descriptor state and catalog migration.
- [ ] Add catalog-only runtime mutation preserving row storage and SQLite schema
      generation.
- [ ] Preserve descriptor authority for `SELECT *`, implicit inserts, explicit
      DML references, and introspection.
- [ ] Add parser, catalog, runtime, persistence, migration, metadata,
      unsupported syntax, and file-format safety tests.
- [ ] Update compatibility docs without overclaiming full invisible-column
      support.
- [ ] Run focused parser/runtime/MySQL checks and `cmake --workflow --preset check`.
- [ ] Review the final diff, fix findings, and commit atomically.
