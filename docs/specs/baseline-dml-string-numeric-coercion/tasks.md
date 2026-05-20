# Baseline DML String Numeric Coercion Tasks

- [x] Read current compatibility, DML, parser, runtime, numeric conversion,
  non-strict coercion, result, storage, and test context.
- [x] Research official MySQL 8.4 type-conversion, SQL-mode, numeric
  out-of-range, and default-value documentation.
- [x] Probe MySQL 8.4.9 runtime behavior for quoted integer, decimal, and
  approximate DML values, strict and non-strict diagnostics, warnings, affected
  rows, and broader string forms outside this slice.
- [x] Write the independent feature spec with MyLite grammar snippets,
  ownership boundaries, conversion rules, physical SQLite handling,
  diagnostics, performance notes, and tests.
- [x] Add MySQL-runtime expectation script for this feature.
- [x] Update compatibility documentation for the exact limited subset.
- [x] Commit and push the start-feature artifacts.
- [x] Extend descriptor-driven `INSERT`, `REPLACE`, admitted duplicate-key, and
  `UPDATE` planning to decode and convert exact quoted numeric strings for
  numeric target descriptors.
- [x] Add runtime lifecycle tests for strict values, warnings, errors,
  `INSERT IGNORE` clipping where inherited, persistence, preamble preservation,
  and unsupported forms.
- [x] Register a new C test binary in `packages/libmylite/CMakeLists.txt` if a
  dedicated test remains clearer than extending existing DML tests.
- [x] Run focused build/tests and the MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff for MySQL evidence, descriptor authority,
  architecture boundaries, performance, cleanup on failure, compatibility
  wording, and test relevance.
- [x] Commit, push `main`, and continue to the next baseline slice.
