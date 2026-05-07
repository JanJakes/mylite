# MySQL testing feedback task list

This task list tracks the reported compatibility feedback as implementation
work items. Expected behavior must be verified against the `mylite-mysql-849`
MySQL 8.4.9 runtime before each item is marked complete.

## Transaction and locking

- [x] Implement non-temporary DDL implicit-commit behavior inside explicit
      transactions, starting with `START TRANSACTION; CREATE TABLE ...` and
      ordinary `DROP TABLE` / `TRUNCATE TABLE` / standalone `CREATE INDEX` /
      `DROP INDEX` / `RENAME TABLE` / `ALTER TABLE` success/failure
      boundaries.
- [x] Support MyLite file-backed lock probes that use SQLite-style
      `BEGIN IMMEDIATE` without treating this as a MySQL SQL surface.

## Information schema and SHOW metadata

- [x] Align `SELECT * FROM information_schema.TABLES` metadata with MySQL for
      row format, size fields, timestamps, and placeholder values. Row-format
      zero-placeholder fields, row counts, and small-table size fields are
      covered; `CREATE_TIME` now reports MySQL-shaped non-NULL datetimes while
      `UPDATE_TIME` and `CHECK_TIME` remain `NULL`.
- [x] Align `USE information_schema; SELECT * FROM tables` with the same
      MySQL-compatible metadata rows. Resolution and zero-placeholder fields
      plus row-count, small-table size fields, and timestamps are covered with
      the qualified `information_schema.TABLES` task.
- [x] Verify and complete `SHOW DATABASES` parser and runtime coverage;
      verify `SHOW TABLE SCHEMAS` remains a MySQL syntax error.
- [x] Align `SHOW TABLE STATUS` values, including row counts, data length,
      timestamps, and `AUTO_INCREMENT`. Row counts, small-table size fields,
      secondary-index length, `AUTO_INCREMENT`, and timestamp shapes are
      covered.
- [x] Verify and complete `SHOW TABLE STATUS WHERE ...` filtering.
- [x] Align `CREATE TABLE ... AUTO_INCREMENT=100` metadata and status
      reporting with MySQL next-value expectations.
- [x] Align `SHOW CREATE TABLE` formatting and metadata, including index
      spacing, text-column defaults, comments, `USING BTREE`, foreign keys, and
      CHECK constraints.
- [x] Align `SHOW CREATE TABLE missing_table` diagnostics with MySQL.
- [x] Verify and complete `DESCRIBE` / `SHOW COLUMNS` for generated schemas.
      Stored, virtual, and default-virtual generated-column metadata now
      reports MySQL-compatible `Extra` values.
- [x] Verify and complete `SHOW INDEX` parsing and metadata, including
      cardinality placeholders where MySQL reports `0`.
- [x] Align `information_schema.TABLE_CONSTRAINTS` ordering and content with
      MySQL for primary-key, unique, and CHECK rows. CHECK rows now report
      MySQL-compatible generated names and enforcement values; foreign-key rows
      remain tied to the deferred foreign-key catalog/enforcement work.
- [x] Complete check-constraint metadata needed by
      `information_schema.CHECK_CONSTRAINTS`. `CREATE TABLE ... CHECK` now
      records catalog-backed rows with MySQL 8.4.9-verified names, clauses, and
      enforcement flags.
- [x] Normalize information-schema write-protection diagnostics to
      MySQL-style access-denied errors.
- [x] Align dynamic database-name queries against
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
- [x] Complete `CREATE INDEX` forms with ordering, comments, and complex index
      options.
- [x] Verify and complete `DROP INDEX ...`.
- [x] Complete complex `ALTER TABLE ... MODIFY/CHANGE COLUMN` forms,
      preservation behavior, and affected rows. Runtime coverage now verifies
      successful metadata replacement, row preservation/conversion, copied-row
      affected counts, and strict rejection without mutation for invalid
      integer conversion, existing `NULL` values becoming `NOT NULL`, and
      overlong `VARCHAR` narrowing.
- [x] Support `ALTER TABLE ... ADD/DROP/ALTER CHECK` for CHECK-only and mixed
      statements, including generated names, existing-row validation,
      `NOT ENFORCED`, enforcement toggling, temporary tables, and
      MySQL-compatible 3819/3821/3822 diagnostics. CHECK actions mixed with
      supported column, index, and `AUTO_INCREMENT` table-option actions now
      share the atomic shadow-rewrite path.
- [x] Complete `CREATE TABLE ... CHECK (...)` syntax forms. Inline and
      table-level CHECK clauses parse and record catalog-backed metadata for
      `INFORMATION_SCHEMA.CHECK_CONSTRAINTS` and CHECK rows in
      `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`. Enforced CHECK constraints now
      reject covered invalid `INSERT`, ODKU, `REPLACE`, single-table `UPDATE`,
      joined `UPDATE`, and temporary-table DML rows; covered `IGNORE` paths
      skip invalid rows with warning 3819. DDL-time validation now rejects
      subqueries, variables, unsupported functions, unknown columns, and
      `AUTO_INCREMENT` column references with MySQL-compatible diagnostics.
