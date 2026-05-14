# Baseline SET Type Tasks

- [x] Research official MySQL 8.4 documentation and MySQL 8.4.9 runtime
  behavior for set syntax, assignment, defaults, comparisons, metadata,
  ordering, invalid values, and key/index interactions.
- [x] Specify the narrow descriptor-owned set type lifecycle, conversion
  policy, metadata, physical storage, performance boundary, diagnostics, and
  deferred bitmap-storage behavior.
- [x] Add MySQL-runtime expectation script for the supported set subset and
  intentionally deferred behavior.
- [ ] Add parser/AST support for `SET('member'[, ...])` column types while
  keeping `SET` usable as an identifier outside type contexts.
- [ ] Add descriptor serialization/parsing helpers for set member lists,
  trailing-space normalization, comma rejection, duplicate detection,
  max-display metadata, bitmap lookup, and canonical display rendering.
- [ ] Add create/add-column planning and catalog descriptor support with
  physical SQLite `TEXT` storage and default validation.
- [ ] Add strict set conversion for `INSERT`, `REPLACE`, and one-assignment
  `UPDATE`, including omitted/default no-explicit-default behavior.
- [ ] Add descriptor-backed set predicate conversion for equality,
  null-safe equality, inequality, `IS NULL`, and `IS NOT NULL`.
- [ ] Add set introspection and result metadata for `SHOW COLUMNS`,
  `SHOW CREATE TABLE`, `INFORMATION_SCHEMA.COLUMNS`, and public result-column
  metadata flags.
- [ ] Add fast C parser/runtime coverage for success cases, diagnostics,
  unsupported syntax, metadata, DML, predicates, persistence, file-format
  safety, independent handles, and zero-initialized cleanup.
- [ ] Update `COMPATIBILITY.md` and detailed compatibility docs with exact
  limited wording.
- [ ] Run focused build/tests and the MySQL expectation script.
- [ ] Run `cmake --workflow --preset check`.
- [ ] Review the final diff for MySQL behavior, descriptor authority, member
  normalization, conversion correctness, metadata accuracy, physical SQL
  safety, cleanup on failure, performance, scope control, and docs accuracy.
