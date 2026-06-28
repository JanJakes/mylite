# Baseline Spatial Convex Hull Function

## Scope

This slice implements the SRID-0 `ST_ConvexHull()` baseline:

- `ST_ConvexHull(g)`

The function is exposed through the existing scalar UDF bridge in scalar and
row-backed projection contexts.

## Source Evidence

Behavior is specified from the MySQL 8.4 Reference Manual spatial operator
functions section:

- `https://dev.mysql.com/doc/refman/8.4/en/spatial-operator-functions.html`

Expected outputs and diagnostics are verified against the local MySQL 8.4.9
runtime container `mylite-mysql-849`.

## Runtime Observations

The MySQL 8.4.9 probes used for the baseline returned:

| Expression | Result |
| --- | --- |
| `ST_AsText(ST_ConvexHull(Point(1,2)))` | `POINT(1 2)` |
| `ST_AsText(ST_ConvexHull(ST_GeomFromText('MULTIPOINT(1 1,1 1,1 1)')))` | `POINT(1 1)` |
| `ST_AsText(ST_ConvexHull(ST_GeomFromText('MULTIPOINT(0 0,1 1,2 2,1 1)')))` | `LINESTRING(0 0,2 2)` |
| `ST_AsText(ST_ConvexHull(ST_GeomFromText('MULTIPOINT(5 0,25 0,15 10,15 25)')))` | `POLYGON((5 0,25 0,15 25,5 0))` |
| `ST_AsText(ST_ConvexHull(ST_GeomFromText('LINESTRING(0 0,1 1,2 0,3 1)')))` | `POLYGON((0 0,2 0,3 1,1 1,0 0))` |
| `ST_AsText(ST_ConvexHull(ST_GeomFromText('POLYGON((0 0,4 0,4 4,2 2,0 4,0 0),(1 1,2 1,1 2,1 1))')))` | `POLYGON((0 0,4 0,4 4,0 4,0 0))` |
| `ST_AsText(ST_ConvexHull(ST_GeomFromText('MULTIPOLYGON(((0 0,2 0,2 2,0 0)),((5 0,7 0,6 3,5 0)))')))` | `POLYGON((0 0,7 0,6 3,2 2,0 0))` |
| `ST_AsText(ST_ConvexHull(ST_GeomFromText('GEOMETRYCOLLECTION(POINT(0 0),LINESTRING(1 2,3 1),POLYGON((0 4,2 4,1 5,0 4)))')))` | `POLYGON((0 0,3 1,2 4,1 5,0 4,0 0))` |
| `ST_AsText(ST_ConvexHull(ST_GeomFromText('GEOMETRYCOLLECTION EMPTY')))` | `NULL` |
| `ST_AsText(ST_ConvexHull(NULL))` | `NULL` |

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

Convex-hull behavior:

- All vertex points from the input geometry are considered.
- `GeometryCollection` recursively contributes all child vertex points.
- Duplicate vertices are removed.
- A single unique vertex returns a `Point`.
- Multiple collinear vertices return a two-point `LineString` containing the
  low and high hull endpoints.
- Non-collinear vertices return a single-ring `Polygon`, closed by repeating
  the first hull vertex.
- Polygon interior rings contribute vertices to the candidate set, but they do
  not otherwise affect hull computation.

## Known Gaps

The executable baseline is SRID-0 only. MyLite does not yet implement
geographic or projected SRS convex-hull semantics, SRS catalog lookup, complete
invalid-geometry repair or validation, descriptor-backed string assignment
conversion parity for geometry results, or a separate topology engine.

## SQLite Integration

The implementation uses SQLite's public scalar UDF API. MyLite owns geometry
argument validation, WKB traversal, vertex extraction, hull construction, MySQL
diagnostics, and result construction. No SQLite fork hook is required.

## Grammar

No grammar change is required. The function is admitted through the existing
generic function AST surface:

```lemon
expr(A) ::= function_name(NM) LP optional_expr_list(ARGS) RP.
```

The runtime maps the admitted function name to
`MYLITE_SPATIAL_FUNCTION_ST_CONVEXHULL` and validates argument count and
argument semantics.
