# Baseline INFORMATION_SCHEMA KEYWORDS Tasks

## Design

- [x] Read project architecture, compatibility, and information-schema guidance.
- [x] Verify MySQL 8.4.9 behavior for `INFORMATION_SCHEMA.KEYWORDS` row counts,
  reserved counts, representative rows, metadata, case-insensitive `WORD`
  predicates, and successful warning/row-count behavior.
- [x] Specify static metadata ownership, query-surface reuse, diagnostics,
  ordering, compatibility gaps, and test expectations.

## Implementation

- [ ] Add `KEYWORDS` to the limited information-schema registry.
- [ ] Add the MySQL 8.4.9-verified MyLite-owned static keyword row list.
- [ ] Add `WORD` and `RESERVED` system column definitions.
- [ ] Add the static system row builder and route it through the existing
  information-schema row construction path.
- [ ] Preserve descriptor authority, physical-name privacy, file-format safety,
  independent handles, reopen behavior, and zero-init cleanup.

## Tests and Docs

- [ ] Add the MySQL 8.4.9 expectation script for the feature surface.
- [ ] Add focused C runtime tests for rows, metadata, predicates, ordering,
  limits, diagnostics, handle/reopen behavior, and result status.
- [ ] Update `COMPATIBILITY.md` and `docs/compatibility/metadata-information-schema.md`.
- [ ] Run focused build/tests, the MySQL expectation script, and
  `cmake --workflow --preset check`.
- [ ] Review the final diff for MySQL behavior, metadata claims, performance,
  file-format safety, and scope control.

