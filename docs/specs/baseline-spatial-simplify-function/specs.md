# Baseline Spatial Simplify Function

## Scope

Implement a MySQL 8.4.9-compatible SRID-0 baseline for:

```sql
ST_Simplify(geometry, max_distance)
```

`ST_Simplify()` returns a geometry simplified with the Douglas-Peucker
algorithm. The supported baseline covers MyLite's existing representable
SRID-0 `Point`, `LineString`, `Polygon`, `MultiPoint`, `MultiLineString`,
`MultiPolygon`, and `GeometryCollection` values in scalar, row-backed, and DML
value contexts.

The slice does not add a general SRS catalog or geographic/projected
simplification. Nonzero-SRID geometry keeps the existing spatial
not-implemented diagnostics.

## MySQL 8.4.9 Behavior

Observed against MySQL 8.4.9 and aligned with the official MySQL 8.4
convenience-function documentation:

- `NULL` geometry or `NULL` distance returns `NULL`.
- Wrong arity returns error `1582`, SQLSTATE `42000`, naming native function
  `ST_Simplify`.
- Invalid geometry bytes return error `3037`, SQLSTATE `22023`, naming
  `st_simplify`.
- `max_distance <= 0`, nonnumeric text that coerces to `0`, or nonfinite
  distance returns error `1210`, SQLSTATE `HY000`, `Incorrect arguments to
  st_simplify`.
- Text distances use MySQL numeric coercion: `'1abc'` is accepted as `1`.
- `Point` and `MultiPoint` values return unchanged.
- `LineString` and `MultiLineString` values remove vertices within
  `max_distance` of replacement segments and keep the same geometry type.
- `Polygon` and `MultiPolygon` values simplify each ring. Collapsed holes are
  omitted. A polygon with a collapsed exterior ring is omitted; if no polygon
  remains, the result is `NULL`.
- `GeometryCollection` values simplify children independently. Collapsed
  polygon children are omitted. An empty input collection, or a collection whose
  children all collapse away, returns `NULL`.
- Geographic SRS input returns MySQL's not-implemented-for-geographic-SRS
  diagnostic for `ST_Simplify`.

Representative MySQL 8.4.9 results:

```sql
SELECT ST_AsText(ST_Simplify(
  ST_GeomFromText('LINESTRING(0 0,0 1,1 1,1 2,2 2,2 3,3 3)'),
  0.5
));
-- LINESTRING(0 0,0 1,1 1,2 3,3 3)

SELECT ST_AsText(ST_Simplify(
  ST_GeomFromText('LINESTRING(0 0,0 1,1 1,1 2,2 2,2 3,3 3)'),
  1.0
));
-- LINESTRING(0 0,3 3)

SELECT ST_AsText(ST_Simplify(Point(1,2), 1));
-- POINT(1 2)

SELECT ST_AsText(ST_Simplify(
  ST_GeomFromText('POLYGON((0 0,0.1 0,0.1 0.1,0 0.1,0 0))'),
  1
));
-- NULL
```

## MyLite Semantics

MyLite evaluates `ST_Simplify()` inside the existing spatial function runtime:

1. Validate exactly two arguments.
2. Return `NULL` if either argument is `NULL`.
3. Read and validate the geometry argument with the existing internal geometry
   decoder.
4. Parse `max_distance` with MySQL-style permissive numeric coercion and reject
   nonpositive or nonfinite values with `Incorrect arguments to st_simplify`.
5. Reject nonzero-SRID input with the existing geographic-SRS not-implemented
   diagnostic.
6. Decode the internal geometry into MyLite's spatial geometry tree.
7. Apply iterative Douglas-Peucker simplification to LineString and ring point
   sequences.
8. Re-serialize the simplified geometry tree to MyLite's internal
   SRID-prefixed WKB representation.

The implementation preserves SRID 0, the input geometry family, and current
geometry bytes for point-only values. It may return geometries that are
geometrically invalid, matching MySQL's documented behavior for simplification.

## Lemon Grammar

`ST_Simplify()` is registered through MyLite's generic spatial native-function
surface. No SQL grammar extension is required beyond accepting ordinary
function-call syntax.

## Tests

Coverage must include:

- MySQL-runtime expectation script for the documented line examples, point and
  multipoint identity, LineString and MultiLineString simplification, polygon
  and multipolygon simplification, hole collapse, geometry collection member
  simplification, empty/all-collapsed `NULL`, `NULL` inputs, wrong arity,
  nonpositive/nonnumeric distance, invalid geometry, and nonzero-SRID
  diagnostics.
- Focused libmylite runtime test covering scalar, row-backed, and DML value
  contexts.
- Regression coverage for result serialization through `ST_AsText()` and
  descriptor-backed geometry columns.

## Compatibility Notes

This slice is a constructive geometry baseline, not a general overlay engine.
It intentionally does not implement `ST_Buffer()`, `ST_Difference()`,
`ST_Intersection()`, `ST_SymDifference()`, `ST_Transform()`, or `ST_Union()`.
Those functions require additional geometry kernels or SRS transformation
support and remain separate compatibility rows.
