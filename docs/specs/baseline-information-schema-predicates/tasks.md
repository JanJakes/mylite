# Baseline INFORMATION_SCHEMA predicates tasks

1. Research and expectations
   - [x] Verify MySQL 8.4.9 behavior for metadata `LIKE`, `IN`, `BETWEEN`,
     `NULL`, and boolean predicate composition.
   - [x] Add a MySQL-runtime expectation script for the admitted surface.

2. Design and docs
   - [x] Specify the independently authored supported SQL subset.
   - [x] Document ownership boundaries and why filtering stays in the
     synthetic metadata row path.
   - [x] Update `COMPATIBILITY.md` and `docs/compatibility/metadata-information-schema.md`.

3. Runtime
   - [x] Add three-valued metadata predicate evaluation.
   - [x] Support metadata `LIKE` with current SQL-mode escape behavior.
   - [x] Support metadata literal-list `IN`.
   - [x] Support metadata `BETWEEN`.
   - [x] Preserve existing descriptor-authority and no-SQLite-pass-through
     boundaries.

4. Tests
   - [x] Add or extend C tests for supported metadata predicates.
   - [x] Cover unsupported subquery `IN`, unsupported explicit `ESCAPE`,
     unknown columns, warnings, and reopen behavior.

5. Verification and review
   - [x] `cmake --build --preset dev`
   - [x] Focused parser and information-schema CTest entries.
   - [x] MySQL expectation script.
   - [x] `cmake --workflow --preset check`
   - [x] Subagent review, amend if needed, commit, and push `main`.
