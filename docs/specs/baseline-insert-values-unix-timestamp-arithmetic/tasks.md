# Baseline INSERT Values UNIX_TIMESTAMP Arithmetic Tasks

- [x] Specify the narrow DML value grammar and runtime semantics.
- [x] Verify MySQL 8.4.9 behavior for supported and deferred forms.
- [x] Add MySQL expectation artifact for the admitted user-visible behavior.
- [x] Extend parser support for no-argument `UNIX_TIMESTAMP()` insert values
      and plus/minus signed integer or `NULL` deltas.
- [x] Add descriptor-driven runtime conversion for integer-family targets.
- [x] Extend descriptor-driven runtime conversion to nonbinary string targets.
- [x] Add fast runtime tests for success, diagnostics, persistence, and file
      safety.
- [x] Update compatibility documentation for the exact supported subset.
- [x] Run focused tests, MySQL expectations, `cmake --build --preset dev`, and
      `cmake --workflow --preset check`.
- [x] Review the feature and fix any release-gate findings before committing.
