# Robust Spatial Topology And Metrics

## Status

Implemented; release qualification is pending.

## Summary

MyLite currently uses one absolute `1e-12` tolerance for unrelated spatial
decisions. It treats small positive distances as zero, treats points just
outside a segment or polygon as coincident with the boundary, expands
broad-phase bounds by the same constant, and uses the constant as a
collinearity test. Consequently, translating or scaling the same represented
shape can change `ST_Distance()`, `ST_Intersects()`, `ST_Disjoint()`,
containment, touches, validity, and other topology results.

This feature removes the global absolute tolerance. Topology uses exact
coordinate equality, an adaptive orientation filter with an exact binary64
fallback, and segment predicates derived from orientation signs and inclusive
coordinate bounds. Metric functions preserve every positive representable
distance rather than turning values at or below `1e-12` into zero. Topology
predicates no longer infer intersection solely from a thresholded metric
distance.

## Sources

- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing spatial validity contract:
  `docs/specs/baseline-spatial-validity-functions/specs.md`
- Existing distance contract:
  `docs/specs/baseline-spatial-measure-accessor-functions/specs.md`
- Existing object-shape relation contracts under `docs/specs/baseline-spatial-*`.
- MySQL 8.4 Reference Manual, geometry well-formedness and validity:
  <https://dev.mysql.com/doc/refman/8.4/en/geometry-well-formedness-validity.html>
- MySQL 8.4 Reference Manual, spatial function argument handling:
  <https://dev.mysql.com/doc/refman/8.4/en/spatial-function-argument-handling.html>
- MySQL 8.4 Reference Manual, object-shape relation functions:
  <https://dev.mysql.com/doc/refman/8.4/en/spatial-relation-functions-object-shapes.html>
- MySQL 8.4 Reference Manual, spatial convenience functions:
  <https://dev.mysql.com/doc/refman/8.4/en/spatial-convenience-functions.html>
- MySQL 8.4.9 runtime observations recorded by
  `packages/libmylite/tests/mysql_robust_spatial_topology_metrics_expectations.sh`.
- IEEE 754 binary64 representation and ordinary fixed-width integer
  arithmetic are used to define the exact fallback. No third-party topology
  implementation source is used.

This specification is independently authored from official documentation,
observed MySQL 8.4.9 behavior, the public binary64 representation, and existing
MyLite source. It does not copy MySQL, MariaDB, Percona, Boost.Geometry, GEOS,
SQLite, or other implementation sources.

## MySQL 8.4.9 Observations

The pinned runtime establishes the following for SRID-0 Cartesian geometry:

- Point-to-line and parallel-line separations of `5e-13`, `1e-12`, and
  `2e-12` remain positive `ST_Distance()` results. The inputs are disjoint and
  do not intersect at all three values.
- A point `5e-13` outside a polygon boundary has distance `5e-13`, is
  disjoint, is not contained, and does not touch. A point exactly on the
  boundary has zero distance, intersects and touches, and is not contained.
- A vertical segment beginning `5e-13` beyond another segment's endpoint is
  disjoint. Its reported distance is the positive represented coordinate
  difference.
- Lines crossing with endpoints only `5e-13` to either side of the other line
  still cross and intersect.
- A two-point line whose length is `5e-13` is valid and simple and retains its
  positive length.
- Scaling the order-one point/line, line/line, polygon-boundary, and
  near-endpoint cases by `0.5` and `2` preserves their topology and scales
  their distances.
- Translating the same cases by `1000` preserves topology. The metric result is
  the nearest represented binary64 difference
  (`4.547473508864641e-13` for the probed decimal inputs), not the original
  decimal text.
- At a translation of `1e9`, adding `5e-13` is not representable in binary64;
  MySQL stores the same coordinate, returns zero distance, and reports
  intersection. MyLite must operate on represented coordinates rather than
  unrecoverable source decimal text.
- MySQL 8.4.9 itself has a narrower topology precision envelope at extreme
  scale: a point `5e-19` above a line of length `1e-6` has positive metric
  distance but `ST_Intersects()` reports `1`, and a square with side `5e-13`
  has positive area but `ST_IsValid()` reports `0`.

