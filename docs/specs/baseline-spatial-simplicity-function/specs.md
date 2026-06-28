# Baseline Spatial Simplicity Function

## Summary

This slice adds MySQL-shaped simplicity checks for MyLite's representable
geometry values:

```sql
SELECT ST_IsSimple(g);
```

`ST_IsSimple()` returns `1` for simple geometry values, `0` for nonsimple
geometry values, and `NULL` for `NULL`.

The implementation is a runtime spatial function over MyLite's existing
internal geometry BLOB format. It does not add a full topology engine, loaded
SRS catalog, geographic SRS range rechecking beyond existing constructors, or
spatial relation predicates.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- MySQL 8.4 Reference Manual, General Geometry Property Functions:
  <https://dev.mysql.com/doc/refman/8.4/en/gis-general-property-functions.html>
- MySQL 8.4 Reference Manual, OpenGIS geometry model class pages:
  <https://dev.mysql.com/doc/refman/8.4/en/gis-class-geometry.html>,
  <https://dev.mysql.com/doc/refman/8.4/en/gis-class-curve.html>,
  <https://dev.mysql.com/doc/refman/8.4/en/gis-class-linestring.html>,
  <https://dev.mysql.com/doc/refman/8.4/en/gis-class-multipoint.html>,
  <https://dev.mysql.com/doc/refman/8.4/en/gis-class-multicurve.html>,
  <https://dev.mysql.com/doc/refman/8.4/en/gis-class-polygon.html>, and
  <https://dev.mysql.com/doc/refman/8.4/en/gis-class-geometrycollection.html>.
- MySQL 8.4.9 runtime observations recorded by
  `packages/libmylite/tests/mysql_baseline_spatial_simplicity_function_expectations.sh`.

This specification is independently authored from official MySQL 8.4
documentation, observed MySQL 8.4.9 behavior, public SQLite APIs, and existing
MyLite source. It does not copy MySQL, MariaDB, Percona, SQLite implementation
internals, or restrictively licensed implementation sources.

## MySQL 8.4.9 Observations

Runtime probes establish:

- `ST_IsSimple(NULL)` returns `NULL`.
- Points and empty geometry collections are simple.
- Simple open and closed LineStrings return `1`.
- LineStrings that repeat a point, backtrack through an already traversed
  segment, contain consecutive duplicate points, or self-intersect return `0`.
- `MULTIPOINT` is simple only when its point values are unique.
- `MULTILINESTRING` is simple when all child LineStrings are simple and any
  cross-child contacts are shared open-LineString endpoints. Interior
  crossings, endpoint-to-interior contacts, overlapping segments, repeated
  child LineStrings, and contacts involving closed child boundaries return `0`.
- Polygon and MultiPolygon values return `1` for the observed valid and invalid
  surface examples; validity is separate from simplicity for these surfaces.
- Geometry collections return `0` when child values are nonsimple or when
  cross-child contacts happen in interiors. Boundary contacts such as a point
  on a LineString endpoint or polygon boundary return `1`.
- Argument-count and invalid-GIS-data diagnostics match the existing spatial
  property function family.

## Scope

Supported:

- `ST_IsSimple(geometry)` with exactly one argument;
- `NULL` propagation;
- MyLite internal geometry values for `POINT`, `LINESTRING`, `POLYGON`,
  `MULTIPOINT`, `MULTILINESTRING`, `MULTIPOLYGON`, and
  `GEOMETRYCOLLECTION`;
- empty geometry collection simplicity;
- LineString duplicate-point, backtracking, segment-overlap, and
  self-intersection checks;
- MultiPoint duplicate coordinate checks;
- MultiLineString child simplicity and boundary-only cross-child contacts;
- practical GeometryCollection child simplicity and point/line/polygon
  cross-child contact checks for the MySQL-verified baseline cases;
- scalar, table-backed, and DML value contexts supported by existing spatial
  function execution.

Deferred:

- complete OGC topology for every exotic GeometryCollection and
  Polygon/Polygon boundary-interaction case;
- geographic SRS catalog lookup and coordinate range checks for raw stored
  values outside existing constructor validation;
- relation predicates such as `ST_Intersects()` and `ST_Touches()`;
- constructive geometry operations or geometry repair.

## Ownership Boundaries

- Public API: no new ABI.
- Parser: existing generic scalar function grammar admits this name.
- Runtime: MyLite evaluates simplicity from decoded geometry bytes.
- SQLite: no new extension point or fork patch is needed.
- Storage: no catalog or side-table state is introduced.

## Supported SQL Grammar

Existing generic function grammar admits the function in this slice.

MyLite Lemon-syntax sketch:

```lemon
expr ::= generic_function_call.

generic_function_call ::=
    spatial_simplicity_function LPAREN expr RPAREN.

spatial_simplicity_function ::=
    ST_ISSIMPLE.
```

## Runtime Semantics

- `ST_IsSimple(NULL)` returns `NULL`.
- Syntactically malformed geometry input raises the same invalid-GIS-data
  diagnostic used by existing spatial functions.
- Simple geometry returns `1`; nonsimple geometry returns `0`.
- Result metadata follows existing spatial result typing:
  `ST_IsSimple()` is integer-valued.

## Tests

The test suite covers:

- MySQL expectation capture for simple, nonsimple, and `NULL` values;
- point, line, polygon, multipoint, multiline, multipolygon, collection, and
  empty collection cases;
- table-backed projection and DML scalar contexts;
- argument-count and invalid-GIS-data diagnostics.
