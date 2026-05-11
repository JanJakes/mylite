# Type system, literals, and conversion

## Numeric types

| Feature | Status | Notes |
| --- | --- | --- |
| `TINYINT` | 🟡 | Limited DDL descriptors plus optional single explicit `SIGNED` or `UNSIGNED`, deprecated display width `0..255`, optional `DEFAULT NULL` or in-range decimal integer/`TRUE`/`FALSE` defaults, `ALTER TABLE ... MODIFY [COLUMN]` and `CHANGE [COLUMN]` descriptor replacement, integer/`NULL` `INSERT`/`REPLACE` row and `UPDATE` assignment conversion, text readback, descriptor-driven filtered `SELECT`/`DELETE`/`UPDATE` predicate conversion with MyLite's current descriptor-range literal check, and single-column sort support; only signed `TINYINT(1)` is persisted/rendered with width, other widths normalize away with warning 1681; no `ZEROFILL`, repeated/combined attributes, aliases, general expression defaults, general expression semantics, compact physical storage, or protocol-grade result metadata |
| `SMALLINT` | 🟡 | Limited DDL descriptors plus optional single explicit `SIGNED` or `UNSIGNED`, deprecated display width `0..255`, optional `DEFAULT NULL` or in-range decimal integer/`TRUE`/`FALSE` defaults, `ALTER TABLE ... MODIFY [COLUMN]` and `CHANGE [COLUMN]` descriptor replacement, integer/`NULL` `INSERT`/`REPLACE` row and `UPDATE` assignment conversion, text readback, descriptor-driven filtered `SELECT`/`DELETE`/`UPDATE` predicate conversion with MyLite's current descriptor-range literal check, and single-column sort support; display widths normalize away with warning 1681; no `ZEROFILL`, repeated/combined attributes, aliases, general expression defaults, general expression semantics, compact physical storage, or protocol-grade result metadata |
| `MEDIUMINT` | 🟡 | Limited DDL descriptors plus optional single explicit `SIGNED` or `UNSIGNED`, deprecated display width `0..255`, optional `DEFAULT NULL` or in-range decimal integer/`TRUE`/`FALSE` defaults, `ALTER TABLE ... MODIFY [COLUMN]` and `CHANGE [COLUMN]` descriptor replacement, integer/`NULL` `INSERT`/`REPLACE` row and `UPDATE` assignment conversion, text readback, descriptor-driven filtered `SELECT`/`DELETE`/`UPDATE` predicate conversion with MyLite's current descriptor-range literal check, and single-column sort support; display widths normalize away with warning 1681; no `ZEROFILL`, repeated/combined attributes, aliases, general expression defaults, general expression semantics, compact physical storage, or protocol-grade result metadata |
| `INT` / `INTEGER` | 🟡 | Limited DDL descriptors plus optional single explicit `SIGNED` or `UNSIGNED`, deprecated display width `0..255`, optional `DEFAULT NULL` or in-range decimal integer/`TRUE`/`FALSE` defaults, `ALTER TABLE ... MODIFY [COLUMN]` and `CHANGE [COLUMN]` descriptor replacement, integer/`NULL` `INSERT`/`REPLACE` row and `UPDATE` assignment conversion, text readback, descriptor-driven filtered `SELECT`/`DELETE`/`UPDATE` predicate conversion, and single-column sort support; display widths normalize away with warning 1681; no general expression defaults, general expression semantics, `ZEROFILL`, repeated/combined attributes, or protocol-grade result metadata |
| `BIGINT` | 🟡 | Limited DDL descriptors plus optional single explicit `SIGNED` or `UNSIGNED`, deprecated display width `0..255`, optional `DEFAULT NULL` or in-range decimal integer/`TRUE`/`FALSE` defaults, `ALTER TABLE ... MODIFY [COLUMN]` and `CHANGE [COLUMN]` descriptor replacement, integer/`NULL` `INSERT`/`REPLACE` row and `UPDATE` assignment conversion, text readback, descriptor-driven filtered `SELECT`/`DELETE`/`UPDATE` predicate conversion, and single-column sort support; display widths normalize away with warning 1681; `BIGINT UNSIGNED` defaults and row values are capped at the signed 64-bit SQLite integer range in this slice; no `ZEROFILL`, repeated/combined attributes, general expression defaults, or protocol-grade result metadata |
| Integer type aliases | 🟡 | Limited `INT1`, `INT2`, `INT3`, `INT4`, and `INT8` aliases normalize to existing integer-family descriptors, with optional single `SIGNED` or `UNSIGNED`, deprecated display width `0..255`, optional `DEFAULT NULL` or in-range decimal integer/`TRUE`/`FALSE` defaults, descriptor-driven DDL/DML/conversion/introspection coverage, and normalized type rendering; only signed `INT1(1)` persists/renders as `tinyint(1)`, other alias widths normalize away with warning 1681; no `SERIAL`, `ZEROFILL`, combined/repeated attributes, general expression defaults, casts, or protocol-grade metadata |
| `DECIMAL` / `NUMERIC` | 🟡 | Limited exact fixed-point descriptors, optional deprecated `UNSIGNED`, exact canonical `TEXT` storage/readback, decimal defaults, row DML conversion with scale padding, half-away-from-zero rounding, truncation notes, range diagnostics, descriptor cloning/copying, `IS NULL` / `IS NOT NULL` predicates, supported secondary-index declaration/metadata, and `SHOW` / `INFORMATION_SCHEMA.COLUMNS` rendering; no decimal comparison/order/math, `ZEROFILL`, decimal primary or unique keys, auto-increment, string/float/hex/bit conversion, casts, or protocol-grade numeric metadata |
| `FIXED` | 🟡 | Limited alias for the supported `DECIMAL` descriptor subset, normalized to `decimal(M,D)` metadata and storage; same gaps as `DECIMAL` |
| `FLOAT` | ❌ | Approximate numeric metadata |
| `DOUBLE` / `REAL` | ❌ | Approximate numeric metadata |
| `FLOAT4` / `FLOAT8` | ❌ | Alias rewrites and metadata |
| `BIT` | ❌ | Bit storage and conversion |
| `BOOL` / `BOOLEAN` | 🟡 | Bare column-type aliases normalize to signed `TINYINT(1)` descriptors with integer/`NULL`, optional `DEFAULT NULL` or in-range decimal integer/`TRUE`/`FALSE` defaults, plus limited `TRUE`/`FALSE` DDL/DML/conversion/introspection coverage and no display-width warning; no `BOOL(1)`, `BOOLEAN(1)`, `SIGNED`, `UNSIGNED`, `ZEROFILL`, general expression defaults, general expression truth semantics, casts, or protocol-grade metadata |
| `SERIAL` | ❌ | BIGINT AUTO_INCREMENT alias |

