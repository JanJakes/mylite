# Baseline GET_FORMAT Function Tasks

- [x] Verify MySQL 8.4.9 behavior for supported and deferred `GET_FORMAT()`
      forms.
- [x] Write the independently authored feature specification.
- [x] Add a MySQL-runtime expectation script for the supported and deferred
      surface.
- [ ] Add lexer/parser/AST support for `GET_FORMAT()`.
- [ ] Integrate constant scalar evaluation with existing no-source, `DUAL`,
      `DO`, row-scalar projection, `DATE_FORMAT()`, `TIME_FORMAT()`, and
      `STR_TO_DATE()` paths.
- [ ] Add fast C runtime and parser tests.
- [ ] Update compatibility documentation with exact limited support.
- [ ] Run focused MySQL expectation, build, focused CTest entries, and full
      `cmake --workflow --preset check`.
- [ ] Review the final diff, fix findings, commit, and push.
