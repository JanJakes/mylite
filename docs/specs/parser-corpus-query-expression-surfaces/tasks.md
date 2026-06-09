# Parser Corpus Query Expression Surfaces Tasks

- [x] Verify representative MySQL 8.4.9 syntax and runtime expectations.
- [x] Add parser tests for parenthesized query expressions, query-block set
  operands, derived `VALUES`, `NATURAL` / `USING` joins, and window null
  treatment.
- [x] Add parser/AST support for the admitted query-expression surfaces.
- [x] Add runtime delegation for simple parenthesized wrappers and explicit
  diagnostics for unsupported query-expression semantics.
- [x] Add runtime tests for delegated wrappers and unsupported diagnostics.
- [x] Update compatibility documentation.
- [x] Rerun focused tests and parser corpus benchmark.
- [x] Review, fix findings, commit, and push.
