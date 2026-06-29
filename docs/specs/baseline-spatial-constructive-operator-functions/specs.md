# Baseline Spatial Constructive Operator Functions

## Scope

This slice narrows the remaining spatial operator gap for:

- `ST_Buffer(g, d[, strategy1[, strategy2[, strategy3]]])`
- `ST_Buffer_Strategy(strategy[, points_per_circle])`
- `ST_Difference(g1, g2)`
- `ST_Intersection(g1, g2)`
- `ST_SymDifference(g1, g2)`
- `ST_Transform(g, target_srid)`
- `ST_Union(g1, g2)`

The implemented baseline is intentionally bounded:

- `ST_Buffer_Strategy()` is fully implemented for MySQL's named strategy byte
  strings.
- `ST_Buffer()` supports MySQL-compatible `NULL` propagation and
  zero-distance identity results. Nonzero buffer construction remains deferred.
- `ST_Difference()`, `ST_Intersection()`, `ST_SymDifference()`, and
  `ST_Union()` support Point and MultiPoint arguments in the same SRS.
- `ST_Transform()` supports identity transforms where source and target SRID
  are both `0` or both `4326`, plus MySQL-compatible diagnostics for unsupported
  SRID-0 to nonzero and nonzero to SRID-0 transforms.

Full constructive topology for lines, polygons, mixed collections, buffer
generation, and coordinate transformation remains outside this slice.

## Source Evidence

Behavior is specified from the MySQL 8.4 Reference Manual spatial operator
functions section:

- <https://dev.mysql.com/doc/refman/8.4/en/spatial-operator-functions.html>

Expected outputs and diagnostics are verified against the local MySQL 8.4.9
runtime container `mylite-mysql-849`.

## Runtime Observations

The MySQL 8.4.9 probes used for this slice returned:

| Expression | Result |
| --- | --- |
| `HEX(ST_Buffer_Strategy('point_square'))` | `060000000000000000000000` |
| `HEX(ST_Buffer_Strategy('end_flat'))` | `020000000000000000000000` |
| `HEX(ST_Buffer_Strategy('point_circle', 2.9))` | `050000003333333333330740` |
| `HEX(ST_Buffer_Strategy('join_round', 32))` | `030000000000000000004040` |
| `HEX(ST_Buffer_Strategy('join_miter', 32))` | `040000000000000000004040` |
| `HEX(ST_Buffer_Strategy('end_round', 32))` | `010000000000000000004040` |
| `ST_AsText(ST_Buffer(Point(0,0), 0))` | `POINT(0 0)` |
| `ST_AsText(ST_Buffer(Point(0,0), 'abc'))` | `POINT(0 0)` |
| `ST_AsText(ST_Buffer(Point(0,0), 0, NULL))` | `NULL` |
| `ST_AsText(ST_Difference(Point(1,1), Point(2,2)))` | `POINT(1 1)` |
| `ST_AsText(ST_Difference(Point(1,1), Point(1,1)))` | `GEOMETRYCOLLECTION EMPTY` |
| `ST_AsText(ST_Intersection(Point(1,1), Point(1,1)))` | `POINT(1 1)` |
| `ST_AsText(ST_Intersection(Point(1,1), Point(2,2)))` | `GEOMETRYCOLLECTION EMPTY` |
| `ST_AsText(ST_Union(Point(1,1), Point(2,2)))` | `MULTIPOINT((1 1),(2 2))` |
| `ST_AsText(ST_SymDifference(Point(1,1), Point(2,2)))` | `MULTIPOINT((1 1),(2 2))` |
| `ST_AsText(ST_SymDifference(Point(1,1), Point(1,1)))` | `GEOMETRYCOLLECTION EMPTY` |
| `ST_AsText(ST_Transform(Point(1,1), 0))` | `POINT(1 1)` |
| `ST_AsText(ST_Transform(Point(1,1), '0abc'))` | `POINT(1 1)` |

Additional diagnostics:

