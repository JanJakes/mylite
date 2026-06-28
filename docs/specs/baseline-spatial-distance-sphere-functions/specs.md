# Baseline Spatial Distance Sphere Function

## Scope

This slice implements the SRID-0 `ST_Distance_Sphere()` baseline for Point and
MultiPoint geometries:

- `ST_Distance_Sphere(g1, g2)`
- `ST_Distance_Sphere(g1, g2, radius)`

The function is exposed through the existing scalar UDF bridge in scalar and
row-backed projection contexts.

## Source Evidence

Behavior is specified from the MySQL 8.4 Reference Manual spatial convenience
functions section:

- `https://dev.mysql.com/doc/refman/8.4/en/spatial-convenience-functions.html`

Expected outputs and diagnostics are verified against the local MySQL 8.4.9
runtime container `mylite-mysql-849`.

## Semantics

Common behavior:

- SQL `NULL` arguments return SQL `NULL`.
- Invalid geometry bytes return MySQL error `3037` / SQLSTATE `22023`.
- Geometry arguments must have identical SRIDs.
- This baseline accepts SRID-0 `Point` and `MultiPoint` geometry arguments.
- Non-Point and non-MultiPoint SRID-0 geometry arguments return MySQL error
  `3704` / SQLSTATE `22S00`.
- Nonzero SRIDs remain outside this slice and return the existing geographic
  SRS not-implemented diagnostic.

Distance behavior:

- Point X is interpreted as longitude in degrees.
- Point Y is interpreted as latitude in degrees.
- Longitude must be in `(-180, 180]`.
- Latitude must be in `[-90, 90]`.
- With no radius argument, the radius is `6370986` meters.
- If present, `radius` must be greater than zero.
- The result is the shortest spherical distance between every point pair in the
  two argument geometries.

## Known Gaps

The executable baseline is SRID-0 only. MyLite does not yet implement
geographic SRS mean-radius behavior, projected-SRS diagnostics, overflow
diagnostic parity for extreme radius values, nonnumeric string truncation
warnings, descriptor-backed string assignment conversion parity for double
results, or full SRS catalog semantics.

## SQLite Integration

The implementation uses SQLite's public scalar UDF API. MyLite owns geometry
argument validation, point extraction, range diagnostics, haversine distance
calculation, MySQL diagnostics, and result metadata. No SQLite fork hook is
required.

## Grammar

No grammar change is required. The function is admitted through the existing
generic function AST surface:

```lemon
expr(A) ::= function_name(NM) LP optional_expr_list(ARGS) RP.
```

The runtime maps the admitted function name to
`MYLITE_SPATIAL_FUNCTION_ST_DISTANCESPHERE` and validates argument count and
argument semantics.
