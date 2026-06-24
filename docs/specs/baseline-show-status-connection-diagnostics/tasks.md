# Baseline SHOW STATUS Connection Diagnostics Tasks

- [x] Verify MySQL 8.4.9 `Connection_%` row names and scope visibility.
- [x] Expand the MyLite `SHOW STATUS` descriptor registry with connection
  diagnostic placeholder rows.
- [x] Update fast runtime tests for exact row order, placeholder values, and
  `sys.metrics` derived rows.
- [x] Update the MySQL expectation script to pin row names without relying on
  mutable live counter values.
- [x] Update compatibility and spec documentation for the placeholder surface
  and remaining live-connection gaps.
