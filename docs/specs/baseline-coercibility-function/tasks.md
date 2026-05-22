# Baseline COERCIBILITY Function Tasks

- [x] Verify `COERCIBILITY()` behavior against MySQL 8.4.9 for supported
  literals, scalar functions, binary casts/conversions, `CONCAT()` inputs,
  descriptor-backed columns, `DUAL`, `DO`, warnings, and argument-count errors.
- [x] Write the independently authored feature spec with MyLite grammar subset,
  ownership boundaries, diagnostics, SQLite handling, and test plan.
- [x] Add MySQL-runtime expectation script for supported and explicitly
  deferred user-visible behavior.
- [x] Add parser/AST support for exact one-argument `COERCIBILITY()`.
- [x] Reuse and extend the existing metadata-function planning path without
  changing public API or SQLite fork code.
- [x] Add fast parser and runtime C tests.
- [x] Update compatibility docs with limited support wording.
- [x] Run focused CTest entries, the MySQL expectation script, and
  `cmake --workflow --preset check`.
- [x] Review the diff for MySQL behavior, descriptor authority, ABI stability,
  docs accuracy, cleanup, and scope control.
