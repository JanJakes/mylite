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
- [x] Support `SERIAL` and `SERIAL DEFAULT VALUE` create-table aliases with
      MySQL-compatible normalized column, index, `SHOW CREATE TABLE`, and
      generated auto-increment insert behavior for supported base tables.
- [x] Align `SHOW CREATE TABLE` formatting and metadata, including index
      spacing, text-column defaults, comments, `USING BTREE`, foreign keys, and
      CHECK constraints. Stored, virtual, and default-virtual generated columns
      now render MySQL-compatible `GENERATED ALWAYS AS (...) STORED|VIRTUAL`
      clauses.
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
- [x] Support first-slice spatial column DDL and metadata, including geometry
      subtypes, `GEOMETRYCOLLECTION` normalization, SRID catalog metadata,
      `SHOW COLUMNS`, `SHOW CREATE TABLE`, `INFORMATION_SCHEMA.COLUMNS`, and
      table-backed `GEOMETRY` result metadata.
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
- [x] Support `DEFAULT NOW()` and parenthesized `DEFAULT (NOW())` in
      `CREATE TABLE`, including MySQL-compatible `INFORMATION_SCHEMA.COLUMNS`
      and `SHOW CREATE TABLE` normalization for fractional precision forms.
      `SHOW CREATE TABLE` now also renders parenthesized generated arithmetic
      defaults as expression defaults instead of quoted string literals.
- [x] Attach MySQL-specific duplicate DDL error codes for supported
      `CREATE TABLE` duplicate table, column, key-name, and primary-key
      validation failures instead of falling back to generic 1105 diagnostics.
- [x] Support first-slice `BIT` column DDL and metadata, including `BIT`,
      `BIT(n)` widths `1..64`, `INFORMATION_SCHEMA.COLUMNS`, `SHOW COLUMNS`,
      `SHOW CREATE TABLE`, table-backed result metadata, and bit/hex literal
      inserts with strict/`IGNORE` width diagnostics.

## DML

- [x] Verify and complete `UPDATE ... WHERE ... ORDER BY ... LIMIT`.
- [x] Fix joined `UPDATE ... JOIN ... ON ...` name resolution such as `t1.id`.
- [x] Complete more complex joined update forms. Comma joins, `USING` joins,
      `RIGHT JOIN`, self-join aliases, unqualified `USING` assignment targets,
      case-sensitive assignment qualifiers, and parenthesized nested join
      operands are covered against MySQL 8.4.9 expectations. Overlapping
      aliases of the same physical target row now merge assignments before the
      final write, including same-row updates, change-and-revert affected
      counts, and self-chain updates.
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
      values are covered. `SET GLOBAL group_concat_max_len` and
      `SET GLOBAL sql_mode` now mutate only the global value, with session
      `DEFAULT` picking up the current global value.
- [x] Decide and document whether `SELECT VERSION()` / `@@version` should
      report the MySQL compatibility target (`8.4.9`) or MyLite's own version,
      then align tests and docs.

## Expressions, literals, casts, and temporal coercion

- [x] Align `NO_BACKSLASH_ESCAPES` string and pattern escaping, including
      `\0` and pattern matching. Scalar literal decoding, DML string storage,
      scalar `LIKE`, explicit `LIKE ... ESCAPE`, and `SHOW ... LIKE` patterns
      now follow MySQL 8.4.9-observed behavior.
- [x] Fix binary literal evaluation such as `SELECT 0b000000001`.
- [x] Store bit and hex literals into `BIT` columns with MySQL-compatible byte
      width, zero padding, and overflow handling for covered insert paths.
- [x] Preserve string values containing null bytes instead of truncating at
      `\0`. Scalar `SELECT` literals and DML storage/update paths now preserve
      embedded NUL byte lengths, and scalar `LIKE` matching is length-aware for
      decoded NUL bytes in values and patterns. Raw decoded scalar results now
      preserve byte lengths through `mylite_column_bytes()`, and covered
      `QUOTE()`, `REPEAT()`, and `REVERSE()` paths are length-aware for
      embedded NUL bytes.
