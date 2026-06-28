# Baseline Spatial Geohash Functions

## Scope

This slice implements MySQL-compatible geohash import/export helpers for the
current MyLite spatial baseline:

- `ST_GeoHash(longitude, latitude, max_length)`
- `ST_GeoHash(point, max_length)`
- `ST_LatFromGeoHash(geohash_str)`
- `ST_LongFromGeoHash(geohash_str)`
- `ST_PointFromGeoHash(geohash_str, srid)`

The implementation is intentionally limited to coordinate encoding and decoding.
It does not add a general SRS catalog, geographic computations outside the
geohash functions, or spatial search indexes.

## Source Evidence

Behavior is specified from the MySQL 8.4 Reference Manual spatial geohash
section:

- `https://dev.mysql.com/doc/refman/8.4/en/spatial-geohash-functions.html`

Expected outputs and diagnostics are verified against the local MySQL 8.4.9
runtime container `mylite-mysql-849`.

## Semantics

Geohash strings use the alphabet
`0123456789bcdefghjkmnpqrstuvwxyz`. Decoding ignores characters after the
433rd character, matching MySQL's documented internal precision limit.

Common behavior:

- SQL `NULL` arguments return SQL `NULL`.
- Invalid geometry arguments return MySQL error `3037` / SQLSTATE `22023`.
- Invalid geohash text returns MySQL error `1411` / SQLSTATE `HY000`.
- Longitude and latitude range failures and geohash length or SRID range
  failures return MySQL error `1690` / SQLSTATE `22003` with the matching
  MySQL diagnostic subject.

`ST_GeoHash(longitude, latitude, max_length)`:

- Encodes numeric longitude in `[-180, 180]` and latitude in `[-90, 90]`.
- `max_length` must be an integer from 1 through 100.
- The returned string has at most `max_length` characters. MyLite emits
  MySQL-compatible fixed-length output for the verified finite-coordinate
  subset.

`ST_GeoHash(point, max_length)`:

- Requires an internal Point geometry.
- Point SRID `0` uses stored X as longitude and stored Y as latitude.
- Point SRID `4326` uses stored X as latitude and stored Y as longitude, so
  values produced by MySQL's geographic-axis display round-trip through
  `ST_PointFromGeoHash(..., 4326)`.
- Other point SRIDs return MySQL error `3548` / SQLSTATE `SR001`.

`ST_LatFromGeoHash()` and `ST_LongFromGeoHash()`:

- Decode the interval midpoint from a valid geohash string.
- Empty strings and strings with invalid characters in the first 433 bytes are
  invalid geohash values.

`ST_PointFromGeoHash(geohash_str, srid)`:

- Decodes the geohash midpoint and returns a Point.
- SRID `0` stores X as longitude and Y as latitude.
- SRID `4326` stores X as latitude and Y as longitude, matching MySQL's
  displayed axis order for EPSG 4326.
- Other nonnegative SRIDs return MySQL error `3548` / SQLSTATE `SR001`;
  negative or non-uint32 SRID values return `1690` / `22003`; fractional SRID
  values return `3064` / `HY000`.

## Known Gaps

This slice does not implement:

- a general SRS catalog;
- SRS-aware behavior outside the geohash functions;
- geohash-backed spatial indexes or optimizer behavior;
- SRS catalog lookups beyond accepting SRID `0` and `4326` in the geohash
  functions.

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
