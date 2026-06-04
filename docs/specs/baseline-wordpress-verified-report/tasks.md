# Baseline WordPress Verified Report Tasks

- [x] Inspect the php-extension worktree, active processes, and latest verified
  report.
- [x] Rerun the verified report after the first harness cleanup to refresh the
  failure inventory.
- [x] Fix remaining experiment-harness and SQLite-driver-internal assumption
  failures without broadening core SQL semantics.
- [ ] Fix metadata/collation/table-status failures.
- [ ] Fix auto-increment and identity failures.
- [ ] Fix DDL, index, and constraint failures.
- [ ] Fix query expression, scalar function, and DML table-reference failures.
- [ ] Fix mysqli/result-column metadata failures.
- [ ] Rerun the verified WordPress report until it has no failures.
- [ ] Run release-gate checks, review, commit, push, and continue if the report
  exposes new failures.