## Temporal types

| Feature | Status | Notes |
| --- | --- | --- |
| `DATE` | 🟡 | Limited canonical `YYYY-MM-DD` descriptors stored as `TEXT`, with valid range `1000-01-01` through `9999-12-31`, canonical date-string defaults, `INSERT`/`REPLACE`/`UPDATE` assignment, `INSERT IGNORE` invalid/zero/`NULL` adjustments to `0000-00-00`, readback, descriptor cloning/copying, `IS NULL` / `IS NOT NULL`, string-literal comparison/`BETWEEN`/`IN` predicates, single-column ordering with MySQL-compatible `NULL` placement, supported secondary-index declaration/metadata, and `SHOW` / `INFORMATION_SCHEMA.COLUMNS` rendering; no relaxed date strings, `DATE '...'` temporal literals, time parts, casts, date arithmetic, functions, date primary or unique keys, auto-increment, time zones, or protocol-grade temporal metadata |
| `TIME` | ❌ | Time range and formatting |
| `DATETIME` | ❌ | Datetime range and defaults |
| `TIMESTAMP` | ❌ | UTC conversion and defaults |
| `YEAR` | ❌ | Year storage, two-digit handling, casts, and display |

## String and binary types

| Feature | Status | Notes |
| --- | --- | --- |
| `CHAR` | 🟡 | Limited bare `CHAR` and `CHAR(0..255)` descriptors with UTF-8 non-`NUL` `TEXT` storage/readback in MySQL's default trimmed `CHAR` shape, silent excess-trailing-space truncation, strict nonspace overlength errors, `NULL` and empty-string behavior including `CHAR(0)`, admitted `INSERT`/`REPLACE`/`UPDATE` string assignments, `IS NULL` / `IS NOT NULL` predicates, descriptor cloning/copying, supported secondary-index declaration/metadata, and `SHOW` / `INFORMATION_SCHEMA.COLUMNS` rendering; no `CHARACTER` aliases, explicit string defaults, `PAD_CHAR_TO_FULL_LENGTH`, charset conversion, collation comparison, ordering/grouping/distinct, primary or unique keys, collation-aware index semantics, or protocol-grade metadata |
| `VARCHAR` | 🟡 | Limited `VARCHAR(0..255)` descriptors with UTF-8 non-`NUL` `TEXT` storage/readback, `NULL` and empty-string behavior, strict overlength errors, admitted `INSERT`/`REPLACE`/`UPDATE` string assignments, `IS NULL` / `IS NOT NULL` predicates, descriptor cloning/copying, supported secondary-index declaration/metadata, and `SHOW` rendering; no aliases, explicit string defaults, lengths above 255, charset conversion, collation comparison, ordering/grouping/distinct, string aggregates, primary or unique keys, collation-aware index semantics, or protocol-grade metadata |
| `CHARACTER` / `CHARACTER VARYING` | ❌ | CHAR/VARCHAR aliases |
| `NCHAR` / `NATIONAL CHAR` | ❌ | National-character aliases |
| `NVARCHAR` / `NATIONAL VARCHAR` | ❌ | National-character aliases |
| `BINARY` | ❌ | Fixed-length binary semantics and padding |
| `VARBINARY` | ❌ | Variable binary semantics and length limits |
| `CHAR BYTE` | ❌ | Alias behavior for `BINARY`, metadata rewrites |

