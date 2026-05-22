# Baseline Multi-Action ALTER TABLE Tasks

- [x] Research official MySQL 8.4 docs and MySQL 8.4.9 runtime behavior for
  supported multi-action `ALTER TABLE` lists, result counts, warning counts,
  metadata side effects, and atomic rollback.
- [x] Add MySQL-runtime expectation script covering successful add-column,
  add/drop-index, add-column-plus-index, duplicate-unique rollback, and
  deferred accepted MySQL forms.
- [x] Add parser/AST support for a shared target-table multi-action node and
  action-list child nodes without changing existing single-action grammar.
- [x] Add runtime execution with one catalog mutation, one commit, rollback on
  failure, and connection-local SQLite schema generation incremented once when
  physical schema changed.
- [x] Refactor existing `ADD COLUMN`, persistent `ADD INDEX`/`ADD UNIQUE`, and
  persistent `DROP INDEX` execution into lower-level helpers that can run
  inside an active mutation while preserving single-action behavior.
- [x] Add C runtime coverage for successful action lists, descriptor visibility
  after each action, atomic rollback, reopen persistence, result shape,
  diagnostics, and unsupported multi-action forms.
- [x] Update `COMPATIBILITY.md`,
  `docs/compatibility/sql-table-ddl.md`, and
  `docs/compatibility/sql-indexes-constraints.md` with exact limited wording.
- [x] Run focused parser/runtime tests, the MySQL expectation script, and
  `cmake --workflow --preset check`.
- [x] Review the final diff for architecture boundaries, public ABI stability,
  independently authored grammar/spec text, MySQL evidence, catalog authority,
  atomicity, cleanup on failure, warning preservation, compatibility accuracy,
  and test relevance.
