# Baseline SHOW STATUS SSL Placeholder Tasks

- [x] Probe MySQL 8.4.9 for `SHOW STATUS LIKE 'Ssl\_%'` row names and scope
      behavior.
- [x] Add all missing `Ssl_%` status descriptors with deterministic no-TLS
      placeholder values.
- [x] Add focused MyLite runtime assertions for default/local/global SSL status
      rows.
- [x] Update `sys.metrics` representative coverage and row count.
- [x] Update compatibility docs and the baseline matrix.
