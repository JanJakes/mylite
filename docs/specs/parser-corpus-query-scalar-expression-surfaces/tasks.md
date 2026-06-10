# Parser Corpus Query Scalar Expression Surfaces Tasks

- [x] Verify representative MySQL 8.4.9 syntax and runtime expectations.
- [x] Add parser tests for scalar `IN`, quantified subqueries, query-expression
  subquery containers, clause expression keys, and lateral derived tables.
- [x] Extend grammar support without silently executing unsupported semantics.
- [x] Add runtime tests for preserved supported behavior and unsupported
  diagnostics.
- [x] Update compatibility documentation.
- [x] Rerun focused tests and parser corpus benchmark.
- [x] Review, fix findings, commit, push, and continue to the next bucket.

Parser corpus benchmark after implementation:

- `ok=66734`, `syntax_error=2839`, `lexer_error=21`, `stack_overflow=1`
  across 69,595 mysql-server-test queries.
