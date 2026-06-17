# Baseline Row-Scalar ORDER BY Expressions Tasks

- [x] Specify the executable subset and compatibility limits.
- [x] Add MySQL-runtime expectation probes for representative supported
  function families.
- [x] Widen parser admission for supported row-scalar `SELECT ORDER BY` keys.
- [x] Route admitted single-table row-scalar order keys through the existing
  row-scalar planner.
- [x] Keep joined, grouped, DML, and tableless order-expression limits explicit.
- [x] Add focused runtime regression coverage.
- [x] Run focused parser/runtime/MySQL verification.
- [x] Run full release checks and review the final diff.
