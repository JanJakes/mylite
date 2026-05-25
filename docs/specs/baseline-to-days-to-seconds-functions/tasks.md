# Baseline TO_DAYS And TO_SECONDS Functions Tasks

- [x] Verify MySQL 8.4.9 behavior for supported scalar, `DUAL`, `DO`, and
      row-backed projection cases.
- [x] Add MySQL expectation script for the supported behavior and deferred
      surface.
- [x] Extend lexer/parser/AST support for `TO_DAYS()` and `TO_SECONDS()`,
      including argument-count diagnostics and identifier fallback.
- [x] Extend MyLite-owned temporal runtime evaluation for scalar and row-backed
      `TO_DAYS()` / `TO_SECONDS()`.
- [x] Add fast C runtime and parser coverage.
- [x] Update compatibility documentation for the exact supported subset.
- [x] Run focused parser/runtime tests and the MySQL expectation script.
- [x] Run `cmake --build --preset dev` and `cmake --workflow --preset check`.
- [x] Review architecture boundaries, warning behavior, calendar arithmetic,
      cleanup, and scope control before committing.
