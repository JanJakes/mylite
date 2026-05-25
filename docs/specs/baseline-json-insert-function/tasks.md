# Baseline JSON_INSERT Function Tasks

- [x] Verify MySQL 8.4.9 behavior for the admitted `JSON_INSERT()` scalar and
  row-scalar surface, including existing-path no-op, object insertion, array
  append, scalar autowrap, `NULL` path precedence, root-path no-op, and
  diagnostics.
- [x] Write an independently authored feature spec with MyLite grammar snippets,
  architecture boundaries, diagnostics, performance posture, and compatibility
  gaps.
- [x] Add MySQL-runtime expectation coverage in
  `packages/libmylite/tests/mysql_baseline_json_insert_function_expectations.sh`.
- [x] Extend the lexer/parser/AST for `JSON_INSERT()` and parser tests.
- [x] Extend the runtime JSON mutation module and SQLite scalar callback for
  insert-only mutation.
- [x] Extend row-scalar planning, SQL generation, parameter binding, metadata,
  and nested JSON consumer support.
- [x] Add runtime C coverage under `packages/libmylite/tests/` and register it
  in `packages/libmylite/CMakeLists.txt`.
- [x] Update `COMPATIBILITY.md`, `docs/compatibility/functions-json.md`, JSON
  type/path compatibility wording, and query-expression wording for the exact
  supported subset.
- [x] Run focused parser/runtime/MySQL expectation tests.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff for MySQL equivalence, architecture boundaries,
  descriptor-driven execution, path/value conversion, `NULL` precedence, memory
  cleanup, docs accuracy, and scope control.
