# Baseline National Character Aliases Tasks

- [x] Research official MySQL 8.4 documentation for national character aliases.
- [x] Verify MySQL 8.4.9 runtime behavior for accepted spellings, warnings,
  metadata, DML, clone behavior, and deferred forms.
- [x] Specify the narrow grammar, descriptor model, warnings, ownership
  boundaries, diagnostics, metadata, performance posture, and non-goals.
- [x] Add MySQL-runtime expectation script for supported and deferred behavior.
- [x] Extend parser support for national `CHAR` and `VARCHAR` aliases.
- [x] Preserve national logical descriptors with existing SQLite `TEXT`
  physical storage.
- [x] Add national descriptor metadata for `SHOW`, `INFORMATION_SCHEMA`, and
  public result-column metadata.
- [x] Add fast C runtime tests for warnings, metadata, defaults, DML,
  persistence, diagnostics, and file-format safety.
- [x] Update `COMPATIBILITY.md` and detailed compatibility docs.
- [x] Run focused parser/runtime/MySQL expectation tests.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff for MySQL behavior, descriptor authority,
  physical SQL safety, metadata accuracy, and scope control.