## Large object types

| Feature | Status | Notes |
| --- | --- | --- |
| `TINYBLOB` | ❌ | BLOB length and metadata |
| `BLOB` | ❌ | BLOB length and metadata |
| `MEDIUMBLOB` | ❌ | BLOB length and metadata |
| `LONGBLOB` | ❌ | BLOB length and metadata |
| `TINYTEXT` | 🟡 | Limited bare `TINYTEXT` descriptors with UTF-8 non-`NUL` `TEXT` storage/readback, strict 255-byte row-value checks, `NULL` and empty-string behavior, admitted `INSERT`/`REPLACE`/`UPDATE` string assignments, `IS NULL` / `IS NOT NULL` predicates, descriptor cloning/copying, and `SHOW` / `INFORMATION_SCHEMA.COLUMNS` rendering; no `TINYTEXT(M)`, explicit string defaults, charset conversion, collation comparison, ordering/grouping/distinct, indexes, or protocol-grade metadata |
| `TEXT` | 🟡 | Limited bare `TEXT` descriptors with UTF-8 non-`NUL` `TEXT` storage/readback, strict 65535-byte row-value checks, `NULL` and empty-string behavior, admitted `INSERT`/`REPLACE`/`UPDATE` string assignments, `IS NULL` / `IS NOT NULL` predicates, descriptor cloning/copying, and `SHOW` / `INFORMATION_SCHEMA.COLUMNS` rendering; no `TEXT(M)`, explicit string defaults, charset conversion, collation comparison, ordering/grouping/distinct, indexes, or protocol-grade metadata |
| `MEDIUMTEXT` | 🟡 | Limited bare `MEDIUMTEXT` descriptors with UTF-8 non-`NUL` `TEXT` storage/readback, strict 16777215-byte row-value checks, `NULL` and empty-string behavior, admitted `INSERT`/`REPLACE`/`UPDATE` string assignments, `IS NULL` / `IS NOT NULL` predicates, descriptor cloning/copying, and `SHOW` / `INFORMATION_SCHEMA.COLUMNS` rendering; no `MEDIUMTEXT(M)`, explicit string defaults, charset conversion, collation comparison, ordering/grouping/distinct, indexes, or protocol-grade metadata |
| `LONGTEXT` | 🟡 | Limited bare `LONGTEXT` descriptors with UTF-8 non-`NUL` `TEXT` storage/readback, strict 4294967295-byte row-value checks within process memory limits, `NULL` and empty-string behavior, admitted `INSERT`/`REPLACE`/`UPDATE` string assignments, `IS NULL` / `IS NOT NULL` predicates, descriptor cloning/copying, and `SHOW` / `INFORMATION_SCHEMA.COLUMNS` rendering; no `LONGTEXT(M)`, explicit string defaults, charset conversion, collation comparison, ordering/grouping/distinct, indexes, streaming I/O, or protocol-grade metadata |
| `LONG` / `LONG VARCHAR` | ❌ | Alias rewrites and metadata |
| `LONG VARBINARY` | ❌ | Alias rewrites and metadata |