- [x] Make `FROM_BASE64(TO_BASE64('binary\\0data'))` preserve binary null
      bytes, including raw C API result access and covered downstream scalar
      string helpers.
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
- [x] Align first-slice `FLOAT`/`DOUBLE` target range coercion for covered DML
      assignment paths. Covered behavior includes strict 1264 rejection,
      non-strict and `INSERT IGNORE` clipping to finite endpoints, invalid
      approximate text truncation diagnostics, and single-precision storage
      rounding for `FLOAT` targets.
- [x] Align first-slice `BIGINT UNSIGNED AUTO_INCREMENT` generation and
      metadata above signed 64-bit range. Covered behavior includes
      `CREATE TABLE ... AUTO_INCREMENT=9223372036854775808`, generated inserts,
      explicit high-value sequence advancement, `ALTER TABLE ... AUTO_INCREMENT`
      raising and lowering, `INFORMATION_SCHEMA.TABLES.AUTO_INCREMENT`,
      `SHOW CREATE TABLE`, and public last-insert-id preservation.
- [ ] Align remaining numeric/string casts and coercions in strict and
      non-strict modes, including TEXT/BLOB limits, binary-string byte edge
      cases, charset-specific string validation, remaining floating-point
      display and warning-order edge cases, scalar
      `CAST`/`CONVERT` value semantics beyond the covered floating-point target
      slice, and conversion/truncation
      `IGNORE` demotion beyond the currently covered `INSERT IGNORE` invalid
      integer, integer range, invalid date, and `VARCHAR` truncation slice plus
      single-table `UPDATE IGNORE` numeric, temporal, string-length, `NULL`
      not-null, duplicate-key, and foreign-key slices plus joined
      `UPDATE IGNORE` assignment coercion, explicit `NULL` not-null coercion,
      and duplicate-key slices. Signed and unsigned `TINYINT`, `SMALLINT`,
      `MEDIUMINT`, `INT`, and covered `BIGINT` range
      clipping, plus first-slice `FLOAT`/`DOUBLE` target clipping, is covered
      for strict, non-strict, `INSERT IGNORE`, and single-table `UPDATE IGNORE`
      paths. `TINYTEXT` and `TINYBLOB`
      255-byte write limits and plain `TEXT`/`BLOB` 65,535-byte write limits
      are now covered for strict and non-strict `INSERT ... VALUES`, `REPLACE`,
      `REPLACE ... SET`, `INSERT IGNORE`, strict single-table `UPDATE`, and
      single-table `UPDATE IGNORE`, including strict `REPLACE` conflict-row
      preservation and UTF-8 boundary truncation for `TINYTEXT` and `TEXT`.
      Table-backed `DECIMAL` column stringification is now covered for
      `CAST(... AS CHAR)`, `CONCAT`, `HEX(CAST(... AS CHAR))`, `TO_BASE64`,
      and single-table `UPDATE` expression assignment.
      `MEDIUMTEXT` and `MEDIUMBLOB` 16,777,215-byte write limits are now
      covered for strict and non-strict `INSERT ... VALUES`, `INSERT ... SET`,
      `INSERT IGNORE`, `REPLACE ... VALUES`, `REPLACE ... SET`, strict and
      non-strict ODKU update assignments, `INSERT IGNORE ... ON DUPLICATE KEY
      UPDATE` demotion, strict and non-strict single-table `UPDATE`, and
      single-table `UPDATE IGNORE`, including strict `REPLACE` conflict-row
      preservation and UTF-8 boundary truncation for `MEDIUMTEXT`.
      Fixed `BINARY(N)` values now right-pad shorter write values with
      `0x00` bytes while preserving `VARBINARY(N)` shorter values, and covered
      strict/non-strict/`UPDATE IGNORE` overlength paths truncate or reject
      according to MySQL. Invalid UTF-8 byte sequences assigned to covered
      `utf8mb4` `VARCHAR` and `TEXT` columns now reject in strict mode and
      store the valid prefix with warning 1366 in non-strict and
      single-table `UPDATE IGNORE` paths, while binary columns preserve raw
      bytes. Scalar `CAST`/`CONVERT` to `DECIMAL(M,D)` now covers
      target-scale rounding, out-of-range endpoint clipping with warning 1264,
      truncated decimal strings with warning 1292 before range warning 1264,
      and non-finite decimal strings returning formatted zero with warning 1292.
      Scalar unsigned integer arithmetic now covers exact `BIGINT UNSIGNED`
      addition, subtraction, multiplication, `/`, `DIV`, and modulo above
      signed 64-bit range, including MySQL-style 1690 overflow/underflow
      diagnostics. Scalar `CAST`/`CONVERT` to signed and unsigned integer
      targets now reject negative approximate values below the signed 64-bit
      floor with MySQL-style 1690 diagnostics while preserving MySQL's
      positive approximate overflow clipping behavior. Hex and bit literal
      `CAST` / `CONVERT` to signed and unsigned integer targets now use the
      literal's numeric value instead of the decoded binary string bytes.
      Scalar numeric conversion for embedded-NUL text values now keeps the
      numeric prefix for signed, unsigned, decimal, and approximate targets
      while still treating the NUL plus following bytes as trailing garbage for
      warning emission.
      Positive signed integer string casts above the unsigned 64-bit range now
      preserve MySQL's truncation warning and signed-complement warning pair.
      DML numeric coercion now applies the same embedded-NUL handling for
      covered integer and approximate assignments, rejecting in strict mode and
      warning/storing the prefix in non-strict mode.
      `INSERT IGNORE ... ON DUPLICATE KEY UPDATE` update assignments now demote
      covered numeric, string-length, temporal, and `NOT NULL` coercion
      failures through MySQL-compatible warnings and coerced values, including
      source row numbers for conversion warnings and once-per-target-column
      `NOT NULL` warnings.
      Scalar `CAST` and
      `CONVERT` to explicit `utf8mb4`/`utf8mb3` character targets now reject
      invalid byte sequences with warning 1300 and return `NULL`, while
      binary targets preserve raw bytes. Default scalar `CHAR` casts and
      `CONVERT(..., CHAR)` now use the connection character set for the same
      invalid-byte validation, while `latin1` connection casts preserve raw
      bytes. Explicit `ascii` character targets and `CONVERT(... USING ascii)`
      now preserve source bytes and expose `ascii_general_ci` introspection like
      MySQL. Length-qualified nonbinary `CHAR(N)` casts now validate the full
      source byte string before truncation, matching MySQL's warning 1300
      precedence for invalid UTF-8 after the retained prefix. Length-qualified
      `CAST`/`CONVERT` to `CHAR(N) CHARACTER SET binary` now share `BINARY(N)`
      byte truncation, right-padding, warning text, and binary metadata.
      Single-byte `latin1` connection `CHAR(N)` casts and conversions now
      truncate by bytes rather than UTF-8 code points. Hex and
      bit literals in covered `INSERT ... VALUES`, `INSERT ... SET`, ODKU
      update assignments, single-table `UPDATE`, and joined `UPDATE` paths now
      resolve with target-aware MySQL semantics: numeric columns receive the
      literal numeric value and text/binary columns receive decoded bytes before
      normal column coercion.
      Scalar `CONVERT(... USING ...)` now transcodes between known `latin1` and
      `utf8mb4` character values for the covered representable-value slice
      while preserving invalid unknown byte-string validation behavior.
      `CAST(... AS CHAR ASCII)` and `CONVERT(..., CHAR ASCII)` now parse as
      MySQL's `latin1` shorthand, including charset/collation/coercibility
      introspection and incompatible-collation diagnostics.
      `LONGTEXT` and `LONGBLOB` now enter the covered string/binary DML
      coercion path for approximate numeric text conversion, and covered
      `LONGTEXT` invalid UTF-8 writes reject or demote like MySQL while paired
      `LONGBLOB` values preserve raw bytes. `LONGTEXT`/`LONGBLOB` maximum-size
      exhaustion remains deferred.
