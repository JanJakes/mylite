# Spatial functions

Spatial constructors, predicates, measurements, and conversion functions.

MyLite supports a basic SRID-0 geometry surface: WKT/WKB constructors,
MySQL-specific geometry constructors, WKT/WKB conversion, geometry type/SRID
readback, point coordinate access, core property/accessor functions, Cartesian
length/area/distance/envelope helpers, spatial collection aggregation, SRID-0
spherical distance, centroid, convex hull, discrete trajectory distances,
LineString interpolation, geometry simplification, simplicity and validity
checks, coordinate swapping, envelope construction, and MBR predicates in
scalar, row-backed, and descriptor DML value contexts. SRID-0 `ST_Disjoint()`
and `ST_Intersects()` cover representable object-shape
disjoint/intersection checks, and SRID-0
`ST_Contains()` / `ST_Within()` cover boundary-sensitive containment for
representable Point, LineString, Polygon, and child-decomposed collection
values. SRID-0 `ST_Equals()` covers object-shape equality for the same
representable geometry surface, and SRID-0 `ST_Touches()` covers
boundary-contact checks for that surface. SRID-0 `ST_Overlaps()` covers
same-dimension partial overlap checks for that surface, and SRID-0
`ST_Crosses()` covers dimension-sensitive crossing checks. Point/MultiPoint
constructive set operators cover difference, intersection, symmetric
difference, and union for same-SRID inputs, and buffer/transform support covers
bounded identity cases. Geohash helpers support coordinate encoding/decoding
plus SRID 0 and 4326 point round-trips. GeoJSON helpers support 2D geometry,
Feature extraction, and FeatureCollection extraction for SRID 0 and 4326.
Geographic point accessors support SRID 4326 latitude/longitude getters. MyLite
enforces a deterministic maximum geometry depth of 50 across WKT, WKB,
internal geometry, GeoJSON, conversions, traversal, constructors, and
aggregation. Depth 51 is rejected before unbounded recursion or result
publication; see the
[bounded geometry nesting specification](../specs/bounded-geometry-nesting/specs.md).
Polygon, multipolygon, and collection validity checks use bounded AABB sweep
broad phases, deterministic resource ceilings, explicit
`mylite_interrupt()` cancellation, and session `max_execution_time` polling;
see the
[scalable and cancellable spatial validation specification](../specs/scalable-cancellable-spatial-validation/specs.md).
Shared SRID-0 topology decisions use exact represented-coordinate equality,
an adaptive orientation filter with a fixed-width exact binary64 fallback,
exact segment bounds, and non-expanded validation AABBs. Cartesian distance
aggregation preserves positive values below `1e-12`, and relation predicates
do not derive intersection from a metric tolerance; see the
[robust spatial topology and metrics specification](../specs/robust-spatial-topology-metrics/specs.md).
It does not yet implement a general SRS catalog, full topology predicate
coverage, general constructive geometry operations, coordinate transformation
pipelines, nonzero buffer construction, or physical spatial search.

