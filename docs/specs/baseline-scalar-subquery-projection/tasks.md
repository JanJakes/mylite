# Baseline Scalar Subquery Projection Tasks

- [x] Create independently authored feature spec.
- [x] Verify supported scalar subquery projection behavior against MySQL 8.4.9.
- [x] Add MySQL-runtime expectation artifact.
- [x] Add parser/AST support for scalar subquery expression nodes.
- [x] Add scalar subquery evaluation for no-source and `DUAL` inner `SELECT`.
- [x] Admit scalar subqueries in scalar projection and row-scalar `CONCAT()` arguments.
- [x] Add fast parser and runtime C tests.
- [x] Update compatibility docs for the exact supported subset.
- [x] Run focused parser/runtime tests and MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff, commit atomically, and push `main`.
