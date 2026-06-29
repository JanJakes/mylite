#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_spatial_touches_function_expectations: $1" >&2
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

touches_expected=$(cat <<EXPECTED
null_left	NULL
empty_empty	NULL
empty_point	NULL
point_same	NULL
point_far	NULL
line_point_endpoint	1
line_point_mid	0
polygon_point_boundary	1
polygon_point_inside	0
polygon_point_outside	0
polygon_hole_boundary_point	1
polygon_hole_inside_point	0
line_line_endpoint	1
line_t_endpoint_to_mid	1
line_line_cross_mid	0
line_line_overlap	0
line_line_same	0
polygon_line_boundary	1
polygon_line_boundary_to_inside	0
polygon_line_cross	0
polygon_polygon_edge	1
polygon_polygon_point	1
polygon_polygon_overlap	0
polygon_polygon_contains	0
polygon_same	0
multipoint_line_endpoint	1
multipoint_line_endpoint_mid	0
collection_touch_and_disjoint	1
collection_touch_and_inside	0
EXPECTED
)
expect_output \
    "touches results" \
    "$touches_expected" \
    "SELECT 'null_left', ST_Touches(NULL, Point(1,1)) UNION ALL "\
"SELECT 'empty_empty', ST_Touches(ST_GeomFromText('GEOMETRYCOLLECTION EMPTY'), "\
"ST_GeomFromText('GEOMETRYCOLLECTION EMPTY')) UNION ALL "\
"SELECT 'empty_point', ST_Touches(ST_GeomFromText('GEOMETRYCOLLECTION EMPTY'), Point(1,1)) "\
"UNION ALL SELECT 'point_same', ST_Touches(Point(1,1), Point(1,1)) UNION ALL "\
"SELECT 'point_far', ST_Touches(Point(1,1), Point(2,2)) UNION ALL "\
"SELECT 'line_point_endpoint', ST_Touches(ST_GeomFromText('LINESTRING(0 0,2 2)'), "\
"Point(0,0)) UNION ALL "\
"SELECT 'line_point_mid', ST_Touches(ST_GeomFromText('LINESTRING(0 0,2 2)'), Point(1,1)) "\
"UNION ALL SELECT 'polygon_point_boundary', "\
"ST_Touches(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))'), Point(0,0)) "\
"UNION ALL SELECT 'polygon_point_inside', "\
"ST_Touches(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))'), Point(1,1)) "\
"UNION ALL SELECT 'polygon_point_outside', "\
"ST_Touches(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))'), Point(5,5)) "\
"UNION ALL SELECT 'polygon_hole_boundary_point', "\
"ST_Touches(ST_GeomFromText('POLYGON((0 0,6 0,6 6,0 6,0 0), "\
"(2 2,4 2,4 4,2 4,2 2))'), Point(2,3)) UNION ALL "\
"SELECT 'polygon_hole_inside_point', "\
"ST_Touches(ST_GeomFromText('POLYGON((0 0,6 0,6 6,0 6,0 0), "\
"(2 2,4 2,4 4,2 4,2 2))'), Point(3,3)) UNION ALL "\
"SELECT 'line_line_endpoint', ST_Touches(ST_GeomFromText('LINESTRING(0 0,2 2)'), "\
"ST_GeomFromText('LINESTRING(2 2,4 4)')) UNION ALL "\
"SELECT 'line_t_endpoint_to_mid', ST_Touches(ST_GeomFromText('LINESTRING(0 0,2 0)'), "\
"ST_GeomFromText('LINESTRING(1 0,1 1)')) UNION ALL "\
"SELECT 'line_line_cross_mid', ST_Touches(ST_GeomFromText('LINESTRING(0 0,2 2)'), "\
"ST_GeomFromText('LINESTRING(0 2,2 0)')) UNION ALL "\
"SELECT 'line_line_overlap', ST_Touches(ST_GeomFromText('LINESTRING(0 0,4 4)'), "\
"ST_GeomFromText('LINESTRING(2 2,6 6)')) UNION ALL "\
"SELECT 'line_line_same', ST_Touches(ST_GeomFromText('LINESTRING(0 0,4 4)'), "\
"ST_GeomFromText('LINESTRING(0 0,4 4)')) UNION ALL "\
"SELECT 'polygon_line_boundary', "\
"ST_Touches(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))'), "\
"ST_GeomFromText('LINESTRING(0 0,4 0)')) UNION ALL "\
"SELECT 'polygon_line_boundary_to_inside', "\
"ST_Touches(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))'), "\
"ST_GeomFromText('LINESTRING(0 0,2 2)')) UNION ALL "\
"SELECT 'polygon_line_cross', "\
"ST_Touches(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))'), "\
"ST_GeomFromText('LINESTRING(-1 2,5 2)')) UNION ALL "\
"SELECT 'polygon_polygon_edge', "\
"ST_Touches(ST_GeomFromText('POLYGON((0 0,2 0,2 2,0 2,0 0))'), "\
"ST_GeomFromText('POLYGON((2 0,4 0,4 2,2 2,2 0))')) UNION ALL "\
"SELECT 'polygon_polygon_point', "\
"ST_Touches(ST_GeomFromText('POLYGON((0 0,2 0,2 2,0 2,0 0))'), "\
"ST_GeomFromText('POLYGON((2 2,4 2,4 4,2 4,2 2))')) UNION ALL "\
"SELECT 'polygon_polygon_overlap', "\
"ST_Touches(ST_GeomFromText('POLYGON((0 0,3 0,3 3,0 3,0 0))'), "\
"ST_GeomFromText('POLYGON((2 2,5 2,5 5,2 5,2 2))')) UNION ALL "\
"SELECT 'polygon_polygon_contains', "\
"ST_Touches(ST_GeomFromText('POLYGON((0 0,5 0,5 5,0 5,0 0))'), "\
"ST_GeomFromText('POLYGON((1 1,2 1,2 2,1 2,1 1))')) UNION ALL "\
"SELECT 'polygon_same', "\
"ST_Touches(ST_GeomFromText('POLYGON((0 0,5 0,5 5,0 5,0 0))'), "\
"ST_GeomFromText('POLYGON((0 0,5 0,5 5,0 5,0 0))')) UNION ALL "\
"SELECT 'multipoint_line_endpoint', "\
"ST_Touches(ST_GeomFromText('MULTIPOINT((0 0),(5 5))'), "\
"ST_GeomFromText('LINESTRING(0 0,2 2)')) UNION ALL "\
"SELECT 'multipoint_line_endpoint_mid', "\
"ST_Touches(ST_GeomFromText('MULTIPOINT((0 0),(1 1))'), "\
"ST_GeomFromText('LINESTRING(0 0,2 2)')) UNION ALL "\
"SELECT 'collection_touch_and_disjoint', "\
"ST_Touches(ST_GeomFromText('GEOMETRYCOLLECTION(POINT(0 0),POINT(9 9))'), "\
"ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))')) UNION ALL "\
"SELECT 'collection_touch_and_inside', "\
"ST_Touches(ST_GeomFromText('GEOMETRYCOLLECTION(POINT(0 0),POINT(1 1))'), "\
"ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))'));"

expect_error \
    "touches invalid data" \
    3037 \
    22023 \
    "Invalid GIS data provided to function st_touches" \
    "SELECT ST_Touches(X'010203', Point(1,1));"

expect_error \
    "touches parameter count" \
    1582 \
    42000 \
    "Incorrect parameter count" \
    "SELECT ST_Touches();"

expect_error \
    "touches srid mismatch" \
    3033 \
    HY000 \
    "Binary geometry function st_touches given two geometries of different srids" \
    "SELECT ST_Touches(ST_PointFromGeoHash('mh2n0p0581',4326), Point(1,1));"

printf '%s\n' "mysql_baseline_spatial_touches_function_expectations: ok"
