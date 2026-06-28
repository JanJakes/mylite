# Baseline Spatial Centroid Function

## Scope

This slice implements the SRID-0 `ST_Centroid()` baseline:

- `ST_Centroid(g)`

The function is exposed through the existing scalar UDF bridge in scalar and
row-backed projection contexts.

## Source Evidence

Behavior is specified from the MySQL 8.4 Reference Manual geometry property
functions section:

- `https://dev.mysql.com/doc/refman/8.4/en/gis-polygon-property-functions.html`
- `https://dev.mysql.com/doc/refman/8.4/en/gis-general-property-functions.html`

Expected outputs and diagnostics are verified against the local MySQL 8.4.9
runtime container `mylite-mysql-849`.

## Runtime Observations

The MySQL 8.4.9 probes used for the baseline returned:

| Expression | Result |
| --- | --- |
| `ST_AsText(ST_Centroid(Point(2,4)))` | `POINT(2 4)` |
| `ST_AsText(ST_Centroid(ST_GeomFromText('MULTIPOINT(0 0,2 2,4 2)')))` | `POINT(2 1.3333333333333333)` |
| `ST_AsText(ST_Centroid(ST_GeomFromText('LINESTRING(0 0,0 4,3 4)')))` | `POINT(0.6428571428571429 2.857142857142857)` |
| `ST_AsText(ST_Centroid(ST_GeomFromText('MULTILINESTRING((0 0,0 4),(0 0,6 0))')))` | `POINT(1.8 0.8)` |
| `ST_AsText(ST_Centroid(ST_GeomFromText('POLYGON((0 0,10 0,10 10,0 10,0 0),(5 5,7 5,7 7,5 7,5 5))')))` | `POINT(4.958333333333333 4.958333333333333)` |
| `ST_AsText(ST_Centroid(ST_GeomFromText('MULTIPOLYGON(((0 0,4 0,4 4,0 4,0 0)),((10 0,12 0,12 2,10 2,10 0)))')))` | `POINT(3.8 1.8)` |
| `ST_AsText(ST_Centroid(ST_GeomFromText('GEOMETRYCOLLECTION(POINT(100 100),LINESTRING(0 0,0 4),POLYGON((0 0,6 0,6 6,0 6,0 0)))')))` | `POINT(3 3)` |
| `ST_AsText(ST_Centroid(ST_GeomFromText('GEOMETRYCOLLECTION(POINT(100 100),LINESTRING(0 0,0 4),LINESTRING(0 0,6 0))')))` | `POINT(1.8 0.8)` |
| `ST_AsText(ST_Centroid(ST_GeomFromText('GEOMETRYCOLLECTION(POINT(0 0),MULTIPOINT(2 2,4 2))')))` | `POINT(2 1.3333333333333333)` |
| `ST_AsText(ST_Centroid(ST_GeomFromText('GEOMETRYCOLLECTION EMPTY')))` | `NULL` |
| `ST_AsText(ST_Centroid(NULL))` | `NULL` |
| `ST_AsText(ST_Centroid(ST_GeomFromText('LINESTRING(1 1,1 1)')))` | `POINT(1 1)` |

Additional diagnostics:

- A geographic SRS argument returns `3618 / 22S00` with the existing
  not-implemented-for-geographic-SRS message.
- A geometrically invalid polygon may be rejected with `3037 / 22023`.

## Semantics

Common behavior:

- SQL `NULL` arguments return SQL `NULL`.
- Invalid geometry bytes return MySQL error `3037 / 22023`.
- Nonzero SRIDs remain outside this slice and return the existing geographic
  SRS not-implemented diagnostic.
- Empty geometry collections return SQL `NULL`.

Centroid behavior:

- `Point` returns the same point.
- `MultiPoint` returns the arithmetic mean of member point coordinates.
- `LineString` returns the segment-length-weighted average of segment
  midpoints. A zero-length line returns its first point.
- `MultiLineString` returns the segment-length-weighted centroid across all
  member line strings. If every segment has zero length, it falls back to the
  first available point.
- `Polygon` returns the area-weighted centroid. The exterior ring contributes
  positive area; interior rings subtract from the centroid, regardless of ring
  winding direction.
- `MultiPolygon` returns the area-weighted centroid across all member polygons.
- `GeometryCollection` recursively computes the centroid over the highest
  dimensional non-empty components only: polygons first, then lines, then
  points. Empty child collections do not contribute.

## Known Gaps

The executable baseline is SRID-0 only. MyLite does not yet implement
geographic or projected SRS centroid semantics, SRS catalog lookup, full
invalid-geometry repair or validation, descriptor-backed string assignment
conversion parity for geometry results, or a separate topology engine.

## SQLite Integration

The implementation uses SQLite's public scalar UDF API. MyLite owns geometry
argument validation, WKB traversal, centroid accumulation, MySQL diagnostics,
and result construction. No SQLite fork hook is required.

## Grammar

No grammar change is required. The function is admitted through the existing
generic function AST surface:

```lemon
expr(A) ::= function_name(NM) LP optional_expr_list(ARGS) RP.
```

The runtime maps the admitted function name to
`MYLITE_SPATIAL_FUNCTION_ST_CENTROID` and validates argument count and argument
semantics.
