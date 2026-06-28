#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_spatial_measure_accessor_functions_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names "$@"
}

expect_output() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql "$sql" "$@")
    if [ "$output" != "$expected" ]; then
        fail "$label: expected [$expected], got [$output]"
    fi
}

expect_error() {
    label=$1
    code=$2
    state=$3
    message=$4
    sql=$5
    shift 5

    set +e
    output=$(run_mysql "$sql" "$@" 2>&1)
    status_code=$?
    set -e

    if [ "$status_code" -eq 0 ]; then
        fail "$label: expected error $code/$state, command succeeded with [$output]"
    fi
    case "$output" in
        *"ERROR $code ($state)"*"$message"*) ;;
        *) fail "$label: expected error $code/$state containing [$message], got [$output]" ;;
    esac
}

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

dimension_expected=$(cat <<EXPECTED
0	1	2	NULL	1	0	1
EXPECTED
)
expect_output \
    "dimension and state functions" \
    "$dimension_expected" \
    "SELECT ST_Dimension(Point(1,2)), "\
"ST_Dimension(ST_GeomFromText('LINESTRING(0 0,3 4)')), "\
"ST_Dimension(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))')), "\
"ST_Dimension(ST_GeomFromText('GEOMETRYCOLLECTION EMPTY')), "\
"ST_Dimension(ST_GeomFromText('GEOMETRYCOLLECTION(POINT(1 2),LINESTRING(0 0,1 1))')), "\
"ST_IsEmpty(Point(1,2)), ST_IsEmpty(ST_GeomFromText('GEOMETRYCOLLECTION EMPTY'));"

collection_expected=$(cat <<EXPECTED
NULL	0	2	LINESTRING(0 0,1 1)	NULL
EXPECTED
)
expect_output \
    "collection accessors" \
    "$collection_expected" \
    "SELECT ST_NumGeometries(Point(1,2)), "\
"ST_NumGeometries(ST_GeomFromText('GEOMETRYCOLLECTION EMPTY')), "\
"ST_NumGeometries(ST_GeomFromText('GEOMETRYCOLLECTION(POINT(1 2),LINESTRING(0 0,1 1))')), "\
"ST_AsText(ST_GeometryN(ST_GeomFromText('GEOMETRYCOLLECTION(POINT(1 2),LINESTRING(0 0,1 1))'),2)), "\
"ST_AsText(ST_GeometryN(Point(1,2),1));"

line_expected=$(cat <<EXPECTED
3	POINT(3 4)	NULL	POINT(0 0)	POINT(6 0)	0	1	NULL	10
EXPECTED
)
expect_output \
    "line accessors and length" \
    "$line_expected" \
    "SELECT ST_NumPoints(ST_GeomFromText('LINESTRING(0 0,3 4,6 0)')), "\
"ST_AsText(ST_PointN(ST_GeomFromText('LINESTRING(0 0,3 4,6 0)'),2)), "\
"ST_AsText(ST_PointN(ST_GeomFromText('LINESTRING(0 0,3 4,6 0)'),0)), "\
"ST_AsText(ST_StartPoint(ST_GeomFromText('LINESTRING(0 0,3 4,6 0)'))), "\
"ST_AsText(ST_EndPoint(ST_GeomFromText('LINESTRING(0 0,3 4,6 0)'))), "\
"ST_IsClosed(ST_GeomFromText('LINESTRING(0 0,1 1)')), "\
"ST_IsClosed(ST_GeomFromText('LINESTRING(0 0,1 1,0 0)')), "\
"ST_IsClosed(Point(1,2)), "\
"ST_Length(ST_GeomFromText('LINESTRING(0 0,3 4,6 0)'));"

polygon_expected=$(cat <<EXPECTED
1	1	LINESTRING(0 0,4 0,4 4,0 4,0 0)	LINESTRING(1 1,2 1,2 2,1 1)	NULL	6	6.5
EXPECTED
)
expect_output \
    "polygon accessors and area" \
    "$polygon_expected" \
    "SELECT ST_NumInteriorRing(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0),(1 1,2 1,2 2,1 1))')), "\
