# Bounded Geometry Nesting

## Status

Implemented and qualified.

## Summary

MyLite accepts geometry values from WKT, WKB, internal geometry bytes, and
GeoJSON. A `GEOMETRYCOLLECTION` can recursively contain another collection, so
every parser, decoder, formatter, topology helper, transformer, and cleanup
path must use the same deterministic nesting limit.

MyLite defines a maximum geometry depth of 50 geometry nodes. The root geometry
has depth 1. A point wrapped in 49 geometry collections therefore has depth 50
and is accepted; one wrapped in 50 collections has depth 51 and is rejected.
The limit is independent of host thread-stack size and applies identically to
WKT, WKB, internal geometry bytes, and GeoJSON.

## Sources

- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- MySQL 8.4 Reference Manual, spatial argument handling:
  <https://dev.mysql.com/doc/refman/8.4/en/spatial-function-argument-handling.html>
- MySQL 8.4 Reference Manual, supported spatial formats:
  <https://dev.mysql.com/doc/refman/8.4/en/gis-data-formats.html>
- MySQL 8.4 Reference Manual, geometry well-formedness and validity:
  <https://dev.mysql.com/doc/refman/8.4/en/geometry-well-formedness-validity.html>
- MySQL 8.4 Reference Manual, spatial GeoJSON functions:
  <https://dev.mysql.com/doc/refman/8.4/en/spatial-geojson-functions.html>
- MySQL 8.4.9 runtime observations recorded by the expectation fixture
  introduced with this feature.

This specification is independently authored from official MySQL 8.4
documentation, observed MySQL 8.4.9 behavior, and existing MyLite source. It
does not copy MySQL, MariaDB, Percona, SQLite, or other database implementation
sources.

## MySQL 8.4.9 Observations

Runtime probes against the pinned MySQL 8.4.9 image establish:

- WKT and WKB accept a point wrapped in at least 3,584 and 4,096 nested
  geometry collections, respectively, with the runtime's 1 MiB
  `thread_stack`.
- Sufficiently deep WKT and WKB fail with `1436 / HY000`, reporting a thread
  stack overrun. The observed boundary is an environment property rather than
  a stable geometry-format limit.
- GeoJSON accepts a point wrapped in 49 geometry collections. A point wrapped
  in 50 collections fails with `3157 / 22032` and
  `The JSON document exceeds the maximum depth.`
- MySQL documents that nested geometry collections are flattened for many GIS
  computations. Nesting is consequently valid input, not malformed syntax by
  itself.

MyLite intentionally replaces the environment-dependent WKT/WKB stack boundary
with the deterministic cross-format limit below. This is stricter than MySQL
for deeply nested WKT and WKB, and matches the greatest geometry depth admitted
by MySQL's GeoJSON path.

## Depth And Work Contract

- `MYLITE_SPATIAL_MAX_GEOMETRY_DEPTH` is 50.
- A root geometry consumes one depth unit.
- Each child geometry consumes one additional depth unit.
- Coordinate arrays, polygon rings, and the JSON object/array containers used
  to spell one geometry do not consume geometry-depth units.
- Sibling geometries reuse the parent depth after the preceding sibling
  completes.
- Every recursive or iterative geometry operation carries an explicit budget.
  Entering a geometry node must succeed before its header, child count,
  coordinates, or heap representation is consumed.
- A budget cannot be reset by converting between WKT, WKB, internal bytes, or
  GeoJSON. Each public operation starts one fresh budget at its untrusted input
  boundary and preserves the 50-node path invariant throughout the operation.
- Total point and allocation limits remain governed by the existing spatial
  size checks. Scalable topology work is specified separately by `SEC-02`.

The budget is an internal implementation contract and does not add public ABI.

## Input Semantics

### WKT

The WKT parser checks the budget before parsing each geometry type. Depth 50 is
accepted. At depth 51, parsing stops before descending into the child and
returns `3037 / 22023`:

```text
Invalid GIS data provided to function <function>.
```

The partial output buffer is released without recursively walking input-owned
state.

### WKB And Internal Geometry

WKB validation checks the budget before reading each geometry header. Internal
geometry validation applies the same rule after the four-byte SRID prefix.
Depth 50 is accepted. Depth 51 returns `3037 / 22023`.

