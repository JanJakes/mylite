# Baseline Spatial GeoJSON Functions

## Scope

This slice implements MySQL-compatible GeoJSON conversion helpers for MyLite's
current spatial baseline:

- `ST_AsGeoJSON(geometry [, max_dec_digits [, options]])`
- `ST_GeomFromGeoJSON(str [, options [, srid]])`

The implementation covers GeoJSON geometry objects, Feature extraction, and
FeatureCollection extraction into a geometry collection. It supports MyLite's
existing two-dimensional geometry types, SRID `0`, and SRID `4326` axis/range
behavior. It does not add a general SRS catalog, topology, transformations,
GeoJSON link CRS objects, or spatial search indexes.

## Source Evidence

Behavior is specified from the MySQL 8.4 Reference Manual spatial GeoJSON
section:

- `https://dev.mysql.com/doc/refman/8.4/en/spatial-geojson-functions.html`

Expected outputs and diagnostics are verified against the local MySQL 8.4.9
runtime container `mylite-mysql-849`.

## Semantics

Common behavior:

- SQL `NULL` arguments return SQL `NULL`.
- Invalid internal geometry arguments return MySQL error `3037` / SQLSTATE
  `22023`.
- Invalid JSON text in `ST_GeomFromGeoJSON()` returns MySQL error `3141` /
  SQLSTATE `22032`.
- Structurally invalid GeoJSON returns MySQL error `3070` or `3072` /
  SQLSTATE `HY000`, matching the observed MySQL diagnostic class for missing
  required members and invalid GeoJSON data.
- Coordinate arrays with more than two dimensions are rejected by default with
  MySQL error `3073` / SQLSTATE `HY000`; options `2`, `3`, and `4` accept the
  document and strip additional dimensions.

`ST_AsGeoJSON()`:

- Accepts one, two, or three arguments.
- Emits GeoJSON for Point, LineString, Polygon, MultiPoint, MultiLineString,
  MultiPolygon, and GeometryCollection.
- Coordinates are emitted as JSON numbers with at least one fractional digit for
  finite whole-number values, matching MySQL display such as `1.0`.
- `max_dec_digits` rounds coordinate output. It defaults to the effective full
  precision envelope used by the existing MyLite spatial formatter.
- `options` is a bitmask from `0` through `7`:
  - bit `1` emits `bbox`;
  - bit `2` emits short CRS for nonzero SRIDs;
  - bit `4` emits long CRS for nonzero SRIDs and overrides bit `2`.
- SRID `0` never emits CRS metadata.
- SRID `4326` emits coordinates in GeoJSON longitude/latitude order from
  MyLite's internal latitude/longitude storage for that SRID.

`ST_GeomFromGeoJSON()`:

- Accepts one, two, or three arguments.
- Parses GeoJSON geometry objects, Feature objects with a non-`NULL` geometry,
  and FeatureCollection objects into a GeometryCollection.
- The `type` member value is case-sensitive.
- Other GeoJSON member names are matched ASCII case-insensitively, matching
  MySQL's observed non-type parsing behavior.
- `options` defaults to `1` and must be `1`, `2`, `3`, or `4`.
- `srid` defaults to `4326`; SRID `0` and `4326` are accepted. Other nonnegative
  SRIDs return MySQL error `3548` / SQLSTATE `SR001`; negative or out-of-range
  SRIDs return MySQL error `1690` / SQLSTATE `22003`.
- SRID `0` stores GeoJSON coordinate arrays as X/Y.
- SRID `4326` interprets coordinate arrays as longitude/latitude, validates the
  geographic ranges, and stores internal coordinates in MySQL's displayed
  latitude/longitude order for EPSG 4326.

## JSON Parser Interaction

GeoJSON coordinates require decimal and exponent JSON numbers. This slice
extends MyLite's existing JSON DOM parser to preserve finite decimal/exponent
number text instead of rejecting it as unsupported. Existing integer JSON number
behavior remains unchanged; decimal and exponent numbers are typed as `DOUBLE`
for JSON introspection.

No new third-party JSON dependency is introduced.

## Known Gaps

This slice does not implement:

- GeoJSON CRS link objects;
- parsing CRS members when no explicit SRID argument is supplied, beyond the
  default SRID `4326` behavior;
- SRS catalog lookups beyond accepting SRID `0` and `4326`;
- geographic computations outside GeoJSON/geohash axis handling;
- three-dimensional or measured geometry storage;
- topology validation beyond the current baseline spatial parser checks.

## SQLite Integration

The functions use SQLite's public scalar UDF API through the existing MyLite
spatial function bridge. No SQLite fork hook is required.

## Grammar

No grammar change is required. All functions are admitted through the existing
generic function AST surface:

```lemon
expr(A) ::= function_name(NM) LP optional_expr_list(ARGS) RP.
```

The runtime maps admitted function names to `enum mylite_spatial_function_kind`
values and handles diagnostics in the spatial evaluator.
