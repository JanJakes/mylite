# Baseline DML DEFAULT Keyword Values Tasks

- [x] Read current compatibility, default, insert, replace, update, parser,
  runtime, catalog, result, storage, and test context.
- [x] Research official MySQL 8.4 `INSERT`, `REPLACE`, `UPDATE`, and default
  value documentation.
- [x] Probe MySQL 8.4.9 runtime behavior for strict DML `DEFAULT`,
  `INSERT IGNORE`, dropped defaults, no-default columns, no-match update,
  `LIMIT 0`, affected rows, warnings, and errors.
- [x] Write the independent feature spec with MyLite grammar snippets,
  ownership boundaries, descriptor-default semantics, physical SQLite handling,
  diagnostics, and tests.
- [x] Add MySQL-runtime expectation script for this feature.
- [x] Update compatibility documentation for the exact limited subset.
- [ ] Commit and push the start-feature artifacts.
- [ ] Add parser grammar, AST support, parser helpers, and parser tests for
  DML `DEFAULT` value nodes.
- [ ] Extend descriptor-driven insert/replace/update planning to resolve
  DML `DEFAULT` through target descriptors, including `INSERT IGNORE`
  warning demotion.
- [ ] Add runtime lifecycle tests for strict DML, ignore DML, update behavior,
  persistence, preamble preservation, independent handles, and deterministic
  rejection of unsupported broader forms.
- [ ] Confirm whether a new test binary is needed in
  `packages/libmylite/CMakeLists.txt`.
- [ ] Run focused build/tests and the MySQL expectation script.
- [ ] Run `cmake --workflow --preset check`.
- [ ] Review the final diff for architecture boundaries, descriptor authority,
  performance, cleanup, compatibility wording, and test relevance.
- [ ] Commit, push `main`, and continue to the next baseline slice.
