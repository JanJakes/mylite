# Scalable And Cancellable Spatial Validation

## Status

Specified; implementation and qualification are pending.

## Summary

`ST_IsValid()` and `ST_Validate()` currently compare every non-adjacent pair
of segments in a polygon ring. Ring pairs, hole pairs, and multipolygon
members repeat the same exhaustive pattern. A valid 64K-vertex polygon
therefore performs more than two billion exact segment tests without checking
for statement interruption.

MyLite will replace exhaustive comparisons with an AABB sweep broad phase,
retain the existing exact topology predicates for candidate pairs, and bound
pathological broad-phase work. Long validation loops will poll a shared
statement work controller for explicit interruption and
`max_execution_time` expiry. These changes preserve the existing
MySQL-runtime-verified validity results while making ordinary large geometry
scale near linearly and ensuring hostile geometry cannot monopolize the host
indefinitely.

## Sources

- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing validity semantics:
  `docs/specs/baseline-spatial-validity-functions/specs.md`
- MySQL 8.4 Reference Manual, geometry well-formedness and validity:
  <https://dev.mysql.com/doc/refman/8.4/en/geometry-well-formedness-validity.html>
- MySQL 8.4 Reference Manual, spatial function argument handling:
  <https://dev.mysql.com/doc/refman/8.4/en/spatial-function-argument-handling.html>
- MySQL 8.4 Reference Manual, `max_execution_time`:
  <https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html#sysvar_max_execution_time>
- SQLite public interrupt API:
  <https://sqlite.org/c3ref/interrupt.html>
- MySQL 8.4.9 runtime observations recorded by
  `packages/libmylite/tests/mysql_scalable_cancellable_spatial_validation_expectations.sh`.

This specification is independently authored from official MySQL and SQLite
documentation, observed MySQL 8.4.9 behavior, and existing MyLite source. It
does not copy MySQL, MariaDB, Percona, SQLite, or other database implementation
sources.

## Measured Baseline

The dedicated benchmark constructs a regular SRID-0 polygon directly in
MyLite's internal geometry format. Geometry construction and SQL/WKT parsing
are outside the timed region. The Release build on 2026-07-27 produced:

| Vertices | Geometry bytes | Exact segment pairs | Validation time |
| ---: | ---: | ---: | ---: |
| 8,192 | 131,105 | 33,542,144 | 0.678 s |
| 16,384 | 262,177 | 134,193,152 | 3.070 s |
| 32,768 | 524,321 | 536,821,760 | 12.000 s |
| 65,536 | 1,048,609 | 2,147,385,344 | 48.001 s |

The exact work grows by 4.00 times for each vertex doubling. Wall-clock values
are evidence, not portable pass/fail thresholds.

## MySQL 8.4.9 Observations

Runtime probes against the pinned MySQL 8.4.9 image establish:

- polygon holes may touch the exterior at an isolated point;
- multipolygon members may touch at an isolated point;
- multipolygon members sharing an edge are invalid;
- `max_execution_time` expiry returns `3024 / HY000` with:

```text
Query execution was interrupted, maximum statement execution time exceeded
```

The existing baseline validity fixture remains authoritative for valid,
invalid, empty, collection, `NULL`, and malformed-input semantics. Algorithm
choice, index layout, and MyLite's defensive resource ceiling are
implementation policy rather than observable MySQL contracts.

## Scope

This feature covers:

- `ST_IsValid()` and `ST_Validate()` polygon, multipolygon, and collection
  validation;
- self-ring, ring-pair, hole-pair, and polygon-pair candidate discovery;
- explicit statement interruption while first-party spatial code is running;
- `max_execution_time` checks inside long validation work;
- deterministic segment, temporary-memory, and candidate-work limits;
- operation-count and scaling qualification at 8K through 64K vertices.

This feature does not change:

- the validity results specified by the baseline validity feature;
- the fixed exact-orientation tolerance, which belongs to `SEM-01`;
- WKT, WKB, internal geometry, or GeoJSON syntax;
- the depth-50 geometry nesting contract;
- relation, measurement, or constructive algorithms except where they invoke
  the shared validity precondition.

## Sweep Broad Phase

Every segment participating in a validity comparison receives:

- its original ring and segment identity;
- its comparison group;
- `min_x`, `max_x`, `min_y`, and `max_y`, expanded only by the existing
  spatial comparison tolerance.

Entries are sorted deterministically by `min_x`, then the remaining bounds,
group, and original identity. For each entry, the forward scan stops when the
next `min_x` exceeds the current `max_x`. Pairs with disjoint y intervals,
the wrong comparison groups, or adjacent positions in the same closed ring
are discarded. Only surviving candidates reach the existing exact segment
intersection predicate.

The algorithm has `O(n log n + c)` time and `O(n)` temporary memory, where
`c` is the number of broad-phase candidates examined. It is not a substitute
for exact topology: sorting and bounds may only discard pairs that cannot
intersect.

