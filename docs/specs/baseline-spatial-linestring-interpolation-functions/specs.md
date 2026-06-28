# Baseline Spatial LineString Interpolation Functions

## Scope

This slice extends MyLite's spatial baseline with SRID-0 LineString
interpolation operators:

- `ST_LineInterpolatePoint()`
- `ST_LineInterpolatePoints()`
- `ST_PointAtDistance()`

The functions are exposed through the existing scalar UDF bridge in scalar,
row-backed projection, and descriptor-backed DML value contexts.

## Source Evidence

Behavior is specified from the MySQL 8.4 Reference Manual spatial operator
functions section:

- `https://dev.mysql.com/doc/refman/8.4/en/spatial-operator-functions.html`

Expected outputs and diagnostics are verified against the local MySQL 8.4.9
runtime container `mylite-mysql-849`.

## Semantics

Common behavior:

- SQL `NULL` arguments return SQL `NULL`.
- Invalid geometry bytes return MySQL error `3037` / SQLSTATE `22023`.
- Non-LineString geometry inputs return MySQL error `3516` / SQLSTATE `22S01`
  with a `LINESTRING value` diagnostic.
- Distance and fraction arguments accept numeric values and numeric strings.
- Out-of-range distance and fraction values return MySQL error `1690` /
  SQLSTATE `22003`.

Function behavior:

- `ST_LineInterpolatePoint(line, fraction)` accepts fractions from `0` through
  `1` and returns the point at `fraction * ST_Length(line)` along the LineString.
- `ST_LineInterpolatePoints(line, fraction)` accepts fractions from `0` through
  `1` and returns a `MULTIPOINT` containing points at each positive multiple of
  `fraction` that does not exceed `1`. A zero fraction returns a one-point
  `MULTIPOINT` at the start of the line. A zero-length LineString also returns
  a one-point `MULTIPOINT` at the start of the line.
- `ST_PointAtDistance(line, distance)` accepts distances from `0` through the
  LineString length and returns the point at that Cartesian distance.
- Returned geometries preserve the input geometry SRID.

## Known Gaps

The executable baseline is Cartesian and verified for SRID-0 LineString values.
MyLite does not yet implement geographic interpolation, SRS-unit-aware
geodesics, nonnumeric string truncation warnings, or full SRS catalog semantics.

## SQLite Integration

The implementation uses SQLite's public scalar UDF API. MyLite owns WKB parsing,
numeric coercion, interpolation, result geometry construction, MySQL
diagnostics, and result metadata. No SQLite fork hook is required.

## Grammar

No grammar change is required. All functions are admitted through the existing
generic function AST surface:

```lemon
expr(A) ::= function_name(NM) LP optional_expr_list(ARGS) RP.
```

The runtime maps admitted function names to `enum mylite_spatial_function_kind`
values and rejects unsupported spatial names through the existing generic
function path.
