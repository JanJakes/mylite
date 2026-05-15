# Baseline Trigonometric Functions Tasks

## Design

- [x] Verify official MySQL 8.4 documentation for `SIN()`, `COS()`, `TAN()`,
  and `COT()`.
- [x] Probe MySQL 8.4.9 runtime behavior for admitted values, diagnostics,
  warning staging, wrong arity, `COT(0)`, `FROM DUAL`, and `DO`.
- [x] Write independently authored `specs.md` with MyLite grammar snippets,
  ownership boundaries, runtime semantics, diagnostics, tests, and scope.

## Implementation

- [x] Extend lexer/parser/AST support for `SIN`, `COS`, `TAN`, and `COT`.
- [x] Add runtime evaluation using the current scalar approximate operand
  helper and double formatter.
- [x] Preserve descriptor/catalog/storage isolation and `.mylite` preamble
  invariants.
- [x] Add deterministic unsupported diagnostics for deferred placements and
  operand forms.

## Tests and Docs

- [x] Add the MySQL 8.4.9 expectation script for the feature surface.
- [x] Add parser tests.
- [x] Add runtime tests for values, diagnostics, warning ordering, result
  conventions, and file-safety invariants.
- [x] Update `COMPATIBILITY.md` and
  `docs/compatibility/functions-numeric-math.md`.
- [x] Run focused build/tests, the MySQL expectation script, and
  `cmake --workflow --preset check`.
- [x] Review the final diff for MySQL behavior, architecture boundaries,
  performance, cleanup, and scope control.
