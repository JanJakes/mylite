# Baseline SHOW STATUS Sort, Cache, Log, And Slow Counter Tasks

- [x] Probe MySQL 8.4.9 for `Sort_%`, `Table_open_cache_%`, `Tc_log_%`, and
      `Slow_%` row names and scope behavior.
- [x] Add missing status descriptors with deterministic `0` placeholder values.
- [x] Add focused MyLite runtime assertions for the four row-name families.
- [x] Update `sys.metrics` representative coverage and row count.
- [x] Update compatibility docs and the baseline matrix.
