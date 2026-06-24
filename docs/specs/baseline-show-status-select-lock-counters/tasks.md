# Baseline SHOW STATUS Select And Table-Lock Counter Tasks

- [x] Probe MySQL 8.4.9 for `SHOW STATUS LIKE 'Select\_%'` row names and
      scope behavior.
- [x] Probe MySQL 8.4.9 for `SHOW STATUS LIKE 'Table_locks\_%'` row names and
      scope behavior.
- [x] Add the missing `Select_range_check` status descriptor.
- [x] Add focused MyLite runtime assertions for default/session/local/global
      `Select_%` and `Table_locks_%` status rows.
- [x] Update `sys.metrics` representative coverage and row count.
- [x] Update compatibility docs and the baseline matrix.
