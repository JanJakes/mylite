# Baseline TEXT Family Length Arguments Tasks

## Design

- [x] Read project architecture, compatibility, text type, binary string type,
  charset/collation, parser, runtime, storage, and SQLite fork guidance.
- [x] Verify MySQL 8.4.9 behavior for `TEXT(M)`, normalized metadata,
  effective `utf8mb4` / `ascii` / `binary` character sets, boundary values,
  and unsupported family-specific or signed/quoted forms.
- [x] Specify the independently authored MyLite grammar, descriptor
  normalization, diagnostics, ownership boundaries, and test plan.

## Implementation

- [x] Extend parser and AST support for optional `TEXT(M)` length spans.
- [x] Add planner normalization from pending `TEXT(M)` descriptors to existing
  `TEXT` or BLOB family descriptors based on effective charset.
- [x] Apply normalization consistently for `CREATE TABLE`, `ALTER TABLE ...
  ADD COLUMN`, `ALTER TABLE ... MODIFY COLUMN`, and `ALTER TABLE ... CHANGE
  COLUMN`.
- [x] Preserve existing row storage, result, metadata, descriptor cloning, and
  file-format paths by storing only normalized descriptors.

## Tests and Docs

- [x] Add the MySQL 8.4.9 expectation script for this feature surface.
- [x] Add parser and runtime C tests for supported and rejected behavior.
- [x] Update `COMPATIBILITY.md` and detailed compatibility docs only for the
  implemented subset.
- [x] Run focused parser/runtime tests, the MySQL expectation script, and
  `cmake --workflow --preset check`.
- [x] Review the final diff for architecture boundaries, descriptor authority,
  MySQL evidence, performance, and scope control.
