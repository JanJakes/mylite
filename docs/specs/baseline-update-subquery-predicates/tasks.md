# Baseline UPDATE Subquery Predicates Tasks

- [x] Verify MySQL 8.4.9 behavior for `UPDATE` `IN` and `EXISTS` subquery
      predicates, correlated references, affected rows, warnings, outer
      `ORDER BY` / `LIMIT`, empty subqueries, `NULL` membership, and same-table
      source restrictions.
- [x] Specify the narrow MyLite grammar and runtime subset before code changes.
- [x] Add descriptor-driven planning for single-table `UPDATE` predicates that
      admit the existing `IN` and `EXISTS` subquery envelopes.
- [x] Alias generated physical update targets when planned predicates require
      source-qualified descriptor columns.
- [x] Reject same-table update subquery sources with MySQL-compatible
      `1093 / HY000`.
- [x] Add runtime tests and MySQL expectation artifacts.
- [x] Update compatibility documentation for the exact supported subset.
- [x] Run focused tests, the MySQL expectation script, and
      `cmake --workflow --preset check`.
- [x] Review the feature and fix findings.
- [ ] Commit and push `main`.
