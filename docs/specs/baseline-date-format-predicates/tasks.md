# Baseline DATE_FORMAT Predicates Tasks

- [x] Read existing DATE_FORMAT, row-scalar, and SELECT WHERE specs and tests.
- [x] Verify official MySQL 8.4 documentation for `DATE_FORMAT()` and comparison predicates.
- [x] Verify MySQL 8.4.9 runtime behavior for the admitted `DATE_FORMAT(..., '%H.%i') = numeric_literal` predicate surface.
- [x] Write an independently authored feature spec with MyLite Lemon-syntax snippets.
- [x] Add MySQL expectation script for the admitted DATE_FORMAT predicate surface.
- [x] Extend parser and predicate planning to admit the exact DATE_FORMAT numeric equality predicate over one descriptor table source.
- [x] Reuse descriptor-driven row-scalar DATE_FORMAT SQL generation and parameter binding without row materialization.
- [x] Add runtime and parser tests for successful filtering, warnings, ordering/limit, and diagnostics.
- [x] Update compatibility documentation for the exact admitted subset.
- [x] Run targeted runtime/MySQL expectation tests.
- [x] Run `cmake --workflow --preset check`.
- [x] Run subagent release-gate review, fix findings, commit atomically, and push `main`.
