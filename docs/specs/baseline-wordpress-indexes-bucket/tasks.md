# Baseline WordPress Indexes Bucket Tasks

- [x] Reproduce and classify every reported failure against current MyLite and
  the WordPress test sources.
- [x] Fix fixture DDL blockers for string/text defaults and text/blob key
  length handling without broad MySQL-incompatible behavior.
- [x] Fix standalone `CREATE INDEX` / `DROP INDEX` paths for prefix parts,
  descending parts, comments, metadata, and physical index creation/removal.
- [x] Fix `SHOW TABLE STATUS` auto-increment metadata, timestamp placeholders,
  and numeric `WHERE` predicate behavior.
- [x] Fix `SHOW CREATE TABLE` rendering for comments, defaults, keys, table
  comments/options, and descriptor-owned indexes; keep SQLite-driver-only
  missing-table assertion behavior as a documented non-goal.
- [x] Fix `SELECT` index hint key resolution for aliases, table ownership,
  exact names, `PRIMARY`, and unambiguous prefixes.
- [x] Fix composite primary-key duplicate diagnostics and prefix-key metadata
  readback in `DESCRIBE` / `SHOW INDEX`; keep raw SQLite duplicate diagnostics
  as a documented non-goal for core MyLite.
- [x] Run focused MySQL expectation scripts, focused CTests, and applicable
  syntax checks.
- [x] Run `git diff --check`, `git diff --cached --check`, and
  `cmake --workflow --preset check`.
- [x] Review the batch, fix review findings, and record that the external
  WordPress verified bucket runner is not present in this checkout.
