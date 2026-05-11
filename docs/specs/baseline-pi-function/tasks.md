# Baseline PI Function Tasks

## Design

- [x] Read MyLite architecture, engineering standards, compatibility matrix,
  scalar function specs, parser/runtime patterns, and SQLite fork policy.
- [x] Verify official MySQL 8.4 documentation for `PI()` and related scalar
  statement surfaces.
- [x] Verify MySQL 8.4.9 runtime behavior for supported `PI()` result text,
  casing, whitespace, parenthesization, `FROM DUAL`, `DO`, warnings, wrong
  arity, bare identifier behavior, and accepted-but-deferred wider forms.
- [x] Write the independently authored feature specification in `specs.md`,
  including MyLite grammar snippets, ownership boundaries, runtime semantics,
  diagnostics, unsupported forms, metadata boundary, and verification plan.

## Implementation

- [x] Add the MySQL-runtime expectation artifact for the baseline `PI()`
  function slice.
- [x] Extend lexer/parser/AST support for `PI()` and wrong-arity nodes without
  widening general function behavior.
- [x] Implement MyLite-owned top-level no-source/`DUAL`/`DO` evaluation as a
  constant visible scalar value.
- [x] Preserve native function arity diagnostics, unsupported-form diagnostics,
  row-count behavior, warning behavior, source-span labels, aliases, and public
  result conventions.
- [x] Add parser and runtime tests for successful values, labels, aliases,
  `FROM DUAL`, `DO`, diagnostics, unsupported forms, file safety, independent
  handles, and catalog/schema-generation immutability.
- [x] Update compatibility documentation for only the admitted limited `PI()`
  subset.

## Verification

- [x] Run `packages/libmylite/tests/mysql_baseline_pi_function_expectations.sh`.
- [x] Run `cmake --build --preset dev`.
- [x] Run focused parser/runtime CTest entries for `PI()` and adjacent scalar
  function/scalar-expression surfaces.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff for architecture boundaries, MySQL 8.4.9 evidence,
  approximate-value scope control, warning behavior, file-format safety,
  unsupported-form diagnostics, and compatibility-doc accuracy.
- [x] Commit atomically, run a subagent release-gate review, amend if needed,
  push `main`, and continue to the next baseline slice.
