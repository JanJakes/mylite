# Baseline Built-In Schema Table Directory Tasks

- [x] Verify MySQL 8.4.9 built-in schema table counts, table types, and hashes.
- [x] Specify metadata-only ownership and the non-queryable system-table boundary.
- [x] Add MySQL expectation script for the built-in table directory.
- [x] Add runtime static directory rows for built-in schema tables.
- [x] Route `SHOW TABLES` / `SHOW FULL TABLES` for built-in schemas through the directory.
- [x] Route `SHOW TABLE STATUS` for built-in schemas through the directory.
- [x] Extend `INFORMATION_SCHEMA.TABLES` with directory rows.
- [x] Add focused C tests for counts, representative rows, SHOW filters, and unsupported reads.
- [x] Update compatibility docs.
- [x] Run focused CTests and MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review, commit, and push.