The last behavior demonstrates that MySQL's metric and topology engines can
disagree at extreme ratios. MyLite deliberately guarantees the remediation
matrix around coordinate spans `0.5` through `2`, its translation by `1000`,
and all cases where represented coordinates are equal or distinct. MyLite's
exact fallback follows represented binary64 geometry outside that matrix,
which can be more scale-stable than MySQL 8.4.9's observed topology artifact.
That narrow runtime divergence remains explicit under the yellow overall
spatial-family row.

## Scope

This feature covers:

- exact equality of represented coordinates;
- orientation, collinearity, point-on-segment, segment-intersection, and
  collinear-overlap predicates;
- polygon boundary classification and ray-crossing containment;
- polygon ring simplicity, validity, and multipolygon boundary checks;
- `ST_Distance()` for Point, LineString, Polygon, Multi*, and collection
  combinations;
- `ST_Intersects()`, `ST_Disjoint()`, `ST_Contains()`, `ST_Within()`,
  `ST_Equals()`, `ST_Touches()`, `ST_Crosses()`, and `ST_Overlaps()` paths
  that share these predicates;
- topology-sensitive `ST_IsSimple()`, `ST_IsValid()`, `ST_Validate()`, and
  `ST_ConvexHull()` decisions;
- removal or replacement of every use of the global
  `spatial_distance_epsilon`;
- below/at/above-`1e-12`, translation, and scaling differential tests.

This feature does not add:

- geographic SRS topology or distance;
- complete DE-9IM behavior beyond existing function-specific baselines;
- arbitrary-precision metric result types;
- recovery of decimal distinctions already lost when input is converted to
  binary64;
- compatibility with MySQL's observed extreme-ratio topology artifact;
- a third-party geometry dependency or SQLite fork patch.

## Represented Coordinate Contract

Geometry coordinates are finite IEEE 754 binary64 values after WKT, WKB,
GeoJSON, or constructor conversion. All topology decisions operate on those
values.

- Coordinates compare equal only when the two binary64 numeric values compare
  equal. Positive and negative zero compare equal.
- No decimal-source precision or textual spelling is retained.
- Nonfinite coordinates continue through the existing invalid-geometry
  diagnostics and never enter the exact orientation fallback as valid
  topology input.
- Translation or scaling can legitimately merge coordinates when binary64
  rounding produces equal represented values. That is not tolerance-based
  topology drift.

## Adaptive Orientation

The orientation predicate returns negative, zero, or positive for:

```text
(b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x)
```

The fast path evaluates the determinant in binary64 and accepts its sign only
when a conservative error bound proves the rounded sign cannot be zero or
reversed. Nonfinite products, underflow-sensitive products, and determinants
inside the error bound use the exact fallback.

The fallback interprets each finite binary64 coordinate exactly as a signed
integer multiplied by `2^-1074`:

- subnormal significands need no left shift;
- normal significands are shifted by the encoded exponent minus one;
- signed coordinate differences fit in 66 32-bit limbs;
- products and their difference fit in 132 32-bit limbs.

Signed-magnitude addition, subtraction, comparison, and grade-school
multiplication determine only the determinant sign. The common power-of-two
factor is irrelevant. The fallback:

- allocates no heap memory;
- does not depend on host `long double` width;
- handles the full finite binary64 exponent range;
- is invoked only for numerically uncertain orientation cases.

This is an independently authored exact-sign implementation over the public
binary64 format, not an adaptation of an external robust-predicate library.

## Segment And Boundary Predicates

A point is on a closed segment exactly when:

1. the adaptive orientation is zero; and
2. both coordinates fall within the segment's inclusive min/max bounds.

Two closed segments intersect when their orientation signs straddle in both
directions, or when a zero-orientation endpoint lies within the other
segment's bounds. Collinear overlap has positive length only when the selected
axis intervals overlap with `overlap_max > overlap_min`; endpoint equality is
a touch, not positive-length overlap.

The SEC02 sweep remains a broad phase, but its AABB tests no longer expand
bounds by `1e-12`. Exact coordinate overlap is sufficient because the
narrow-phase predicate no longer treats separated geometry as coincident.

