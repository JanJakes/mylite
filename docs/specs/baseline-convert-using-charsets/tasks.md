# Baseline `CONVERT ... USING` Character Sets Tasks

- [x] Verify MySQL 8.4.9 scalar behavior for `utf8`, `utf8mb3`, `latin1`,
  quoted charset names, postfix `COLLATE`, warnings, and diagnostics.
- [x] Write the independently authored feature specification.
- [x] Update MySQL expectation scripts for conversion, charset/collation, and
  coercibility behavior.
- [x] Extend parser/AST support for scalar postfix `COLLATE`.
- [x] Generalize scalar `CONVERT(... USING charset)` runtime metadata and
  warning handling without broadening table charset support.
- [x] Add fast runtime tests for scalar values, metadata, warnings, and
  diagnostics.
- [x] Update compatibility documentation for the exact supported subset.
- [x] Run focused MySQL expectation scripts and CTest entries.
- [x] Run `cmake --build --preset dev`.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff, commit atomically, and push to `origin/main`.
