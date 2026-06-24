# Baseline SHOW STATUS Binary-Log Counters Tasks

- [x] Verify MySQL 8.4.9 `Binlog_%` row names, order, and scope visibility.
- [x] Expand the MyLite `SHOW STATUS` descriptor registry with the observed
  binary-log counter rows.
- [x] Update fast runtime tests for exact row order, placeholder values, scope
  filtering, and `sys.metrics` derived row count.
- [x] Update the MySQL expectation script to pin names and scope behavior
  without pinning volatile live values.
- [x] Update compatibility and spec documentation for the placeholder surface
  and remaining live-counter gaps.
