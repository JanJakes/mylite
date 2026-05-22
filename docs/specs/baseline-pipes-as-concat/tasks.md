# Baseline PIPES_AS_CONCAT Tasks

- [x] Verify `PIPES_AS_CONCAT` behavior against MySQL 8.4.9 for activation,
  `ANSI`, `NULL` propagation, integer coercion, associativity, precedence,
  no-source/`DUAL`, table-backed, `DO`, warnings, and inactive-mode behavior.
- [x] Write the independently authored feature spec with MyLite grammar subset,
  ownership boundaries, diagnostics, SQLite handling, and test plan.
- [x] Add MySQL-runtime expectation script for supported and explicitly
  deferred user-visible behavior.
- [x] Add parser mode support and grammar for mode-sensitive `||`
  concatenation.
- [x] Reuse the existing row-scalar `CONCAT()` planning, SQL generation, and
  binding shape for supported `||` expression trees.
- [x] Keep inactive-mode scalar `||`, predicates, DML assignments, ordering,
  grouping, and broad expression contexts out of scope.
- [x] Add fast parser and runtime C tests.
- [x] Update compatibility docs with limited support wording.
- [x] Run focused CTest entries, the MySQL expectation script, and
  `cmake --workflow --preset check`.
- [x] Review the diff for MySQL behavior, parser mode isolation, descriptor
  authority, no row materialization, ABI stability, docs accuracy, cleanup, and
  scope control.