"ST_NumInteriorRings(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0),(1 1,2 1,2 2,1 1))')), "\
"ST_AsText(ST_ExteriorRing(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0),(1 1,2 1,2 2,1 1))'))), "\
"ST_AsText(ST_InteriorRingN(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0),(1 1,2 1,2 2,1 1))'),1)), "\
"ST_AsText(ST_InteriorRingN(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0),(1 1,2 1,2 2,1 1))'),2)), "\
"ST_Area(ST_GeomFromText('POLYGON((0 0,4 0,4 3,0 0))')), "\
"ST_Area(ST_GeomFromText('MULTIPOLYGON(((0 0,4 0,4 3,0 0)),((0 0,1 0,1 1,0 0)))'));"

geometry_expected=$(cat <<EXPECTED
POINT(1 2)	POLYGON((0 0,3 0,3 4,0 4,0 0))	POLYGON((0 0,4 0,4 3,0 3,0 0))	GEOMETRYCOLLECTION EMPTY	POINT(2 1)	LINESTRING(1 0,3 2)	POLYGON((0 0,0 4,3 4,0 0))	POINT(1 2)	LINESTRING(1 2,3 2)	POLYGON((1 2,3 2,3 4,1 4,1 2))
EXPECTED
)
expect_output \
    "envelope and swap geometry results" \
    "$geometry_expected" \
    "SELECT ST_AsText(ST_Envelope(Point(1,2))), "\
"ST_AsText(ST_Envelope(ST_GeomFromText('LINESTRING(0 0,3 4)'))), "\
"ST_AsText(ST_Envelope(ST_GeomFromText('POLYGON((0 0,4 0,4 3,0 0))'))), "\
"ST_AsText(ST_Envelope(ST_GeomFromText('GEOMETRYCOLLECTION EMPTY'))), "\
"ST_AsText(ST_SwapXY(Point(1,2))), "\
"ST_AsText(ST_SwapXY(ST_GeomFromText('LINESTRING(0 1,2 3)'))), "\
"ST_AsText(ST_SwapXY(ST_GeomFromText('POLYGON((0 0,4 0,4 3,0 0))'))), "\
"ST_AsText(ST_MakeEnvelope(Point(1,2), Point(1,2))), "\
"ST_AsText(ST_MakeEnvelope(Point(1,2), Point(3,2))), "\
"ST_AsText(ST_MakeEnvelope(Point(1,2), Point(3,4)));"

mbr_expected=$(cat <<EXPECTED
1	1	0	1	1	1	1	1	1	0	0	1	0	NULL
EXPECTED
)
expect_output \
    "MBR predicates" \
    "$mbr_expected" \
    "SELECT MBRContains(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))'), Point(1,1)), "\
"MBRWithin(Point(1,1), ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))')), "\
"MBRIntersects(ST_GeomFromText('LINESTRING(0 0,1 1)'), ST_GeomFromText('LINESTRING(2 2,3 3)')), "\
"MBREquals(Point(1,1), Point(1,1)), "\
"MBRDisjoint(Point(1,1), Point(2,2)), "\
"MBRTouches(ST_GeomFromText('LINESTRING(0 0,1 1)'), ST_GeomFromText('LINESTRING(1 1,2 2)')), "\
"MBROverlaps(ST_GeomFromText('POLYGON((0 0,3 0,3 3,0 3,0 0))'), ST_GeomFromText('POLYGON((2 2,5 2,5 5,2 5,2 2))')), "\
"MBRCovers(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))'), Point(0,0)), "\
"MBRCoveredBy(Point(0,0), ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))')), "\
"MBRContains(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))'), Point(0,0)), "\
"MBRWithin(Point(0,0), ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))')), "\
"MBREquals(ST_GeomFromText('GEOMETRYCOLLECTION EMPTY'), ST_GeomFromText('GEOMETRYCOLLECTION EMPTY')), "\
"MBREquals(ST_GeomFromText('GEOMETRYCOLLECTION EMPTY'), Point(1,1)), "\
"MBRIntersects(ST_GeomFromText('GEOMETRYCOLLECTION EMPTY'), Point(1,1));"

