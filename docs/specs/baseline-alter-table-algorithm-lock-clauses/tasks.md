# Baseline ALTER TABLE ALGORITHM/LOCK Clauses Tasks

- [x] Verify MySQL 8.4.9 behavior for supported option tails, invalid placement,
      and selected invalid combinations.
- [x] Specify the exact supported grammar, runtime semantics, diagnostics,
      architecture boundaries, and documentation scope.
- [x] Extend parser/AST support for comma-separated option tails without moving
      existing ALTER statement child indexes.
- [x] Add runtime validation for admitted algorithm/action and lock combinations.
- [x] Reuse existing descriptor-driven ALTER implementations after validation.
- [x] Add MySQL-runtime expectation script coverage.
- [x] Add focused C parser/runtime tests and register any new CTest binary.
- [x] Update compatibility documentation with partial support wording.
- [x] Run focused parser/runtime/ALTER tests and MySQL 8.4.9 behavior probes.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the diff for scope, MySQL evidence, architecture, file-format
      safety, and deterministic diagnostics.
