# Baseline Spatial Equals Function

## Summary

This slice adds MySQL-shaped SRID-0 object-shape equality:

```sql
SELECT ST_Equals(g1, g2);
```

`ST_Equals(g1, g2)` returns `1` when two geometries have the same object shape
under MySQL's spatial relation semantics and `0` otherwise. The function
returns `NULL` when either argument is `NULL`. Unlike containment predicates,
two empty geometry collections compare equal, while exactly one empty geometry
collection compares unequal.

The implementation reuses MyLite's decoded spatial geometry representation and
evaluates non-empty SRID-0 equality as mutual containment over representable
simple geometries and child-decomposed collections. It does not add a complete
DE-9IM matrix engine, constructive geometry operations, geographic SRS
evaluation, or physical spatial search.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- MySQL 8.4 Reference Manual, Spatial Relation Functions That Use Object
  Shapes:
  <https://dev.mysql.com/doc/refman/8.4/en/spatial-relation-functions-object-shapes.html>
- MySQL 8.4.9 runtime observations recorded by
  `packages/libmylite/tests/mysql_baseline_spatial_equals_function_expectations.sh`.

This specification is independently authored from official MySQL 8.4
documentation, observed MySQL 8.4.9 behavior, public SQLite APIs, and existing
MyLite source. It does not copy MySQL, MariaDB, Percona, SQLite implementation
internals, or restrictively licensed implementation sources.

## MySQL 8.4.9 Observations

Runtime probes establish:

- `NULL` arguments return `NULL`.
- `ST_Equals(GEOMETRYCOLLECTION EMPTY, GEOMETRYCOLLECTION EMPTY)` returns `1`.
- Exactly one empty geometry collection returns `0`.
- Equal points compare equal and different points compare unequal.
- Equal LineStrings compare equal regardless of endpoint order, while strict
  sub-lines compare unequal.
- Equal Polygons compare equal regardless of ring orientation, while contained
  smaller Polygons compare unequal.
- Multi* and GeometryCollection arguments compare equal when their child order
  differs but each child shape is represented in both arguments.
- Invalid raw geometry data raises the spatial invalid-data diagnostic for the
  called function.
- Incorrect argument counts raise the native-function parameter-count
  diagnostic for the called function.

## Scope

Supported:

- `ST_Equals(geometry, geometry)` with exactly two arguments;
- `NULL` propagation;
- empty-geometry equality and inequality against non-empty geometry;
- SRID mismatch diagnostics;
- SRID-0 object-shape equality checks for MyLite's representable simple Point,
  LineString, and Polygon values;
- child-decomposed equality for Multi* and GeometryCollection values;
- scalar, table-backed, and DML value contexts supported by existing spatial
  function execution.

Deferred:

- full union topology across differently decomposed collection children;
- full DE-9IM matrix evaluation for every invalid or self-overlapping geometry;
- geographic SRS relation evaluation;
- related predicates such as `ST_Touches()`, `ST_Overlaps()`, and
  `ST_Crosses()`;
- constructive operations such as `ST_Intersection()`.

## Ownership Boundaries

- Public API: no new ABI.
- Parser: existing generic scalar function grammar admits this name.
- Runtime: MyLite evaluates equality from decoded geometry bytes.
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
    ST_EQUALS.
```

## Runtime Semantics

- `NULL` arguments return `NULL`.
- Two empty geometry collections return `1`.
- Exactly one empty geometry collection returns `0`.
- Syntactically malformed geometry input raises the same invalid-GIS-data
  diagnostic used by existing spatial functions.
- Different SRIDs raise the existing binary-geometry different-SRID diagnostic.
- Nonzero SRIDs currently raise MyLite's existing geographic-SRS not implemented
  diagnostic for this relation baseline.
- Non-empty SRID-0 equality is evaluated as mutual object-shape containment.
- Points compare equal only with equal coordinates.
- LineStrings compare equal when each shape contains the other, including
  reversed endpoint order.
- Polygons compare equal when each surface contains the other, including
  reversed ring orientation.
- Multi* and GeometryCollection equality is evaluated child-by-child through
  the same relation helpers. This covers order-insensitive child-decomposed
  equality when each child in either argument has a matching contained shape in
  the other argument.
- Result metadata follows existing spatial result typing: `ST_Equals()` is
  integer-valued.

## Tests

The test suite covers:

- MySQL expectation capture for `NULL`, empty, point, line, polygon, Multi*,
  and GeometryCollection cases;
- table-backed projection and DML scalar contexts;
- argument-count, invalid-GIS-data, SRID-mismatch, and geographic-SRS
  diagnostics.
