# Baseline Big Tables System Variable Tasks

- [x] Research MySQL 8.4 documentation and MySQL 8.4.9 runtime behavior for
      `big_tables`.
- [x] Write the independently authored feature specification.
- [x] Add MySQL 8.4.9 expectation probes for reads, assignments, diagnostics,
      user variables, fixed global no-op handling, and rollback.
- [x] Add handle-local session storage and initialization.
- [x] Register `big_tables` in scalar system-variable, `HEX()` numeric-variable,
      and `SHOW VARIABLES` paths.
- [x] Implement session-local `SET big_tables` parsing, diagnostics,
      user-variable assignment, and multi-assignment rollback.
- [x] Add focused C runtime coverage for values, assignment diagnostics,
      file safety, and independent handles.
- [x] Update compatibility documentation without overclaiming temporary-table
      storage behavior or mutable global system variables.
- [x] Run focused expectation/C tests and the full check workflow.
- [x] Review the final diff for architecture boundaries, MySQL evidence,
      zero-init safety, and scope control.
