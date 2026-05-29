# Baseline SHOW DATABASES WHERE Tasks

- [x] Verify MySQL 8.4.9 `SHOW DATABASES` / `SHOW SCHEMAS` `WHERE` behavior
      against official documentation and runtime probes.
- [x] Write an independently authored feature specification with grammar,
      semantics, diagnostics, architecture, and tests.
- [x] Add a MySQL 8.4.9 expectation script for the supported behavior and
      documented gaps.
- [x] Extend the parser to accept `SHOW DATABASES` / `SHOW SCHEMAS` `WHERE`
      predicates while preserving existing syntax errors.
- [x] Implement runtime filtering over the displayed `Database` output column.
- [x] Add focused C runtime tests and parser assertions.
- [x] Update compatibility documentation.
- [x] Run focused tests, MySQL expectations, diff checks, and the full check
      workflow.
- [x] Review the feature, fix findings, commit, and push.
