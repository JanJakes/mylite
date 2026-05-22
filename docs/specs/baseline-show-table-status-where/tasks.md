# Baseline SHOW TABLE STATUS WHERE Tasks

## Design

- [x] Read project architecture, compatibility, and existing SHOW statement specs.
- [x] Verify `SHOW TABLE STATUS ... WHERE` syntax and behavior against MySQL
  8.4.9.
- [x] Specify the intentionally limited output-column predicate subset.
- [x] Define diagnostics, ownership boundaries, storage impact, and tests.

## Implementation

- [x] Extend the parser grammar and AST constructor span handling for
  `SHOW TABLE STATUS ... WHERE predicate`.
- [x] Add descriptor-row predicate filtering in runtime without querying SQLite
  metadata or changing catalog/file-format state.
- [x] Preserve existing `LIKE` behavior and reject `LIKE ... WHERE`, `ORDER BY`,
  and unsupported predicate forms.
- [x] Add MySQL expectation script coverage.
- [x] Extend parser and runtime C tests.
- [x] Update `COMPATIBILITY.md` and `docs/compatibility/sql-show-statements.md`.

## Verification

- [x] Run the MySQL 8.4.9 expectation script.
- [x] Run parser and `SHOW TABLE STATUS` runtime CTest entries.
- [x] Run focused SHOW/metadata regression entries if touched code warrants it.
- [x] Run `cmake --workflow --preset check`.
- [x] Review, commit, and push.
