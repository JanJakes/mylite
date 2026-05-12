# Baseline TIME Type Tasks

- [x] Research official MySQL 8.4 documentation for `TIME` syntax, literals,
  storage, and metadata.
- [x] Verify MySQL 8.4.9 runtime behavior for bare `TIME` descriptors, strict
  conversion, `INSERT IGNORE`, defaults, ordering, predicates, indexes, and
  deferred wider inputs.
- [x] Specify the narrow descriptor lifecycle, canonical conversion, signed
  time ordering, diagnostics, metadata, index behavior, and non-goals.
- [x] Add MySQL-runtime expectation script for supported and deferred behavior.
- [x] Add parser/AST support for bare `TIME` column types.
- [x] Add descriptor mapping, catalog validation, row-size accounting, and
  metadata rendering for `TIME`.
- [x] Add row DML/default conversion for canonical `TIME` strings, `NULL`, and
  `DEFAULT`, including supported `INSERT IGNORE` adjustment.
- [x] Add descriptor-backed `TIME` predicate and ordering support using signed
  total-second semantics rather than plain text order.
- [x] Add fast C runtime tests for DDL, DML, predicates, ordering, indexes,
  persistence, file-format safety, and diagnostics.
- [x] Update `COMPATIBILITY.md` and detailed compatibility docs.
- [x] Run focused parser/runtime/MySQL expectation tests.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff for MySQL behavior, descriptor authority, signed
  time ordering, physical SQL safety, metadata accuracy, and scope control.
