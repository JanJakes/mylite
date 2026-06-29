#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_spatial_overlaps_function_expectations: $1" >&2
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

overlaps_expected=$(cat <<EXPECTED
null_left	NULL
empty_empty	NULL
empty_point	NULL
point_same	0
point_far	0
multipoint_partial	1
multipoint_same	0
multipoint_disjoint	0
point_line	NULL
point_polygon	NULL
multipoint_point	0
collection_mixed_pointset_partial	1
line_line_partial_overlap	1
line_line_endpoint	0
line_line_cross_mid	0
line_line_contains	0
line_line_same	0
line_polygon	NULL
polygon_polygon_overlap	1
polygon_polygon_edge_touch	0
polygon_polygon_contains	0
polygon_polygon_same	0
multipolygon_partial	1
collection_line_overlap	1
EXPECTED
)
expect_output \
    "overlaps results" \
    "$overlaps_expected" \
    "$(cat <<'SQL'
SELECT 'null_left', ST_Overlaps(NULL, Point(1,1)) UNION ALL
SELECT 'empty_empty',
ST_Overlaps(ST_GeomFromText('GEOMETRYCOLLECTION EMPTY'),
ST_GeomFromText('GEOMETRYCOLLECTION EMPTY')) UNION ALL
SELECT 'empty_point',
ST_Overlaps(ST_GeomFromText('GEOMETRYCOLLECTION EMPTY'), Point(1,1)) UNION ALL
SELECT 'point_same', ST_Overlaps(Point(1,1), Point(1,1)) UNION ALL
SELECT 'point_far', ST_Overlaps(Point(1,1), Point(2,2)) UNION ALL
SELECT 'multipoint_partial',
ST_Overlaps(ST_GeomFromText('MULTIPOINT((1 1),(2 2))'),
ST_GeomFromText('MULTIPOINT((2 2),(3 3))')) UNION ALL
SELECT 'multipoint_same',
ST_Overlaps(ST_GeomFromText('MULTIPOINT((1 1),(2 2))'),
ST_GeomFromText('MULTIPOINT((2 2),(1 1))')) UNION ALL
SELECT 'multipoint_disjoint',
ST_Overlaps(ST_GeomFromText('MULTIPOINT((1 1),(2 2))'),
ST_GeomFromText('MULTIPOINT((3 3),(4 4))')) UNION ALL
SELECT 'point_line', ST_Overlaps(Point(0,0), ST_GeomFromText('LINESTRING(0 0,2 2)')) UNION ALL
SELECT 'point_polygon',
ST_Overlaps(Point(0,0), ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))')) UNION ALL
SELECT 'multipoint_point',
ST_Overlaps(ST_GeomFromText('MULTIPOINT((0 0),(2 2))'), Point(0,0)) UNION ALL
SELECT 'collection_mixed_pointset_partial',
ST_Overlaps(ST_GeomFromText('GEOMETRYCOLLECTION(POINT(0 0),POINT(2 2))'),
ST_GeomFromText('MULTIPOINT((0 0),(3 3))')) UNION ALL
SELECT 'line_line_partial_overlap',
ST_Overlaps(ST_GeomFromText('LINESTRING(0 0,4 4)'),
ST_GeomFromText('LINESTRING(2 2,6 6)')) UNION ALL
SELECT 'line_line_endpoint',
ST_Overlaps(ST_GeomFromText('LINESTRING(0 0,2 2)'),
ST_GeomFromText('LINESTRING(2 2,4 4)')) UNION ALL
SELECT 'line_line_cross_mid',
ST_Overlaps(ST_GeomFromText('LINESTRING(0 0,2 2)'),
ST_GeomFromText('LINESTRING(0 2,2 0)')) UNION ALL
SELECT 'line_line_contains',
ST_Overlaps(ST_GeomFromText('LINESTRING(0 0,4 4)'),
ST_GeomFromText('LINESTRING(1 1,3 3)')) UNION ALL
SELECT 'line_line_same',
ST_Overlaps(ST_GeomFromText('LINESTRING(0 0,4 4)'),
ST_GeomFromText('LINESTRING(4 4,0 0)')) UNION ALL
SELECT 'line_polygon',
ST_Overlaps(ST_GeomFromText('LINESTRING(0 0,4 0)'),
ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))')) UNION ALL
SELECT 'polygon_polygon_overlap',
ST_Overlaps(ST_GeomFromText('POLYGON((0 0,3 0,3 3,0 3,0 0))'),
ST_GeomFromText('POLYGON((2 2,5 2,5 5,2 5,2 2))')) UNION ALL
SELECT 'polygon_polygon_edge_touch',
ST_Overlaps(ST_GeomFromText('POLYGON((0 0,2 0,2 2,0 2,0 0))'),
ST_GeomFromText('POLYGON((2 0,4 0,4 2,2 2,2 0))')) UNION ALL
SELECT 'polygon_polygon_contains',
ST_Overlaps(ST_GeomFromText('POLYGON((0 0,5 0,5 5,0 5,0 0))'),
ST_GeomFromText('POLYGON((1 1,2 1,2 2,1 2,1 1))')) UNION ALL
SELECT 'polygon_polygon_same',
ST_Overlaps(ST_GeomFromText('POLYGON((0 0,5 0,5 5,0 5,0 0))'),
ST_GeomFromText('POLYGON((0 0,0 5,5 5,5 0,0 0))')) UNION ALL
SELECT 'multipolygon_partial',
ST_Overlaps(ST_GeomFromText('MULTIPOLYGON(((0 0,3 0,3 3,0 3,0 0)))'),
ST_GeomFromText('MULTIPOLYGON(((2 2,5 2,5 5,2 5,2 2)))')) UNION ALL
SELECT 'collection_line_overlap',
ST_Overlaps(ST_GeomFromText('GEOMETRYCOLLECTION(LINESTRING(0 0,4 4),POINT(9 9))'),
ST_GeomFromText('LINESTRING(2 2,6 6)'));
SQL
)"

expect_error \
    "overlaps invalid data" \
    3037 \
    22023 \
    "Invalid GIS data provided to function st_overlaps" \
    "SELECT ST_Overlaps(X'010203', Point(1,1));"

expect_error \
    "overlaps parameter count" \
    1582 \
    42000 \
    "Incorrect parameter count" \
    "SELECT ST_Overlaps();"

expect_error \
    "overlaps srid mismatch" \
    3033 \
    HY000 \
    "Binary geometry function st_overlaps given two geometries of different srids" \
    "SELECT ST_Overlaps(ST_PointFromGeoHash('mh2n0p0581',4326), Point(1,1));"

printf '%s\n' "mysql_baseline_spatial_overlaps_function_expectations: ok"