distance_expected=$(cat <<EXPECTED
5	NULL	NULL	3	0	0	1	1	1	0	1	0	3	4	NULL
EXPECTED
)
expect_output \
    "distance measurements" \
    "$distance_expected" \
    "SELECT ST_Distance(Point(0,0), Point(3,4)), "\
"ST_Distance(NULL, Point(1,1)), "\
"ST_Distance(ST_GeomFromText('GEOMETRYCOLLECTION EMPTY'), Point(1,1)), "\
"ST_Distance(Point(0,0), ST_GeomFromText('LINESTRING(3 0,3 4)')), "\
"ST_Distance(ST_GeomFromText('LINESTRING(0 0,4 4)'), ST_GeomFromText('LINESTRING(0 4,4 0)')), "\
"ST_Distance(Point(2,2), ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))')), "\
"ST_Distance(Point(5,2), ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))')), "\
"ST_Distance(Point(2,2), ST_GeomFromText('POLYGON((0 0,5 0,5 5,0 5,0 0),(1 1,4 1,4 4,1 4,1 1))')), "\
"ST_Distance(ST_GeomFromText('LINESTRING(5 0,5 4)'), ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))')), "\
"ST_Distance(ST_GeomFromText('LINESTRING(-1 2,1 2)'), ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))')), "\
"ST_Distance(ST_GeomFromText('POLYGON((0 0,2 0,2 2,0 2,0 0))'), ST_GeomFromText('POLYGON((3 0,5 0,5 2,3 2,3 0))')), "\
"ST_Distance(ST_GeomFromText('POLYGON((0 0,3 0,3 3,0 3,0 0))'), ST_GeomFromText('POLYGON((2 2,5 2,5 5,2 5,2 2))')), "\
"ST_Distance(ST_GeomFromText('MULTIPOINT(0 0,10 10)'), ST_GeomFromText('LINESTRING(3 0,3 4)')), "\
"ST_Distance(ST_GeomFromText('GEOMETRYCOLLECTION(POINT(10 10),LINESTRING(0 5,5 5))'), Point(1,1)), "\
"ST_Distance(Point(0,0), Point(1,1), NULL);"

distance_sphere_expected=$(cat <<EXPECTED
20015042.813723423	10007521.40686171	3.141592653589793	6.283185307179586	0	NULL	NULL	1111946.8229846344	1111946.8229846344	1111946.8229846344
EXPECTED
)
expect_output \
    "distance sphere measurements" \
    "$distance_sphere_expected" \
    "SELECT ST_Distance_Sphere(Point(0,0), Point(180,0)), "\
"ST_Distance_Sphere(Point(0,0), Point(0,90)), "\
"ST_Distance_Sphere(Point(0,0), Point(180,0), 1), "\
"ST_Distance_Sphere(Point(0,0), Point(180,0), '2'), "\
"ST_Distance_Sphere(Point(0,0), Point(0,0)), "\
"ST_Distance_Sphere(NULL, Point(1,1)), "\
"ST_Distance_Sphere(Point(0,0), Point(1,1), NULL), "\
"ST_Distance_Sphere(Point(0,0), ST_GeomFromText('MULTIPOINT(10 0,20 0)')), "\
"ST_Distance_Sphere(ST_GeomFromText('MULTIPOINT(10 0,20 0)'), Point(0,0)), "\
"ST_Distance_Sphere(ST_GeomFromText('MULTIPOINT(0 0,10 0)'), "\
"ST_GeomFromText('MULTIPOINT(20 0,30 0)'));"

discrete_distance_expected=$(cat <<EXPECTED
2.8284271247461903	5	1	2.8284271247461903	5	5	5	4.242640687119285	4.242640687119285	6	NULL	NULL
EXPECTED
)
expect_output \
    "discrete distance measurements" \
    "$discrete_distance_expected" \
    "SELECT ST_FrechetDistance(ST_GeomFromText('LINESTRING(0 0,0 5,5 5)'), "\
