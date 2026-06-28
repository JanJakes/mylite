# Baseline Spatial Validity Functions

## Summary

This slice adds MySQL-shaped validity checks for MyLite's representable
geometry values:

```sql
SELECT ST_IsValid(g);
SELECT ST_AsText(ST_Validate(g));
```

`ST_IsValid()` returns `1` for geometrically valid values, `0` for
geometrically invalid values, and `NULL` for `NULL`. `ST_Validate()` returns
the original geometry when it is valid and `NULL` when it is invalid or `NULL`.

The slice is a runtime spatial function implementation over MyLite's existing
internal geometry BLOB format. It does not add a topology engine, loaded SRS
catalog, geographic SRS range rechecking beyond existing constructors, geometry
repair, `ST_IsSimple()`, or relation predicates.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- MySQL 8.4 Reference Manual, Spatial Convenience Functions:
  <https://dev.mysql.com/doc/refman/8.4/en/spatial-convenience-functions.html>
- MySQL 8.4.9 runtime observations recorded by
  `packages/libmylite/tests/mysql_baseline_spatial_validity_functions_expectations.sh`.

This specification is independently authored from official MySQL 8.4
documentation, observed MySQL 8.4.9 behavior, public SQLite APIs, and existing
MyLite source. It does not copy MySQL, MariaDB, Percona, SQLite implementation
internals, or restrictively licensed implementation sources.

## MySQL 8.4.9 Observations

Runtime probes establish:

- `ST_IsValid(NULL)` and `ST_Validate(NULL)` return `NULL`.
- Points and empty geometry collections are valid.
- `LINESTRING(0 0,0 0)` is geometrically invalid, while
  `LINESTRING(0 0,0 0,1 1)` is valid.
- A self-crossing line can still be valid; simplicity is separate from
  validity.
- `MULTIPOINT((0 0),(0 0))` is valid.
- Valid polygons return `1` and `ST_Validate()` returns the same WKT.
- Degenerate, self-intersecting, outside-hole, crossing-hole, and overlapping
  multipolygon surfaces return `0` from `ST_IsValid()` and `NULL` from
  `ST_Validate()`.
- Geometry collections are valid only when each member is valid.
- Constructor-level syntax errors, such as a one-point `LINESTRING`, fail before
  `ST_IsValid()` or `ST_Validate()` evaluates.

## Scope

Supported:

- `ST_IsValid(geometry)` and `ST_Validate(geometry)` with exactly one
  argument;
- `NULL` propagation;
- MyLite internal geometry values for `POINT`, `LINESTRING`, `POLYGON`,
  `MULTIPOINT`, `MULTILINESTRING`, `MULTIPOLYGON`, and
  `GEOMETRYCOLLECTION`;
- empty geometry collection validity;
- line validity requiring at least two points and at least two distinct points;
- polygon validity requiring closed non-degenerate rings, simple ring segments,
  holes inside the exterior ring, no holes inside holes, and no invalid ring
  crossings;
- multipolygon validity rejecting overlapping or edge-sharing polygon surfaces;
- scalar, table-backed, and DML value contexts supported by existing spatial
  function execution.

Deferred:

- complete OGC topology for all exotic boundary cases;
- geographic SRS catalog lookup and coordinate range checks for raw stored
  values outside existing constructor validation;
- clockwise-ring rewriting described for broader MySQL `ST_Validate()`
  behavior;
- invalid geometry repair;
- `ST_IsSimple()`, relation predicates, and constructive operations.

## Ownership Boundaries

- Public API: no new ABI.
- Parser: existing generic scalar function grammar admits these names.
- Runtime: MyLite evaluates validity from decoded geometry bytes.
- SQLite: no new extension point or fork patch is needed.
- Storage: no catalog or side-table state is introduced.

## Supported SQL Grammar

Existing generic function grammar admits the functions in this slice.

MyLite Lemon-syntax sketch:

```lemon
expr ::= generic_function_call.

generic_function_call ::=
    spatial_validity_function LPAREN expr RPAREN.

spatial_validity_function ::=
    ST_ISVALID | ST_VALIDATE.
```

## Runtime Semantics

- `ST_IsValid(NULL)` returns `NULL`.
- `ST_Validate(NULL)` returns `NULL`.
- Syntactically malformed geometry input raises the same invalid-GIS-data
  diagnostic used by existing spatial functions.
- Valid geometry returns `1` from `ST_IsValid()` and a copied original geometry
  value from `ST_Validate()`.
- Invalid geometry returns `0` from `ST_IsValid()` and `NULL` from
  `ST_Validate()`.
- Result metadata follows existing spatial result typing:
  `ST_IsValid()` is integer-valued and `ST_Validate()` is geometry-valued.

## Tests

The test suite covers:

- MySQL expectation capture for valid, invalid, and `NULL` values;
- point, line, polygon, multipolygon, collection, and empty collection cases;
- `ST_Validate()` WKT readback for valid values and `NULL` for invalid values;
- table-backed projection and DML scalar contexts;
- argument-count and invalid-GIS-data diagnostics.
