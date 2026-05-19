# Baseline TIME Function Tasks

- [x] Probe MySQL 8.4.9 behavior for `TIME()` scalar, descriptor-backed,
  warning, syntax-error, and deferred accepted inputs.
- [x] Specify the exact supported MyLite subset and deferred MySQL surfaces.
- [x] Add a MySQL 8.4.9 expectation script for the user-visible behavior.
- [x] Add parser/AST support for `TIME(expr)`.
- [x] Extend the temporal extraction runtime for string-valued time extraction.
- [x] Add MyLite parser and runtime coverage.
- [x] Update compatibility docs for the exact limited `TIME()` surface.
- [x] Run focused MySQL, build, CTest, and full workflow verification.
- [x] Review the final diff for MySQL evidence, architecture boundaries,
  SQLite pushdown, diagnostics, and scope control.
