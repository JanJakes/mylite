# Baseline SYSDATE Function Tasks

- [x] Probe MySQL 8.4.9 runtime behavior for `SYSDATE()` syntax,
      `SET timestamp`, `time_zone`, `DO`, DML assignment, and deferred forms.
- [x] Write the independently authored feature specification.
- [x] Add MySQL-runtime expectation script for the supported and deferred
      user-visible behavior.
- [x] Extend lexer/parser/AST support for zero-argument `SYSDATE()`.
- [x] Add runtime scalar and DML conversion for the supported descriptor
      targets.
- [x] Add fast C parser/runtime coverage.
- [x] Update compatibility documentation with limited support wording.
- [x] Run focused tests, MySQL expectation script, and full check workflow.
- [x] Review the final diff, amend any findings, commit atomically, and push.
