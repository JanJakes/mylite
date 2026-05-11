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
| `DECIMAL` / `NUMERIC` | ❌ | Exact math and metadata |
| `FIXED` | ❌ | Alias rewrites and metadata |
| `FLOAT` | ❌ | Approximate numeric metadata |
| `DOUBLE` / `REAL` | ❌ | Approximate numeric metadata |
| `FLOAT4` / `FLOAT8` | ❌ | Alias rewrites and metadata |
| `BIT` | ❌ | Bit storage and conversion |
| `BOOL` / `BOOLEAN` | 🟡 | Bare column-type aliases normalize to signed `TINYINT(1)` descriptors with integer/`NULL`, optional `DEFAULT NULL` or in-range decimal integer/`TRUE`/`FALSE` defaults, plus limited `TRUE`/`FALSE` DDL/DML/conversion/introspection coverage and no display-width warning; no `BOOL(1)`, `BOOLEAN(1)`, `SIGNED`, `UNSIGNED`, `ZEROFILL`, general expression defaults, general expression truth semantics, casts, or protocol-grade metadata |
| `SERIAL` | ❌ | BIGINT AUTO_INCREMENT alias |

## Temporal types

| Feature | Status | Notes |
| --- | --- | --- |
| `DATE` | ❌ | Date range and formatting |
| `TIME` | ❌ | Time range and formatting |
| `DATETIME` | ❌ | Datetime range and defaults |
| `TIMESTAMP` | ❌ | UTC conversion and defaults |
| `YEAR` | ❌ | Year storage, two-digit handling, casts, and display |

## String and binary types

| Feature | Status | Notes |
| --- | --- | --- |
| `CHAR` | ❌ | Padding, charsets, metadata |
| `VARCHAR` | ❌ | Length, charsets, metadata |
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
| `TINYTEXT` | ❌ | TEXT length and metadata |
| `TEXT` | ❌ | TEXT length and metadata |
| `MEDIUMTEXT` | ❌ | TEXT length and metadata |
| `LONGTEXT` | ❌ | TEXT length and metadata |
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
| Numeric literals | 🟡 | Decimal integer literals with optional unary sign only as supported no-source/`DUAL` literal projection values, no-source/`DUAL` signed-64 arithmetic projection operands, limited no-source/`DUAL` `ABS()` and `BIT_COUNT()` operands including direct unsigned-64 magnitude envelopes, no-source/`DUAL` keyword scalar logical operands, limited `DO` scalar-expression operands, column default values, `INSERT ... VALUES`, `INSERT ... SET`, `REPLACE ... VALUES`, `REPLACE ... SET`, and single-table `UPDATE` assignment inputs plus supported filtered `SELECT`/`DELETE`/`UPDATE` predicate right operands, `BETWEEN` bounds, and `IN` list values; `INSERT IGNORE ... VALUES` and `INSERT IGNORE ... SET` clip out-of-range descriptor integer inputs to the current supported range with warnings; unsigned decimal integer literals for supported `SELECT` `LIMIT`/`OFFSET` and `DELETE`/`UPDATE LIMIT`; no table-backed expression-level numeric semantics, non-decimal formats, decimals, floats, hex, or bit literals |
| Boolean literals | 🟡 | `TRUE` and `FALSE` are accepted as `1` and `0` only in supported no-source/`DUAL` literal projection values, limited no-source/`DUAL` signed-64 scalar arithmetic projection operands including unary `+` and unary `-`, limited no-source/`DUAL` `ABS()` and `BIT_COUNT()` operands, no-source/`DUAL` keyword scalar logical operands, limited `DO` scalar-expression operands, column default values, integer row-value, `INSERT ... SET`, `REPLACE ... VALUES`, `REPLACE ... SET`, single-table `UPDATE` assignment, filtered `SELECT`/`DELETE`/`UPDATE` predicate right-operand, `BETWEEN` bound, and `IN` list positions, `COUNT(TRUE)` / `COUNT(FALSE)` aggregate argument positions, and descriptor-column `IS [NOT] TRUE` / `FALSE` truth-test targets; no `WHERE TRUE`, literal-left truth tests, general boolean expressions outside the limited scalar and descriptor-backed subsets, scalar truth metadata, general expression evaluation, or `LIMIT TRUE` / `LIMIT FALSE` |
| String literals | ❌ | Escapes, introducers, sql_mode |
| Temporal literals | ❌ | DATE/TIME/TIMESTAMP literal syntax and coercion |
| JSON path literals | ❌ | Path grammar, quoting, wildcards, ranges, and errors |

## Variables and conversion

| Feature | Status | Notes |
| --- | --- | --- |
| User variables | ❌ | Retention, coercion, metadata |
| Local variables | ❌ | Stored-program variable typing, scope, and diagnostics |
| Type conversion | 🟡 | Limited strict conversion for supported integer/`NULL`/`TRUE`/`FALSE` defaults plus inserted, replaced, and updated values, descriptor resolution for limited DML `DEFAULT` keyword values in supported `INSERT`/`REPLACE` `VALUES` and `SET` paths plus one-assignment `UPDATE`, limited `INSERT IGNORE ... VALUES` / `SET` adjustment for omitted no-explicit-default columns, explicit `DEFAULT` for no-default columns, explicit `NULL` into numeric `NOT NULL`, and out-of-range descriptor integer inputs, descriptor-driven existing-row validation for supported `ALTER TABLE ... MODIFY [COLUMN]` and `CHANGE [COLUMN]`, descriptor-driven integer and boolean-literal predicate conversion for `SELECT`/`DELETE`/`UPDATE` comparisons, `BETWEEN` bounds, and non-`NULL` `IN` list values, descriptor-preserving `NULL` `IN` list values, and unsigned signed-64 range conversion for supported `SELECT` `LIMIT`/`OFFSET` plus `DELETE`/`UPDATE LIMIT` literals only |
| Aggregate numeric result envelope | 🟡 | Limited descriptor-backed integer `SUM(column)` results use MyLite's signed-64 text result envelope; MySQL's exact decimal widening for integer sums beyond signed 64 bits is not yet implemented |
| Collation coercibility | ❌ | Coercibility and diagnostics |

[Back to compatibility overview](../../COMPATIBILITY.md)
