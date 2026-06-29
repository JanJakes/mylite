#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_spatial_crosses_function_expectations: $1" >&2
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

crosses_expected=$(cat <<EXPECTED
null_left	NULL
empty_empty	NULL
empty_point	NULL
point_point_same	NULL
point_line_mid	0
point_line_endpoint	0
point_polygon_inside	0
point_polygon_boundary	0
multipoint_line_partial	1
multipoint_line_all	0
multipoint_polygon_partial	1
line_point_mid	NULL
line_line_cross_mid	1
line_line_endpoint	0
line_t_endpoint_to_mid	0
line_line_overlap	0
line_line_contains	0
line_line_same	0
line_polygon_cross	1
line_polygon_inside	0
line_polygon_boundary	0
polygon_line_cross	NULL
polygon_polygon_overlap	NULL
collection_line_cross	1
collection_line_overlap	0
EXPECTED
)
expect_output \
    "crosses results" \
    "$crosses_expected" \
    "$(cat <<'SQL'
SELECT 'null_left', ST_Crosses(NULL, Point(1,1)) UNION ALL
SELECT 'empty_empty',
ST_Crosses(ST_GeomFromText('GEOMETRYCOLLECTION EMPTY'),
ST_GeomFromText('GEOMETRYCOLLECTION EMPTY')) UNION ALL
SELECT 'empty_point',
ST_Crosses(ST_GeomFromText('GEOMETRYCOLLECTION EMPTY'), Point(1,1)) UNION ALL
SELECT 'point_point_same', ST_Crosses(Point(1,1), Point(1,1)) UNION ALL
SELECT 'point_line_mid',
ST_Crosses(Point(1,1), ST_GeomFromText('LINESTRING(0 0,2 2)')) UNION ALL
SELECT 'point_line_endpoint',
ST_Crosses(Point(0,0), ST_GeomFromText('LINESTRING(0 0,2 2)')) UNION ALL
SELECT 'point_polygon_inside',
ST_Crosses(Point(1,1), ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))')) UNION ALL
SELECT 'point_polygon_boundary',
ST_Crosses(Point(0,0), ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))')) UNION ALL
SELECT 'multipoint_line_partial',
ST_Crosses(ST_GeomFromText('MULTIPOINT((1 1),(5 5))'),
ST_GeomFromText('LINESTRING(0 0,2 2)')) UNION ALL
SELECT 'multipoint_line_all',
ST_Crosses(ST_GeomFromText('MULTIPOINT((1 1),(2 2))'),
ST_GeomFromText('LINESTRING(0 0,3 3)')) UNION ALL
SELECT 'multipoint_polygon_partial',
ST_Crosses(ST_GeomFromText('MULTIPOINT((1 1),(5 5))'),
ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))')) UNION ALL
SELECT 'line_point_mid',
ST_Crosses(ST_GeomFromText('LINESTRING(0 0,2 2)'), Point(1,1)) UNION ALL
SELECT 'line_line_cross_mid',
ST_Crosses(ST_GeomFromText('LINESTRING(0 0,2 2)'),
ST_GeomFromText('LINESTRING(0 2,2 0)')) UNION ALL
SELECT 'line_line_endpoint',
ST_Crosses(ST_GeomFromText('LINESTRING(0 0,2 2)'),
ST_GeomFromText('LINESTRING(2 2,4 4)')) UNION ALL
SELECT 'line_t_endpoint_to_mid',
ST_Crosses(ST_GeomFromText('LINESTRING(0 0,2 0)'),
ST_GeomFromText('LINESTRING(1 0,1 1)')) UNION ALL
SELECT 'line_line_overlap',
ST_Crosses(ST_GeomFromText('LINESTRING(0 0,4 4)'),
ST_GeomFromText('LINESTRING(2 2,6 6)')) UNION ALL
SELECT 'line_line_contains',
ST_Crosses(ST_GeomFromText('LINESTRING(0 0,4 4)'),
ST_GeomFromText('LINESTRING(1 1,3 3)')) UNION ALL
SELECT 'line_line_same',
ST_Crosses(ST_GeomFromText('LINESTRING(0 0,4 4)'),
ST_GeomFromText('LINESTRING(4 4,0 0)')) UNION ALL
SELECT 'line_polygon_cross',
ST_Crosses(ST_GeomFromText('LINESTRING(-1 2,5 2)'),
ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))')) UNION ALL
SELECT 'line_polygon_inside',
ST_Crosses(ST_GeomFromText('LINESTRING(1 1,3 3)'),
ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))')) UNION ALL
SELECT 'line_polygon_boundary',
ST_Crosses(ST_GeomFromText('LINESTRING(0 0,4 0)'),
ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))')) UNION ALL
SELECT 'polygon_line_cross',
ST_Crosses(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))'),
ST_GeomFromText('LINESTRING(-1 2,5 2)')) UNION ALL
SELECT 'polygon_polygon_overlap',
ST_Crosses(ST_GeomFromText('POLYGON((0 0,3 0,3 3,0 3,0 0))'),
ST_GeomFromText('POLYGON((2 2,5 2,5 5,2 5,2 2))')) UNION ALL
SELECT 'collection_line_cross',
ST_Crosses(ST_GeomFromText('GEOMETRYCOLLECTION(LINESTRING(0 0,2 2),POINT(9 9))'),
ST_GeomFromText('LINESTRING(0 2,2 0)')) UNION ALL
SELECT 'collection_line_overlap',
ST_Crosses(ST_GeomFromText('GEOMETRYCOLLECTION(LINESTRING(0 0,4 4),POINT(9 9))'),
ST_GeomFromText('LINESTRING(2 2,6 6)'));
SQL
)"

expect_error \
    "crosses invalid data" \
    3037 \
    22023 \
    "Invalid GIS data provided to function st_crosses" \
    "SELECT ST_Crosses(X'010203', Point(1,1));"

expect_error \
    "crosses parameter count" \
    1582 \
    42000 \
    "Incorrect parameter count" \
    "SELECT ST_Crosses();"

expect_error \
    "crosses srid mismatch" \
    3033 \
    HY000 \
    "Binary geometry function st_crosses given two geometries of different srids" \
    "SELECT ST_Crosses(ST_PointFromGeoHash('mh2n0p0581',4326), Point(1,1));"

printf '%s\n' "mysql_baseline_spatial_crosses_function_expectations: ok"
