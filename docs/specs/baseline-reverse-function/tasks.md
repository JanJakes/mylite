# Baseline REVERSE String Function Tasks

## Design

- [x] Verify official MySQL 8.4 documentation for `REVERSE(str)`.
- [x] Verify MySQL 8.4.9 runtime behavior for supported scalar, `DUAL`, `DO`,
  row-backed, `NULL`, multibyte, binary-input, and malformed-arity forms.
- [x] Specify parser, AST, runtime, SQLite-helper, descriptor, diagnostics, and
  compatibility boundaries.

## Implementation

- [x] Add AST and Lemon grammar support for the exact one-argument
  `REVERSE()` function shape.
- [x] Add a MyLite-owned UTF-8 reversal helper and register it with SQLite
  through the public scalar-function API.
- [x] Add scalar no-source, `DUAL`, and `DO` evaluation.
- [x] Add descriptor-backed row-scalar planning, SQL emission, and parameter
  binding.
- [x] Add focused runtime and parser tests.
- [x] Update `COMPATIBILITY.md` and string compatibility docs.

## Verification

- [x] Run the MySQL 8.4.9 expectation script.
- [x] Run focused parser/runtime CTests.
- [x] Run `cmake --build --preset dev`.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff, fix gaps, commit, push `origin main`, and run a
  review subagent.
