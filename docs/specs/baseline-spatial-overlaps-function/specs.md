# Baseline Spatial Overlaps Function

## Summary

This slice adds MySQL-shaped SRID-0 object-shape overlap checks:

```sql
SELECT ST_Overlaps(g1, g2);
```

`ST_Overlaps(g1, g2)` returns `1` when two non-empty geometries have the same
topological dimension, intersect in that same dimension, and the intersection is
not equal to either input. It returns `0` for disjoint, equal, contained,
endpoint-only, boundary-only, or point-only line-crossing cases. It returns
`NULL` when either argument is `NULL`, either argument is an empty geometry
collection, or the two arguments have different dimensions.

The implementation reuses MyLite's decoded geometry representation and existing
relation helpers for containment, collinear line segment overlap, and polygon
interior intersection. It does not add a full DE-9IM matrix engine,
constructive intersection output, geographic SRS relation evaluation, or
physical spatial search.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- MySQL 8.4 Reference Manual, Spatial Relation Functions That Use Object
  Shapes:
  <https://dev.mysql.com/doc/refman/8.4/en/spatial-relation-functions-object-shapes.html>
- MySQL 8.4.9 runtime observations recorded by
  `packages/libmylite/tests/mysql_baseline_spatial_overlaps_function_expectations.sh`.

This specification is independently authored from official MySQL 8.4
documentation, observed MySQL 8.4.9 behavior, public SQLite APIs, and existing
MyLite source. It does not copy MySQL, MariaDB, Percona, SQLite implementation
internals, or restrictively licensed implementation sources.

## MySQL 8.4.9 Observations

Runtime probes establish:

- `NULL` arguments return `NULL`.
- Empty geometry collection arguments return `NULL`.
- Mixed-dimension pairs return `NULL`.
- Point/Point comparisons return `0`.
- MultiPoint and zero-dimensional GeometryCollection values overlap when they
  share at least one point and neither point set contains the other.
- LineStrings overlap when they share a positive-length collinear segment and
  neither geometry contains the other.
- LineStrings that only share an endpoint, cross at a single interior point, are
  equal, or contain one another return `0`.
- Polygons overlap when their interiors intersect with area and neither Polygon
  contains the other.
- Polygons that only touch on an edge or point, are equal, or contain one
  another return `0`.
- MultiPolygon and GeometryCollection values are evaluated through their child
  geometry surfaces for the supported SRID-0 cases.
- Invalid raw geometry data raises the spatial invalid-data diagnostic for the
  called function.
- Incorrect argument counts raise the native-function parameter-count
  diagnostic for the called function.

## Scope

Supported:

- `ST_Overlaps(geometry, geometry)` with exactly two arguments;
- `NULL` propagation and empty-geometry `NULL` results;
- mixed-dimension `NULL` results;
- SRID mismatch diagnostics;
- SRID-0 object-shape overlap checks for MyLite's representable simple Point,
  LineString, and Polygon values;
- child-decomposed overlap checks for Multi* and GeometryCollection values;
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
- Runtime: MyLite evaluates overlaps from decoded geometry bytes.
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
    ST_OVERLAPS.
```

## Runtime Semantics

- `NULL` arguments return `NULL`.
- Empty geometry collection arguments return `NULL`.
- Different SRIDs raise the existing binary-geometry different-SRID diagnostic.
- Nonzero SRIDs currently raise MyLite's existing geographic-SRS not implemented
  diagnostic for this relation baseline.
- Pairs with different maximum topological dimensions return `NULL`.
- Equal or contained pairs return `0`.
- Zero-dimensional values return `1` only for partial point-set overlap.
- One-dimensional values return `1` only for positive-length collinear overlap
  not equal to either input.
- Two-dimensional values return `1` only for polygon interior-area overlap not
  equal to either input.
- Result metadata follows existing spatial result typing: `ST_Overlaps()` is
  integer-valued.

## Tests

The test suite covers:

- MySQL expectation capture for `NULL`, empty, mixed-dimension, point,
  MultiPoint, line/line, line/polygon, polygon/polygon, MultiPolygon, and
  GeometryCollection cases;
- table-backed projection and DML scalar contexts;
- argument-count, invalid-GIS-data, SRID-mismatch, and geographic-SRS
  diagnostics.
