# Baseline VALUES() Function Outside ODKU Tasks

- [x] Verify MySQL 8.4.9 non-ODKU `VALUES(column)` result, metadata, warning,
  and error behavior.
- [x] Add a dedicated row-scalar plan node for non-ODKU `VALUES(column)`.
- [x] Add count-only diagnostics support for hidden `VALUES()` warnings.
- [x] Expose MySQL-shaped nullable `varbinary(0)` result metadata.
- [x] Add MySQL expectation coverage.
- [x] Add focused MyLite runtime coverage.
- [x] Update compatibility documentation.
