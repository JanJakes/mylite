# Baseline REVERSE String Function Tasks

## Design

- [x] Verify official MySQL 8.4 documentation for `REVERSE(str)`.
- [x] Verify MySQL 8.4.9 runtime behavior for supported scalar, `DUAL`, `DO`,
  row-backed, `NULL`, multibyte, binary-input, and malformed-arity forms.
- [x] Specify parser, AST, runtime, SQLite-helper, descriptor, diagnostics, and
  compatibility boundaries.

## Implementation

- [ ] Add AST and Lemon grammar support for the exact one-argument
  `REVERSE()` function shape.
- [ ] Add a MyLite-owned UTF-8 reversal helper and register it with SQLite
  through the public scalar-function API.
- [ ] Add scalar no-source, `DUAL`, and `DO` evaluation.
- [ ] Add descriptor-backed row-scalar planning, SQL emission, and parameter
  binding.
- [ ] Add focused runtime and parser tests.
- [ ] Update `COMPATIBILITY.md` and string compatibility docs.

## Verification

- [ ] Run the MySQL 8.4.9 expectation script.
- [ ] Run focused parser/runtime CTests.
- [ ] Run `cmake --build --preset dev`.
- [ ] Run `cmake --workflow --preset check`.
- [ ] Review the final diff, fix gaps, commit, push `origin main`, and run a
  review subagent.
