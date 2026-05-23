# Baseline Information Schema Stats Expiry System Variable Tasks

- [x] Research MySQL 8.4 documentation and MySQL 8.4.9 runtime behavior for
      `information_schema_stats_expiry`.
- [x] Write the independently authored feature specification.
- [x] Add MySQL 8.4.9 expectation probes for reads, assignments, warnings,
      unsupported forms, and no-op global assignment.
- [x] Add handle-local session storage and initialization.
- [x] Register `information_schema_stats_expiry` in scalar system-variable,
      `HEX()` numeric-variable, and `SHOW VARIABLES` paths.
- [x] Implement session-local `SET information_schema_stats_expiry` parsing,
      clamping, diagnostics, user-variable assignment, and multi-assignment
      rollback.
- [x] Add focused C runtime coverage for values, assignment diagnostics,
      file safety, and independent handles.
- [x] Update compatibility documentation without overclaiming statistics-cache
      behavior or mutable global system variables.
- [x] Run focused expectation/C tests and the full check workflow.
- [x] Review the final diff for architecture boundaries, MySQL evidence,
      warning semantics, zero-init safety, and scope control.
