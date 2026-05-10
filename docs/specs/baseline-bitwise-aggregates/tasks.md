# Baseline Bitwise Aggregates Tasks

- [x] Read project architecture, compatibility, parser, runtime aggregate,
  SQLite bootstrap/registration, diagnostics, and storage context.
- [x] Research official MySQL 8.4 aggregate, bit-operation, and function-name
  parsing documentation.
- [x] Probe MySQL 8.4.9 runtime behavior for `BIT_AND(column)`,
  `BIT_OR(column)`, and `BIT_XOR(column)`, including integer-family values,
  `NULL`, empty and no-match inputs, unsigned-64 output, labels, predicates,
  accepted-but-deferred forms, function-name whitespace, and diagnostics.
- [x] Write the independent feature spec with MyLite grammar snippets,
  ownership boundaries, runtime semantics, SQLite callback handling,
  diagnostics, and tests.
- [x] Add MySQL-runtime expectation script for this feature.
- [x] Update compatibility documentation for the exact limited subset.
- [ ] Add parser grammar, AST node names, keyword mapping, and parser tests.
- [ ] Register internal SQLite bitwise aggregate callbacks during connection
  bootstrap.
- [ ] Implement descriptor-driven bitwise aggregate planning and execution
  while keeping SQLite responsible for scans and predicate filtering.
- [ ] Add C runtime tests for success, diagnostics, persistence, labels,
  unsigned-64 formatting, and callback registration.
- [ ] Run focused build/tests and the MySQL expectation script.
- [ ] Run `cmake --workflow --preset check`.
- [ ] Review the final diff for architecture boundaries, result semantics,
  descriptor authority, scope control, and compatibility wording.
- [ ] Commit the implementation slice.