- [x] Complete inline and table-level foreign-key DDL syntax coverage. Inline
      `REFERENCES` is parsed and ignored like the verified MySQL 8.4.9 shape;
      table-level `FOREIGN KEY` clauses parse and return a deterministic
      unsupported diagnostic without catalog mutation.
- [x] Support or intentionally diagnose `ALTER TABLE ... ADD/DROP FOREIGN KEY`.

## DML

- [x] Verify and complete `UPDATE ... WHERE ... ORDER BY ... LIMIT`.
- [x] Fix joined `UPDATE ... JOIN ... ON ...` name resolution such as `t1.id`.
- [x] Complete more complex joined update forms. Comma joins, `USING` joins,
      `RIGHT JOIN`, and self-join aliases are covered against MySQL 8.4.9
      expectations.
- [x] Support `INSERT ... SELECT FROM DUAL`.
- [x] Verify and complete `INSERT ... SET ...` defaults and generated values.
- [x] Verify and complete `INSERT` without `INTO`.
- [x] Complete complex insert value expressions.
- [x] Align `ON DUPLICATE KEY UPDATE` affected-row semantics.
- [x] Align composite primary-key duplicate handling. Composite primary-key
      errors, `INSERT IGNORE` warnings, nullable composite unique-key behavior,
      and composite primary-key ODKU updates are covered.
- [x] Make `FOUND_ROWS()` return `0` on a fresh connection before any prior
      successful `SELECT`.

## Variables and version reporting

- [x] Support `SELECT @@gtid_purged`, `@@log_bin`, and
      `@@log_bin_trust_function_creators` with embedded-compatible values.
- [x] Complete additional `SET` syntaxes for session variables, booleans,
      keywords, and dump-style backup/restore scripts. Boolean keyword values,
      unquoted string keyword values, and mutable `sql_notes` are covered; mixed
      user/system-variable dump assignment lists and user-variable restore
      values are covered.
- [x] Decide and document whether `SELECT VERSION()` / `@@version` should
      report the MySQL compatibility target (`8.4.9`) or MyLite's own version,
      then align tests and docs.

## Expressions, literals, casts, and temporal coercion

- [x] Align `NO_BACKSLASH_ESCAPES` string and pattern escaping, including
      `\0` and pattern matching. Scalar literal decoding, DML string storage,
      scalar `LIKE`, explicit `LIKE ... ESCAPE`, and `SHOW ... LIKE` patterns
      now follow MySQL 8.4.9-observed behavior.
- [x] Fix binary literal evaluation such as `SELECT 0b000000001`.
- [x] Preserve string values containing null bytes instead of truncating at
      `\0`. Scalar `SELECT` literals and DML storage/update paths now preserve
      embedded NUL byte lengths.
- [x] Make `FROM_BASE64(TO_BASE64('binary\\0data'))` preserve binary null
      bytes.
- [x] Align invalid and zero-date coercion/rejection with MySQL in strict and
      non-strict modes for supported `DATE`, `DATETIME`, and `TIMESTAMP` DML
      write paths. Covered strict/default `NO_ZERO_DATE` and `NO_ZERO_IN_DATE`
      rejection, non-strict malformed/calendar coercion warnings 1265/1264,
      DATE/DATETIME zero-in-date preservation, TIMESTAMP zero-in-date coercion,
      `ALLOW_INVALID_DATES` for DATE/DATETIME, and non-strict no-zero warnings.
      Broader temporal input variants and scalar temporal edge cases remain
      tracked in `COMPATIBILITY.md`.
- [x] Align non-strict `INSERT`/`UPDATE` behavior for missing and `NULL`
      `NOT NULL` values across the currently supported `INSERT ... VALUES`,
      `INSERT ... SET`, single-table `UPDATE`, and joined `UPDATE` surfaces.
      Covered behavior includes missing-default warning 1364 coercion, multi-row
      `INSERT ... VALUES` `NULL` warning 1048 coercion, single-row insert `NULL`
      error preservation, update `NULL`/`DEFAULT` coercion, changed-row affected
      counts, and numeric/text/date/datetime/time implicit defaults.
- [x] Align integer and decimal DML coercion in strict and non-strict modes for
      currently supported `INSERT ... VALUES`, `INSERT ... SET`, `REPLACE`,
      ODKU update assignments, single-table `UPDATE`, and joined `UPDATE`
      paths. Covered behavior includes integer half-away rounding, numeric
      string prefixes, strict truncation/incorrect-value rejection, non-strict
      warning coercion, decimal scale rounding notes, and DECIMAL result text
      shape.
- [x] Align `BIGINT UNSIGNED` DML assignment values above signed 64-bit range
      for currently supported write paths. Covered behavior includes exact
      endpoint storage, strict overflow rejection, non-strict and `IGNORE`
      clipping to `18446744073709551615`, quoted endpoint trailing-character
      diagnostics, direct integer literals above `UINT64_MAX`, and
      `CAST(... AS UNSIGNED)` update assignments into narrower unsigned
      integer columns.
