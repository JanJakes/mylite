# Baseline BIN and OCT Functions Tasks

## Design and Evidence

- [x] Read `AGENTS.md`, `README.md`, `COMPATIBILITY.md`, engineering
  standards, scalar-expression specs, numeric/string function docs,
  lexer/parser specs, runtime sources, parser tests, runtime tests, and SQLite
  fork notes.
- [x] Research official MySQL 8.4 documentation for `BIN()`, `OCT()`, numeric
  literals, arithmetic operators, bit operators, `SELECT ... FROM DUAL`, and
  `DO`.
- [x] Verify MySQL 8.4.9 runtime behavior for admitted `BIN()` / `OCT()`
  values, booleans, `NULL`, signed zero, signed and unsigned boundaries,
  bitwise children, arithmetic children, warnings, overflow, wrong arity, and
  accepted-but-deferred operand forms.
- [x] Write the independently authored feature specification in `specs.md`,
  including MyLite grammar snippets, ownership boundaries, runtime semantics,
  diagnostics, unsupported forms, and verification plan.

## Implementation

- [x] Add the MySQL-runtime expectation artifact for the baseline `BIN()` /
  `OCT()` function slice.
- [x] Extend lexer/parser/AST support for `BIN(expr)`, `OCT(expr)`, and
  wrong-arity nodes without widening general function behavior.
- [x] Implement MyLite-owned top-level no-source/`DUAL`/`DO` evaluation over
  the admitted integer/boolean/`NULL`, direct decimal integer literal,
  signed-64 arithmetic, and limited numeric bitwise operand domain.
- [x] Preserve warning staging, native function arity diagnostics, overflow
  diagnostics, unsupported-form diagnostics, row-count behavior, and public
  result conventions.
- [x] Add parser and runtime tests for successful values, aliases,
  `FROM DUAL`, `DO`, warnings, unsupported forms, file safety, independent
  handles, and catalog/schema-generation immutability.
- [x] Update compatibility documentation for only the admitted limited
  `BIN()` / `OCT()` subset.

## Verification

- [x] Run `packages/libmylite/tests/mysql_baseline_bin_oct_functions_expectations.sh`.
- [x] Run `cmake --build --preset dev`.
- [x] Run focused parser/runtime CTest entries for `BIN()` / `OCT()` and
  adjacent scalar-function/scalar-expression surfaces.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff for architecture boundaries, MySQL 8.4.9 evidence,
  unsigned formatting correctness, warning behavior, file-format safety, scope
  control, and compatibility-doc accuracy.
- [x] Commit atomically, run a subagent release-gate review, amend if needed,
  and push `main`.
