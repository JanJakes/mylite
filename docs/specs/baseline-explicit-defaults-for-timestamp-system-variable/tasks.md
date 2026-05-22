# Baseline Explicit Defaults For Timestamp System Variable Tasks

- [x] Verify MySQL 8.4.9 scalar, `SHOW VARIABLES`, and `SET` behavior for
  `explicit_defaults_for_timestamp`.
- [x] Specify the fixed modern-mode MyLite subset and explicitly defer
  deprecated `OFF` timestamp semantics.
- [x] Add runtime system-variable registry entries, scalar readback,
  `SHOW VARIABLES` value rendering, and fixed no-op `SET` validation.
- [x] Add a MySQL 8.4.9 expectation script.
- [x] Add focused C runtime coverage plus existing `SHOW VARIABLES` and fixed
  `SET` regression coverage.
- [x] Update compatibility documentation.
- [x] Run focused build/tests, the MySQL expectation script, and
  `cmake --workflow --preset check`.
- [x] Review, commit, and push.
