#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_spatial_disjoint_intersects_functions_expectations: $1" >&2
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
point_same	0	1
point_far	1	0
point_on_line	0	1
point_off_line	1	0
line_cross	0	1
line_disjoint	1	0
point_in_polygon	0	1
point_boundary_polygon	0	1
point_out_polygon	1	0
polygon_overlap	0	1
polygon_touch	0	1
collection_member	0	1
EXPECTED
)
expect_output \
    "relation results" \
    "$relation_expected" \
    "SELECT 'null_left', ST_Disjoint(NULL, Point(1,1)), ST_Intersects(NULL, Point(1,1)) "\
"UNION ALL "\
"SELECT 'empty_left', "\
"ST_Disjoint(ST_GeomFromText('GEOMETRYCOLLECTION EMPTY'), Point(1,1)), "\
"ST_Intersects(ST_GeomFromText('GEOMETRYCOLLECTION EMPTY'), Point(1,1)) UNION ALL "\
"SELECT 'point_same', ST_Disjoint(Point(1,1), Point(1,1)), "\
"ST_Intersects(Point(1,1), Point(1,1)) UNION ALL "\
"SELECT 'point_far', ST_Disjoint(Point(1,1), Point(2,2)), "\
"ST_Intersects(Point(1,1), Point(2,2)) UNION ALL "\
"SELECT 'point_on_line', ST_Disjoint(Point(1,1), ST_GeomFromText('LINESTRING(0 0,2 2)')), "\
"ST_Intersects(Point(1,1), ST_GeomFromText('LINESTRING(0 0,2 2)')) UNION ALL "\
"SELECT 'point_off_line', ST_Disjoint(Point(1,2), ST_GeomFromText('LINESTRING(0 0,2 2)')), "\
"ST_Intersects(Point(1,2), ST_GeomFromText('LINESTRING(0 0,2 2)')) UNION ALL "\
"SELECT 'line_cross', "\
"ST_Disjoint(ST_GeomFromText('LINESTRING(0 0,2 2)'), "\
"ST_GeomFromText('LINESTRING(0 2,2 0)')), "\
"ST_Intersects(ST_GeomFromText('LINESTRING(0 0,2 2)'), "\
"ST_GeomFromText('LINESTRING(0 2,2 0)')) UNION ALL "\
"SELECT 'line_disjoint', "\
"ST_Disjoint(ST_GeomFromText('LINESTRING(0 0,1 1)'), "\
"ST_GeomFromText('LINESTRING(2 2,3 3)')), "\
"ST_Intersects(ST_GeomFromText('LINESTRING(0 0,1 1)'), "\
"ST_GeomFromText('LINESTRING(2 2,3 3)')) UNION ALL "\
"SELECT 'point_in_polygon', ST_Disjoint(Point(1,1), "\
"ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))')), "\
"ST_Intersects(Point(1,1), ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))')) "\
"UNION ALL "\
"SELECT 'point_boundary_polygon', ST_Disjoint(Point(0,0), "\
"ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))')), "\
"ST_Intersects(Point(0,0), ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))')) "\
"UNION ALL "\
"SELECT 'point_out_polygon', ST_Disjoint(Point(5,5), "\
"ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))')), "\
"ST_Intersects(Point(5,5), ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))')) "\
"UNION ALL "\
"SELECT 'polygon_overlap', "\
"ST_Disjoint(ST_GeomFromText('POLYGON((0 0,3 0,3 3,0 3,0 0))'), "\
"ST_GeomFromText('POLYGON((2 2,5 2,5 5,2 5,2 2))')), "\
"ST_Intersects(ST_GeomFromText('POLYGON((0 0,3 0,3 3,0 3,0 0))'), "\
"ST_GeomFromText('POLYGON((2 2,5 2,5 5,2 5,2 2))')) UNION ALL "\
"SELECT 'polygon_touch', "\
"ST_Disjoint(ST_GeomFromText('POLYGON((0 0,2 0,2 2,0 2,0 0))'), "\
"ST_GeomFromText('POLYGON((2 0,4 0,4 2,2 2,2 0))')), "\
"ST_Intersects(ST_GeomFromText('POLYGON((0 0,2 0,2 2,0 2,0 0))'), "\
"ST_GeomFromText('POLYGON((2 0,4 0,4 2,2 2,2 0))')) UNION ALL "\
"SELECT 'collection_member', "\
"ST_Disjoint(ST_GeomFromText('GEOMETRYCOLLECTION(POINT(1 1),LINESTRING(5 5,6 6))'), "\
"Point(1,1)), "\
"ST_Intersects(ST_GeomFromText('GEOMETRYCOLLECTION(POINT(1 1),LINESTRING(5 5,6 6))'), "\
"Point(1,1));"

expect_error \
    "disjoint invalid data" \
    3037 \
    22023 \
    "Invalid GIS data provided to function st_disjoint" \
    "SELECT ST_Disjoint(X'010203', Point(1,1));"

expect_error \
    "intersects invalid data" \
    3037 \
    22023 \
    "Invalid GIS data provided to function st_intersects" \
    "SELECT ST_Intersects(X'010203', Point(1,1));"

expect_error \
    "disjoint parameter count" \
    1582 \
    42000 \
    "Incorrect parameter count" \
    "SELECT ST_Disjoint();"

expect_error \
    "intersects parameter count" \
    1582 \
    42000 \
    "Incorrect parameter count" \
    "SELECT ST_Intersects();"

printf '%s\n' "mysql_baseline_spatial_disjoint_intersects_functions_expectations: ok"
