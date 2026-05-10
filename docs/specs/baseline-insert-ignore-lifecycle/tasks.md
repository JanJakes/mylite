# Baseline Insert Ignore Lifecycle Tasks

- [x] Read project architecture, compatibility, parser, runtime diagnostics,
  and existing insert/modifier lifecycle context.
- [x] Research official MySQL 8.4 `INSERT`, SQL mode `IGNORE`, implicit
  defaults, and out-of-range handling documentation.
- [x] Probe MySQL 8.4.9 runtime behavior for `INSERT IGNORE`, priority plus
  `IGNORE`, delayed plus `IGNORE`, warning order, value adjustment, non-
  ignorable errors, and upstream `INSERT IGNORE ... SELECT` acceptance.
- [x] Write the independent feature spec with MyLite grammar snippets,
  ownership boundaries, warning semantics, diagnostics, and tests.
- [x] Add MySQL-runtime expectation script for this feature.
- [x] Update compatibility documentation for the exact limited subset.
- [ ] Add parser grammar, AST ignore node, AST name, and parser helpers.
- [ ] Implement `IGNORE` handling for supported `INSERT ... VALUES` and
  `INSERT ... SET` without changing descriptor authority or SQLite metadata
  ownership.
- [ ] Add C parser/runtime tests for success, warning, and diagnostic paths.
- [ ] Run focused build/tests and the MySQL expectation script.
- [ ] Run `cmake --workflow --preset check`.
- [ ] Review the final diff for architecture boundaries, warning semantics,
  descriptor authority, scope control, and compatibility wording.
- [ ] Commit the implementation slice.
