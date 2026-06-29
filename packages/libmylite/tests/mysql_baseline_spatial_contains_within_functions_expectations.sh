#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_spatial_contains_within_functions_expectations: $1" >&2
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

relation_expected=$(cat <<EXPECTED
null_left	NULL	NULL
empty_left	NULL	NULL
point_same	1	1
point_far	0	0
polygon_point_inside	1	1
polygon_point_boundary	0	0
polygon_point_outside	0	0
line_point_mid	1	1
line_point_endpoint	0	0
line_line_sub	1	1
line_line_same	1	1
polygon_line_inside	1	1
polygon_line_boundary	0	0
polygon_line_cross	0	0
polygon_line_cross_hole_off_midpoint	0	0
polygon_line_boundary_to_inside	1	1
polygon_polygon_contains	1	1
polygon_same	1	1
polygon_inside_shares_boundary	1	1
polygon_hole_point	0	0
polygon_hole_boundary_point	0	0
multipoint_contains_point	1	1
multiline_contains_point	1	1
multipolygon_contains_point	1	1
collection_contains_point	1	1
EXPECTED
)
expect_output \
    "contains/within results" \
    "$relation_expected" \
    "SELECT 'null_left', ST_Contains(NULL, Point(1,1)), ST_Within(NULL, Point(1,1)) "\
