# MySQL testing feedback task list

This task list tracks the reported compatibility feedback as implementation
work items. Expected behavior must be verified against the `mylite-mysql-849`
MySQL 8.4.9 runtime before each item is marked complete.

## Transaction and locking

- [x] Implement non-temporary DDL implicit-commit behavior inside explicit
      transactions, starting with `START TRANSACTION; CREATE TABLE ...`.
- [x] Support MyLite file-backed lock probes that use SQLite-style
      `BEGIN IMMEDIATE` without treating this as a MySQL SQL surface.

## Information schema and SHOW metadata

- [ ] Align `SELECT * FROM information_schema.TABLES` metadata with MySQL for
      row format, size fields, timestamps, and placeholder values. Row-format
      zero-placeholder fields, row counts, and small-table size fields are
      covered; timestamps remain open.
- [ ] Align `USE information_schema; SELECT * FROM tables` with the same
      MySQL-compatible metadata rows. Resolution and zero-placeholder fields
      plus row-count and small-table size fields are covered; timestamps remain
      open with the qualified `information_schema.TABLES` task.
- [x] Verify and complete `SHOW DATABASES` parser and runtime coverage;
      verify `SHOW TABLE SCHEMAS` remains a MySQL syntax error.
- [ ] Align `SHOW TABLE STATUS` values, including row counts, data length,
      timestamps, and `AUTO_INCREMENT`. Row counts, small-table size fields,
      secondary-index length, and `AUTO_INCREMENT` are covered; timestamps
      remain open.
- [x] Verify and complete `SHOW TABLE STATUS WHERE ...` filtering.
- [x] Align `CREATE TABLE ... AUTO_INCREMENT=100` metadata and status
      reporting with MySQL next-value expectations.
- [x] Align `SHOW CREATE TABLE` formatting and metadata, including index
      spacing, text-column defaults, comments, and `USING BTREE`.
- [x] Align `SHOW CREATE TABLE missing_table` diagnostics with MySQL.
- [ ] Verify and complete `DESCRIBE` / `SHOW COLUMNS` for generated schemas.
- [x] Verify and complete `SHOW INDEX` parsing and metadata, including
      cardinality placeholders where MySQL reports `0`.
- [ ] Align `information_schema.TABLE_CONSTRAINTS` ordering and content with
      MySQL.
- [ ] Complete check-constraint metadata needed by
      `information_schema.CHECK_CONSTRAINTS`.
- [ ] Normalize information-schema write-protection diagnostics to
      MySQL-style access-denied errors.
- [ ] Align dynamic database-name queries against
      `information_schema.SCHEMATA`, including casing and row counts.

## DDL grammar and metadata

- [x] Support `ALTER TABLE ... AUTO_INCREMENT=50`.
- [x] Support `CREATE TEMPORARY TABLE ... AUTO_INCREMENT=...`.
- [x] Make `CREATE TABLE IF NOT EXISTS t ...` a MySQL-compatible no-op when
      `t` exists.
- [x] Make `CREATE TEMPORARY TABLE IF NOT EXISTS t ...` a MySQL-compatible
      no-op when the temporary `t` exists.
- [x] Support or intentionally diagnose `CREATE FULLTEXT INDEX`.
- [x] Support or intentionally diagnose `ALTER TABLE ... ADD FULLTEXT INDEX`.
- [x] Support or intentionally diagnose `CREATE SPATIAL INDEX`.
- [ ] Complete `CREATE INDEX` forms with ordering, comments, and complex index
      options.
- [x] Verify and complete `DROP INDEX ...`.
- [ ] Complete complex `ALTER TABLE ... MODIFY/CHANGE COLUMN` forms,
      preservation behavior, and affected rows.
- [x] Support or intentionally diagnose `ALTER TABLE ... ADD/DROP CHECK`.
- [ ] Complete `CREATE TABLE ... CHECK (...)` syntax forms.
- [ ] Complete inline and table-level foreign-key DDL syntax coverage.
- [x] Support or intentionally diagnose `ALTER TABLE ... ADD/DROP FOREIGN KEY`.

## DML

- [x] Verify and complete `UPDATE ... WHERE ... ORDER BY ... LIMIT`.
- [x] Fix joined `UPDATE ... JOIN ... ON ...` name resolution such as `t1.id`.
- [ ] Complete more complex joined update forms.
- [x] Support `INSERT ... SELECT FROM DUAL`.
- [x] Verify and complete `INSERT ... SET ...` defaults and generated values.
- [x] Verify and complete `INSERT` without `INTO`.
- [ ] Complete complex insert value expressions.
- [x] Align `ON DUPLICATE KEY UPDATE` affected-row semantics.
- [ ] Align composite primary-key duplicate handling.
- [x] Make `FOUND_ROWS()` return `0` on a fresh connection before any prior
      successful `SELECT`.

## Variables and version reporting

- [x] Support `SELECT @@gtid_purged`, `@@log_bin`, and
      `@@log_bin_trust_function_creators` with embedded-compatible values.
- [ ] Complete additional `SET` syntaxes for session variables, booleans,
      keywords, and dump-style backup/restore scripts.
- [x] Decide and document whether `SELECT VERSION()` / `@@version` should
      report the MySQL compatibility target (`8.4.9`) or MyLite's own version,
      then align tests and docs.

## Expressions, literals, casts, and temporal coercion

- [ ] Align `NO_BACKSLASH_ESCAPES` string and pattern escaping, including
      `\0` and pattern matching.
- [x] Fix binary literal evaluation such as `SELECT 0b000000001`.
- [ ] Preserve string values containing null bytes instead of truncating at
      `\0`.
- [ ] Make `FROM_BASE64(TO_BASE64('binary\\0data'))` preserve binary null
      bytes.
- [ ] Align invalid and zero-date coercion/rejection with MySQL in strict and
      non-strict modes.
- [ ] Align non-strict `INSERT`/`UPDATE` behavior for missing and `NULL`
      `NOT NULL` values.
- [ ] Align numeric/string casts in strict and non-strict modes.
- [x] Complete `CAST(...)` and `CONVERT(...)` syntax, including
      `CONVERT ... USING utf8`.
- [x] Complete `SELECT DATE(...)` and date-function predicates.
- [ ] Align ambiguous-name handling in `GROUP BY`, `HAVING`, and `ORDER BY`.
- [x] Align literal expression column labels, such as `SELECT 'abc'`.
- [ ] Complete result column metadata for expressions that currently report no
      metadata.

## Test harness classification

- [ ] Classify SQLite-driver-internal probes that depend on `sqlite_master`,
      PRAGMA-like assumptions, or SQLite helper methods as internal-assumption
      tests rather than direct MySQL API gaps.
