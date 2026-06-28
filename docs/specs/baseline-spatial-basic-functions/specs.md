# Baseline Spatial Basic Functions

## Summary

This slice adds a bounded non-topology spatial value surface:

```sql
SELECT ST_AsText(Point(1, 2));
SELECT ST_AsText(ST_GeomFromText('LINESTRING(1 1,2 2)'));
SELECT ST_AsWKB(Point(1, 2));
SELECT ST_GeometryType(g), ST_SRID(g), ST_X(g), ST_Y(g) FROM t;
INSERT INTO t VALUES (1, Point(1, 2));
```

MyLite stores non-NULL geometry values as MySQL-compatible internal geometry
bytes: a four-byte little-endian SRID prefix followed by WKB. This keeps the
value portable in the existing SQLite `BLOB` storage path and lets conversion
functions operate without a side table or in-memory spatial catalog.

The slice does not add a topology engine, physical spatial indexes, geographic
coordinate-system behavior, loaded SRS catalog enforcement, GeoJSON, geohash,
measurement, buffering, transformation, or relationship predicates.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- MySQL 8.4 Reference Manual, creating spatial values from WKT:
  <https://dev.mysql.com/doc/refman/8.4/en/gis-wkt-functions.html>
- MySQL 8.4 Reference Manual, creating spatial values from WKB:
  <https://dev.mysql.com/doc/refman/8.4/en/gis-wkb-functions.html>
- MySQL 8.4 Reference Manual, MySQL-specific spatial functions:
  <https://dev.mysql.com/doc/refman/8.4/en/gis-mysql-specific-functions.html>
- MySQL 8.4.9 runtime observations recorded by
  `packages/libmylite/tests/mysql_baseline_spatial_basic_functions_expectations.sh`.

This specification is independently authored from official MySQL 8.4
documentation, observed MySQL 8.4.9 behavior, public SQLite APIs, and existing
MyLite source. It does not copy MySQL, MariaDB, Percona, SQLite implementation
internals, or restrictively licensed implementation sources.

## MySQL 8.4.9 Observations

Runtime probes establish:

- `Point(1,2)` returns a `GEOMETRY` value whose bytes are
  `00000000` plus little-endian WKB point bytes.
- `ST_AsWKB(Point(1,2))` returns plain WKB without the SRID prefix.
- `ST_AsText(Point(1,2))` returns `POINT(1 2)`.
- `ST_GeometryType(Point(1,2))` returns `POINT`; `ST_SRID(Point(1,2))`
  returns `0`; `ST_X(Point(1,2))` and `ST_Y(Point(1,2))` return `1` and
  `2`.
- The constructor surface `LineString(Point(...), ...)`,
  `Polygon(LineString(...))`, `MultiPoint(Point(...), ...)`, and
  `GeometryCollection(...)` returns MySQL-normalized WKT through `ST_AsText()`.
- `ST_GeomFromText('GEOMETRYCOLLECTION()')` and
  `ST_GeomFromText('GEOMETRYCOLLECTION EMPTY')` both display as
  `GEOMETRYCOLLECTION EMPTY`.
- `ST_GeomFromText(NULL)`, `ST_AsText(NULL)`, `ST_AsWKB(NULL)`,
  `ST_GeometryType(NULL)`, `ST_SRID(NULL)`, `ST_X(NULL)`, and `ST_Y(NULL)`
  return `NULL`.
- Invalid WKT or WKB returns `3037 / 22023` with an invalid-GIS-data
  diagnostic naming the function.
- A type-specific WKT constructor given the wrong geometry class returns
  `3516 / 22S01`.
- A spatial table column stores the same internal bytes returned by a geometry
  constructor.

## Scope

Supported:

- internal geometry bytes with SRID `0`;
- WKB types `POINT`, `LINESTRING`, `POLYGON`, `MULTIPOINT`,
  `MULTILINESTRING`, `MULTIPOLYGON`, and `GEOMETRYCOLLECTION`;
- WKT parsing and formatting for the same type set, including empty geometry
  collections;
- MySQL-specific constructors `Point()`, `LineString()`, `Polygon()`,
  `MultiPoint()`, `MultiLineString()`, `MultiPolygon()`,
  `GeometryCollection()`, and `GeomCollection()`;
- `ST_GeomFromText()`, `ST_GeometryFromText()`, type-specific `...FromText()`
  aliases, and the `ST_GeomCollFromTxt()` alias;
- `ST_GeomFromWKB()`, `ST_GeometryFromWKB()`, and type-specific
  `...FromWKB()` aliases;
- `ST_AsText()`, `ST_AsWKT()`, `ST_AsBinary()`, `ST_AsWKB()`,
  `ST_GeometryType()`, `ST_SRID()` getter, `ST_X()`, and `ST_Y()`;
- row-backed projection and DML constants using the supported constructors and
  conversion functions;
- MySQL-shaped result metadata for supported geometry, text, WKB, integer, and
  double spatial function outputs.

Deferred:

- SRID values other than `0`, SRS catalog lookup, geographic axis-order
  formatting, and `ST_SRID(geom, srid)` mutation;
- spatial comparisons, `ORDER BY`, grouping, indexing/search, and optimizer
  behavior over non-NULL geometry values;
- topology predicates, measurement functions, buffer/union/difference,
  validation, simplification, transformation, GeoJSON, and geohash functions;
- empty non-collection geometries and full MySQL validity rules for polygons;
- WKB variants with Z/M/collection flags, EWKB, and MySQL-specific SRS options.

## Ownership Boundaries

- Public API: no new ABI.
- Storage: spatial column values remain ordinary SQLite `BLOB` cells.
- Runtime: MyLite owns geometry parsing/formatting and SQLite scalar callback
  registration. No SQLite fork patch is needed for this slice.
- Metadata: result descriptors are assigned by MyLite planning, not inferred
  from SQLite callback output.
- Catalog: spatial column descriptors and metadata-only spatial index
  descriptors remain unchanged.

## Supported SQL Grammar

Existing generic function grammar admits the function names in this slice.
No new parser productions are required.

MyLite Lemon-syntax sketch:

```lemon
expr ::= generic_function_call.

generic_function_call ::=
    spatial_function_name LPAREN function_argument_list_opt RPAREN.

spatial_function_name ::=
    POINT | LINESTRING | POLYGON | MULTIPOINT | MULTILINESTRING
  | MULTIPOLYGON | GEOMETRYCOLLECTION | GEOMCOLLECTION
  | identifier_starting_with_ST_spatial_name.
```

## Runtime Semantics

- Constructors and converters return `NULL` if any required value argument is
  `NULL`.
- WKT and WKB constructors produce internal geometry bytes with SRID `0`.
- Type-specific WKT/WKB constructors validate the parsed geometry type.
- `ST_AsText()` formats MySQL-style uppercase WKT with compact numeric output.
- `ST_AsWKB()` returns plain WKB bytes without the SRID prefix.
- `ST_SRID()` getter reads the internal SRID prefix.
- `ST_X()` and `ST_Y()` require a `POINT` geometry.

## Tests

The test suite covers:

- MySQL expectation capture for result values, metadata, NULL propagation,
  internal bytes, table storage, and diagnostics;
- MyLite runtime scalar values and metadata;
- row-backed projection from spatial columns;
- INSERT and UPDATE of supported non-NULL geometry values;
- rejection of invalid WKT/WKB, wrong type-specific constructors, and raw
  string DML into spatial columns.
