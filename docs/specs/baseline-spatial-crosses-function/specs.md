# Baseline Spatial Crosses Function

## Summary

This slice adds MySQL-shaped SRID-0 object-shape crossing checks:

```sql
SELECT ST_Crosses(g1, g2);
```

`ST_Crosses(g1, g2)` returns `1` when the interiors of the geometries intersect
in MySQL's dimension-dependent crossing sense. For LineString/LineString pairs,
the intersection must be a finite set of interior points rather than a shared
line segment. For other supported combinations, the interiors must intersect
and the right geometry must not cover all of the left geometry's interior.

The function returns `NULL` when either argument is `NULL`, either argument is
an empty geometry collection, the left geometry is dimension 2, or the right
geometry is dimension 0. It returns `0` for contained, equal, endpoint-only,
boundary-only, disjoint, or line-overlap cases.

The implementation reuses MyLite's decoded geometry representation and existing
interior-intersection and containment helpers. It does not add a full DE-9IM
matrix engine, constructive intersection output, geographic SRS relation
evaluation, or physical spatial search.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- MySQL 8.4 Reference Manual, Spatial Relation Functions That Use Object
  Shapes:
  <https://dev.mysql.com/doc/refman/8.4/en/spatial-relation-functions-object-shapes.html>
- MySQL 8.4.9 runtime observations recorded by
  `packages/libmylite/tests/mysql_baseline_spatial_crosses_function_expectations.sh`.

This specification is independently authored from official MySQL 8.4
documentation, observed MySQL 8.4.9 behavior, public SQLite APIs, and existing
MyLite source. It does not copy MySQL, MariaDB, Percona, SQLite implementation
internals, or restrictively licensed implementation sources.

## MySQL 8.4.9 Observations

Runtime probes establish:

- `NULL` arguments return `NULL`.
- Empty geometry collection arguments return `NULL`.
- Point/Point comparisons return `NULL` because the right geometry is
  zero-dimensional.
- Point/LineString and Point/Polygon comparisons return `0` when the single
  point lies in the right geometry's interior because the right geometry covers
  the whole left interior.
- MultiPoint values cross LineString or Polygon values when some, but not all,
  points are in the right geometry interior.
- LineString/Point comparisons return `NULL` because the right geometry is
  zero-dimensional.
- LineString/LineString pairs cross when their interiors meet at a point.
  Endpoint-only intersections, T-shapes where the shared point is a line
  endpoint, positive-length overlaps, containment, and equality return `0`.
- A LineString crossing through a Polygon interior and exterior returns `1`.
  A LineString wholly inside the Polygon or on the Polygon boundary returns
  `0`.
- Polygon left operands return `NULL`.
- GeometryCollection values participate through their supported child geometry
  surfaces.
- Invalid raw geometry data raises the spatial invalid-data diagnostic for the
  called function.
- Incorrect argument counts raise the native-function parameter-count
  diagnostic for the called function.

## Scope

Supported:

- `ST_Crosses(geometry, geometry)` with exactly two arguments;
- `NULL` propagation and empty-geometry `NULL` results;
- MySQL's dimension-based `NULL` cases for dimension-2 left operands and
  dimension-0 right operands;
- SRID mismatch diagnostics;
- SRID-0 object-shape crossing checks for MyLite's representable simple Point,
  LineString, and Polygon values;
- child-decomposed crossing checks for Multi* and GeometryCollection values;
- scalar, table-backed, and DML value contexts supported by existing spatial
  function execution.

Deferred:

- full DE-9IM matrix evaluation for every invalid or self-overlapping geometry;
- geographic SRS relation evaluation for dimension combinations that require
  actual relation computation;
- constructive operations such as `ST_Intersection()`;
- physical spatial search.

## Ownership Boundaries

- Public API: no new ABI.
- Parser: existing generic scalar function grammar admits this name.
- Runtime: MyLite evaluates crosses from decoded geometry bytes.
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
    ST_CROSSES.
```

## Runtime Semantics

- `NULL` arguments return `NULL`.
- Empty geometry collection arguments return `NULL`.
- Different SRIDs raise the existing binary-geometry different-SRID diagnostic.
- Pairs where the left dimension is 2 or the right dimension is 0 return
  `NULL` in the supported SRID-0 surface.
- Nonzero same-SRS relation evaluation is deferred.
- SRID-0 LineString/LineString values return `1` only for finite-point interior
  crossings.
- Other supported SRID-0 combinations return `1` when the interiors intersect
  and the right geometry does not cover all of the left geometry's interior.
- Result metadata follows existing spatial result typing: `ST_Crosses()` is
  integer-valued.

## Tests

The test suite covers:

- MySQL expectation capture for `NULL`, empty, dimension-null, point,
  MultiPoint, line/line, line/polygon, polygon-left, and GeometryCollection
  cases;
- table-backed projection and DML scalar contexts;
- argument-count, invalid-GIS-data, SRID-mismatch, and geographic-SRS
  diagnostics.
