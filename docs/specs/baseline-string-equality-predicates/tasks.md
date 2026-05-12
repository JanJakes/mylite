# Baseline String Equality Predicates Tasks

- [x] Create independently authored feature spec.
- [x] Verify supported string predicate behavior against MySQL 8.4.9.
- [x] Add MySQL-runtime expectation artifact.
- [x] Extend the shared descriptor predicate planner for admitted string columns.
- [x] Emit generated SQLite predicates with descriptor identifiers, bound values,
  and MyLite's registered ASCII `utf8mb4_0900_ai_ci` collation.
- [x] Add fast runtime C tests for `SELECT`, `UPDATE`, `DELETE`, diagnostics,
  and cleanup behavior.
- [x] Update compatibility docs for the exact supported subset.
- [x] Run focused runtime tests and the MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff, commit atomically, and push `main`.
