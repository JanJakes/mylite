# Baseline CAST Binary Tasks

## Design

- [x] Read MyLite architecture, engineering standards, compatibility matrix,
  scalar expression specs, parser/runtime patterns, and SQLite fork policy.
- [x] Verify official MySQL 8.4 documentation for `CAST(... AS BINARY)`,
  binary-string display, `SELECT ... FROM DUAL`, and `DO`.
- [x] Verify MySQL 8.4.9 runtime behavior for supported result values,
  aliases, labels, warning counts, row counts, `FROM DUAL`, `DO`, and
  accepted-but-deferred wider forms.
- [x] Write the independently authored feature specification in `specs.md`,
  including MyLite grammar snippets, ownership boundaries, runtime semantics,
  diagnostics, unsupported forms, metadata boundary, and verification plan.

## Implementation

- [x] Add the MySQL-runtime expectation artifact for the baseline
  `CAST(... AS BINARY)` slice.
- [x] Extend lexer/parser/AST support for `CAST(value AS BINARY)` without
  widening general cast/function behavior.
- [x] Implement MyLite-owned no-source/`DUAL`/`DO` evaluation for the admitted
  string/integer/boolean/`NULL` input subset.
- [x] Preserve unsupported-form diagnostics, row-count behavior, warning
  behavior, source-span labels, aliases, public result conventions, and
  statement cleanup.
- [x] Add parser and runtime tests for successful values, labels, aliases,
  `FROM DUAL`, `DO`, diagnostics, unsupported forms, file safety, independent
  handles, and catalog/schema-generation immutability.
- [x] Update compatibility documentation for only the admitted limited
  `CAST(... AS BINARY)` subset.

## Verification

- [x] Run `packages/libmylite/tests/mysql_baseline_cast_binary_expectations.sh`.
- [x] Run `cmake --build --preset dev`.
- [x] Run focused parser/runtime CTest entries for `CAST(... AS BINARY)` and
  adjacent scalar expression surfaces.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff for architecture boundaries, MySQL 8.4.9 evidence,
  binary-value scope control, warning behavior, file-format safety,
  unsupported-form diagnostics, and compatibility-doc accuracy.
- [x] Commit atomically, run a subagent release-gate review, amend if needed,
  push `main`, and continue to the next baseline slice.
