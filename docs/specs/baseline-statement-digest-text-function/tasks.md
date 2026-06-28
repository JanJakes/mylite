# Baseline STATEMENT_DIGEST_TEXT function tasks

- [x] Verify MySQL 8.4.9 behavior for scalar output, row-backed arguments,
      `NULL`, diagnostics, charset/collation/coercibility, and protocol metadata.
- [x] Document the feature scope and the `STATEMENT_DIGEST()` hash boundary.
- [x] Add the native sys-function registration.
- [x] Add a parser-gated statement digest text normalizer.
- [x] Add scalar and row-backed result metadata.
- [x] Add charset, collation, and coercibility wrapper support.
- [x] Add MySQL expectation and runtime regression tests.
- [x] Run focused verification and full release checks.
