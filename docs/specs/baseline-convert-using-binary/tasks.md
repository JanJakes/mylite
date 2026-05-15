# Baseline CONVERT Using Binary Tasks

## Design

- [x] Read MyLite architecture, engineering standards, compatibility matrix,
  existing CAST binary spec, parser/runtime patterns, and SQLite fork policy.
- [x] Verify official MySQL 8.4 documentation for `CONVERT(expr USING
  transcoding_name)`, `CONVERT(expr USING BINARY)`, binary-string display,
  `SELECT ... FROM DUAL`, and `DO`.
- [x] Reuse the existing MySQL 8.4.9 runtime artifact that already records
  `CONVERT('ABC' USING BINARY)` returning `0x414243` under
  `--binary-as-hex=1`.
- [x] Write the independently authored feature specification in `specs.md`,
  including MyLite grammar snippets, ownership boundaries, runtime semantics,
  diagnostics, unsupported forms, metadata boundary, and verification plan.

## Implementation

- [ ] Add the MySQL-runtime expectation artifact for the baseline
  `CONVERT('string' USING BINARY)` slice.
- [ ] Extend parser/AST support for `CONVERT(string_literal USING BINARY)`
  without widening general `CONVERT()` or `CAST()` behavior.
- [ ] Implement MyLite-owned no-source/`DUAL`/`DO` evaluation for the admitted
  string-literal input subset.
- [ ] Preserve unsupported-form diagnostics, row-count behavior, warning
  behavior, source-span labels, aliases, public result conventions, and
  statement cleanup.
- [ ] Add parser and runtime tests for successful values, labels, aliases,
  `FROM DUAL`, `DO`, diagnostics, unsupported forms, file safety, independent
  handles, and catalog/schema-generation immutability.
- [ ] Update compatibility documentation for only the admitted limited
  `CONVERT('string' USING BINARY)` subset.

## Verification

- [ ] Run `packages/libmylite/tests/mysql_baseline_convert_using_binary_expectations.sh`
  when the MySQL 8.4.9 Docker runtime is available.
- [ ] Run `cmake --build --preset dev`.
- [ ] Run focused parser/runtime CTest entries for `CONVERT(... USING BINARY)`
  and adjacent scalar expression surfaces.
- [ ] Run `cmake --workflow --preset check`.
- [ ] Review the final diff for architecture boundaries, MySQL 8.4.9 evidence,
  binary-value scope control, warning behavior, file-format safety,
  unsupported-form diagnostics, and compatibility-doc accuracy.
- [ ] Commit atomically, run a subagent release-gate review, amend if needed,
  push `main`, and continue to the next baseline slice.
