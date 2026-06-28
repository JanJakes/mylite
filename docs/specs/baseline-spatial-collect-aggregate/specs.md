# Baseline Spatial ST_Collect Aggregate

## Scope

This slice implements the MySQL 8.4.9 `ST_Collect([DISTINCT] g)` spatial
aggregate for the current MyLite aggregate envelopes. The implemented runtime
surface is:

- ungrouped `SELECT ST_Collect(geometry_column) FROM table`;
- ungrouped `SELECT ST_Collect(DISTINCT geometry_column) FROM table`;
- grouped `SELECT group_key, ST_Collect(geometry_column) FROM table GROUP BY group_key`;
- the same grouped form with `DISTINCT`;
- `ST_AsText()`, `ST_GeometryType()`, and `ST_SRID()` over the aggregate result
  through existing spatial scalar functions.

The aggregate accepts descriptor-backed spatial columns only. It is not a
window function in this slice, and it does not support arbitrary scalar
expressions, joins beyond the current aggregate planner envelope, generated
function arguments, `ORDER BY` inside the aggregate, or tableless scalar
aggregate execution.

The behavior is based on the MySQL 8.4 spatial aggregate documentation at
`https://dev.mysql.com/doc/refman/8.4/en/spatial-aggregate-functions.html` and
real MySQL 8.4.9 probes.

## Syntax

MyLite grammar snippet:

```lemon
selected_grouped_aggregate_expression ::= ST_COLLECT LPAREN aggregate_geometry_argument RPAREN.
selected_grouped_aggregate_expression ::= ST_COLLECT LPAREN DISTINCT aggregate_geometry_argument RPAREN.

aggregate_geometry_argument ::= qualified_identifier.
```

`ST_Collect()` with zero arguments or more than one argument remains a syntax
error, matching MySQL's parse-time rejection for those shapes.

MyLite's broad unsupported-statement fallback currently classifies full
malformed `SELECT ST_Collect()` and `SELECT ST_Collect(g,h)` statements as
unsupported utility placeholders after the native grammar rejects them. The
supported aggregate surface remains the single-argument grammar above; the
MySQL expectation script records the native MySQL syntax errors for malformed
arity.

## Semantics

`NULL` geometry arguments are ignored. If the aggregate has no non-`NULL`
geometry values, the result is `NULL`.

The result SRID is the common SRID of all non-`NULL` geometry values. If the
aggregate observes more than one SRID, MyLite returns MySQL error
`4034 / 22S05` with a message identifying the two SRIDs.

Result geometry type is chosen from the exact non-`NULL` input geometry types:

- all `POINT` values produce `MULTIPOINT`;
- all `LINESTRING` values produce `MULTILINESTRING`;
- all `POLYGON` values produce `MULTIPOLYGON`;
- any mixed type, multi-geometry input, or geometry collection input produces
  `GEOMETRYCOLLECTION`.

For `DISTINCT`, duplicate geometry values are removed before result assembly.
The baseline distinct comparison is bytewise over MyLite's internal geometry
representation, which matches the current binary storage model for equivalent
MyLite-produced geometry values.

Invalid geometry blobs return `3037 / 22023`. Undefined SRS handling is limited
by the current SRID catalog baseline; MyLite currently recognizes embedded SRID
numbers stored in geometry blobs but does not yet implement a full SRS catalog.

## MySQL 8.4.9 Observations

Representative probes:

```sql
SELECT ST_AsText(ST_Collect(Point(0,0))), ST_GeometryType(ST_Collect(Point(0,0)));
-- MULTIPOINT((0 0)), MULTIPOINT

SELECT ST_AsText(ST_Collect(ST_GeomFromText('LINESTRING(0 0,1 1)')));
-- MULTILINESTRING((0 0,1 1))

SELECT ST_AsText(ST_Collect(ST_GeomFromText('POLYGON((0 0,1 0,1 1,0 0))')));
-- MULTIPOLYGON(((0 0,1 0,1 1,0 0)))

SELECT ST_AsText(ST_Collect(ST_GeomFromText('MULTIPOINT((0 0),(1 1))')));
-- GEOMETRYCOLLECTION(MULTIPOINT((0 0),(1 1)))

SELECT ST_AsText(ST_Collect(geom))
FROM (SELECT ST_GeomFromText('GEOMETRYCOLLECTION EMPTY') AS geom
      UNION ALL SELECT Point(1,1)) AS ep;
-- GEOMETRYCOLLECTION(GEOMETRYCOLLECTION EMPTY,POINT(1 1))

SELECT ST_AsText(ST_Collect(DISTINCT geom))
FROM (SELECT Point(0,0) geom UNION ALL SELECT Point(0,0)
      UNION ALL SELECT Point(1,1)) AS d;
-- MULTIPOINT((0 0),(1 1))
```

Mixed SRID probe:

```sql
SELECT ST_AsText(ST_Collect(geom))
FROM (SELECT ST_SRID(Point(0,0),3857) AS geom UNION ALL SELECT Point(1,1)) AS s;
-- ERROR 4034 (22S05): Arguments to function st_collect contains geometries
-- with different SRIDs: 3857 and 0. All geometries must have the same SRID.
```

## Architecture

The implementation uses SQLite's public aggregate UDF API. No SQLite fork hook
is required. The aggregate callback validates MyLite internal geometry blobs,
tracks SRID and result type, optionally records a bytewise distinct set, and
assembles a MyLite internal geometry blob in finalization.

Parser and planner changes are explicit because MyLite does not dispatch
aggregate functions through the generic scalar-function path. `ST_Collect()`
gets a dedicated AST node and maps into existing column/grouped aggregate
planning paths, then lowers to a private SQLite aggregate function such as
`_mylite_st_collect()`.

## Known Gaps

- `ST_Collect() OVER (...)` window execution remains unsupported.
- Aggregate argument expressions beyond descriptor-backed geometry columns are
  unsupported.
- Geometry equality for `DISTINCT` is bytewise, not topology-normalized.
- Native MySQL syntax diagnostics for malformed `ST_Collect()` arity are still
  hidden by the generic unsupported-statement fallback for full statements.
- Full SRS catalog validation for undefined nonzero SRIDs remains a broader
  spatial-reference-system task.
