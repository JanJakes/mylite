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
- [x] Extend AST/parser support from `DEFAULT NULL` to the admitted default
      value subset.
- [x] Add durable descriptor default metadata and catalog schema migration.
- [x] Convert and validate defaults through MyLite-owned integer/default
      logic, preserving `CREATE TABLE IF NOT EXISTS` diagnostic ordering.
- [x] Apply descriptor defaults to supported omitted-column `INSERT` paths.
- [x] Preserve descriptor authority for create/add/modify/change and
      introspection.
- [x] Add parser, catalog, runtime, persistence, migration, and unsupported
      syntax tests.
- [x] Update compatibility docs without overclaiming full defaults.
- [x] Run focused parser/runtime/MySQL checks and `cmake --workflow --preset check`.
- [x] Review the final diff, fix findings, and commit atomically.
