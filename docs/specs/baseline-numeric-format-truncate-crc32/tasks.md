# Baseline Numeric FORMAT/TRUNCATE/CRC32 Tasks

- [x] Read current scalar numeric function, parser, compatibility, and testing
  context.
- [x] Verify MySQL 8.4.9 `CRC32()`, `FORMAT()`, and `TRUNCATE()` behavior for
  the admitted scalar subset and relevant deferred cases.
- [x] Write the independent feature spec with grammar snippets, ownership
  boundaries, runtime behavior, diagnostics, performance notes, and test plan.
- [x] Add a MySQL-runtime expectation script for this feature.
- [x] Extend parser/AST support for the three functions and deterministic arity
  shapes.
- [x] Implement MyLite-owned scalar runtime evaluation for the supported subset.
- [x] Update compatibility documentation for the exact supported subset.
- [x] Add focused runtime/parser tests and CMake registration.
- [x] Run focused build/tests and the MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review final diff for MySQL evidence, scalar-expression scope, diagnostics,
  file-format safety, docs accuracy, and test relevance.
- [x] Commit, review with a subagent, amend if needed, and push `main`.
