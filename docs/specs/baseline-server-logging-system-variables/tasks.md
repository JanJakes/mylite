# Baseline Server Logging System Variables Tasks

- [x] Record MySQL 8.4.9 default values, warnings, and diagnostics.
- [x] Add runtime registry visibility for server logging variables.
- [x] Return scalar and `SHOW VARIABLES` values for global logging placeholders.
- [x] Add global-only scope diagnostics, read-only diagnostics, and fixed
      global no-op assignment handling.
- [x] Add deprecation warnings for legacy logging variable names.
- [x] Add session-local `long_query_time` state, formatting, clamp warnings,
      invalid-type diagnostics, and rollback behavior.
- [x] Reject unsupported process-global logging mutation deterministically.
- [x] Add MySQL expectation and runtime regression tests.
- [x] Update compatibility and `SHOW VARIABLES` documentation.
- [x] Run focused and full verification.
