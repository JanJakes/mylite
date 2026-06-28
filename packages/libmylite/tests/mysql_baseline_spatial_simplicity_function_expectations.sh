#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_spatial_simplicity_function_expectations: $1" >&2
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

simplicity_expected=$(cat <<EXPECTED
NULL	NULL
point	1
emptygc	1
line_simple	1
line_closed_triangle	1
line_repeat_mid	0
line_self_cross	0
line_duplicate_consecutive	0
line_collinear_backtrack	0
multipoint_unique	1
multipoint_dup	0
multiline_disjoint	1
multiline_endpoint_touch	1
multiline_endpoint_to_interior	0
multiline_interior_cross	0
multiline_interior_overlap	0
multiline_same_reversed	0
multiline_closed_touch_closed	0
polygon_valid	1
polygon_invalid_self	1
multipolygon_overlap	1
collection_simple	1
collection_nonsimple_member	0
collection_crossing_lines	0
collection_duplicate_points	0
collection_line_point_endpoint	1
collection_line_point_interior	0
collection_polygon_point_boundary	1
collection_polygon_point_interior	0
collection_polygon_line_outside	1
collection_polygon_line_cross	0
EXPECTED
)
expect_output \
    "simplicity results" \
    "$simplicity_expected" \
    "SELECT 'NULL', ST_IsSimple(NULL) UNION ALL "\
"SELECT 'point', ST_IsSimple(Point(1,2)) UNION ALL "\
"SELECT 'emptygc', ST_IsSimple(ST_GeomFromText('GEOMETRYCOLLECTION EMPTY')) UNION ALL "\
"SELECT 'line_simple', ST_IsSimple(ST_GeomFromText('LINESTRING(0 0,1 1)')) UNION ALL "\
"SELECT 'line_closed_triangle', ST_IsSimple(ST_GeomFromText('LINESTRING(0 0,1 0,0 1,0 0)')) UNION ALL "\
"SELECT 'line_repeat_mid', ST_IsSimple(ST_GeomFromText('LINESTRING(0 0,1 1,0 0)')) UNION ALL "\
"SELECT 'line_self_cross', ST_IsSimple(ST_GeomFromText('LINESTRING(0 0,2 2,0 2,2 0)')) UNION ALL "\
"SELECT 'line_duplicate_consecutive', ST_IsSimple(ST_GeomFromText('LINESTRING(0 0,0 0,1 1)')) UNION ALL "\
"SELECT 'line_collinear_backtrack', ST_IsSimple(ST_GeomFromText('LINESTRING(0 0,2 0,1 0)')) UNION ALL "\
"SELECT 'multipoint_unique', ST_IsSimple(ST_GeomFromText('MULTIPOINT((0 0),(1 1))')) UNION ALL "\
"SELECT 'multipoint_dup', ST_IsSimple(ST_GeomFromText('MULTIPOINT((0 0),(0 0))')) UNION ALL "\
"SELECT 'multiline_disjoint', ST_IsSimple(ST_GeomFromText('MULTILINESTRING((0 0,1 1),(2 2,3 3))')) UNION ALL "\
"SELECT 'multiline_endpoint_touch', ST_IsSimple(ST_GeomFromText('MULTILINESTRING((0 0,1 1),(1 1,2 2))')) UNION ALL "\
"SELECT 'multiline_endpoint_to_interior', ST_IsSimple(ST_GeomFromText('MULTILINESTRING((0 0,2 0),(1 0,1 1))')) UNION ALL "\
"SELECT 'multiline_interior_cross', ST_IsSimple(ST_GeomFromText('MULTILINESTRING((0 0,2 2),(0 2,2 0))')) UNION ALL "\
"SELECT 'multiline_interior_overlap', ST_IsSimple(ST_GeomFromText('MULTILINESTRING((0 0,2 0),(1 0,3 0))')) UNION ALL "\
"SELECT 'multiline_same_reversed', ST_IsSimple(ST_GeomFromText('MULTILINESTRING((0 0,1 1),(1 1,0 0))')) UNION ALL "\
"SELECT 'multiline_closed_touch_closed', ST_IsSimple(ST_GeomFromText('MULTILINESTRING((0 0,1 0,0 1,0 0),(0 0,-1 0,0 -1,0 0))')) UNION ALL "\
"SELECT 'polygon_valid', ST_IsSimple(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))')) UNION ALL "\
"SELECT 'polygon_invalid_self', ST_IsSimple(ST_GeomFromText('POLYGON((0 0,2 2,2 0,0 2,0 0))')) UNION ALL "\
"SELECT 'multipolygon_overlap', ST_IsSimple(ST_GeomFromText('MULTIPOLYGON(((0 0,2 0,2 2,0 2,0 0)),((1 1,3 1,3 3,1 3,1 1)))')) UNION ALL "\
"SELECT 'collection_simple', ST_IsSimple(ST_GeomFromText('GEOMETRYCOLLECTION(POINT(1 2),LINESTRING(0 0,1 1))')) UNION ALL "\
"SELECT 'collection_nonsimple_member', ST_IsSimple(ST_GeomFromText('GEOMETRYCOLLECTION(POINT(1 2),MULTIPOINT((0 0),(0 0)))')) UNION ALL "\
"SELECT 'collection_crossing_lines', ST_IsSimple(ST_GeomFromText('GEOMETRYCOLLECTION(LINESTRING(0 0,2 2),LINESTRING(0 2,2 0))')) UNION ALL "\
"SELECT 'collection_duplicate_points', ST_IsSimple(ST_GeomFromText('GEOMETRYCOLLECTION(POINT(0 0),POINT(0 0))')) UNION ALL "\
"SELECT 'collection_line_point_endpoint', ST_IsSimple(ST_GeomFromText('GEOMETRYCOLLECTION(LINESTRING(0 0,2 0),POINT(0 0))')) UNION ALL "\
"SELECT 'collection_line_point_interior', ST_IsSimple(ST_GeomFromText('GEOMETRYCOLLECTION(LINESTRING(0 0,2 0),POINT(1 0))')) UNION ALL "\
"SELECT 'collection_polygon_point_boundary', ST_IsSimple(ST_GeomFromText('GEOMETRYCOLLECTION(POLYGON((0 0,4 0,4 4,0 4,0 0)),POINT(0 0))')) UNION ALL "\
"SELECT 'collection_polygon_point_interior', ST_IsSimple(ST_GeomFromText('GEOMETRYCOLLECTION(POLYGON((0 0,4 0,4 4,0 4,0 0)),POINT(1 1))')) UNION ALL "\
"SELECT 'collection_polygon_line_outside', ST_IsSimple(ST_GeomFromText('GEOMETRYCOLLECTION(POLYGON((0 0,4 0,4 4,0 4,0 0)),LINESTRING(5 5,6 6))')) UNION ALL "\
"SELECT 'collection_polygon_line_cross', ST_IsSimple(ST_GeomFromText('GEOMETRYCOLLECTION(POLYGON((0 0,4 0,4 4,0 4,0 0)),LINESTRING(-1 2,5 2))'));"

expect_error \
    "issimple invalid data" \
    3037 \
    22023 \
    "Invalid GIS data provided to function st_issimple" \
    "SELECT ST_IsSimple(X'010203');"

expect_error \
    "issimple parameter count" \
    1582 \
    42000 \
    "Incorrect parameter count" \
    "SELECT ST_IsSimple();"

printf '%s\n' "mysql_baseline_spatial_simplicity_function_expectations: ok"