- [x] Align signed `BIGINT` DML underflow for direct and quoted integer values.
      Covered behavior includes exact `-9223372036854775808` endpoint storage,
      strict error 1264 for `-9223372036854775809`, non-strict clipping to the
      signed endpoint, negative unsigned clipping to zero, and underflow range
      warnings before truncation warnings for numeric strings.
- [x] Align `CHAR`/`VARCHAR` length coercion in strict and non-strict modes for
      currently supported `INSERT ... VALUES`, `INSERT ... SET`, `REPLACE`,
      ODKU update assignments, single-table `UPDATE`, and joined `UPDATE`
      paths. Covered behavior includes strict 1406 rejection, non-strict 1265
      warning truncation, and assignment/column-order warning emission for
      covered forms.
- [ ] Align remaining numeric/string casts and coercions in strict and
      non-strict modes, including TEXT/BLOB limits, binary-string byte edge
      cases, charset-specific string validation, floating-point column edge
      cases, `BIGINT UNSIGNED` auto-increment and broad unsigned arithmetic
      beyond signed 64-bit range, scalar `CAST`/`CONVERT` value semantics, and
      conversion/truncation
      `IGNORE` demotion beyond the currently covered `INSERT IGNORE` invalid
      integer, integer range, invalid date, and `VARCHAR` truncation slice plus
      single-table `UPDATE IGNORE` numeric, temporal, string-length, `NULL`
      not-null, duplicate-key, and foreign-key slices. Signed and unsigned
      `TINYINT`, `SMALLINT`, `MEDIUMINT`, `INT`, and covered `BIGINT` range
      clipping is covered for strict, non-strict, `INSERT IGNORE`, and
      single-table `UPDATE IGNORE` paths.
- [x] Complete `CAST(...)` and `CONVERT(...)` syntax, including
      MySQL-verified `CONVERT ... USING utf8` normalization to `utf8mb3`
      with warning 3719.
- [x] Complete `SELECT DATE(...)` and date-function predicates.
- [x] Align ambiguous-name handling in `GROUP BY`, `HAVING`, and `ORDER BY`.
      Covered behavior includes duplicate output-label diagnostics in the
      correct clause context, table-column-vs-alias warning behavior, and
      nonaggregate `HAVING` filtering over projected labels without treating
      `HAVING` alone as an aggregate query.
- [x] Align literal expression column labels, such as `SELECT 'abc'`.
- [ ] Complete result column metadata for expressions that currently report no
      metadata. FK information-schema result metadata is now covered for
      `TABLE_CONSTRAINTS`, `KEY_COLUMN_USAGE`, and `REFERENTIAL_CONSTRAINTS`;
      `SHOW DATABASES`, `SHOW TABLES`, `SHOW TABLE STATUS`, and table
      maintenance result sets now attach MySQL-verified descriptors; static
      `SHOW VARIABLES`, `SHOW STATUS`, `SHOW CHARACTER SET`, `SHOW COLLATION`,
      `SHOW WARNINGS`, `SHOW ERRORS`, and diagnostic count result sets are now
      covered too; `SHOW COLUMNS` / `SHOW FIELDS`, `DESCRIBE`, `SHOW INDEX`,
      `SHOW CREATE DATABASE`, and `SHOW CREATE TABLE` target result sets are
      covered as well. Remaining expression and other SQLite-backed result
      metadata gaps are still tracked here.

## Test harness classification

- [x] Classify SQLite-driver-internal probes that depend on `sqlite_master`,
      PRAGMA-like assumptions, or SQLite helper methods as internal-assumption
      tests rather than direct MySQL API gaps. MySQL 8.4.9 treats
      `sqlite_master` / `sqlite_schema` as ordinary missing user tables and
      rejects `PRAGMA` syntax; therefore MyLite must not implement SQLite
      catalog or PRAGMA compatibility as a MySQL-facing feature. Direct
      `sqlite3_*` storage probes remain valid only as MyLite internal storage
      invariant tests, not MySQL runtime comparison failures.

## Test harness classification policy

Compatibility failures must be sorted by the API surface they exercise before
they become product tasks:

- MySQL API probes are SQL statements or client metadata calls issued through
  MyLite's MySQL-facing public API. They must be compared against MySQL 8.4.9
  and tracked as compatibility work when behavior differs.
- MyLite storage-invariant probes are direct SQLite-handle checks, physical
  table-name checks, `.mylite` file-format checks, VFS offset checks,
  `sqlite_schema` / `sqlite_master` reads, PRAGMA statements, or helper methods
  that exist only in a SQLite-backed test adapter. These are classified as
  `sqlite-internal-assumption` and must not be counted as MySQL API gaps.
- If an application sends SQLite introspection SQL through the MySQL-facing
  API, MyLite should follow MySQL behavior instead of SQLite behavior:
  `sqlite_master` / `sqlite_schema` are ordinary missing tables in a user
  schema, and `PRAGMA ...` is a syntax error.
