# Baseline Spatial Touches Function

## Summary

This slice adds MySQL-shaped SRID-0 object-shape touching:

```sql
SELECT ST_Touches(g1, g2);
```

`ST_Touches(g1, g2)` returns `1` when two geometries spatially intersect while
their interiors do not intersect, and the pair is not a purely zero-dimensional
comparison. It returns `0` for intersecting geometries whose interiors intersect
and for disjoint non-empty mixed-dimension geometries. It returns `NULL` when
either argument is `NULL`, when either argument is an empty geometry collection,
or when both arguments are zero-dimensional.

The implementation reuses MyLite's decoded geometry representation and distance
helpers for contact detection, then applies a MyLite-owned interior-intersection
check over representable SRID-0 Point, LineString, Polygon, Multi*, and
GeometryCollection values. It does not add a full DE-9IM matrix engine,
constructive geometry operations, geographic SRS evaluation, or physical
spatial search.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- MySQL 8.4 Reference Manual, Spatial Relation Functions That Use Object
  Shapes:
  <https://dev.mysql.com/doc/refman/8.4/en/spatial-relation-functions-object-shapes.html>
- MySQL 8.4.9 runtime observations recorded by
  `packages/libmylite/tests/mysql_baseline_spatial_touches_function_expectations.sh`.

This specification is independently authored from official MySQL 8.4
documentation, observed MySQL 8.4.9 behavior, public SQLite APIs, and existing
MyLite source. It does not copy MySQL, MariaDB, Percona, SQLite implementation
internals, or restrictively licensed implementation sources.

## MySQL 8.4.9 Observations

Runtime probes establish:

- `NULL` arguments return `NULL`.
- Empty geometry collection arguments return `NULL`.
- Point/Point and MultiPoint/Point comparisons return `NULL`.
- A Point on a LineString endpoint touches the LineString; a Point on the
  LineString interior does not.
- A Point on a Polygon outer or inner ring boundary touches the Polygon; a
  Point in the Polygon interior or exterior does not.
- LineStrings that share only an endpoint touch. LineStrings that cross in
  their interiors, overlap with positive length, or are equal do not touch.
- A LineString on a Polygon boundary touches the Polygon. A LineString from the
  boundary into the Polygon interior, or through the Polygon interior, does not.
- Polygons that share an edge or a point touch. Equal Polygons and Polygons
  with overlapping or contained interiors do not.
- Collection arguments touch when at least one child pair has boundary contact
  and no child pair has interior intersection; disjoint child members do not
  cancel the touch.
- Invalid raw geometry data raises the spatial invalid-data diagnostic for the
  called function.
- Incorrect argument counts raise the native-function parameter-count
  diagnostic for the called function.

## Scope

Supported:

- `ST_Touches(geometry, geometry)` with exactly two arguments;
- `NULL` propagation and empty-geometry `NULL` results;
- zero-dimensional-pair `NULL` results;
- SRID mismatch diagnostics;
- SRID-0 object-shape touching checks for MyLite's representable simple Point,
  LineString, and Polygon values;
- child-decomposed touch checks for Multi* and GeometryCollection values;
- scalar, table-backed, and DML value contexts supported by existing spatial
  function execution.

Deferred:

- full DE-9IM matrix evaluation for every invalid or self-overlapping geometry;
- geographic SRS relation evaluation;
- constructive operations such as `ST_Intersection()`;
- physical spatial search.

## Ownership Boundaries

- Public API: no new ABI.
- Parser: existing generic scalar function grammar admits this name.
- Runtime: MyLite evaluates touches from decoded geometry bytes.
- SQLite: no new extension point or fork patch is needed.
- Storage: no catalog or side-table state is introduced.

## Supported SQL Grammar

Existing generic function grammar admits the function in this slice.

MyLite Lemon-syntax sketch:

```lemon
expr ::= generic_function_call.

generic_function_call ::=
    spatial_relation_function LPAREN expr COMMA expr RPAREN.

spatial_relation_function ::=
    ST_TOUCHES.
```

## Runtime Semantics

- `NULL` arguments return `NULL`.
- Empty geometry collection arguments return `NULL`.
- Pairs whose maximum topological dimension is zero return `NULL`.
- Syntactically malformed geometry input raises the same invalid-GIS-data
  diagnostic used by existing spatial functions.
- Different SRIDs raise the existing binary-geometry different-SRID diagnostic.
- Nonzero SRIDs currently raise MyLite's existing geographic-SRS not implemented
  diagnostic for this relation baseline.
- For non-empty nonzero-dimension SRID-0 values, `ST_Touches()` first checks
  whether the geometries intersect. Disjoint geometries return `0`.
- Intersecting values return `1` only when no interior-intersection exists.
- Collection arguments are evaluated child-by-child: any child contact may
  establish the intersection, while any child-pair interior intersection makes
  the result `0`.
- Result metadata follows existing spatial result typing: `ST_Touches()` is
  integer-valued.

## Tests

The test suite covers:

- MySQL expectation capture for `NULL`, empty, zero-dimensional, point/line,
  point/polygon, line/line, line/polygon, polygon/polygon, and collection cases;
- table-backed projection and DML scalar contexts;
- argument-count, invalid-GIS-data, SRID-mismatch, and geographic-SRS
  diagnostics.