"ST_GeomFromText('LINESTRING(0 1,0 6,3 3,5 6)')), "\
"ST_FrechetDistance(ST_GeomFromText('LINESTRING(1 1,1 1)'), "\
"ST_GeomFromText('LINESTRING(4 5,4 5)')), "\
"ST_HausdorffDistance(ST_GeomFromText('LINESTRING(0 0,0 5,5 5)'), "\
"ST_GeomFromText('LINESTRING(0 1,0 6,3 3,5 6)')), "\
"ST_HausdorffDistance(ST_GeomFromText('LINESTRING(0 1,0 6,3 3,5 6)'), "\
"ST_GeomFromText('LINESTRING(0 0,0 5,5 5)')), "\
"ST_HausdorffDistance(Point(0,0), ST_GeomFromText('MULTIPOINT(3 4,10 10)')), "\
"ST_HausdorffDistance(ST_GeomFromText('MULTIPOINT(3 4,10 10)'), Point(0,0)), "\
"ST_HausdorffDistance(ST_GeomFromText('MULTIPOINT(0 0,3 4)'), "\
"ST_GeomFromText('MULTIPOINT(6 8,3 4)')), "\
"ST_HausdorffDistance(ST_GeomFromText('LINESTRING(0 0,0 5)'), "\
"ST_GeomFromText('MULTILINESTRING((0 1,0 6),(3 3,5 6))')), "\
"ST_HausdorffDistance(ST_GeomFromText('MULTILINESTRING((0 1,0 6),(3 3,5 6))'), "\
"ST_GeomFromText('LINESTRING(0 0,0 5)')), "\
"ST_HausdorffDistance(ST_GeomFromText('MULTILINESTRING((0 0,0 5),(5 5,6 6))'), "\
"ST_GeomFromText('MULTILINESTRING((0 1,0 6),(3 3,5 6))')), "\
"ST_FrechetDistance(NULL, ST_GeomFromText('LINESTRING(0 0,1 1)')), "\
"ST_HausdorffDistance(ST_GeomFromText('GEOMETRYCOLLECTION EMPTY'), Point(0,0));"

centroid_expected=$(cat <<EXPECTED
POINT(2 4)	POINT(2 2)	POINT(1 3)	POINT(1 1)	POINT(5 5)	POINT(6 1)	POINT(3 3)	POINT(1 1)	POINT(4 4)	NULL	NULL	POINT(1 1)
EXPECTED
)
expect_output \
    "centroid measurements" \
    "$centroid_expected" \
    "SELECT ST_AsText(ST_Centroid(Point(2,4))), "\
"ST_AsText(ST_Centroid(ST_GeomFromText('MULTIPOINT(0 0,2 2,4 4)'))), "\
"ST_AsText(ST_Centroid(ST_GeomFromText('LINESTRING(0 0,0 4,4 4)'))), "\
"ST_AsText(ST_Centroid(ST_GeomFromText('MULTILINESTRING((0 0,0 4),(0 0,4 0))'))), "\
"ST_AsText(ST_Centroid(ST_GeomFromText('POLYGON((0 0,10 0,10 10,0 10,0 0),(4 4,6 4,6 6,4 6,4 4))'))), "\
"ST_AsText(ST_Centroid(ST_GeomFromText('MULTIPOLYGON(((0 0,2 0,2 2,0 2,0 0)),((10 0,12 0,12 2,10 2,10 0)))'))), "\
"ST_AsText(ST_Centroid(ST_GeomFromText('GEOMETRYCOLLECTION(POINT(100 100),LINESTRING(0 0,0 4),POLYGON((0 0,6 0,6 6,0 6,0 0)))'))), "\
"ST_AsText(ST_Centroid(ST_GeomFromText('GEOMETRYCOLLECTION(POINT(100 100),LINESTRING(0 0,0 4),LINESTRING(0 0,4 0))'))), "\
"ST_AsText(ST_Centroid(ST_GeomFromText('GEOMETRYCOLLECTION(POINT(0 0),MULTIPOINT(4 4,8 8))'))), "\
"ST_AsText(ST_Centroid(ST_GeomFromText('GEOMETRYCOLLECTION EMPTY'))), "\
"ST_AsText(ST_Centroid(NULL)), "\
"ST_AsText(ST_Centroid(ST_GeomFromText('LINESTRING(1 1,1 1)')));"

