# Baseline ADDTIME and SUBTIME Functions Tasks

- [x] Read current architecture, compatibility, parser, runtime scalar, temporal
  arithmetic, and test patterns.
- [x] Verify the supported and deferred `ADDTIME()` / `SUBTIME()` behavior
  against MySQL 8.4.9.
- [x] Write the feature specification before implementation.
- [x] Add MySQL 8.4.9 expectation script for the supported and deferred
  user-visible behavior.
- [x] Extend lexer/parser/AST support for `ADDTIME()` and `SUBTIME()`,
  including wrong-arity AST nodes and ordinary identifier handling.
- [x] Add parser tests for valid calls, wrong arity, whitespace, identifiers,
  and `IGNORE_SPACE` behavior.
- [x] Add runtime scalar implementation for canonical datetime/time string and
  `NULL` operands.
- [x] Add runtime tests for success, diagnostics, SQL modes, file safety, and
  independent handles.
- [x] Update compatibility documentation for the exact supported subset.
- [x] Run focused parser/runtime tests and the MySQL expectation script.
- [x] Run `cmake --build --preset dev` and `cmake --workflow --preset check`.
- [x] Review the final diff for architecture boundaries, MySQL evidence,
  temporal conversion scope, diagnostics, file-format safety, and test
  relevance.
