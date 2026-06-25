# Baseline SHOW VARIABLES Optional Absence Tasks

- [x] Probe the MySQL 8.4.9 target runtime for red system variables absent
      from `performance_schema.variables_info`.
- [x] Add MySQL expectation coverage for default/session/local/global
      `SHOW VARIABLES` absence and representative scalar diagnostics.
- [x] Add focused MyLite runtime coverage for the same absence behavior.
- [x] Update CMake test registration.
- [x] Update the baseline and detailed compatibility docs without moving
      target-present variables out of red.
- [x] Run focused verification, formatting/static checks, and the full check
      workflow.
