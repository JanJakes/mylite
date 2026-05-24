# Baseline ROUND Places Function Tasks

## Design and Evidence

- [x] Read `AGENTS.md`, `README.md`, `COMPATIBILITY.md`, engineering
  standards, the existing `baseline-round-function` spec/tasks, parser/runtime
  sources, runtime tests, and numeric compatibility docs.
- [x] Verify MySQL 8.4.9 runtime behavior for supported
  `ROUND(value, places)` integer-domain values, `NULL`, booleans, negative
  places, signed overflow, and warning staging.
- [x] Write the independently authored feature specification in `specs.md`,
  including ownership boundaries, runtime semantics, diagnostics, unsupported
  forms, and verification plan.

## Implementation

- [x] Update the MySQL-runtime expectation artifact for the expanded
  `ROUND(value, places)` subset.
- [x] Implement MyLite-owned top-level no-source/`DUAL`/`DO` evaluation for the
  admitted two-argument integer-domain subset.
- [x] Preserve existing one-argument behavior, warning staging, arity
  diagnostics, overflow diagnostics, row-count behavior, file safety, and
  public result conventions.
- [x] Add runtime tests for successful places behavior, warning staging,
  overflow, unsupported broad forms, `DO`, and file-safety invariants.
- [x] Update compatibility documentation for only the admitted limited
  two-argument subset.

## Verification

- [x] Run `packages/libmylite/tests/mysql_baseline_round_function_expectations.sh`.
- [x] Run the focused parser/runtime build and CTest entries.
- [x] Run `cmake --workflow --preset check`.
- [x] Run a subagent release-gate review and fix findings.
