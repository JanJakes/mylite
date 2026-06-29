# Baseline Spatial Disjoint/Intersects Functions

## Summary

This slice adds MySQL-shaped SRID-0 object-shape relation predicates:

```sql
SELECT ST_Disjoint(g1, g2), ST_Intersects(g1, g2);
```

`ST_Disjoint()` returns `1` when two non-empty geometry values do not
intersect and `0` when they do. `ST_Intersects()` returns the inverse. Both
functions return `NULL` when either argument is `NULL` or an empty geometry
collection.

The implementation is a runtime spatial function over MyLite's existing
internal geometry BLOB format and distance/intersection helpers. It does not
add a full relation algebra, geographic SRS support, constructive geometry
operations, or spatial index search.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- MySQL 8.4 Reference Manual, Spatial Relation Functions That Use Object
  Shapes:
  <https://dev.mysql.com/doc/refman/8.4/en/spatial-relation-functions-object-shapes.html>
- MySQL 8.4.9 runtime observations recorded by
  `packages/libmylite/tests/mysql_baseline_spatial_disjoint_intersects_functions_expectations.sh`.

This specification is independently authored from official MySQL 8.4
documentation, observed MySQL 8.4.9 behavior, public SQLite APIs, and existing
MyLite source. It does not copy MySQL, MariaDB, Percona, SQLite implementation
internals, or restrictively licensed implementation sources.

## MySQL 8.4.9 Observations

Runtime probes establish:

- `ST_Disjoint(NULL, g)` and `ST_Intersects(NULL, g)` return `NULL`.
- Empty geometry collection arguments return `NULL`.
- Equal points intersect and are not disjoint; separate points are disjoint.
- Points on LineStrings, line crossings, points inside or on Polygon
  boundaries, overlapping Polygons, touching Polygons, and collection members
  intersect.
- Points outside Lines/Polygons and separated Lines are disjoint.
- Invalid raw geometry data raises the spatial invalid-data diagnostic for the
  called function.
- Incorrect argument counts raise the native-function parameter-count
  diagnostic for the called function.

## Scope

Supported:

- `ST_Disjoint(geometry, geometry)` and `ST_Intersects(geometry, geometry)` with
  exactly two arguments;
- `NULL` propagation and empty-geometry `NULL` results;
- SRID mismatch diagnostics;
- SRID-0 object-shape relation checks for MyLite's representable `POINT`,
  `LINESTRING`, `POLYGON`, `MULTIPOINT`, `MULTILINESTRING`, `MULTIPOLYGON`,
  and `GEOMETRYCOLLECTION` values;
- scalar, table-backed, and DML value contexts supported by existing spatial
  function execution.

Deferred:

- geographic SRS relation evaluation;
- exhaustive undefined behavior for geometrically invalid arguments;
- relation predicates with stricter DE-9IM distinctions such as
  `ST_Contains()`, `ST_Within()`, `ST_Touches()`, `ST_Overlaps()`, and
  `ST_Crosses()`;
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
    ST_DISJOINT | ST_INTERSECTS.
```

## Runtime Semantics

- `NULL` arguments return `NULL`.
- Empty geometry collection arguments return `NULL`.
- Syntactically malformed geometry input raises the same invalid-GIS-data
  diagnostic used by existing spatial functions.
- Different SRIDs raise the existing binary-geometry different-SRID diagnostic.
- Nonzero SRIDs currently raise MyLite's existing geographic-SRS not implemented
  diagnostic for this relation baseline.
- For SRID-0 non-empty geometries, zero object-shape distance means
  `ST_Intersects()` returns `1` and `ST_Disjoint()` returns `0`; nonzero
  distance means `ST_Intersects()` returns `0` and `ST_Disjoint()` returns `1`.
- Result metadata follows existing spatial result typing: both functions are
  integer-valued.

## Tests

The test suite covers:

- MySQL expectation capture for `NULL`, empty, point, line, polygon,
  multipolygon, and geometry-collection cases;
- table-backed projection and DML scalar contexts;
- argument-count, invalid-GIS-data, and SRID-mismatch diagnostics.
