# Baseline FROM_UNIXTIME Function Tasks

- [x] Probe MySQL 8.4.9 behavior for `FROM_UNIXTIME()` scalar, row-backed,
  time-zone, range, arity, and deferred accepted inputs.
- [x] Specify the exact supported MyLite subset and deferred MySQL surfaces.
- [x] Add a MySQL 8.4.9 expectation script for the user-visible behavior.
- [x] Add parser/AST support for `FROM_UNIXTIME(expr)`.
- [x] Add MyLite-owned scalar and row-backed runtime conversion.
- [x] Add MyLite parser and runtime coverage.
- [x] Update compatibility docs for the exact limited conversion surface.
- [x] Run focused MySQL, build, CTest, and full workflow verification.
- [x] Review the final diff for MySQL evidence, architecture boundaries,
  SQLite pushdown, diagnostics, time-zone semantics, and scope control.
- [x] Commit, review with a subagent, amend if needed, and push `main`.
