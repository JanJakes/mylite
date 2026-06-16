# Baseline Row Base-Conversion Functions Tasks

- [x] Specify the row-backed integer-domain `BIN()`, `OCT()`, and `CONV()` scope.
- [x] Add private SQLite UDFs for row base conversion and register them at
  bootstrap.
- [x] Plan and lower admitted row-scalar base-conversion calls.
- [x] Add MySQL-runtime expectation coverage for the row-backed forms.
- [x] Add focused runtime tests for table-backed values, nesting, and warnings.
- [x] Update compatibility documentation to describe the expanded row support and
  remaining gaps.
- [x] Run focused tests, MySQL expectation scripts, diff checks, and the full
  CMake check workflow.
