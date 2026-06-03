# Baseline WordPress Indexes Bucket

## Status

This slice tracks the current WordPress SQLite-driver indexes bucket as a
focused compatibility batch. The bucket is not a broad SQLite-driver emulation
mode. It adds MyLite-owned, MySQL-shaped behavior for SQL surfaces that
WordPress-oriented tests reach during setup and index introspection:

- WordPress-style text defaults and index DDL fixture setup;
- standalone `CREATE INDEX` / `DROP INDEX` metadata paths;
- `SHOW TABLE STATUS` auto-increment and predicate behavior;
- `SHOW CREATE TABLE` rendering for comments, defaults, keys, and table
  options;
- index hint key-name resolution;
- duplicate-key diagnostics for composite primary keys;
- `DESCRIBE` / `SHOW INDEX` prefix-key metadata.

Where a reported setup query is not accepted by MySQL 8.4.9, MyLite must keep
the compatibility decision narrow, documented, and deterministic. The default
policy remains MySQL 8 LTS behavior. Any tolerated extension must be justified
as an application-setup compatibility bridge and must not move compatibility
logic into the SQLite fork.

## Sources And Evidence

- MyLite project context:
  - `README.md`
  - `AGENTS.md`
  - `docs/architecture/engineering-standards.md`
- Existing MyLite feature specifications:
  - `docs/specs/baseline-wordpress-core-ddl-fixtures/specs.md`
  - `docs/specs/baseline-wordpress-posts-comments-ddl-fixtures/specs.md`
  - `docs/specs/baseline-wordpress-remaining-core-ddl-fixtures/specs.md`
  - `docs/specs/baseline-index-prefix-key-parts/specs.md`
  - `docs/specs/baseline-create-index-lifecycle/specs.md`
  - `docs/specs/baseline-drop-index-lifecycle/specs.md`
  - `docs/specs/baseline-descending-index-key-parts/specs.md`
  - `docs/specs/baseline-index-options-metadata/specs.md`
  - `docs/specs/baseline-show-create-table/specs.md`
  - `docs/specs/baseline-show-table-status-metadata/specs.md`
  - `docs/specs/baseline-show-table-status-where/specs.md`
  - `docs/specs/baseline-select-index-hints-noop/specs.md`
  - `docs/specs/baseline-alter-table-change-column-lifecycle/specs.md`
- Official MySQL 8.4 Reference Manual:
  - `CREATE TABLE`: <https://dev.mysql.com/doc/refman/8.4/en/create-table.html>
  - `CREATE INDEX`: <https://dev.mysql.com/doc/refman/8.4/en/create-index.html>
  - `DROP INDEX`: <https://dev.mysql.com/doc/refman/8.4/en/drop-index.html>
  - `SHOW CREATE TABLE`: <https://dev.mysql.com/doc/refman/8.4/en/show-create-table.html>
  - `SHOW TABLE STATUS`: <https://dev.mysql.com/doc/refman/8.4/en/show-table-status.html>
  - Index hints: <https://dev.mysql.com/doc/refman/8.4/en/index-hints.html>
- Reported WordPress test methods in the cloned
  `build/wp-sqlite-integration-src/packages/mysql-on-sqlite/tests/` tree.
- MySQL 8.4.9 runtime probes from existing expectation scripts and new probes
  added for this bucket.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, WordPress implementation code, or
other restrictively licensed sources.

## Scope

The batch must address the reported failure groups:

- fixture DDL rejected before assertions:
  - string-family empty defaults in setup tables;
  - empty ordinary string defaults on `TEXT` family setup columns, accepted as
    a narrow WordPress bridge with the MySQL `1101` warning shape and no visible
    metadata default;
  - full text-family key parts with missing prefixes, accepted as descriptor
    owned SQLite full-value indexes for setup compatibility while BLOB-family
    and JSON key parts remain rejected;
- standalone index paths:
  - `CREATE INDEX ... (text_col(prefix))`;
  - ordered key parts such as `value DESC`;
  - `COMMENT '...'` index options;
  - `DROP INDEX name ON table`;
  - no PRAGMA text may be passed through MyLite SQL parsing;
- `SHOW TABLE STATUS`:
  - non-auto-increment tables report `Auto_increment = NULL`;
  - fresh auto-increment tables report `1`;
  - sequence values advance and reset through DML / `TRUNCATE`;
  - numeric `WHERE` predicates over `Auto_increment` compare as numbers;
  - timestamp and placeholder metadata remain deterministic for MyLite tests;
- `SHOW CREATE TABLE`:
  - comments, defaults, index comments, and table comments render from
    descriptors;
  - standalone-created indexes render through descriptors;
  - not-found behavior remains MySQL-compatible in core MyLite tests and may
    be assertion-overridden by the external verified runner;
- index hints:
  - valid names and unambiguous prefixes resolve against the hinted table's own
    descriptors, including aliases;
  - mixed valid and stale names are accepted as a no-op when at least one name
    resolves, matching the reported WordPress setup shape;
  - all-missing name lists keep the MySQL `1176 / 42000` shape;
- key diagnostics and metadata:
  - composite primary-key duplicate diagnostics use MySQL-shaped tuple text;
  - `DESCRIBE` and `SHOW INDEX` report prefix key metadata consistently.

## Non-Goals

This batch must not implement:

- broad SQLite-driver internal behavior or PRAGMA result emulation in core SQL;
- full WordPress or `dbDelta()` compatibility;
- a MySQL-incompatible global parser mode;
- optimizer use of hints or secondary indexes;
- full InnoDB statistics, privileges, partitions, or temporary-table status;
- full Unicode collation semantics beyond existing MyLite baselines;
- SQLite-driver-only assertion behavior such as raw SQLite duplicate-key
  diagnostics or zero-row `SHOW CREATE TABLE` for missing tables;
- SQLite fork patches.

## Verification

The release gate for this batch includes:

- focused C runtime tests for each changed surface;
- existing MySQL expectation scripts for the reused baseline specs;
- new MySQL expectation probes where behavior is not already recorded;
- `git diff --check` before staging and `git diff --cached --check` after
  staging;
- `cmake --workflow --preset check`;
- a final feature review against this spec and the referenced baseline specs.
