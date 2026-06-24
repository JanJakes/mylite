# Baseline SHOW STATUS Temporary and Open Resource Counters Tasks

- [x] Verify MySQL 8.4.9 `Created_%` and `Open%` row names, order, and scope
  visibility.
- [x] Add fast runtime tests for exact row order, placeholder values, scope
  filtering, and `sys.metrics` readback.
- [x] Update the MySQL expectation script to pin names and scope behavior
  without pinning volatile live values.
- [x] Update compatibility and spec documentation for the placeholder surface
  and remaining live-counter gaps.
