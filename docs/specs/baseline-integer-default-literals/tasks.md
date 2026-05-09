# Baseline Integer Default Literals Tasks

- [x] Read project architecture, engineering standards, compatibility docs,
      existing default-null, row-values, insert, catalog, alter-table, and
      introspection sources and tests.
- [x] Verify MySQL 8.4.9 behavior for integer default literals, boolean
      defaults, omitted inserts, alter-table backfill, metadata, replacement
      definitions, diagnostics, and intentionally deferred wider syntax.
- [x] Write independently authored feature spec with MyLite Lemon-style grammar
      snippets and exact non-goals.
- [x] Add MySQL-runtime expectation script for this slice.
- [ ] Extend AST/parser support from `DEFAULT NULL` to the admitted default
      value subset.
- [ ] Add durable descriptor default metadata and catalog schema migration.
- [ ] Convert and validate defaults through MyLite-owned integer/default
      logic, preserving `CREATE TABLE IF NOT EXISTS` diagnostic ordering.
- [ ] Apply descriptor defaults to supported omitted-column `INSERT` paths.
- [ ] Preserve descriptor authority for create/add/modify/change and
      introspection.
- [ ] Add parser, catalog, runtime, persistence, migration, and unsupported
      syntax tests.
- [ ] Update compatibility docs without overclaiming full defaults.
- [ ] Run focused parser/runtime/MySQL checks and `cmake --workflow --preset check`.
- [ ] Review the final diff, fix findings, and commit atomically.
