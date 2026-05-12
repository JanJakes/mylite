# Baseline CHARACTER Alias Lifecycle Tasks

- [x] Research official MySQL 8.4 documentation for `CHARACTER` and
  `CHARACTER VARYING` aliases.
- [x] Verify MySQL 8.4.9 runtime behavior for normalized metadata, DML, and
  deferred forms.
- [x] Specify the narrow alias grammar, normalization, ownership boundaries,
  diagnostics, metadata, and non-goals.
- [x] Add MySQL-runtime expectation script for supported and deferred behavior.
- [x] Extend parser support for `CHARACTER`, `CHARACTER(n)`,
  `CHARACTER VARYING(n)`, and `CHAR VARYING(n)`.
- [x] Preserve normalized `CHAR` / `VARCHAR` AST and descriptor behavior with no
  alias-specific catalog storage.
- [x] Add fast C runtime tests for metadata, defaults, DML, persistence,
  diagnostics, and file-format safety.
- [x] Update `COMPATIBILITY.md` and detailed compatibility docs.
- [x] Run focused parser/runtime/MySQL expectation tests.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff for MySQL behavior, descriptor authority, physical
  SQL safety, metadata accuracy, and scope control.