convex_hull_expected=$(cat <<EXPECTED
POINT(1 2)	POINT(1 1)	LINESTRING(0 0,2 2)	POLYGON((5 0,25 0,15 25,5 0))	POLYGON((0 0,2 0,2 2,0 2,0 0))	POLYGON((0 0,2 0,3 1,1 1,0 0))	POLYGON((0 0,4 0,4 4,0 4,0 0))	POLYGON((0 0,7 0,6 3,2 2,0 0))	POLYGON((0 0,3 1,2 4,1 5,0 4,0 0))	NULL	NULL
EXPECTED
)
expect_output \
    "convex hull measurements" \
    "$convex_hull_expected" \
    "SELECT ST_AsText(ST_ConvexHull(Point(1,2))), "\
"ST_AsText(ST_ConvexHull(ST_GeomFromText('MULTIPOINT(1 1,1 1,1 1)'))), "\
"ST_AsText(ST_ConvexHull(ST_GeomFromText('MULTIPOINT(0 0,1 1,2 2,1 1)'))), "\
"ST_AsText(ST_ConvexHull(ST_GeomFromText('MULTIPOINT(5 0,25 0,15 10,15 25)'))), "\
"ST_AsText(ST_ConvexHull(ST_GeomFromText('MULTIPOINT(0 0,0 2,2 0,2 2,1 1)'))), "\
"ST_AsText(ST_ConvexHull(ST_GeomFromText('LINESTRING(0 0,1 1,2 0,3 1)'))), "\
"ST_AsText(ST_ConvexHull(ST_GeomFromText('POLYGON((0 0,4 0,4 4,2 2,0 4,0 0),(1 1,2 1,1 2,1 1))'))), "\
"ST_AsText(ST_ConvexHull(ST_GeomFromText('MULTIPOLYGON(((0 0,2 0,2 2,0 0)),((5 0,7 0,6 3,5 0)))'))), "\
"ST_AsText(ST_ConvexHull(ST_GeomFromText('GEOMETRYCOLLECTION(POINT(0 0),LINESTRING(1 2,3 1),POLYGON((0 4,2 4,1 5,0 4)))'))), "\
"ST_AsText(ST_ConvexHull(ST_GeomFromText('GEOMETRYCOLLECTION EMPTY'))), "\
"ST_AsText(ST_ConvexHull(NULL));"

line_interpolation_expected=$(cat <<EXPECTED
POINT(0 5)	POINT(2.5 5)	POINT(5 5)	POINT(0 0)	MULTIPOINT((0 2.5),(0 5),(2.5 5),(5 5))	MULTIPOINT((0 3),(1 5),(4 5))	MULTIPOINT((0 0))	MULTIPOINT((0 0))	MULTIPOINT((5 5))	POINT(0 0)	POINT(0 5)	POINT(2.5 5)	POINT(5 5)	NULL	NULL	NULL
EXPECTED
)
expect_output \
    "linestring interpolation functions" \
    "$line_interpolation_expected" \
    "SELECT ST_AsText(ST_LineInterpolatePoint(ST_GeomFromText('LINESTRING(0 0,0 5,5 5)'), '0.5')), "\
