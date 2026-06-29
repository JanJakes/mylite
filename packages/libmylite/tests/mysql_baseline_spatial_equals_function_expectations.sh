#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_spatial_equals_function_expectations: $1" >&2
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

equals_expected=$(cat <<EXPECTED
null_left	NULL
empty_empty	1
empty_point	0
point_same	1
point_far	0
line_same	1
line_reversed	1
line_sub	0
polygon_same	1
polygon_reversed	1
polygon_inside	0
multipoint_order	1
multiline_order	1
multipolygon_order	1
collection_order	1
EXPECTED
)
expect_output \
    "equals results" \
    "$equals_expected" \
    "SELECT 'null_left', ST_Equals(NULL, Point(1,1)) UNION ALL "\
"SELECT 'empty_empty', ST_Equals(ST_GeomFromText('GEOMETRYCOLLECTION EMPTY'), "\
"ST_GeomFromText('GEOMETRYCOLLECTION EMPTY')) UNION ALL "\
"SELECT 'empty_point', ST_Equals(ST_GeomFromText('GEOMETRYCOLLECTION EMPTY'), Point(1,1)) "\
"UNION ALL SELECT 'point_same', ST_Equals(Point(1,1), Point(1,1)) UNION ALL "\
"SELECT 'point_far', ST_Equals(Point(1,1), Point(2,2)) UNION ALL "\
"SELECT 'line_same', ST_Equals(ST_GeomFromText('LINESTRING(0 0,4 4)'), "\
"ST_GeomFromText('LINESTRING(0 0,4 4)')) UNION ALL "\
"SELECT 'line_reversed', ST_Equals(ST_GeomFromText('LINESTRING(0 0,4 4)'), "\
"ST_GeomFromText('LINESTRING(4 4,0 0)')) UNION ALL "\
"SELECT 'line_sub', ST_Equals(ST_GeomFromText('LINESTRING(0 0,4 4)'), "\
"ST_GeomFromText('LINESTRING(1 1,3 3)')) UNION ALL "\
"SELECT 'polygon_same', ST_Equals(ST_GeomFromText('POLYGON((0 0,5 0,5 5,0 5,0 0))'), "\
"ST_GeomFromText('POLYGON((0 0,5 0,5 5,0 5,0 0))')) UNION ALL "\
"SELECT 'polygon_reversed', "\
"ST_Equals(ST_GeomFromText('POLYGON((0 0,5 0,5 5,0 5,0 0))'), "\
"ST_GeomFromText('POLYGON((0 0,0 5,5 5,5 0,0 0))')) UNION ALL "\
"SELECT 'polygon_inside', "\
"ST_Equals(ST_GeomFromText('POLYGON((0 0,5 0,5 5,0 5,0 0))'), "\
"ST_GeomFromText('POLYGON((1 1,2 1,2 2,1 2,1 1))')) UNION ALL "\
"SELECT 'multipoint_order', ST_Equals(ST_GeomFromText('MULTIPOINT((1 1),(2 2))'), "\
"ST_GeomFromText('MULTIPOINT((2 2),(1 1))')) UNION ALL "\
"SELECT 'multiline_order', "\
"ST_Equals(ST_GeomFromText('MULTILINESTRING((0 0,2 2),(5 5,6 6))'), "\
"ST_GeomFromText('MULTILINESTRING((5 5,6 6),(0 0,2 2))')) UNION ALL "\
"SELECT 'multipolygon_order', "\
"ST_Equals(ST_GeomFromText('MULTIPOLYGON(((0 0,2 0,2 2,0 2,0 0)), "\
"((5 5,6 5,6 6,5 6,5 5)))'), "\
"ST_GeomFromText('MULTIPOLYGON(((5 5,6 5,6 6,5 6,5 5)), "\
"((0 0,2 0,2 2,0 2,0 0)))')) UNION ALL "\
"SELECT 'collection_order', "\
"ST_Equals(ST_GeomFromText('GEOMETRYCOLLECTION(POINT(1 1),LINESTRING(5 5,6 6))'), "\
"ST_GeomFromText('GEOMETRYCOLLECTION(LINESTRING(5 5,6 6),POINT(1 1))'));"

expect_error \
    "equals invalid data" \
    3037 \
    22023 \
    "Invalid GIS data provided to function st_equals" \
    "SELECT ST_Equals(X'010203', Point(1,1));"

expect_error \
    "equals parameter count" \
    1582 \
    42000 \
    "Incorrect parameter count" \
    "SELECT ST_Equals();"

expect_error \
    "equals srid mismatch" \
    3033 \
    HY000 \
    "Binary geometry function st_equals given two geometries of different srids" \
    "SELECT ST_Equals(ST_PointFromGeoHash('mh2n0p0581',4326), Point(1,1));"

printf '%s\n' "mysql_baseline_spatial_equals_function_expectations: ok"
