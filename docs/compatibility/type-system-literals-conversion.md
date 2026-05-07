# Type system, literals, and conversion

## Numeric types

| Feature | Status | Notes |
| --- | --- | --- |
| `TINYINT` | ❌ | Ranges, display width, metadata |
| `SMALLINT` | ❌ | Ranges, display width, metadata |
| `MEDIUMINT` | ❌ | Ranges, display width, metadata |
| `INT` / `INTEGER` | 🟡 | Limited DDL descriptors plus integer/`NULL` assignment conversion, text readback, descriptor-driven filtered-select predicate conversion, and single-column sort support; no general expression semantics, display width, or protocol-grade result metadata |
| `BIGINT` | 🟡 | Limited DDL descriptors plus integer/`NULL` assignment conversion, text readback, descriptor-driven filtered-select predicate conversion, and single-column sort support; `BIGINT UNSIGNED` is capped at the signed 64-bit SQLite integer range in this slice |
| Integer type aliases | ❌ | Alias rewrites and metadata |
| `DECIMAL` / `NUMERIC` | ❌ | Exact math and metadata |
| `FIXED` | ❌ | Alias rewrites and metadata |
| `FLOAT` | ❌ | Approximate numeric metadata |
| `DOUBLE` / `REAL` | ❌ | Approximate numeric metadata |
| `FLOAT4` / `FLOAT8` | ❌ | Alias rewrites and metadata |
| `BIT` | ❌ | Bit storage and conversion |
| `BOOL` / `BOOLEAN` | ❌ | Alias behavior for TINYINT(1) and expression truth rules |
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
| Numeric literals | 🟡 | Decimal integer literals with optional unary sign only as supported `INSERT ... VALUES` inputs and supported filtered-select predicate right operands; unsigned decimal integer literals for supported `LIMIT`/`OFFSET`; no expression-level numeric semantics, non-decimal formats, decimals, floats, hex, or bit literals |
| String literals | ❌ | Escapes, introducers, sql_mode |
| Temporal literals | ❌ | DATE/TIME/TIMESTAMP literal syntax and coercion |
| JSON path literals | ❌ | Path grammar, quoting, wildcards, ranges, and errors |

## Variables and conversion

| Feature | Status | Notes |
| --- | --- | --- |
| User variables | ❌ | Retention, coercion, metadata |
| Local variables | ❌ | Stored-program variable typing, scope, and diagnostics |
| Type conversion | 🟡 | Limited strict assignment conversion for inserted integer/`NULL` values, descriptor-driven integer predicate conversion, and unsigned signed-64 range conversion for supported `LIMIT`/`OFFSET` literals only |
| Collation coercibility | ❌ | Coercibility and diagnostics |

[Back to compatibility overview](../../COMPATIBILITY.md)
