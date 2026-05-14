# Baseline ENUM Type Tasks

- [x] Research official MySQL 8.4 documentation and MySQL 8.4.9 runtime
  behavior for enum syntax, assignment, defaults, comparisons, metadata,
  ordering, invalid values, and key/index interactions.
- [x] Specify the narrow descriptor-owned enum type lifecycle, conversion
  policy, metadata, physical storage, performance boundary, diagnostics, and
  deferred ordinal-storage behavior.
- [x] Add MySQL-runtime expectation script for the supported enum subset and
  intentionally deferred behavior.
- [x] Add parser/AST support for `ENUM('label'[, ...])` column types while
  keeping `ENUM` usable as an identifier outside type contexts.
- [x] Add descriptor serialization/parsing helpers for enum label lists,
  trailing-space normalization, duplicate detection, max-label metadata, and
  label/ordinal lookup.
- [x] Add create/add-column planning and catalog descriptor support with
  physical SQLite `TEXT` storage and default validation.
- [x] Add strict enum conversion for `INSERT`, `REPLACE`, and one-assignment
  `UPDATE`, including omitted/default `NOT NULL` first-label behavior.
- [x] Add descriptor-backed enum predicate conversion for equality,
  null-safe equality, inequality, `IS NULL`, and `IS NOT NULL`.
- [x] Add enum introspection and result metadata for `SHOW COLUMNS`,
  `SHOW CREATE TABLE`, `INFORMATION_SCHEMA.COLUMNS`, and public result-column
  metadata flags.
- [x] Add fast C parser/runtime coverage for success cases, diagnostics,
  unsupported syntax, metadata, DML, predicates, persistence, file-format
  safety, independent handles, and zero-initialized cleanup.
- [x] Update `COMPATIBILITY.md` and detailed compatibility docs with exact
  limited wording.
- [x] Run focused build/tests and the MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff for MySQL behavior, descriptor authority, label
  normalization, conversion correctness, metadata accuracy, physical SQL
  safety, cleanup on failure, performance, scope control, and docs accuracy.