Validation is mandatory before formatting, decoding to heap geometry,
traversal, copying, topology evaluation, transformation, or aggregate
retention. No downstream operation may treat an unvalidated WKB span as proof
that the nesting contract holds.

### GeoJSON

The existing allocation-bounded, iterative JSON parser remains the first
boundary. JSON documents that exceed its structural depth return MySQL's
observed `3157 / 22032` diagnostic:

```text
The JSON document exceeds the maximum depth.
```

After JSON parsing, GeoJSON geometry and Feature/FeatureCollection traversal
uses the same 50-node geometry budget. Any geometry-depth violation not already
rejected by the JSON parser returns `3072 / HY000`, the existing invalid
GeoJSON-data diagnostic for `ST_GeomFromGeoJSON()`.

## Traversal, Conversion, And Cleanup

- WKB validation, WKT parsing, WKT formatting, GeoJSON formatting, GeoJSON
  conversion, dimension, bounds, area, length, swap, decode, copy,
  simplification, centroid, convex-hull extraction, relation traversal,
  transformation, and WKB re-encoding must either:
  - use an explicit bounded frame stack; or
  - receive a budget already proven by validation and enforce the same depth
    while descending.
- Heap geometry destruction must not recurse on unbounded state. A partially
  constructed tree records only children whose entry budget succeeded, so
  cleanup depth is at most 50. Prefer an explicit cleanup stack where state can
  be built before validation.
- Rejection must not attempt to validate, format, copy, or recursively destroy
  the unentered child.
- Allocation failures retain `MYLITE_NOMEM` and must not be rewritten as a
  depth diagnostic.
- Empty geometry collections have depth 1 and consume no child depth.

## Grammar

No SQL or Lemon grammar change is required. WKT is runtime string data, WKB and
internal geometry are binary values, and GeoJSON is parsed by the runtime JSON
module.

## Ownership And Architecture

- Public ABI: unchanged.
- SQLite fork: unchanged; all checks remain in MyLite-owned runtime code.
- WKT and WKB input spans are borrowed for the duration of evaluation.
- Conversion buffers and decoded geometry trees are owned by the active
  operation and released on every failure path.
- The limit is compile-time and not session-configurable. This prevents
  different adapters or handles from accepting incompatible geometry values.

## Tests

Focused native tests must generate, rather than check in, the following values:

- WKT, little-endian WKB, big-endian WKB, internal geometry, and GeoJSON at
  depths 49, 50, and 51;
- malformed over-limit values truncated before and after the rejected child
  header;
- empty and multi-sibling collections at the limit;
- every public constructor and representative formatting, accessor,
  measurement, topology, validation, simplification, transformation, and
  aggregate path;
- allocation failure at every buffer/tree/frame allocation before, at, and
  immediately after the limit;
- inputs larger than 65,536 bytes whose nesting is below, at, and above the
  limit.

The depth-51 cases must prove deterministic diagnostics, no result publication,
no leak, and continued usability of the database handle.

The MySQL expectation fixture records the observed WKT, WKB, and GeoJSON
below/at/above behavior without claiming MySQL has a stable WKT/WKB numeric
limit.

## Fuzzing And Qualification

- Extend the geometry fuzz target to exercise the SQL spatial dispatcher plus
  direct WKT and WKB constructors.
- Raise the fuzzer input ceiling above 65,536 bytes and seed nested
  little-endian, big-endian, mixed-endian, empty, and truncated collections.
- Run focused tests under Release, Debug, ASan/UBSan, LSan, and deterministic
  allocation failpoints.
- Run the complete spatial native suite and pinned MySQL spatial expectation
  suite before marking the compatibility row implemented.

Implementation at `2bee55dd7` completed this qualification:

- all 21 spatial-related native suites passed in Release and under
  ASan/UBSan with leak detection;
- the focused boundary suite passed in Debug and under deterministic
  allocation failpoints, including exhaustive WKT, WKB, GeoJSON, and decoded
  geometry-tree allocation sweeps;
- all 20 pinned MySQL 8.4.9 spatial expectation fixtures passed;
- the seeded geometry fuzzer completed 10,000 ASan/UBSan runs with a
  262,144-byte input ceiling and direct plus SQL-dispatch WKT/WKB coverage;
- formatting and clang-tidy passed for every changed translation unit.
