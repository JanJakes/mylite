# DML Integer String Prefix Coercion Tasks

- [x] Read current compatibility, DML, parser, runtime, numeric conversion,
  non-strict coercion, SQLite integration, and test context.
- [x] Research official MySQL 8.4 type-conversion, SQL-mode, and out-of-range
  documentation.
- [x] Probe MySQL 8.4.9 runtime behavior for strict, non-strict, and
  `INSERT IGNORE` integer string storage conversion.
- [x] Write the independent feature spec with ownership boundaries, conversion
  rules, diagnostics, physical SQLite handling, performance notes, and tests.
- [x] Add MySQL-runtime expectation script for the feature.
- [x] Update compatibility documentation for the exact supported subset.
- [x] Implement descriptor-owned integer string prefix scanning and warning
  adjustment for row-value `INSERT`, `REPLACE`, admitted duplicate-key
  assignments, and matched single-table `UPDATE`.
- [x] Add fast C runtime coverage.
- [x] Run focused build/tests and the MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review, commit, push `main`, and continue to the next baseline slice.
