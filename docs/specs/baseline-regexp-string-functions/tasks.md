# Baseline REGEXP String Functions Tasks

- [x] Create an independently authored feature specification for the limited
  required-argument `REGEXP_INSTR()`, `REGEXP_SUBSTR()`, and
  `REGEXP_REPLACE()` baseline.
- [x] Verify representative MySQL 8.4.9 behavior for required arguments,
  no-match results, `NULL` propagation, default case-insensitive matching,
  zero-length matches, replacement-all behavior, and empty-pattern errors.
- [x] Extend the lexer, parser, AST names, and parser tests for the three
  function names and deterministic wrong/deferred arities.
- [x] Extend the regex module with match-span discovery and private SQLite
  scalar helpers for the three functions.
- [x] Add scalar no-source/`DUAL`/`DO` execution and result metadata.
- [x] Add single-table descriptor-backed row-scalar planning, SQLite SQL
  generation, parameter binding, and metadata.
- [x] Add MySQL expectation artifacts and focused C runtime tests.
- [x] Update compatibility docs for the exact limited supported surface.
- [x] Run the focused parser/runtime tests, MySQL expectation script, full dev
  build, and `cmake --workflow --preset check`.
- [x] Review the final diff for architecture boundaries, MySQL evidence,
  SQLite-extension use, memory cleanup, deterministic diagnostics, and scope.
