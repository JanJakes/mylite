# Baseline Spatial Discrete Distance Functions

## Scope

This slice adds MySQL 8.4.9-shaped `ST_FrechetDistance(g1, g2[, unit])` and
`ST_HausdorffDistance(g1, g2[, unit])` behavior for Cartesian SRID 0 geometry
values in scalar and row-backed projection contexts.

The implementation is a MyLite scalar-function layer over MyLite's existing
spatial WKB parser. It does not require SQLite fork changes or new
dependencies.

## Sources

- MySQL 8.4 Reference Manual, spatial relation functions using object shapes:
  <https://dev.mysql.com/doc/refman/8.4/en/spatial-relation-functions-object-shapes.html>
- MySQL 8.4.9 runtime probes against the local comparison server.

## Function Semantics

Shared behavior:

- If any argument is `NULL`, return `NULL`.
- If either geometry argument is an empty geometry collection, return `NULL`.
- If either geometry argument is malformed, return `3037 / 22023`.
- If the geometry arguments have different SRIDs, return `3033 / HY000`.
- If the arguments have a nonzero matching SRID, MyLite returns the existing
  geographic-SRS not-implemented diagnostic for this baseline.
- If a unit argument is supplied for SRID 0, return `3882 / SU001`, including
  the unit text in the diagnostic, matching the existing SRID-0 distance-unit
  behavior.
- Results are double-precision values in SRID-0 coordinate units.

`ST_FrechetDistance()`:

- Supports `LineString` / `LineString`.
- Computes the discrete Fréchet distance over the explicit LineString points.
- Unsupported Cartesian type combinations return `3704 / 22S00` with the two
  geometry type names.

`ST_HausdorffDistance()`:

- Supports MySQL's documented Cartesian combinations:
  - `LineString` / `LineString`
  - `Point` / `MultiPoint`
  - `MultiPoint` / `Point`
  - `LineString` / `MultiLineString`
  - `MultiLineString` / `LineString`
  - `MultiPoint` / `MultiPoint`
  - `MultiLineString` / `MultiLineString`
- For `LineString` / `LineString`, computes MySQL's directed discrete
  Hausdorff distance from the first geometry to the second geometry: for each
  explicit point in the first geometry, find the nearest explicit point in the
  second geometry, then return the maximum of those nearest-point distances.
- For `MultiPoint` / `MultiPoint`, applies the same directed point-set rule.
- For `Point` / `MultiPoint` in either argument order, returns the nearest
  explicit point distance between the standalone point and the multipoint.
- For `LineString` / `MultiLineString` in either argument order, evaluates the
  standalone LineString as the directed source against each MultiLineString
  child LineString and returns the maximum child distance.
- For `MultiLineString` / `MultiLineString`, evaluates every directed
  left-child LineString to right-child LineString pair and returns the maximum
  child-pair distance.
- Unsupported Cartesian type combinations return `3704 / 22S00` with the two
  geometry type names.

## Runtime Evidence

MySQL 8.4.9 observations used by the tests:

- `ST_FrechetDistance(LINESTRING(0 0,0 5,5 5), LINESTRING(0 1,0 6,3 3,5 6))`
  returns `2.8284271247461903`.
- `ST_HausdorffDistance(LINESTRING(0 0,0 5,5 5), LINESTRING(0 1,0 6,3 3,5 6))`
  returns `1`.
- The same Hausdorff call with arguments reversed returns
  `2.8284271247461903`, confirming directed behavior.
- `ST_HausdorffDistance(Point(0,0), MULTIPOINT(3 4,10 10))` returns `5`.
- `ST_HausdorffDistance(MULTIPOINT(3 4,10 10), Point(0,0))` returns `5`.
- `ST_HausdorffDistance(MULTIPOINT(100 100,10 10), Point(0,0))` returns
  `14.142135623730951`, confirming nearest point-to-multipoint behavior rather
  than directed multipoint-to-point maximum behavior.
- `ST_HausdorffDistance(LineString, MultiLineString)` and the reversed form are
  supported for SRID 0 and evaluate the standalone LineString against each
  collection child.
- `NULL` and empty geometry arguments return `NULL`.
- `ST_FrechetDistance(Point, LineString)` and `ST_HausdorffDistance(Point,
  Point)` return `3704 / 22S00`.
- SRID-0 unit arguments return `3882 / SU001`.

## Known Gaps

- Geographic SRS distances and unit conversion for nonzero SRIDs are deferred.
- Topology, validity repair, and general spatial relation predicates are outside
  this slice.
- `ST_HausdorffDistance()` remains limited to MySQL's documented point and line
  combinations; polygon and geometry-collection combinations remain unsupported.

## Grammar

No grammar-specific changes are required. The generic scalar function call
surface admits:

```lemon
expr(A) ::= function_name(F) LP exprlist(Args) RP.
```
