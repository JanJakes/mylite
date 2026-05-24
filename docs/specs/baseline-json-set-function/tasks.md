# Baseline JSON_SET Function Tasks

- [x] Verify MySQL 8.4.9 behavior for argument count, NULL handling, root
  replacement, duplicate paths, object insertion, array append, scalar autowrap,
  invalid documents, invalid paths, wildcard paths, and table-backed JSON/string
  value conversion.
- [x] Specify the independently authored MyLite grammar, semantics, diagnostics,
  architecture boundaries, limitations, and test plan.
- [x] Add MySQL-runtime expectation artifact for the admitted behavior.
- [x] Extend lexer/parser/AST support for `JSON_SET`.
- [x] Implement `mylite_json_set()` in the MyLite JSON runtime.
- [x] Register the private SQLite scalar `_mylite_json_set`.
- [x] Add no-source/DUAL/DO evaluation and row-scalar planning/lowering/binding.
- [x] Add parser and runtime C tests.
- [x] Update `COMPATIBILITY.md` and JSON compatibility detail docs.
- [x] Run focused tests, MySQL expectation script, and `cmake --workflow --preset check`.
- [x] Review the feature, amend any issues, commit, and push to remote `main`.
