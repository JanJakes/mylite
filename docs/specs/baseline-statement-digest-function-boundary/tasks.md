# Baseline STATEMENT_DIGEST function boundary tasks

- [x] Verify MySQL 8.4.9 `STATEMENT_DIGEST()` NULL, metadata, arity, and hash
      mismatch behavior.
- [x] Specify the MyLite boundary and explicitly reject guessed non-NULL hash
      computation.
- [x] Register `STATEMENT_DIGEST()` as a native one-argument system function.
- [x] Implement NULL result behavior and deterministic unsupported diagnostics
      for non-NULL arguments.
- [x] Add scalar and row-backed result metadata for the function.
- [x] Extend charset, collation, coercibility, and collate metadata handling.
- [x] Add MySQL expectation and MyLite runtime tests.
- [x] Update compatibility documentation.