"UNION ALL "\
"SELECT 'empty_left', ST_Contains(ST_GeomFromText('GEOMETRYCOLLECTION EMPTY'), Point(1,1)), "\
"ST_Within(ST_GeomFromText('GEOMETRYCOLLECTION EMPTY'), Point(1,1)) UNION ALL "\
"SELECT 'point_same', ST_Contains(Point(1,1), Point(1,1)), "\
"ST_Within(Point(1,1), Point(1,1)) UNION ALL "\
"SELECT 'point_far', ST_Contains(Point(1,1), Point(2,2)), "\
"ST_Within(Point(2,2), Point(1,1)) UNION ALL "\
"SELECT 'polygon_point_inside', ST_Contains(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))'), "\
"Point(1,1)), ST_Within(Point(1,1), ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))')) "\
"UNION ALL "\
"SELECT 'polygon_point_boundary', ST_Contains(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))'), "\
"Point(0,0)), ST_Within(Point(0,0), ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))')) "\
"UNION ALL "\
"SELECT 'polygon_point_outside', ST_Contains(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))'), "\
"Point(5,5)), ST_Within(Point(5,5), ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))')) "\
"UNION ALL "\
"SELECT 'line_point_mid', ST_Contains(ST_GeomFromText('LINESTRING(0 0,2 2)'), Point(1,1)), "\
"ST_Within(Point(1,1), ST_GeomFromText('LINESTRING(0 0,2 2)')) UNION ALL "\
"SELECT 'line_point_endpoint', ST_Contains(ST_GeomFromText('LINESTRING(0 0,2 2)'), Point(0,0)), "\
"ST_Within(Point(0,0), ST_GeomFromText('LINESTRING(0 0,2 2)')) UNION ALL "\
"SELECT 'line_line_sub', ST_Contains(ST_GeomFromText('LINESTRING(0 0,4 4)'), "\
"ST_GeomFromText('LINESTRING(1 1,3 3)')), ST_Within(ST_GeomFromText('LINESTRING(1 1,3 3)'), "\
"ST_GeomFromText('LINESTRING(0 0,4 4)')) UNION ALL "\
"SELECT 'line_line_same', ST_Contains(ST_GeomFromText('LINESTRING(0 0,4 4)'), "\
"ST_GeomFromText('LINESTRING(0 0,4 4)')), ST_Within(ST_GeomFromText('LINESTRING(0 0,4 4)'), "\
"ST_GeomFromText('LINESTRING(0 0,4 4)')) UNION ALL "\
"SELECT 'polygon_line_inside', ST_Contains(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))'), "\
"ST_GeomFromText('LINESTRING(1 1,3 3)')), ST_Within(ST_GeomFromText('LINESTRING(1 1,3 3)'), "\
"ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))')) UNION ALL "\
"SELECT 'polygon_line_boundary', ST_Contains(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))'), "\
"ST_GeomFromText('LINESTRING(0 0,4 0)')), ST_Within(ST_GeomFromText('LINESTRING(0 0,4 0)'), "\
"ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))')) UNION ALL "\
"SELECT 'polygon_line_cross', ST_Contains(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))'), "\
"ST_GeomFromText('LINESTRING(1 1,5 5)')), ST_Within(ST_GeomFromText('LINESTRING(1 1,5 5)'), "\
"ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))')) UNION ALL "\
"SELECT 'polygon_line_cross_hole_off_midpoint', "\
"ST_Contains(ST_GeomFromText('POLYGON((0 0,6 0,6 6,0 6,0 0),(1.5 1.5,2.5 1.5,2.5 2.5,1.5 2.5,1.5 1.5))'), "\
"ST_GeomFromText('LINESTRING(1 1,5 5)')), ST_Within(ST_GeomFromText('LINESTRING(1 1,5 5)'), "\
"ST_GeomFromText('POLYGON((0 0,6 0,6 6,0 6,0 0),(1.5 1.5,2.5 1.5,2.5 2.5,1.5 2.5,1.5 1.5))')) "\
"UNION ALL "\
"SELECT 'polygon_line_boundary_to_inside', "\
"ST_Contains(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))'), "\
"ST_GeomFromText('LINESTRING(0 0,2 2)')), ST_Within(ST_GeomFromText('LINESTRING(0 0,2 2)'), "\
"ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))')) UNION ALL "\
"SELECT 'polygon_polygon_contains', ST_Contains(ST_GeomFromText('POLYGON((0 0,5 0,5 5,0 5,0 0))'), "\
"ST_GeomFromText('POLYGON((1 1,2 1,2 2,1 2,1 1))')), "\
"ST_Within(ST_GeomFromText('POLYGON((1 1,2 1,2 2,1 2,1 1))'), "\
"ST_GeomFromText('POLYGON((0 0,5 0,5 5,0 5,0 0))')) UNION ALL "\
"SELECT 'polygon_same', ST_Contains(ST_GeomFromText('POLYGON((0 0,5 0,5 5,0 5,0 0))'), "\
"ST_GeomFromText('POLYGON((0 0,5 0,5 5,0 5,0 0))')), "\
"ST_Within(ST_GeomFromText('POLYGON((0 0,5 0,5 5,0 5,0 0))'), "\
"ST_GeomFromText('POLYGON((0 0,5 0,5 5,0 5,0 0))')) UNION ALL "\
"SELECT 'polygon_inside_shares_boundary', "\
"ST_Contains(ST_GeomFromText('POLYGON((0 0,5 0,5 5,0 5,0 0))'), "\
"ST_GeomFromText('POLYGON((0 0,3 0,3 3,0 3,0 0))')), "\
"ST_Within(ST_GeomFromText('POLYGON((0 0,3 0,3 3,0 3,0 0))'), "\
"ST_GeomFromText('POLYGON((0 0,5 0,5 5,0 5,0 0))')) UNION ALL "\
"SELECT 'polygon_hole_point', "\
"ST_Contains(ST_GeomFromText('POLYGON((0 0,6 0,6 6,0 6,0 0),(2 2,4 2,4 4,2 4,2 2))'), "\
"Point(3,3)), ST_Within(Point(3,3), "\
"ST_GeomFromText('POLYGON((0 0,6 0,6 6,0 6,0 0),(2 2,4 2,4 4,2 4,2 2))')) "\
"UNION ALL "\
"SELECT 'polygon_hole_boundary_point', "\
"ST_Contains(ST_GeomFromText('POLYGON((0 0,6 0,6 6,0 6,0 0),(2 2,4 2,4 4,2 4,2 2))'), "\
"Point(2,3)), ST_Within(Point(2,3), "\
"ST_GeomFromText('POLYGON((0 0,6 0,6 6,0 6,0 0),(2 2,4 2,4 4,2 4,2 2))')) "\
"UNION ALL "\
"SELECT 'multipoint_contains_point', ST_Contains(ST_GeomFromText('MULTIPOINT((1 1),(2 2))'), "\
"Point(1,1)), ST_Within(Point(1,1), ST_GeomFromText('MULTIPOINT((1 1),(2 2))')) UNION ALL "\
"SELECT 'multiline_contains_point', "\
"ST_Contains(ST_GeomFromText('MULTILINESTRING((0 0,2 2),(5 5,6 6))'), Point(1,1)), "\
"ST_Within(Point(1,1), ST_GeomFromText('MULTILINESTRING((0 0,2 2),(5 5,6 6))')) UNION ALL "\
"SELECT 'multipolygon_contains_point', "\
"ST_Contains(ST_GeomFromText('MULTIPOLYGON(((0 0,4 0,4 4,0 4,0 0)))'), Point(1,1)), "\
"ST_Within(Point(1,1), ST_GeomFromText('MULTIPOLYGON(((0 0,4 0,4 4,0 4,0 0)))')) UNION ALL "\
"SELECT 'collection_contains_point', "\
"ST_Contains(ST_GeomFromText('GEOMETRYCOLLECTION(POINT(1 1),LINESTRING(5 5,6 6))'), "\
"Point(1,1)), ST_Within(Point(1,1), "\
"ST_GeomFromText('GEOMETRYCOLLECTION(POINT(1 1),LINESTRING(5 5,6 6))'));"

expect_error \
    "contains invalid data" \
    3037 \
    22023 \
    "Invalid GIS data provided to function st_contains" \
    "SELECT ST_Contains(X'010203', Point(1,1));"

expect_error \
    "within invalid data" \
    3037 \
    22023 \
    "Invalid GIS data provided to function st_within" \
    "SELECT ST_Within(X'010203', Point(1,1));"

expect_error \
    "contains parameter count" \
    1582 \
    42000 \
    "Incorrect parameter count" \
    "SELECT ST_Contains();"

expect_error \
    "within parameter count" \
    1582 \
    42000 \
    "Incorrect parameter count" \
    "SELECT ST_Within();"

expect_error \
    "contains srid mismatch" \
    3033 \
    HY000 \
    "Binary geometry function st_contains given two geometries of different srids" \
    "SELECT ST_Contains(ST_PointFromGeoHash('mh2n0p0581',4326), Point(1,1));"

expect_error \
    "within srid mismatch" \
    3033 \
    HY000 \
    "Binary geometry function st_within given two geometries of different srids" \
    "SELECT ST_Within(ST_PointFromGeoHash('mh2n0p0581',4326), Point(1,1));"

printf '%s\n' "mysql_baseline_spatial_contains_within_functions_expectations: ok"
