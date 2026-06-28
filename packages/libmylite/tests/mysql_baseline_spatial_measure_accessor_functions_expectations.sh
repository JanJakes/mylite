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

printf '%s\n' "mysql_baseline_spatial_measure_accessor_functions_expectations: ok"