"ST_AsText(ST_LineInterpolatePoint(ST_GeomFromText('LINESTRING(0 0,0 5,5 5)'), '0.75')), "\
"ST_AsText(ST_LineInterpolatePoint(ST_GeomFromText('LINESTRING(0 0,0 5,5 5)'), 1)), "\
"ST_AsText(ST_LineInterpolatePoint(ST_GeomFromText('LINESTRING(0 0,0 5,5 5)'), 0)), "\
"ST_AsText(ST_LineInterpolatePoints(ST_GeomFromText('LINESTRING(0 0,0 5,5 5)'), '0.25')), "\
"ST_AsText(ST_LineInterpolatePoints(ST_GeomFromText('LINESTRING(0 0,0 5,5 5)'), '0.3')), "\
"ST_AsText(ST_LineInterpolatePoints(ST_GeomFromText('LINESTRING(0 0,0 0,0 0)'), '0.5')), "\
"ST_AsText(ST_LineInterpolatePoints(ST_GeomFromText('LINESTRING(0 0,0 5,5 5)'), 0)), "\
"ST_AsText(ST_LineInterpolatePoints(ST_GeomFromText('LINESTRING(0 0,0 5,5 5)'), 1)), "\
"ST_AsText(ST_PointAtDistance(ST_GeomFromText('LINESTRING(0 0,0 5,5 5)'), 0)), "\
"ST_AsText(ST_PointAtDistance(ST_GeomFromText('LINESTRING(0 0,0 5,5 5)'), 5)), "\
"ST_AsText(ST_PointAtDistance(ST_GeomFromText('LINESTRING(0 0,0 5,5 5)'), '7.5')), "\
"ST_AsText(ST_PointAtDistance(ST_GeomFromText('LINESTRING(0 0,0 5,5 5)'), 10)), "\
"ST_AsText(ST_LineInterpolatePoint(NULL, '0.5')), "\
"ST_AsText(ST_LineInterpolatePoints(ST_GeomFromText('LINESTRING(0 0,0 5,5 5)'), NULL)), "\
"ST_AsText(ST_PointAtDistance(ST_GeomFromText('LINESTRING(0 0,0 5,5 5)'), NULL));"

expect_error \
    "area rejects point" \
    3516 \
    "22S01" \
    "POLYGON/MULTIPOLYGON value is a geometry of unexpected type POINT in st_area" \
    "SELECT ST_Area(Point(1,2));"

expect_error \
    "make envelope rejects non-point" \
    1210 \
    "HY000" \
    "Incorrect arguments to st_makeenvelope" \
    "SELECT ST_AsText(ST_MakeEnvelope(Point(1,2), ST_GeomFromText('LINESTRING(0 0,1 1)')));"

expect_error \
    "distance unit rejects srid 0" \
    3882 \
    "SU001" \
    "The geometry passed to function st_distance is in SRID 0, which doesn't specify a length unit. Can't convert to 'metre'." \
    "SELECT ST_Distance(Point(0,0), Point(1,1), 'metre');"

expect_error \
    "distance rejects different srids" \
    3033 \
    "HY000" \
    "Binary geometry function st_distance given two geometries of different srids: 4326 and 0, which should have been identical." \
    "SELECT ST_Distance(ST_PointFromGeoHash('mh2n0p0581',4326), Point(1,1));"

expect_error \
    "line interpolation rejects point" \
    3516 \
    "22S01" \
    "LINESTRING value is a geometry of unexpected type POINT in st_lineinterpolatepoint." \
    "SELECT ST_AsText(ST_LineInterpolatePoint(Point(1,2), '0.5'));"

expect_error \
    "line interpolation rejects fraction over one" \
    1690 \
    "22003" \
    "Distance value is out of range in 'st_lineinterpolatepoints'" \
    "SELECT ST_AsText(ST_LineInterpolatePoints(ST_GeomFromText('LINESTRING(0 0,0 5)'), '1.1'));"

expect_error \
    "point at distance rejects distance past end" \
    1690 \
    "22003" \
    "Distance value is out of range in 'st_pointatdistance'" \
    "SELECT ST_AsText(ST_PointAtDistance(ST_GeomFromText('LINESTRING(0 0,0 5)'), 6));"

expect_error \
    "distance sphere rejects linestring" \
    3704 \
    "22S00" \
    "st_distance_sphere(POINT, LINESTRING) has not been implemented for Cartesian spatial reference systems." \
    "SELECT ST_Distance_Sphere(Point(0,0), ST_GeomFromText('LINESTRING(0 0,1 1)'));"

