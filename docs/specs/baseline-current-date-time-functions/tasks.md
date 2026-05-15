# Baseline Current Date And Time Functions Tasks

- [x] Verify official MySQL 8.4 documentation and MySQL 8.4.9 runtime behavior
      for zero-fractional current date/time scalar, `DO`, and DML assignment
      forms.
- [x] Specify the independently authored MyLite grammar and runtime semantics.
- [x] Add MySQL expectation script coverage for the supported and deferred
      user-visible behavior.
- [ ] Extend lexer/parser/AST support for `CURDATE`, `CURRENT_DATE`,
      `CURTIME`, and `CURRENT_TIME`.
- [ ] Add runtime scalar formatting and compatible `DATE` / `TIME` DML value
      conversion.
- [ ] Add parser and runtime C tests.
- [ ] Update compatibility documentation with limited wording.
- [ ] Run focused build/tests plus `cmake --workflow --preset check`.
- [ ] Review, commit, and push the completed slice.