Ring and polygon bounds form a second broad phase:

- a polygon's exterior is checked against each hole;
- hole pairs are considered only when their ring bounds overlap;
- multipolygon member pairs are considered only when their polygon bounds
  overlap;
- after simple boundaries are proven not to cross invalidly, one
  representative boundary point is sufficient for each containment decision.

This avoids quadratic component enumeration for many disjoint holes or
polygons. Values whose bounds overlap pathologically remain subject to the
work ceiling below.

## Work And Memory Contract

The limits are deterministic and apply to one validity evaluation:

- at most 1,048,576 indexed segments;
- at most 64 MiB of temporary validation-index storage;
- candidate budget:
  `min(67,108,864, max(1,048,576, 256 * indexed_segments))`;
- cancellation/deadline polling before allocation, after sorting, every 4,096
  sequential build or containment operations, and every 1,024 candidate
  inspections.

All size arithmetic is checked before allocation. Allocation failure remains
`MYLITE_NOMEM / HY001`; it is never rewritten as invalid geometry or a work
limit.

Exceeding the segment, index-memory, or candidate budget returns
`1041 / HY000`:

```text
Out of resources while validating spatial geometry
```

Returning an error is compatibility-safer than reporting a geometrically
valid value as invalid. The limits admit the full 64K scaling matrix and match
the existing maximum configurable `max_points_in_geometry` magnitude without
making that partially supported variable control binary acceptance.

## Cancellation And Deadline Semantics

MyLite adds the thread-safe, additive public call:

```c
void mylite_interrupt(mylite_db *database);
```

It requests interruption through the bundled SQLite connection. The caller
must keep the database handle open until the call returns; this API does not
otherwise make concurrent use of one handle valid.

The SQLite spatial adapter passes a work controller to the core evaluator.
Direct internal callers may pass no controller and still receive the
deterministic resource limits. The controller distinguishes:

- explicit interruption: `1317 / 70100`,
  `Query execution was interrupted`;
- deadline expiry: `3024 / HY000`,
  `Query execution was interrupted, maximum statement execution time exceeded`.

A nonzero session `max_execution_time` is measured in milliseconds. The
spatial callback captures the active statement deadline and uses a monotonic
clock. A value of zero disables the deadline. Expiry or interruption releases
all temporary index and decoded-geometry ownership, publishes no scalar or
geometry result, and leaves the handle reusable after the interrupted
statement finishes.

When both conditions are visible at one poll, explicit interruption takes
precedence. A deadline or interrupt observed after validation has already
completed does not retroactively change its result.

## Public ABI And Ownership

- `mylite_interrupt()` is an additive ABI export and must be added to the
  header and symbol snapshots.
- The borrowed geometry bytes and SQLite connection outlive only the active
  callback.
- Decoded geometry and sweep entries are owned by one evaluation and released
  on success, invalidity, cancellation, deadline, work-limit, and allocation
  failure.
- No SQLite fork patch, catalog state, side table, or persistent format change
  is required.
- Benchmark statistics and injected test controllers remain internal and are
  not public ABI.

## Grammar

No SQL or Lemon grammar change is required. Existing generic function grammar
admits `ST_IsValid()` and `ST_Validate()`, and existing `SET` grammar admits
`max_execution_time`.

## Tests

Native correctness tests must cover:

- the complete existing validity matrix without result changes;
- self-crossing segments discovered early, in the middle, and at the end of
  sorted order;
- vertical, horizontal, zero-width-bound, repeated-x, collinear, endpoint
  touch, shared-edge, and near-boundary candidates;
- valid point-touching holes and multipolygons, invalid shared edges, nested
  holes, overlapping holes, and many disjoint holes/multipolygons;
- 8K, 16K, 32K, and 64K valid rings;
- exact segment, index-memory, and candidate-budget boundaries;
- allocation failure at every sweep/index allocation;
- deterministic injected interruption before build, during build, after sort,
  during candidate scanning, and during containment;
- cross-thread `mylite_interrupt()` during SQL `ST_IsValid()`;
- `max_execution_time` expiry with exact `3024 / HY000` diagnostics;
- result non-publication, cleanup, and successful handle reuse after every
  abort class.

The MySQL fixture pins the topology boundary results and deadline diagnostic.

## Performance Qualification

`mylite_spatial_validity_benchmark` remains a CSV-producing diagnostic tool.
The qualification gate uses operation counts:

- every 8K through 64K regular polygon returns valid;
- each vertex doubling may grow examined candidates by at most 2.5 times;
- the 64K run must examine fewer than 32 candidates per segment;
- no run may approach the defensive candidate budget.

Elapsed time is recorded for before/after evidence but is not a hosted-runner
gate. Final qualification includes Release, Debug, ASan/UBSan with leak
detection, deterministic allocation failpoints, the full spatial native suite,
the pinned MySQL spatial fixtures, formatting, static analysis, ABI snapshots,
and a review of cancellation lifetime safety.
