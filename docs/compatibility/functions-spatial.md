# Spatial functions

Spatial constructors, predicates, measurements, and conversion functions.

MyLite supports a basic SRID-0 geometry surface: WKT/WKB constructors,
MySQL-specific geometry constructors, WKT/WKB conversion, geometry type/SRID
readback, point coordinate access, core property/accessor functions, Cartesian
length/area/envelope helpers, coordinate swapping, envelope construction, and
MBR predicates in scalar, row-backed, and descriptor DML value contexts. It does
not yet implement nonzero SRS behavior, topology predicates, constructive
geometry operations, or physical spatial search.

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
| `ST_AsGeoJSON()` | ❌ | Generate GeoJSON object from geometry |
| `ST_AsText(), ST_AsWKT()` | ✅ | Convert internal geometry bytes to WKT |
| `ST_Buffer()` | ❌ | Geometry buffer result |
| `ST_Buffer_Strategy()` | ❌ | Produce strategy option for ST_Buffer() |
| `ST_Centroid()` | ❌ | Return centroid as a point |
| `ST_Collect()` | ❌ | Aggregate spatial values into collection |
| `ST_Contains()` | ❌ | Whether one geometry contains another |
| `ST_ConvexHull()` | ❌ | Return convex hull of geometry |
| `ST_Crosses()` | ❌ | Whether one geometry crosses another |
| `ST_Difference()` | ❌ | Return point set difference of two geometries |
| `ST_Dimension()` | ✅ | Return SRID-0 geometry dimension |
| `ST_Disjoint()` | ❌ | Whether one geometry is disjoint from another |
| `ST_Distance()` | ❌ | The distance of one geometry from another |
| `ST_Distance_Sphere()` | ❌ | Minimum distance on earth between two geometries |
| `ST_EndPoint()` | ✅ | Return LineString end point |
| `ST_Envelope()` | ✅ | Return SRID-0 geometry envelope |
| `ST_Equals()` | ❌ | Whether one geometry is equal to another |
| `ST_ExteriorRing()` | ✅ | Return Polygon exterior ring |
| `ST_FrechetDistance()` | ❌ | The discrete Fréchet distance of one geometry from another |
| `ST_GeoHash()` | ❌ | Produce a geohash value |
| `ST_GeomCollFromText(), ST_GeometryCollectionFromText(), ST_GeomCollFromTxt()` | ✅ | Construct SRID-0 geometry collection from WKT |
| `ST_GeomCollFromWKB(), ST_GeometryCollectionFromWKB()` | ✅ | Construct SRID-0 geometry collection from WKB |
| `ST_GeometryN()` | ✅ | Return one-based GeometryCollection member |
| `ST_GeometryType()` | ✅ | Return name of geometry type |
| `ST_GeomFromGeoJSON()` | ❌ | Generate geometry from GeoJSON object |
| `ST_GeomFromText(), ST_GeometryFromText()` | ✅ | Construct SRID-0 geometry from WKT |
| `ST_GeomFromWKB(), ST_GeometryFromWKB()` | ✅ | Construct SRID-0 geometry from WKB |
| `ST_HausdorffDistance()` | ❌ | The discrete Hausdorff distance of one geometry from another |
| `ST_InteriorRingN()` | ✅ | Return one-based Polygon interior ring |
| `ST_Intersection()` | ❌ | Return point set intersection of two geometries |
| `ST_Intersects()` | ❌ | Whether one geometry intersects another |
| `ST_IsClosed()` | ✅ | Return LineString/MultiLineString closed state |
| `ST_IsEmpty()` | ✅ | Return empty geometry collection state |
| `ST_IsSimple()` | ❌ | Whether a geometry is simple |
| `ST_IsValid()` | ❌ | Whether a geometry is valid |
| `ST_LatFromGeoHash()` | ❌ | Return latitude from geohash value |
| `ST_Latitude()` | ❌ | Return latitude of Point |
| `ST_Length()` | ✅ | Cartesian SRID-0 LineString/MultiLineString length |
| `ST_LineFromText(), ST_LineStringFromText()` | ✅ | Construct SRID-0 LineString from WKT |
| `ST_LineFromWKB(), ST_LineStringFromWKB()` | ✅ | Construct SRID-0 LineString from WKB |
| `ST_LineInterpolatePoint()` | ❌ | The point a given percentage along a LineString |
| `ST_LineInterpolatePoints()` | ❌ | The points a given percentage along a LineString |
| `ST_LongFromGeoHash()` | ❌ | Return longitude from geohash value |
| `ST_Longitude()` | ❌ | Return longitude of Point |
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
| `ST_Overlaps()` | ❌ | Whether one geometry overlaps another |
| `ST_PointAtDistance()` | ❌ | The point a given distance along a LineString |
| `ST_PointFromGeoHash()` | ❌ | Convert geohash value to POINT value |
| `ST_PointFromText()` | ✅ | Construct SRID-0 Point from WKT |
| `ST_PointFromWKB()` | ✅ | Construct SRID-0 Point from WKB |
| `ST_PointN()` | ✅ | Return one-based LineString point |
| `ST_PolyFromText(), ST_PolygonFromText()` | ✅ | Construct SRID-0 Polygon from WKT |
| `ST_PolyFromWKB(), ST_PolygonFromWKB()` | ✅ | Construct SRID-0 Polygon from WKB |
| `ST_Simplify()` | ❌ | Return simplified geometry |
| `ST_SRID()` | ✅ | Return spatial reference system ID for geometry |
| `ST_StartPoint()` | ✅ | Return LineString start point |
| `ST_SwapXY()` | ✅ | Return SRID-preserving X/Y-swapped geometry |
| `ST_SymDifference()` | ❌ | Return point set symmetric difference of two geometries |
| `ST_Touches()` | ❌ | Whether one geometry touches another |
| `ST_Transform()` | ❌ | Transform coordinates of geometry |
| `ST_Union()` | ❌ | Return point set union of two geometries |
| `ST_Validate()` | ❌ | Return validated geometry |
| `ST_Within()` | ❌ | Whether one geometry is within another |
| `ST_X()` | ✅ | Return X coordinate of Point |
| `ST_Y()` | ✅ | Return Y coordinate of Point |

[Back to compatibility overview](../../COMPATIBILITY.md)
