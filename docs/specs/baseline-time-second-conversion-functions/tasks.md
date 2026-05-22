# Baseline Time Second Conversion Functions Tasks

- [x] Probe MySQL 8.4.9 behavior for `TIME_TO_SEC()` and `SEC_TO_TIME()`
  scalar, descriptor-backed, warning, arity, and deferred accepted inputs.
- [x] Specify the exact supported MyLite subset and deferred MySQL surfaces.
- [x] Add a MySQL 8.4.9 expectation script for the user-visible behavior.
- [x] Add parser/AST support for `TIME_TO_SEC(expr)` and `SEC_TO_TIME(expr)`.
- [x] Extend the temporal conversion runtime for `TIME_TO_SEC()` and add a
  MyLite-owned `SEC_TO_TIME()` conversion path.
- [x] Add MyLite parser and runtime coverage.
- [x] Update compatibility docs for the exact limited conversion surface.
- [x] Run focused MySQL, build, CTest, and full workflow verification.
- [x] Review the final diff for MySQL evidence, architecture boundaries,
  SQLite pushdown, diagnostics, clipping/warning semantics, and scope control.
- [x] Commit, review with a subagent, amend if needed, and push `main`.