Ray crossing for point-in-ring classification uses exact boundary detection
first. Crossing parity continues to use binary64 division only after
orientation has established that the point is not on the edge. Translation
and scaling tests protect the resulting boundary/interior/exterior split.

## Metric Semantics

Metric computations are independent of topology tolerance:

- `distance_point_to_point()` uses `hypot()` so large or subnormal deltas do
  not overflow or underflow merely because they are squared first.
- A segment is degenerate only when both endpoint coordinates compare equal,
  not when squared length is at most `1e-12`.
- Point-to-segment projection uses a scaled direction vector so its denominator
  remains in a safe range.
- Distance aggregation records zero only when a candidate is numerically zero.
  Every positive finite candidate remains positive.
- Segment and polygon distance returns zero when the robust topology predicate
  establishes intersection or containment. It does not turn a positive
  computed separation into zero.
- `ST_Intersects()`, `ST_Disjoint()`, and `ST_Touches()` use topology helpers
  directly. They do not classify a thresholded `ST_Distance()` result.

The result remains binary64 and therefore follows ordinary representational
rounding. This contract prevents policy-driven clamping; it does not promise
exact real-number distances.

## Remaining Epsilon Audit

The global `spatial_distance_epsilon` is removed. Existing uses are resolved
as follows:

- coordinate equality remains exact;
- orientation, collinearity, boundary, segment, validity, simplicity, convex
  hull, and relation decisions use the adaptive predicate;
- distance, length, area-degeneracy, buffer-zero, and segment-degeneracy
  branches use numeric zero rather than an absolute tolerance;
- simplification compares the computed distance only with its caller-provided
  threshold;
- LineString interpolation may retain a separately named machine-rounding
  guard for accumulated fractions, but that guard cannot affect topology,
  geometry equality, or metric zero.

Any remaining tolerance must be local, dimensionally appropriate, derived from
machine rounding, and documented at its use. There is no general spatial
epsilon after this feature.

## Errors, Metadata, And Ownership

This feature changes successful values only. Existing argument-count,
invalid-GIS-data, SRID, geographic-SRS, `NULL`, empty-geometry, warning, and
metadata contracts remain unchanged.

The exact fallback uses fixed stack storage and owns no heap allocation.
Decoded geometry ownership and SEC02 cancellation/resource behavior remain
unchanged. No public ABI, serialized format, catalog, grammar, dependency, or
SQLite fork change is required.

## Grammar

No SQL or Lemon grammar change is required. Existing generic-function grammar
already admits every affected spatial function.

## Tests

The MySQL fixture pins:

- `5e-13`, `1e-12`, and `2e-12` point/line and line/line metric and relation
  results;
- outside, exact-boundary, and inside polygon points;
- near-endpoint disjoint segments and near-axis true crossings;
- exact point equality and short nonzero LineString length/validity;
- scaling by `0.5` and `2`;
- translation by `1000` and representational collapse at `1e9`;
- MySQL's extreme-ratio metric/topology disagreement and tiny-square validity
  result as explicit observations.

Native tests must cover:

- the complete MySQL remediation matrix in scalar and row-backed contexts;
- exact positive metric output below and at `1e-12`;
- direct orientation signs for ordinary, collinear, cancellation-heavy,
  subnormal, maximum-exponent, translated, and scaled binary64 inputs;
- point-on-segment and segment-intersection permutations, including reversed
  endpoints, vertical/horizontal edges, endpoint touches, collinear gaps,
  positive collinear overlaps, and near misses on every side;
- polygon boundary/interior/exterior classification for the three threshold
  values and transformed copies;
- validity, simplicity, contains/within, equals, touches, crosses, overlaps,
  intersects, and disjoint regressions;
- SEC02 sweep correctness without epsilon-expanded bounds;
- sanitizer, allocation-failpoint, fuzz, Release, Debug, and Windows builds;
- unchanged metadata and diagnostics.

Qualification must rerun all spatial native and pinned MySQL fixtures, the
large-input geometry fuzzer, ASan/UBSan with leak detection, fault injection,
formatting, static analysis, and the SEC02 scaling benchmark.
