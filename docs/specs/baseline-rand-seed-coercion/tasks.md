# Baseline RAND Seed Coercion Tasks

- [x] Read project guidance, existing RAND specs/tests, scalar cast/control-flow
  behavior, result warning staging, and compatibility documentation.
- [x] Verify MySQL 8.4.9 runtime behavior for decimal, string, signed boolean,
  signed `NULL`, and `NULLIF()` RAND seed coercion.
- [x] Write the independently authored feature specification with grammar
  notes, runtime semantics, diagnostics, architecture boundaries, and deferred
  table-backed seed behavior.
- [x] Extend MySQL-runtime expectation coverage for the admitted seed coercion
  subset.
- [x] Implement source-free scalar RAND seed coercion and staged warnings
  without changing row-scalar table-backed seed planning.
- [x] Add fast runtime tests for successful coerced seeds, warnings, unsupported
  forms, and file-safety preservation.
- [x] Update `COMPATIBILITY.md` and numeric/math compatibility details.
- [x] Run focused RAND runtime tests and MySQL expectation scripts.
- [x] Run `cmake --workflow --preset check`.
- [x] Review and fix release-gate gaps.
- [x] Commit and push to remote `main`.
