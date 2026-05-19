# Baseline UTC Date And Time Functions Tasks

- [x] Verify official MySQL 8.4 documentation and MySQL 8.4.9 runtime behavior
      for zero-fractional UTC date/time scalar, `DO`, and DML assignment forms.
- [x] Specify the independently authored MyLite grammar and runtime semantics.
- [x] Add MySQL expectation script coverage for supported and deferred
      user-visible behavior.
- [ ] Extend lexer/parser/AST support for `UTC_DATE`, `UTC_TIME`, and
      `UTC_TIMESTAMP`.
- [ ] Add runtime UTC scalar formatting and compatible `DATE` / `TIME` /
      `DATETIME` / `TIMESTAMP` DML value conversion.
- [ ] Add parser and runtime C tests.
- [ ] Update compatibility documentation with limited wording.
- [ ] Run focused build/tests plus `cmake --workflow --preset check`.
- [ ] Review, commit, and push the completed slice.