expect_error \
    "distance sphere rejects geometry collection" \
    3704 \
    "22S00" \
    "st_distance_sphere(POINT, GEOMCOLLECTION) has not been implemented for Cartesian spatial reference systems." \
    "SELECT ST_Distance_Sphere(Point(0,0), ST_GeomFromText('GEOMETRYCOLLECTION EMPTY'));"

expect_error \
    "distance sphere rejects longitude below range" \
    3616 \
    "22S02" \
    "Longitude -180.000000 is out of range in function st_distance_sphere. It must be within (-180.000000, 180.000000]." \
    "SELECT ST_Distance_Sphere(Point(-180,0), Point(0,0));"

expect_error \
    "distance sphere rejects latitude above range" \
    3617 \
    "22S03" \
    "Latitude 91.000000 is out of range in function st_distance_sphere. It must be within [-90.000000, 90.000000]." \
    "SELECT ST_Distance_Sphere(Point(0,91), Point(0,0));"

expect_error \
    "distance sphere rejects nonpositive radius" \
    3706 \
    "22003" \
    "Invalid radius provided to function st_distance_sphere: Radius must be greater than zero." \
    "SELECT ST_Distance_Sphere(Point(0,0), Point(1,1), 0);"

expect_error \
    "frechet rejects unsupported cartesian type" \
    3704 \
    "22S00" \
    "st_frechetdistance(POINT, LINESTRING) has not been implemented for Cartesian spatial reference systems." \
    "SELECT ST_FrechetDistance(Point(0,0), ST_GeomFromText('LINESTRING(0 0,1 1)'));"

expect_error \
    "hausdorff rejects unsupported cartesian type" \
    3704 \
    "22S00" \
    "st_hausdorffdistance(POINT, POINT) has not been implemented for Cartesian spatial reference systems." \
    "SELECT ST_HausdorffDistance(Point(0,0), Point(1,1));"

expect_error \
    "frechet unit rejects srid 0" \
    3882 \
    "SU001" \
    "The geometry passed to function st_frechetdistance is in SRID 0" \
    "SELECT ST_FrechetDistance(ST_GeomFromText('LINESTRING(0 0,1 1)'), "\
"ST_GeomFromText('LINESTRING(0 0,1 1)'), 'metre');"

expect_error \
    "hausdorff unit rejects srid 0" \
    3882 \
    "SU001" \
    "The geometry passed to function st_hausdorffdistance is in SRID 0" \
    "SELECT ST_HausdorffDistance(Point(0,0), ST_GeomFromText('MULTIPOINT(1 1)'), 'metre');"

expect_error \
    "centroid rejects geographic srs" \
    3618 \
    "22S00" \
    "st_centroid(POINT) has not been implemented for geographic spatial reference systems." \
    "SELECT ST_AsText(ST_Centroid(ST_PointFromGeoHash('mh2n0p0581',4326)));"

expect_error \
    "centroid rejects invalid polygon" \
    3037 \
    "22023" \
    "Invalid GIS data provided to function st_centroid." \
    "SELECT ST_AsText(ST_Centroid(ST_GeomFromText('POLYGON((0 0,1 1,2 2,0 0))')));"

expect_error \
    "convex hull rejects geographic srs" \
    3618 \
    "22S00" \
    "st_convexhull(POINT) has not been implemented for geographic spatial reference systems." \
    "SELECT ST_AsText(ST_ConvexHull(ST_PointFromGeoHash('mh2n0p0581',4326)));"

expect_error \
    "convex hull rejects invalid polygon" \
    3037 \
    "22023" \
    "Invalid GIS data provided to function st_convexhull." \
    "SELECT ST_AsText(ST_ConvexHull(ST_GeomFromText('POLYGON((0 0,1 1,2 2,0 0))')));"

printf '%s\n' "mysql_baseline_spatial_measure_accessor_functions_expectations: ok"