## Enumerated types

| Feature | Status | Notes |
| --- | --- | --- |
| `ENUM` | ❌ | Indexing, sorting, invalid values |
| `SET` | ❌ | Bitmap membership metadata |

## Structured and spatial types

| Feature | Status | Notes |
| --- | --- | --- |
| `JSON` | ❌ | Validation and binary JSON metadata |
| `GEOMETRY` | ❌ | Base spatial type storage, SRID, validity, and metadata |
| `POINT` | ❌ | Point storage, SRID, coordinate access, and metadata |
| `LINESTRING` | ❌ | LineString storage, SRID, validity, and metadata |
| `POLYGON` | ❌ | Polygon storage, SRID, validity, and metadata |
| `MULTIPOINT` | ❌ | MultiPoint storage, SRID, validity, and metadata |
| `MULTILINESTRING` | ❌ | MultiLineString storage, SRID, validity, and metadata |
| `MULTIPOLYGON` | ❌ | MultiPolygon storage, SRID, validity, and metadata |
| `GEOMETRYCOLLECTION` | ❌ | Geometry collection storage, SRID, validity, and metadata |

## Literals

| Feature | Status | Notes |
| --- | --- | --- |
| Numeric literals | 🟡 | Decimal integer literals with optional unary sign as supported no-source/`DUAL` literal projection values, no-source/`DUAL` signed-64 arithmetic projection operands, limited no-source/`DUAL` numeric functions, no-source/`DUAL` keyword scalar logical operands, limited `DO` scalar-expression operands including top-level `/` division, integer column default values, `INSERT ... VALUES`, `INSERT ... SET`, `REPLACE ... VALUES`, `REPLACE ... SET`, and single-table `UPDATE` assignment inputs plus supported filtered `SELECT`/`DELETE`/`UPDATE` predicate right operands, `BETWEEN` bounds, and `IN` list values; fixed decimal literals with optional unary sign only for compatible decimal column defaults and row DML assignments, with exact canonicalization, rounding, truncation notes, and range diagnostics; `INSERT IGNORE ... VALUES` and `INSERT IGNORE ... SET` clip out-of-range descriptor integer and decimal inputs to the current supported range with warnings; unsigned decimal integer literals for supported `SELECT` `LIMIT`/`OFFSET` and `DELETE`/`UPDATE LIMIT`; no table-backed expression-level numeric semantics, non-decimal formats, floats, hex, or bit literals |
| Boolean literals | 🟡 | `TRUE` and `FALSE` are accepted as `1` and `0` only in supported no-source/`DUAL` literal projection values, limited no-source/`DUAL` signed-64 scalar arithmetic projection operands including unary `+` and unary `-`, limited no-source/`DUAL` `ABS()`, `SIGN()`, `CEIL()`/`CEILING()`/`FLOOR()`, one-argument `ROUND()`, `BIT_COUNT()`, `BIN()`, `OCT()`, `CONV()`, `SQRT()`, `DEGREES()`, `RADIANS()`, `ACOS()`, `ASIN()`, `ATAN()`, and `ATAN2()` operands, no-source/`DUAL` keyword scalar logical operands, limited `DO` scalar-expression operands including top-level `/` division, column default values, integer row-value, `INSERT ... SET`, `REPLACE ... VALUES`, `REPLACE ... SET`, single-table `UPDATE` assignment, filtered `SELECT`/`DELETE`/`UPDATE` predicate right-operand, `BETWEEN` bound, and `IN` list positions, `COUNT(TRUE)` / `COUNT(FALSE)` aggregate argument positions, and descriptor-column `IS [NOT] TRUE` / `FALSE` truth-test targets; no `WHERE TRUE`, literal-left truth tests, general boolean expressions outside the limited scalar and descriptor-backed subsets, scalar truth metadata, general expression evaluation, or `LIMIT TRUE` / `LIMIT FALSE` |
| String literals | 🟡 | Ordinary single- and double-quoted string literals with decoded quote and backslash escapes are supported only in admitted `CHAR`, `VARCHAR`, baseline `TEXT` family row-value positions, canonical `DATE` defaults/row values/predicates, and existing alias/pattern contexts; admitted `CHAR` row values are canonicalized to MySQL's default trimmed readback shape, while other admitted row values preserve ordinary `\%` and `\_` backslash escapes like MySQL outside pattern matching; no introducers, national strings, adjacent literal concatenation, parameters, table-backed expression values, arbitrary scalar projection, embedded `NUL`, relaxed date strings, or mutable SQL mode behavior |
| Temporal literals | 🟡 | Canonical quoted `YYYY-MM-DD` strings are admitted only where the limited `DATE` descriptor subset explicitly allows them; no `DATE '...'`, `TIME`, `TIMESTAMP`, relaxed date strings, intervals, or temporal expression coercion |
| JSON path literals | ❌ | Path grammar, quoting, wildcards, ranges, and errors |