- `ST_Buffer_Strategy()` with zero or more than two arguments returns
  `1582 / 42000`.
- Invalid `ST_Buffer_Strategy()` option combinations return `1210 / HY000`.
- `points_per_circle` values above `@@max_points_in_geometry` return
  `3134 / HY000`.
- `ST_Transform(Point(1,1), 4326)` returns `3741 / 22S00`.
- `ST_Transform(ST_PointFromGeoHash(..., 4326), 0)` returns `3742 / 22S00`.

## Semantics

Common spatial operator behavior:

- SQL `NULL` arguments return SQL `NULL`, except
  `ST_Buffer_Strategy('invalid', NULL)` still diagnoses the invalid first
  argument before considering the nullable second argument.
- Invalid geometry bytes return MySQL error `3037 / 22023`.
- Binary geometry operators require identical SRIDs and return
  `3033 / HY000` when SRIDs differ.

`ST_Buffer_Strategy()`:

- Strategy names are case-insensitive but are not whitespace-trimmed.
- `point_square` and `end_flat` require one argument and encode strategy codes
  `6` and `2` respectively with a zero double payload.
- `point_circle`, `join_round`, `join_miter`, and `end_round` require a
  positive numeric `points_per_circle` argument no greater than
  `max_points_in_geometry` (`65536` in the target runtime) and encode strategy
  codes `5`, `3`, `4`, and `1` respectively.
- String numeric conversion is MySQL-style prefix conversion; nonnumeric text
  becomes `0` and therefore fails the positive-value check.
- The result is a binary byte string suitable for passing to `ST_Buffer()`.

`ST_Buffer()`:

- The accepted arity is two through five arguments.
- Any SQL `NULL` argument returns SQL `NULL`.
- A zero distance returns the input geometry unchanged, preserving SRID.
- Nonzero buffer construction remains a documented MyLite compatibility gap.

Point and MultiPoint constructive operators:

- Duplicate input points are collapsed in result sets.
- `ST_Difference(g1, g2)` emits distinct left-side points not present on the
  right side, preserving left-side encounter order.
- `ST_Intersection(g1, g2)` emits distinct left-side points also present on the
  right side, preserving left-side encounter order.
- `ST_Union(g1, g2)` emits distinct left-side points followed by distinct
  right-side points not already emitted.
- `ST_SymDifference(g1, g2)` emits distinct left-only points followed by
  distinct right-only points.
- Empty results are represented as `GEOMETRYCOLLECTION EMPTY`; single-point
  results are `POINT`; multi-point results are `MULTIPOINT`.

`ST_Transform()`:

- The accepted arity is two arguments.
- Target SRID parsing uses MySQL-style prefix numeric conversion.
- Source SRIDs other than `0` or `4326`, and target SRIDs other than `0` or
  `4326`, are diagnosed as missing SRS entries.
- Identity transforms for SRID `0` and SRID `4326` return the input geometry
  unchanged.
- Transforming from SRID `0` to another known SRID returns MySQL error
  `3741 / 22S00`.
- Transforming from a known nonzero SRID to SRID `0` returns MySQL error
  `3742 / 22S00`.

## Known Gaps

MyLite still does not implement general buffer generation, line or polygon
topology overlays, mixed geometry collection topology, physical spatial search,
SRS catalog transformation pipelines, EPSG coordinate math, or geography
specific buffer generation. These require either a carefully reviewed topology
engine dependency or a substantial first-party implementation and are not
introduced by this slice.

## SQLite Integration

The implementation uses SQLite's public scalar UDF API. MyLite owns geometry
argument validation, strategy byte construction, point-set evaluation, MySQL
diagnostics, and result construction. No SQLite fork hook is required.

## Grammar

No grammar change is required. These functions are admitted through the existing
generic function AST surface:

```lemon
expr(A) ::= function_name(NM) LP optional_expr_list(ARGS) RP.
```

The runtime maps admitted names to spatial function kinds and validates arity
and argument semantics in the spatial evaluator.