- [x] Complete `CAST(...)` and `CONVERT(...)` syntax, including
      MySQL-verified `CONVERT ... USING utf8` normalization to `utf8mb3`
      with warning 3719, `CHAR ASCII` shorthand mapping to `latin1`,
      FLOAT/DOUBLE target casts, DATE/TIME/DATETIME temporal target casts, and
      MySQL-observed character/binary
      result-column metadata for nullable literal character casts, no-length
      binary display lengths, and explicit nonbinary charset conversions.
- [x] Complete `SELECT DATE(...)`, date-function predicates, and typed
      DATE/DATETIME/TIMESTAMP comparisons against ISO date/datetime strings.
- [x] Align ambiguous-name handling in `GROUP BY`, `HAVING`, and `ORDER BY`.
      Covered behavior includes duplicate output-label diagnostics in the
      correct clause context, table-column-vs-alias warning behavior, and
      nonaggregate `HAVING` filtering over projected labels without treating
      `HAVING` alone as an aggregate query.
- [x] Align literal expression column labels, such as `SELECT 'abc'`.
- [ ] Complete result column metadata for expressions that currently report no
      metadata. FK information-schema result metadata is now covered for
      `TABLE_CONSTRAINTS`, `KEY_COLUMN_USAGE`, and `REFERENTIAL_CONSTRAINTS`;
      `INFORMATION_SCHEMA.CHECK_CONSTRAINTS` now reports MySQL-shaped
      descriptors for `CONSTRAINT_CATALOG`, `CONSTRAINT_SCHEMA`,
      `CONSTRAINT_NAME`, and `CHECK_CLAUSE`;
      `INFORMATION_SCHEMA.COLUMNS` and `INFORMATION_SCHEMA.STATISTICS` now
      report MySQL-shaped descriptors for their direct wildcard result sets;
      `INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY` now reports
      MySQL-shaped descriptors for `COLLATION_NAME` and `CHARACTER_SET_NAME`;
      `INFORMATION_SCHEMA.SCHEMATA` now reports MySQL-shaped descriptors for
      `CATALOG_NAME`, `SCHEMA_NAME`, default charset/collation,
      `SQL_PATH`, and `DEFAULT_ENCRYPTION`;
      `INFORMATION_SCHEMA.KEYWORDS` now reports MySQL-shaped `WORD` and
      `RESERVED` result descriptors;
      `SHOW DATABASES`, `SHOW TABLES`, `SHOW TABLE STATUS`, and table
      maintenance result sets now attach MySQL-verified descriptors; static
      `SHOW VARIABLES`, `SHOW STATUS`, `SHOW CHARACTER SET`, `SHOW COLLATION`,
      `SHOW WARNINGS`, `SHOW ERRORS`, and diagnostic count result sets are now
      covered too; `SHOW COLUMNS` / `SHOW FIELDS`, `DESCRIBE`, `SHOW INDEX`,
      `SHOW CREATE DATABASE`, and `SHOW CREATE TABLE` target result sets are
      covered as well. Conditional scalar functions `IF`, `IFNULL`, `NULLIF`,
      and `COALESCE` now infer MySQL-shaped result descriptors for the covered
      scalar domains, and hex/bit literals now infer MySQL-shaped binary
      descriptors for zero-row table-backed results. Table-backed numeric
      scalar functions `ABS`, `MOD`, `FLOOR`, `CEIL`, and `CEILING` now infer
      MySQL-shaped descriptors for covered integer, unsigned integer, decimal,
      approximate, text, and `NULL` argument domains. JSON scalar and path
      functions now infer byte-scaled JSON document lengths, argument-shaped
      `JSON_QUOTE()` / plain-string `JSON_UNQUOTE()` lengths, and long-blob
      metadata for JSON unquote expressions. `DATE_FORMAT()` and
      `TIME_FORMAT()` now infer nullable connection-character-set string
      descriptors for scalar and table-backed expressions, including
      literal-format token expansion and dynamic-format width estimates.
      `REGEXP_REPLACE()` now reports the MySQL 8.4.9-shaped default
      `utf8mb4` result descriptor (`LONG_BLOB`, length `67108864`, nullable,
      no `NOT_NULL` flag) instead of a medium text descriptor.
      User-variable reads now report MySQL-shaped nullable descriptors for
      assigned signed integer and default `utf8mb4` text values, including
      `LONG_BLOB` text metadata with length `268435440` and no flags.
      `REGEXP_SUBSTR()` now reports source-shaped nullable string descriptors
      for literal and table-column source expressions instead of defaulting to
      the generic text length.
      All-`NULL` `ELT()` result lists now report MySQL-shaped binary
      `VAR_STRING` metadata with length 0, collation id 63, decimals 31, and
      the `BINARY` flag.
      `UNIX_TIMESTAMP()` now reports MySQL-shaped `LONGLONG` metadata for
      no-argument, `NULL`, integer, date, and non-fractional temporal inputs,
      plus `NEWDECIMAL(13+fsp, fsp)` metadata for fractional temporal, text,
      approximate, and fixed-point inputs.
      Table-backed `LEFT()` / `RIGHT()` expressions with nonnegative literal
      counts now infer MySQL-shaped connection-character-set widths capped at
      the source width, negative literal counts infer length 0, and dynamic or
      `NULL` counts use the source width.
      Table-backed `REPLACE()` expressions now infer MySQL-shaped string
      widths from the source expression and replacement expression, including
      same-width text replacements, widened text replacements, dynamic text
      replacements, and `NULL` replacements.
      Dynamic `SPACE()` arguments now report MySQL-shaped nullable `LONG_BLOB`
      descriptors with connection-character-set collation and the
      MySQL-observed dynamic width.
      Table-backed `SUBSTRING()` / `SUBSTR()` / `MID()` expressions now infer
      MySQL-shaped widths for constant lengths, literal positive and negative
      positions, dynamic lengths capped by literal positions, and fully dynamic
      source-width fallback.
      Table-backed `LPAD()` / `RPAD()` expressions with nonnegative literal
      target lengths and `REPEAT()` expressions with nonnegative literal counts
      now infer MySQL-shaped connection-character-set string widths, while
      dynamic, `NULL`, and negative target/count expressions infer the
      MySQL-observed nullable `LONG_BLOB` descriptors.
      Supported string functions over `VARBINARY` sources, including
      `CONCAT()`, `CONCAT_WS()`, `LEFT()` / `RIGHT()`, `SUBSTRING()` /
      `SUBSTR()` / `MID()`, `SUBSTRING_INDEX()`, `TRIM()` / `LTRIM()` /
      `RTRIM()`, `REVERSE()`, `LPAD()` / `RPAD()`, `REPEAT()`, `REPLACE()`,
      and `INSERT()`, now infer MySQL-shaped binary `VAR_STRING` descriptors
      with byte-counted lengths, collation id 63, decimals 31, and the
      `BINARY` flag.
      `FOUND_ROWS()` now reports MySQL-shaped signed `LONGLONG` flags while
      neighboring session integer functions retain their verified signedness.
      `RAND()` / `RAND(seed)` now report MySQL-shaped `DOUBLE` metadata with
      seed-nullability-aware `NOT_NULL` flags.
      `GROUP_CONCAT()` over `VARBINARY` and mixed binary/text argument lists now
      reports MySQL-shaped binary metadata with byte-counted lengths, while
      numeric arguments remain connection-text results.
      Table-backed `DECIMAL` arithmetic with exact operands now reports
      MySQL-shaped `NEWDECIMAL` descriptors for the covered `+`, `-`, and `*`
      result columns.
      Standalone `VALUES ROW(...)` result metadata now has MySQL-verified
      regression coverage for zero-row mixed numeric/string descriptors,
      character-width widening, and multi-column numeric/string rows.
      Parenthesized table-column references and unary-positive references
      through parentheses now preserve base-column descriptor and origin
      metadata instead of falling back to empty expression origins.
      Direct wildcard projections now have regression coverage for full
      visible-column descriptors, and empty table-backed result sets without
      `LIMIT 0` now verify metadata before stepping to `DONE`.
      Approximate numeric values stored into `VARCHAR` and `TEXT` through
      covered `INSERT ... VALUES` and single-table `UPDATE` paths now use
      MySQL-shaped compact DOUBLE display text, including lowercase exponent
      markers without positive exponent plus signs.
      Remaining expression and other SQLite-backed result metadata gaps are
      still tracked here.

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
