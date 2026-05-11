# Baseline Scalar Division Projection Tasks

## Design and Evidence

- [x] Read `AGENTS.md`, `README.md`, `COMPATIBILITY.md`, engineering
  standards, scalar-expression specs, numeric function specs, lexer/parser
  specs, runtime sources, parser tests, runtime tests, and SQLite fork notes.
- [x] Research official MySQL 8.4 documentation for `/`, arithmetic
  operators, operator precedence, exact-value expression handling,
  `SELECT ... FROM DUAL`, and `DO`.
- [x] Verify MySQL 8.4.9 runtime behavior for admitted integer division
  values, booleans, `NULL`, signed operands, signed boundaries, result
  formatting, rounding, warnings, child overflow, syntax errors, `DO`, and
  accepted-but-deferred operand/composition forms.
- [x] Write the independently authored feature specification in `specs.md`,
  including MyLite grammar snippets, ownership boundaries, runtime semantics,
  diagnostics, unsupported forms, and verification plan.

## Implementation

- [ ] Add the MySQL-runtime expectation artifact for the baseline scalar `/`
  projection slice.
- [ ] Extend parser tests for `/` AST and precedence coverage if existing
  coverage is incomplete.
- [ ] Implement MyLite-owned top-level no-source/`DUAL`/`DO` division
  evaluation over the admitted signed-64 scalar arithmetic operand domain.
- [ ] Preserve warning staging, child diagnostics, unsupported-form
  diagnostics, row-count behavior, and public result conventions.
- [ ] Add runtime tests for successful values, aliases, `FROM DUAL`, `DO`,
  warnings, unsupported forms, file safety, independent handles, and
  catalog/schema-generation immutability.
- [ ] Update compatibility documentation for only the admitted limited scalar
  division subset.

## Verification

- [ ] Run `packages/libmylite/tests/mysql_baseline_scalar_division_projection_expectations.sh`.
- [ ] Run `cmake --build --preset dev`.
- [ ] Run focused parser/runtime CTest entries for scalar division and adjacent
  scalar-expression surfaces.
- [ ] Run `cmake --workflow --preset check`.
- [ ] Review the final diff for architecture boundaries, MySQL 8.4.9 evidence,
  decimal formatting correctness, warning behavior, file-format safety, scope
  control, and compatibility-doc accuracy.
- [ ] Commit atomically, run a subagent release-gate review, amend if needed,
  and push `main`.