## Variables and conversion

| Feature | Status | Notes |
| --- | --- | --- |
| User variables | ❌ | Retention, coercion, metadata |
| Local variables | ❌ | Stored-program variable typing, scope, and diagnostics |
| Type conversion | 🟡 | Limited strict conversion for supported integer/decimal/canonical-date/`NULL`/`TRUE`/`FALSE` defaults plus inserted, replaced, and updated values, including exact decimal canonicalization, scale padding, half-away-from-zero rounding, truncation notes, range diagnostics for decimal descriptors, and canonical `YYYY-MM-DD` validation for `DATE` descriptors; strict UTF-8 non-`NUL` string-literal conversion for admitted `CHAR`, `VARCHAR`, and baseline `TEXT` family row values; default-mode `CHAR` trailing-space canonicalization; descriptor resolution for limited DML `DEFAULT` keyword values in supported `INSERT`/`REPLACE` `VALUES` and `SET` paths plus one-assignment `UPDATE`; limited `INSERT IGNORE ... VALUES` / `SET` adjustment for omitted no-explicit-default columns, explicit `DEFAULT` for no-default columns, explicit `NULL` into numeric/string/date `NOT NULL`, out-of-range descriptor integer and decimal inputs, invalid date inputs, overlength string values, descriptor-driven existing-row validation for supported integer `ALTER TABLE ... MODIFY [COLUMN]` and `CHANGE [COLUMN]`, descriptor-driven integer/boolean/date predicate conversion for `SELECT`/`DELETE`/`UPDATE` comparisons, `BETWEEN` bounds, and non-`NULL` `IN` list values, descriptor-preserving `NULL` `IN` list values, and unsigned signed-64 range conversion for supported `SELECT` `LIMIT`/`OFFSET` plus `DELETE`/`UPDATE LIMIT` literals only |
| Aggregate numeric result envelope | 🟡 | Limited descriptor-backed integer `SUM(column)` results use MyLite's signed-64 text result envelope; MySQL's exact decimal widening for integer sums beyond signed 64 bits is not yet implemented |
| Collation coercibility | ❌ | Coercibility and diagnostics |

[Back to compatibility overview](../../COMPATIBILITY.md)
