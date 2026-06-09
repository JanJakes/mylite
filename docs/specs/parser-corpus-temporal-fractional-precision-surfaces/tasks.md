# Parser Corpus Temporal Fractional Precision Surfaces Tasks

- [x] Research official MySQL 8.4 fractional temporal precision behavior.
- [x] Verify MySQL 8.4.9 runtime behavior for `fsp = 0`, `fsp = 6`, and
  `fsp = 7` temporal columns, functions, and casts.
- [x] Add MySQL expectation script for the verified cases.
- [x] Add MyLite parser coverage for temporal column, function, and cast
  precision surfaces.
- [x] Add MyLite runtime coverage for `fsp = 0`, nonzero unsupported precision,
  too-large precision diagnostics, and no catalog mutation.
- [x] Implement AST payloads and parser productions for temporal precision.
- [x] Implement DDL and scalar/DML runtime validation.
- [x] Update compatibility docs and existing tests that currently expect syntax
  errors for the admitted syntax.
- [x] Run focused MySQL expectation, parser/runtime tests, corpus benchmark,
  diff checks, and full release gate.
- [x] Perform release-gate review and fix findings.
- [x] Commit and push.
