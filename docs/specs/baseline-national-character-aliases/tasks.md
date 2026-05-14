# Baseline National Character Aliases Tasks

- [x] Research official MySQL 8.4 documentation for national character aliases.
- [x] Verify MySQL 8.4.9 runtime behavior for accepted spellings, warnings,
  metadata, DML, clone behavior, and deferred forms.
- [x] Specify the narrow grammar, descriptor model, warnings, ownership
  boundaries, diagnostics, metadata, performance posture, and non-goals.
- [x] Add MySQL-runtime expectation script for supported and deferred behavior.
- [ ] Extend parser support for national `CHAR` and `VARCHAR` aliases.
- [ ] Preserve national logical descriptors with existing SQLite `TEXT`
  physical storage.
- [ ] Add national descriptor metadata for `SHOW`, `INFORMATION_SCHEMA`, and
  public result-column metadata.
- [ ] Add fast C runtime tests for warnings, metadata, defaults, DML,
  persistence, diagnostics, and file-format safety.
- [ ] Update `COMPATIBILITY.md` and detailed compatibility docs.
- [ ] Run focused parser/runtime/MySQL expectation tests.
- [ ] Run `cmake --workflow --preset check`.
- [ ] Review the final diff for MySQL behavior, descriptor authority,
  physical SQL safety, metadata accuracy, and scope control.
