# Baseline DATE_FORMAT Function Tasks

- [x] Verify MySQL 8.4.9 behavior for supported and deferred DATE_FORMAT forms.
- [x] Write the independently authored feature specification.
- [x] Add a MySQL-runtime expectation script for the supported and deferred
      surface.
- [ ] Add lexer/parser/AST support for `DATE_FORMAT()`.
- [ ] Implement a MyLite-owned DATE_FORMAT formatter and SQLite scalar callback.
- [ ] Integrate no-source, `DUAL`, `DO`, and row-scalar SELECT execution.
- [ ] Add fast C runtime and parser tests.
- [ ] Update compatibility documentation with exact limited support.
- [ ] Run focused MySQL expectation, build, focused CTest entries, and full
      `cmake --workflow --preset check`.
- [ ] Review the final diff, fix findings, commit, and push.
