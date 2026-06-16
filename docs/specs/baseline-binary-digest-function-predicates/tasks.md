# Baseline Binary And Digest Function Predicates Tasks

- [x] Verify MySQL 8.4.9 predicate behavior for digest, compression, and
  random-byte row-scalar function expressions.
- [x] Document supported predicate contexts and intentionally unsupported
  contexts.
- [x] Admit digest, compression, and random-byte functions through the shared
  row-scalar predicate expression allowlist.
- [x] Add MyLite runtime coverage for comparison, `IS NULL`, and `BETWEEN`
  predicates.
- [x] Add MySQL expectation coverage for the new predicate contexts.
- [x] Run focused MySQL and C verification.
- [x] Run release-gate checks and update compatibility docs.
