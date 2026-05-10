# Baseline Replace Modifier Lifecycle Tasks

- [x] Read project architecture, compatibility, parser, runtime diagnostics,
  and existing replace lifecycle context.
- [x] Research official MySQL 8.4 `REPLACE`, `SHOW WARNINGS`, and
  `ROW_COUNT()` documentation.
- [x] Probe MySQL 8.4.9 runtime behavior for `LOW_PRIORITY`, `DELAYED`,
  warning count, `SHOW WARNINGS`, errors after delayed warnings, and repeated
  modifiers.
- [x] Write the independent feature spec with MyLite grammar snippets,
  ownership boundaries, warning semantics, diagnostics, and tests.
- [x] Add MySQL-runtime expectation script for this feature.
- [x] Update compatibility documentation for the exact limited subset.
- [x] Add parser grammar, AST modifier nodes, AST names, and parser helpers.
- [x] Implement modifier handling for supported `REPLACE ... VALUES`,
  `REPLACE ... SET`, and `REPLACE ... SELECT` without changing descriptor
  planning or physical SQLite generation.
- [x] Add C parser/runtime tests for success, warning, and diagnostic paths.
- [x] Run focused build/tests and the MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff for architecture boundaries, warning semantics,
  descriptor authority, scope control, and compatibility wording.
- [x] Commit the implementation slice.
