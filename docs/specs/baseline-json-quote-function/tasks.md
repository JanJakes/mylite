# Baseline JSON_QUOTE Function Tasks

- [x] Review existing JSON construction, extraction, introspection, parser,
  row-scalar, metadata, and SQLite function registration architecture.
- [x] Verify MySQL 8.4.9 behavior for supported `JSON_QUOTE()` output, SQL
  mode interaction, diagnostics, `DO`, warning count, and result metadata.
- [x] Write the independently authored feature specification and compatibility
  scope.
- [x] Add MySQL-runtime expectation artifact for the user-visible behavior.
- [x] Add lexer/parser/AST support for `JSON_QUOTE()` and wrong-arity markers.
- [x] Add MyLite JSON string-quote helper reusing the existing JSON writer.
- [x] Register `_mylite_json_quote()` through the public SQLite scalar function
  API.
- [x] Add scalar and row-scalar planning, execution, parameter binding,
  diagnostics, and result metadata.
- [x] Add parser, runtime, metadata, and compatibility tests.
- [x] Run focused JSON/parser/runtime verification, the MySQL expectation
  script, and `cmake --workflow --preset check`.
- [x] Self-review, commit, request subagent review, amend if needed, and push.
