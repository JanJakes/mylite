# Baseline TIME Type Tasks

- [x] Research official MySQL 8.4 documentation for `TIME` syntax, literals,
  storage, and metadata.
- [x] Verify MySQL 8.4.9 runtime behavior for bare `TIME` descriptors, strict
  conversion, `INSERT IGNORE`, defaults, ordering, predicates, indexes, and
  deferred wider inputs.
- [x] Specify the narrow descriptor lifecycle, canonical conversion, signed
  time ordering, diagnostics, metadata, index behavior, and non-goals.
- [x] Add MySQL-runtime expectation script for supported and deferred behavior.
- [ ] Add parser/AST support for bare `TIME` column types.
- [ ] Add descriptor mapping, catalog validation, row-size accounting, and
  metadata rendering for `TIME`.
- [ ] Add row DML/default conversion for canonical `TIME` strings, `NULL`, and
  `DEFAULT`, including supported `INSERT IGNORE` adjustment.
- [ ] Add descriptor-backed `TIME` predicate and ordering support using signed
  total-second semantics rather than plain text order.
- [ ] Add fast C runtime tests for DDL, DML, predicates, ordering, indexes,
  persistence, file-format safety, and diagnostics.
- [ ] Update `COMPATIBILITY.md` and detailed compatibility docs.
- [ ] Run focused parser/runtime/MySQL expectation tests.
- [ ] Run `cmake --workflow --preset check`.
- [ ] Review the final diff for MySQL behavior, descriptor authority, signed
  time ordering, physical SQL safety, metadata accuracy, and scope control.
