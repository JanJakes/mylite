# Native Prepared-Statement Binding Tasks

- [x] Record the design and MySQL 8.4.9 authorities.
- [x] Add focused MySQL 8.4.9 runtime expectations.
- [x] Add prepared-only parameter tokens and indexed AST nodes.
- [x] Add typed public binding and reset APIs.
- [x] Add parameter descriptors to analysis and lowering.
- [x] Implement generation-safe reprepare and repeated execution.
- [x] Migrate the core PHP extension.
- [x] Migrate the mysqli replacement.
- [x] Migrate the PDO driver.
- [x] Migrate SQL-level `PREPARE`/`EXECUTE` to retained native statements.
- [ ] Add core, PHP, sanitizer, and performance coverage. Core and PHP
  behavioral coverage, retained-execution parse counters, and prepared
  SELECT/UPDATE benchmarks are complete; sanitizer qualification remains.
- [x] Update public API and compatibility documentation.
- [ ] Run complete core, PHP, application, and MySQL verification.
- [ ] Review ABI, ownership, diagnostics, performance, and binary size.
