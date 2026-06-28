# Baseline Spatial Latitude And Longitude Functions

## Summary

This slice implements getter support for MySQL's geographic point coordinate
helpers:

```sql
SELECT ST_Latitude(ST_GeomFromGeoJSON('{"type":"Point","coordinates":[90,45]}'));
SELECT ST_Longitude(ST_GeomFromGeoJSON('{"type":"Point","coordinates":[90,45]}'));
```

MyLite already stores SRID `4326` point values in MySQL display axis order,
with stored X as latitude and stored Y as longitude. The new functions expose
that existing representation through MySQL-compatible scalar results and
diagnostics.

This slice intentionally does not implement the optional two-argument mutation
forms. Those return a geometry for the same function names that normally return
`DOUBLE`, while MyLite's current spatial result metadata is keyed only by
function name. Supporting those forms should be a separate arity-sensitive
spatial metadata slice together with the analogous `ST_X()` and `ST_Y()`
mutation forms.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- MySQL 8.4 Reference Manual, point property functions:
  <https://dev.mysql.com/doc/refman/8.4/en/gis-point-property-functions.html>
- MySQL 8.4.9 runtime observations recorded by
  `packages/libmylite/tests/mysql_baseline_spatial_latitude_longitude_functions_expectations.sh`.

This specification is independently authored from official MySQL 8.4
documentation, observed MySQL 8.4.9 behavior, public SQLite APIs, and existing
MyLite source. It does not copy MySQL, MariaDB, Percona, SQLite implementation
internals, or restrictively licensed implementation sources.

## MySQL 8.4.9 Observations

Runtime probes establish:

- `ST_Latitude(ST_GeomFromGeoJSON('{"type":"Point","coordinates":[90,45]}'))`
  returns `45`.
- `ST_Longitude(ST_GeomFromGeoJSON('{"type":"Point","coordinates":[90,45]}'))`
  returns `90`.
- Table-backed `POINT` values in SRID `4326` produce the same latitude and
  longitude values and return `NULL` for `NULL` geometry cells.
- `ST_Latitude(NULL)` and `ST_Longitude(NULL)` return `NULL`.
- Non-`POINT` geometry arguments fail before the geographic-SRS check with
  `3516 / 22S01`, for example `POINT value is a geometry of unexpected type
  LINESTRING in st_latitude`.
- A `POINT` in SRID `0` fails with `3726 / 22S00`, naming the function and
  reporting that SRID `0` is not geographic.
- Undefined SRIDs fail with `3548 / SR001` before the accessor is evaluated
  when the value is constructed.
- The optional setter forms preserve SRID `4326` in MySQL, but are outside
  this slice for the arity-sensitive metadata reason above.

## Scope

Supported:

- `ST_Latitude(point)` getter for valid SRID `4326` `POINT` geometries;
- `ST_Longitude(point)` getter for valid SRID `4326` `POINT` geometries;
- scalar, row-backed, and DML value contexts admitted through existing generic
  spatial function expression handling;
- SQL `NULL` propagation;
- MySQL-shaped diagnostics for invalid geometry, unexpected geometry type, and
  non-geographic point SRIDs;
- MySQL-shaped `DOUBLE` result metadata for both getter functions.

Deferred:

- `ST_Latitude(point, new_latitude)` and
  `ST_Longitude(point, new_longitude)` geometry-returning mutation forms;
- a general SRS catalog or geographic SRS support beyond the existing SRID
  `4326` constant;
- coordinate transformations, projected SRS handling, topology, and spatial
  search/index execution.

## Ownership Boundaries

- Public API: no new ABI.
- Parser: no grammar change; generic function calls already admit these names.
- Runtime: MyLite owns SRID/type checks and coordinate extraction in the
  spatial evaluator.
- SQLite: functions use the existing public SQLite scalar UDF bridge. No
  targeted SQLite fork hook is needed.
- Storage: no file-format or catalog change; the functions read existing
  internal geometry bytes.

## Supported SQL Grammar

Existing generic function grammar admits the function names.

MyLite Lemon-syntax sketch:

```lemon
expr ::= generic_function_call.

generic_function_call ::=
    spatial_function_name LPAREN function_argument_list_opt RPAREN.

spatial_function_name ::=
    ST_LATITUDE | ST_LONGITUDE | other_supported_spatial_function_name.
```

## Runtime Semantics

- Exactly one argument is accepted in this slice.
- `NULL` geometry returns `NULL`.
- Geometry bytes are validated through the existing internal geometry checker.
- Non-`POINT` geometries return `3516 / 22S01`.
- A `POINT` whose SRID is not `4326` returns:
  - `3726 / 22S00` for SRID `0`, which MyLite treats as known but
    non-geographic;
  - `3548 / SR001` for nonzero unsupported SRIDs, matching the current static
    MyLite SRS subset.
- `ST_Latitude()` returns the stored X coordinate for SRID `4326` points.
- `ST_Longitude()` returns the stored Y coordinate for SRID `4326` points.
- This slice uses the existing GeoJSON and geohash SRID `4326` constructors in
  tests; WKT/WKB constructors with SRID `4326` remain outside this slice.

## Tests

The test suite covers:

- MySQL expectation capture for scalar values, table-backed values, nulls, and
  diagnostics;
- MyLite runtime scalar and row-backed results;
- metadata for the two getter functions;
- invalid arity, non-geographic point, non-point, and invalid geometry errors.
