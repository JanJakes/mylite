# Baseline Trigonometric Functions Tasks

## Design

- [x] Verify official MySQL 8.4 documentation for `SIN()`, `COS()`, `TAN()`,
  and `COT()`.
- [x] Probe MySQL 8.4.9 runtime behavior for admitted values, diagnostics,
  warning staging, wrong arity, `COT(0)`, `FROM DUAL`, and `DO`.
- [x] Write independently authored `specs.md` with MyLite grammar snippets,
  ownership boundaries, runtime semantics, diagnostics, tests, and scope.

## Implementation

- [ ] Extend lexer/parser/AST support for `SIN`, `COS`, `TAN`, and `COT`.
- [ ] Add runtime evaluation using the current scalar approximate operand
  helper and double formatter.
- [ ] Preserve descriptor/catalog/storage isolation and `.mylite` preamble
  invariants.
- [ ] Add deterministic unsupported diagnostics for deferred placements and
  operand forms.

## Tests and Docs

- [x] Add the MySQL 8.4.9 expectation script for the feature surface.
- [ ] Add parser tests.
- [ ] Add runtime tests for values, diagnostics, warning ordering, result
  conventions, and file-safety invariants.
- [ ] Update `COMPATIBILITY.md` and
  `docs/compatibility/functions-numeric-math.md`.
- [ ] Run focused build/tests, the MySQL expectation script, and
  `cmake --workflow --preset check`.
- [ ] Review the final diff for MySQL behavior, architecture boundaries,
  performance, cleanup, and scope control.
