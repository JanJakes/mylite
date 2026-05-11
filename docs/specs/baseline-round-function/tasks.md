# Baseline ROUND Function Tasks

## Design and Evidence

- [x] Read `AGENTS.md`, `README.md`, `COMPATIBILITY.md`, engineering
  standards, scalar-expression specs, numeric function specs, lexer/parser
  specs, runtime sources, parser tests, runtime tests, and SQLite fork notes.
- [x] Research official MySQL 8.4 documentation for `ROUND()`, numeric
  literals, arithmetic operators, `SELECT ... FROM DUAL`, and `DO`.
- [x] Verify MySQL 8.4.9 runtime behavior for admitted one-argument
  `ROUND()` values, booleans, `NULL`, signed zero, signed and unsigned
  boundaries, direct exact decimal literal magnitudes, bitwise children,
  arithmetic children, warnings, overflow, wrong arity, two-argument forms,
  and accepted-but-deferred operand forms.
- [x] Write the independently authored feature specification in `specs.md`,
  including MyLite grammar snippets, ownership boundaries, runtime semantics,
  diagnostics, unsupported forms, and verification plan.

## Implementation

- [x] Add the MySQL-runtime expectation artifact for the baseline `ROUND()`
  function slice.
- [ ] Extend lexer/parser/AST support for `ROUND(expr)`, recognized
  `ROUND(expr, places)`, and wrong-arity nodes without widening general
  function behavior.
- [ ] Implement MyLite-owned top-level no-source/`DUAL`/`DO` evaluation over
  the admitted integer/boolean/`NULL`, direct decimal integer literal,
  signed-64 arithmetic, and limited numeric bitwise operand domain.
- [ ] Preserve warning staging, native function arity diagnostics, overflow
  diagnostics, unsupported-form diagnostics, row-count behavior, and public
  result conventions.
- [ ] Add parser and runtime tests for successful values, aliases,
  `FROM DUAL`, `DO`, warnings, unsupported forms, file safety, independent
  handles, and catalog/schema-generation immutability.
- [ ] Update compatibility documentation for only the admitted limited
  one-argument `ROUND()` subset.

## Verification

- [x] Run `packages/libmylite/tests/mysql_baseline_round_function_expectations.sh`.
- [ ] Run `cmake --build --preset dev`.
- [ ] Run focused parser/runtime CTest entries for `ROUND()` and adjacent
  scalar-function/scalar-expression surfaces.
- [ ] Run `cmake --workflow --preset check`.
- [ ] Review the final diff for architecture boundaries, MySQL 8.4.9 evidence,
  direct decimal literal normalization, warning behavior, file-format safety,
  scope control, and compatibility-doc accuracy.
- [ ] Commit atomically, run a subagent release-gate review, amend if needed,
  and push `main`.