| Function | Status | Notes |
| --- | --- | --- |
| `GeomCollection()` | ✅ | Construct SRID-0 geometry collections |
| `GeometryCollection()` | ✅ | Construct SRID-0 geometry collections |
| `LineString()` | ✅ | Construct SRID-0 LineString values from points |
| `MBRContains()` | ✅ | SRID-0 MBR predicate |
| `MBRCoveredBy()` | ✅ | SRID-0 MBR predicate |
| `MBRCovers()` | ✅ | SRID-0 MBR predicate |
| `MBRDisjoint()` | ✅ | SRID-0 MBR predicate |
| `MBREquals()` | ✅ | SRID-0 MBR predicate |
| `MBRIntersects()` | ✅ | SRID-0 MBR predicate |
| `MBROverlaps()` | ✅ | SRID-0 MBR predicate |
| `MBRTouches()` | ✅ | SRID-0 MBR predicate |
| `MBRWithin()` | ✅ | SRID-0 MBR predicate |
| `MultiLineString()` | ✅ | Construct SRID-0 MultiLineString values |
| `MultiPoint()` | ✅ | Construct SRID-0 MultiPoint values |
| `MultiPolygon()` | ✅ | Construct SRID-0 MultiPolygon values |
| `Point()` | ✅ | Construct SRID-0 Point values |
| `Polygon()` | ✅ | Construct SRID-0 Polygon values from LineString rings |
| `ST_Area()` | ✅ | Cartesian SRID-0 Polygon/MultiPolygon area |
| `ST_AsBinary(), ST_AsWKB()` | ✅ | Convert internal geometry bytes to WKB |
| `ST_AsGeoJSON()` | ✅ | Generate 2D GeoJSON for supported geometry values |
| `ST_AsText(), ST_AsWKT()` | ✅ | Convert internal geometry bytes to WKT |
| `ST_Buffer()` | 🟡 | `NULL` propagation and zero-distance identity result; nonzero buffer construction deferred |
| `ST_Buffer_Strategy()` | ✅ | Produce MySQL-compatible binary strategy options for `ST_Buffer()` |
| `ST_Centroid()` | ✅ | SRID-0 centroid for Point, LineString, Polygon, Multi*, and collections |
| `ST_Collect()` | ✅ | Descriptor-backed grouped and ungrouped geometry aggregation |
| `ST_Contains()` | 🟡 | SRID-0 object-shape containment for Point, LineString, Polygon, and child-decomposed collections; full collection-union topology and geographic SRS deferred |
| `ST_ConvexHull()` | ✅ | SRID-0 Point, LineString, Polygon, Multi*, and collection convex hull |
| `ST_Crosses()` | 🟡 | SRID-0 object-shape crosses for Point, LineString, Polygon, and child-decomposed collections; full DE-9IM coverage and geographic SRS deferred |
| `ST_Difference()` | 🟡 | Point/MultiPoint set difference with SRID preservation; general topology deferred |
| `ST_Dimension()` | ✅ | Return SRID-0 geometry dimension |
| `ST_Disjoint()` | ✅ | Exact represented-coordinate SRID-0 object-shape disjoint predicate |
| `ST_Distance()` | 🟡 | Non-clamped Cartesian SRID-0 geometry distance; geographic distance and unit conversion deferred |
| `ST_Distance_Sphere()` | ✅ | SRID-0 spherical distance for Point and MultiPoint |
| `ST_EndPoint()` | ✅ | Return LineString end point |
| `ST_Envelope()` | ✅ | Return SRID-0 geometry envelope |
| `ST_Equals()` | 🟡 | SRID-0 object-shape equality for Point, LineString, Polygon, and child-decomposed collections; full collection-union topology and geographic SRS deferred |
| `ST_ExteriorRing()` | ✅ | Return Polygon exterior ring |
| `ST_FrechetDistance()` | ✅ | SRID-0 discrete Fréchet distance for LineString pairs |
| `ST_GeoHash()` | ✅ | Encode Point or coordinate pair as a geohash |
| `ST_GeomCollFromText(), ST_GeometryCollectionFromText(), ST_GeomCollFromTxt()` | ✅ | Construct SRID-0 geometry collection from WKT |
| `ST_GeomCollFromWKB(), ST_GeometryCollectionFromWKB()` | ✅ | Construct SRID-0 geometry collection from WKB |
| `ST_GeometryN()` | ✅ | Return one-based GeometryCollection member |
| `ST_GeometryType()` | ✅ | Return name of geometry type |
| `ST_GeomFromGeoJSON()` | ✅ | Parse 2D GeoJSON geometry, Feature, and FeatureCollection values for SRID 0/4326 |
| `ST_GeomFromText(), ST_GeometryFromText()` | ✅ | Construct SRID-0 geometry from WKT |
| `ST_GeomFromWKB(), ST_GeometryFromWKB()` | ✅ | Construct SRID-0 geometry from WKB |
| `ST_HausdorffDistance()` | ✅ | Directed SRID-0 discrete Hausdorff distance for supported Point/MultiPoint and LineString/MultiLineString combinations |
| `ST_InteriorRingN()` | ✅ | Return one-based Polygon interior ring |
| `ST_Intersection()` | 🟡 | Point/MultiPoint set intersection with SRID preservation; general topology deferred |
| `ST_Intersects()` | ✅ | Exact represented-coordinate SRID-0 object-shape intersects predicate |
| `ST_IsClosed()` | ✅ | Return LineString/MultiLineString closed state |
| `ST_IsEmpty()` | ✅ | Return empty geometry collection state |
| `ST_IsSimple()` | ✅ | SRID-0 simplicity check for representable geometry values |
| `ST_IsValid()` | ✅ | Bounded and cancellable SRID-0 validity check for representable geometry values |
| `ST_LatFromGeoHash()` | ✅ | Decode latitude from geohash value |
| `ST_Latitude()` | ✅ | Return latitude of SRID 4326 Point; setter form deferred |
| `ST_Length()` | ✅ | Cartesian SRID-0 LineString/MultiLineString length |
| `ST_LineFromText(), ST_LineStringFromText()` | ✅ | Construct SRID-0 LineString from WKT |
| `ST_LineFromWKB(), ST_LineStringFromWKB()` | ✅ | Construct SRID-0 LineString from WKB |
| `ST_LineInterpolatePoint()` | ✅ | Cartesian SRID-0 point at LineString fraction |
| `ST_LineInterpolatePoints()` | ✅ | Cartesian SRID-0 MultiPoint at repeated LineString fractions |
| `ST_LongFromGeoHash()` | ✅ | Decode longitude from geohash value |
| `ST_Longitude()` | ✅ | Return longitude of SRID 4326 Point; setter form deferred |
| `ST_MakeEnvelope()` | ✅ | Construct SRID-0 Point, LineString, or Polygon envelope from two points |
| `ST_MLineFromText(), ST_MultiLineStringFromText()` | ✅ | Construct SRID-0 MultiLineString from WKT |
| `ST_MLineFromWKB(), ST_MultiLineStringFromWKB()` | ✅ | Construct SRID-0 MultiLineString from WKB |
| `ST_MPointFromText(), ST_MultiPointFromText()` | ✅ | Construct SRID-0 MultiPoint from WKT |
| `ST_MPointFromWKB(), ST_MultiPointFromWKB()` | ✅ | Construct SRID-0 MultiPoint from WKB |
| `ST_MPolyFromText(), ST_MultiPolygonFromText()` | ✅ | Construct SRID-0 MultiPolygon from WKT |
| `ST_MPolyFromWKB(), ST_MultiPolygonFromWKB()` | ✅ | Construct SRID-0 MultiPolygon from WKB |
| `ST_NumGeometries()` | ✅ | Return GeometryCollection member count |
| `ST_NumInteriorRing(), ST_NumInteriorRings()` | ✅ | Return Polygon interior ring count |
| `ST_NumPoints()` | ✅ | Return LineString point count |
| `ST_Overlaps()` | 🟡 | SRID-0 same-dimension object-shape overlaps for Point, LineString, Polygon, and child-decomposed collections; full DE-9IM coverage and geographic SRS deferred |
| `ST_PointAtDistance()` | ✅ | Cartesian SRID-0 point at LineString distance |
| `ST_PointFromGeoHash()` | ✅ | Convert geohash value to SRID 0 or 4326 Point |
| `ST_PointFromText()` | ✅ | Construct SRID-0 Point from WKT |
| `ST_PointFromWKB()` | ✅ | Construct SRID-0 Point from WKB |
| `ST_PointN()` | ✅ | Return one-based LineString point |
| `ST_PolyFromText(), ST_PolygonFromText()` | ✅ | Construct SRID-0 Polygon from WKT |
| `ST_PolyFromWKB(), ST_PolygonFromWKB()` | ✅ | Construct SRID-0 Polygon from WKB |
| `ST_Simplify()` | ✅ | SRID-0 Douglas-Peucker simplification for representable geometry values |
| `ST_SRID()` | ✅ | Return spatial reference system ID for geometry |
| `ST_StartPoint()` | ✅ | Return LineString start point |
| `ST_SwapXY()` | ✅ | Return SRID-preserving X/Y-swapped geometry |
| `ST_SymDifference()` | 🟡 | Point/MultiPoint set symmetric difference with SRID preservation; general topology deferred |
| `ST_Touches()` | 🟡 | SRID-0 object-shape touches for Point, LineString, Polygon, and child-decomposed collections; full DE-9IM coverage and geographic SRS deferred |
| `ST_Transform()` | 🟡 | Identity transforms for SRID 0 and 4326 plus MySQL diagnostics; coordinate transforms deferred |
| `ST_Union()` | 🟡 | Point/MultiPoint set union with SRID preservation; general topology deferred |
| `ST_Validate()` | ✅ | Bounded and cancellable validation; return valid representable geometry values, otherwise `NULL` |
| `ST_Within()` | 🟡 | SRID-0 inverse containment for Point, LineString, Polygon, and child-decomposed collections; full collection-union topology and geographic SRS deferred |
| `ST_X()` | ✅ | Return X coordinate of Point |
| `ST_Y()` | ✅ | Return Y coordinate of Point |

[Back to compatibility overview](../../COMPATIBILITY.md)
