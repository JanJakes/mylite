# Baseline Explicit DEFAULT NULL Tasks

- [x] Read project architecture, engineering standards, compatibility docs,
      existing parser/runtime/catalog/alter-table sources, and current tests.
- [x] Verify MySQL 8.4.9 behavior for explicit nullable `DEFAULT NULL`,
      `NOT NULL DEFAULT NULL`, alter-table forms, row effects, warnings, and
      intentionally deferred wider syntax.
- [x] Write independently authored feature spec with MyLite Lemon-style grammar
      snippets and exact non-goals.
- [x] Add MySQL-runtime expectation script for this slice.
- [x] Extend AST/parser support for `DEFAULT NULL` after current nullability.
- [x] Validate `NOT NULL DEFAULT NULL` with MySQL-compatible diagnostics before
      catalog or SQLite mutation.
- [x] Preserve descriptor/catalog authority and avoid adding general default
      metadata.
- [x] Add parser and runtime tests for create/add/modify/change paths,
      introspection, persistence, diagnostics, and unsupported syntax.
- [x] Update compatibility docs without overclaiming full defaults.
- [x] Run focused parser/runtime/MySQL checks and `cmake --workflow --preset check`.
- [x] Review the final diff, fix findings, and commit atomically.
