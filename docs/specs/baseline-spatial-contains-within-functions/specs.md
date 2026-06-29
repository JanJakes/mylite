# Baseline Spatial Contains/Within Functions

## Summary

This slice adds MySQL-shaped SRID-0 object-shape containment predicates:

```sql
SELECT ST_Contains(g1, g2), ST_Within(g1, g2);
```

`ST_Contains(g1, g2)` returns `1` when `g1` contains `g2` under MySQL's
object-shape relation semantics and `0` otherwise. `ST_Within(g1, g2)` returns
the inverse containment relation. Both functions return `NULL` when either
argument is `NULL` or an empty geometry collection.

The implementation is a runtime spatial relation over MyLite's existing
decoded geometry representation. It handles boundary-sensitive containment for
SRID-0 `POINT`, `LINESTRING`, and `POLYGON` values, plus child-decomposed
`MULTIPOINT`, `MULTILINESTRING`, `MULTIPOLYGON`, and `GEOMETRYCOLLECTION`
values. It does not add a full DE-9IM matrix engine, constructive geometry
operations, geographic SRS containment, or physical spatial search.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- MySQL 8.4 Reference Manual, Spatial Relation Functions That Use Object
  Shapes:
  <https://dev.mysql.com/doc/refman/8.4/en/spatial-relation-functions-object-shapes.html>
- MySQL 8.4.9 runtime observations recorded by
  `packages/libmylite/tests/mysql_baseline_spatial_contains_within_functions_expectations.sh`.

This specification is independently authored from official MySQL 8.4
documentation, observed MySQL 8.4.9 behavior, public SQLite APIs, and existing
MyLite source. It does not copy MySQL, MariaDB, Percona, SQLite implementation
internals, or restrictively licensed implementation sources.

## MySQL 8.4.9 Observations

Runtime probes establish:

- `NULL` arguments return `NULL`.
- Empty geometry collection arguments return `NULL`.
- Equal points contain each other.
- Polygons contain strictly interior points and lines, but not boundary-only
  points or boundary-only lines.
- LineStrings contain points and sub-lines on their interior, but not endpoint
  boundary points.
- Polygons contain equal polygons, strictly interior polygons, and polygons
  whose surface partly shares the container boundary while their interior is
  inside the container.
- Polygon holes exclude points in the hole interior and on the hole boundary.
- Child-decomposed Multi* and GeometryCollection arguments participate in
  containment when one child contains the tested shape.
- Invalid raw geometry data raises the spatial invalid-data diagnostic for the
  called function.
- Incorrect argument counts raise the native-function parameter-count
  diagnostic for the called function.

## Scope

Supported:

- `ST_Contains(geometry, geometry)` and `ST_Within(geometry, geometry)` with
  exactly two arguments;
- `NULL` propagation and empty-geometry `NULL` results;
- SRID mismatch diagnostics;
- SRID-0 object-shape containment checks for MyLite's representable simple
  Point, LineString, and Polygon values;
- child-decomposed containment for Multi* and GeometryCollection values;
- scalar, table-backed, and DML value contexts supported by existing spatial
  function execution.

Deferred:

- full union topology across multiple collection children where no individual
  child contains the target shape;
- full DE-9IM matrix evaluation for every invalid or self-overlapping geometry;
- geographic SRS relation evaluation;
- related predicates such as `ST_Equals()`, `ST_Touches()`, `ST_Overlaps()`,
  and `ST_Crosses()`;
- constructive operations such as `ST_Intersection()`.

## Ownership Boundaries

- Public API: no new ABI.
- Parser: existing generic scalar function grammar admits these names.
- Runtime: MyLite evaluates relations from decoded geometry bytes.
- SQLite: no new extension point or fork patch is needed.
- Storage: no catalog or side-table state is introduced.

## Supported SQL Grammar

Existing generic function grammar admits the functions in this slice.

MyLite Lemon-syntax sketch:

```lemon
expr ::= generic_function_call.

generic_function_call ::=
    spatial_relation_function LPAREN expr COMMA expr RPAREN.

spatial_relation_function ::=
    ST_CONTAINS | ST_WITHIN.
```

## Runtime Semantics

- `NULL` arguments return `NULL`.
- Empty geometry collection arguments return `NULL`.
- Syntactically malformed geometry input raises the same invalid-GIS-data
  diagnostic used by existing spatial functions.
- Different SRIDs raise the existing binary-geometry different-SRID diagnostic.
- Nonzero SRIDs currently raise MyLite's existing geographic-SRS not implemented
  diagnostic for this relation baseline.
- `ST_Within(a, b)` is evaluated as `ST_Contains(b, a)`.
- Points contain only equal points.
- LineStrings contain points or LineStrings only when the tested shape has
  interior on the container LineString and no point outside its surface.
- Polygons contain points, LineStrings, and Polygons only when the tested shape
  is on/in the Polygon surface and has interior inside the Polygon, with equal
  Polygon surfaces treated as contained.
- Multi* and GeometryCollection containers are evaluated child-by-child; tested
  collections require every child to be contained by the container relation.
- Result metadata follows existing spatial result typing: both functions are
  integer-valued.

## Tests

The test suite covers:

- MySQL expectation capture for `NULL`, empty, point, line, polygon,
  polygon-hole, Multi*, and GeometryCollection cases;
- table-backed projection and DML scalar contexts;
- argument-count, invalid-GIS-data, SRID-mismatch, and geographic-SRS
  diagnostics.
