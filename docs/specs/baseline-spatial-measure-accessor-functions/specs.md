# Baseline Spatial Measure and Accessor Functions

## Scope

This slice extends MyLite's SRID-0 spatial baseline with geometry property,
accessor, measurement, envelope, transform, and MBR predicate functions that can
be implemented directly from MyLite-owned WKB parsing:

- `ST_Dimension()`, `ST_IsEmpty()`, `ST_IsClosed()`
- `ST_NumGeometries()`, `ST_GeometryN()`
- `ST_NumPoints()`, `ST_PointN()`, `ST_StartPoint()`, `ST_EndPoint()`
- `ST_ExteriorRing()`, `ST_InteriorRingN()`, `ST_NumInteriorRing()`,
  `ST_NumInteriorRings()`
- `ST_Length()`, `ST_Area()`, `ST_Envelope()`, `ST_SwapXY()`,
  `ST_MakeEnvelope()`
- `MBRContains()`, `MBRCoveredBy()`, `MBRCovers()`, `MBRDisjoint()`,
  `MBREquals()`, `MBRIntersects()`, `MBROverlaps()`, `MBRTouches()`,
  `MBRWithin()`

The functions are exposed in scalar, row-backed projection/filter/order, and
descriptor-backed DML value contexts through the existing generic spatial UDF
bridge.

## Source Evidence

Behavior is specified from the MySQL 8.4 Reference Manual spatial property and
relation sections:

- General property functions:
  `https://dev.mysql.com/doc/refman/8.4/en/gis-general-property-functions.html`
- Point property functions:
  `https://dev.mysql.com/doc/refman/8.4/en/gis-point-property-functions.html`
- LineString and MultiLineString property functions:
  `https://dev.mysql.com/doc/refman/8.4/en/gis-linestring-property-functions.html`
- Polygon and MultiPolygon property functions:
  `https://dev.mysql.com/doc/refman/8.4/en/gis-polygon-property-functions.html`
- GeometryCollection property functions:
  `https://dev.mysql.com/doc/refman/8.4/en/gis-geometrycollection-property-functions.html`
- MBR relation functions:
  `https://dev.mysql.com/doc/refman/8.4/en/spatial-relation-functions-mbr.html`
- Convenience functions:
  `https://dev.mysql.com/doc/refman/8.4/en/spatial-convenience-functions.html`

Expected outputs are verified against the local MySQL 8.4.9 runtime container
`mylite-mysql-849`.

## Semantics

All geometry inputs must be valid MyLite internal geometry bytes: a four-byte
little-endian SRID prefix followed by OGC WKB. WKT/WKB constructors from the
basic spatial slice remain the way to produce geometry values from SQL
literals.

Common behavior:

- SQL `NULL` arguments return SQL `NULL`.
- Invalid geometry bytes return MySQL error `3037` / SQLSTATE `22023`.
- Nonzero SRID inputs remain outside the executable scope because MyLite does
  not yet provide an SRS catalog or geographic computations.
- Empty geometry means `GEOMETRYCOLLECTION EMPTY`, matching MySQL's only
  supported empty geometry value.

Property and accessor behavior:

- `ST_Dimension()` returns `0` for points and multipoints, `1` for lines and
  multilines, `2` for polygons and multipolygons, `NULL` for empty geometry
  collections, and the maximum contained dimension for nonempty geometry
  collections.
- `ST_IsEmpty()` returns `1` only for empty geometry collections, otherwise
  `0`.
- `ST_IsClosed()` returns `1` when a LineString or every MultiLineString member
  has matching first and last coordinates, `0` for open line values, and
  `NULL` for non-line geometries.
- `ST_NumGeometries()` and `ST_GeometryN()` apply only to geometry collections.
  Empty or non-collection inputs return `NULL`; collection indexes are
  one-based and out-of-range indexes return `NULL`.
- Line accessors apply only to LineString values. Wrong types, empty values,
  zero, negative, or out-of-range indexes return `NULL`.
- Polygon ring accessors apply only to Polygon values. Wrong types, empty
  values, and out-of-range ring indexes return `NULL`. Ring indexes are
  one-based.

Measurements and transforms:

- `ST_Length()` returns Cartesian length for LineString and MultiLineString
  values and `NULL` for other geometry types.
- `ST_Area()` returns Cartesian area for Polygon and MultiPolygon values.
  Non-polygon geometries return MySQL error `3516` / SQLSTATE `22S01`.
- `ST_Envelope()` returns a Point, horizontal/vertical LineString, rectangular
  Polygon, or empty geometry collection that covers the input MBR.
- `ST_SwapXY()` swaps all coordinate pairs and preserves the geometry type and
  SRID.
- `ST_MakeEnvelope()` accepts exactly two Point arguments and returns SRID-0
  Point, LineString, or Polygon according to whether the two points are equal,
  axis-aligned, or diagonal.

MBR predicates:

- The MBR functions compute axis-aligned bounds over all coordinates in each
  geometry.
- Empty inputs return `NULL` except `MBREquals()` returns `1` when both MBRs are
  empty and `0` when only one input is empty, matching MySQL's documented empty
  exception for equality.
- Predicate outputs are integer `1` or `0`.

## Known Gaps

This slice deliberately does not implement:

- topology predicates such as `ST_Contains()` or `ST_Intersects()`;
- constructive set operations such as `ST_Union()` and `ST_Buffer()`;
- geographic SRS behavior, unit conversion, range validation, or SRS metadata;
- `ST_Latitude()` / `ST_Longitude()` because MySQL defines them only for
  geographic point SRS values and MyLite currently creates only SRID-0 values;
- invalid-geometry repair or full OGC validity evaluation.

## SQLite Integration

The implementation uses SQLite's public scalar UDF API. MyLite owns the WKB
parser, result construction, MySQL diagnostics, and result metadata. No SQLite
fork hook is required because SQLite only executes the registered function and
does not need to understand geometry internals.

## Grammar

No grammar change is required. All functions are admitted through the existing
generic function AST surface:

```lemon
expr(A) ::= function_name(NM) LP optional_expr_list(ARGS) RP.
```

The runtime maps admitted function names to `enum mylite_spatial_function_kind`
values and rejects unsupported spatial names through the existing generic
function path.
