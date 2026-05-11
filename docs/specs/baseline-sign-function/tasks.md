# Baseline SIGN Function Tasks

## Design and Evidence

- [x] Read `AGENTS.md`, `README.md`, `COMPATIBILITY.md`, engineering
  standards, scalar-expression specs, numeric function specs, lexer/parser
  specs, runtime sources, parser tests, runtime tests, and SQLite fork notes.
- [x] Research official MySQL 8.4 documentation for `SIGN()`, numeric
  literals, arithmetic operators, `SELECT ... FROM DUAL`, and `DO`.
- [x] Verify MySQL 8.4.9 runtime behavior for admitted `SIGN()` values,
  direct exact-decimal integer magnitudes, booleans, `NULL`, arithmetic and
  bitwise children, warnings, overflow, wrong arity, and accepted-but-deferred
  operand forms.
- [x] Write the independently authored feature specification in `specs.md`,
  including MyLite grammar snippets, ownership boundaries, runtime semantics,
  diagnostics, unsupported forms, and verification plan.

## Implementation

- [x] Add the MySQL-runtime expectation artifact for the baseline `SIGN()`
  slice.
- [x] Extend lexer/parser/AST support for `SIGN(expr)` plus wrong-arity
  `SIGN()` / `SIGN(a, b)` nodes without widening general function behavior.
- [x] Implement MyLite-owned top-level no-source/`DUAL`/`DO` `SIGN()`
  evaluation over the admitted integer/boolean/`NULL`, signed-64 arithmetic,
  direct exact-decimal literal, and limited numeric bitwise operand domain.
- [x] Preserve warning staging, native function arity diagnostics, overflow
  diagnostics, unsupported-form diagnostics, row-count behavior, and public
  result conventions.
- [x] Add parser and runtime tests for successful values, aliases, `FROM DUAL`,
  `DO`, warnings, unsupported forms, file safety, independent handles, and
  catalog/schema-generation immutability.
- [x] Update compatibility documentation for only the admitted limited
  `SIGN()` subset.

## Verification

- [x] Run `packages/libmylite/tests/mysql_baseline_sign_function_expectations.sh`.
- [x] Run `cmake --build --preset dev`.
- [x] Run focused parser/runtime CTest entries for `SIGN()` and adjacent
  scalar-function/scalar-expression surfaces.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff for architecture boundaries, MySQL 8.4.9 evidence,
  direct decimal literal sign classification, warning behavior, file-format
  safety, scope control, and compatibility-doc accuracy.
- [x] Commit atomically, run a subagent release-gate review, amend if needed,
  and push `main`.
