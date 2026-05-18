# Baseline UNIX_TIMESTAMP Function Tasks

- [x] Verify MySQL 8.4.9 behavior for no-argument, literal, descriptor-column,
      time-zone, warning, and deferred forms.
- [x] Add the MySQL-runtime expectation script for the supported and deferred
      surface.
- [x] Add lexer/parser/AST support for `UNIX_TIMESTAMP()` and arity errors.
- [x] Implement MyLite-owned Unix timestamp conversion and SQLite scalar
      callback registration.
- [x] Add scalar and row-scalar runtime planning/execution support.
- [x] Add parser and runtime C tests.
- [x] Update compatibility docs with limited wording.
- [x] Run focused tests, MySQL expectation scripts, and
      `cmake --workflow --preset check`.
- [x] Review the diff for scope, warnings, time-zone behavior, descriptor
      authority, performance, file-format safety, and cleanup paths.
