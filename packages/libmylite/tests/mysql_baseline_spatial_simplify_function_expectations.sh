#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_spatial_simplify_function_expectations: $1" >&2
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

simplify_expected=$(cat <<EXPECTED
LINESTRING(0 0,0 1,1 1,2 3,3 3)
LINESTRING(0 0,3 3)
POINT(1 2)
MULTIPOINT((0 0),(1 1),(2 2))
LINESTRING(0 0,3 0)
LINESTRING(0 0,1 0.1,3 0)
POLYGON((2 2,0 2,0 0,2 0,2 2))
NULL
MULTILINESTRING((0 0,2 0),(0 0,0 2))
MULTIPOLYGON(((2 2,0 2,0 0,2 0,2 2)))
POLYGON((5 5,0 5,0 0,5 0,5 5))
GEOMETRYCOLLECTION(POINT(1 2),LINESTRING(0 0,2 0))
GEOMETRYCOLLECTION(LINESTRING(0 0,2 0))
NULL
NULL
NULL
POINT(1 2)
EXPECTED
)
expect_output \
    "simplify results" \
    "$simplify_expected" \
    "SELECT ST_AsText(ST_Simplify(ST_GeomFromText('LINESTRING(0 0,0 1,1 1,1 2,2 2,2 3,3 3)'), 0.5)) UNION ALL "\
"SELECT ST_AsText(ST_Simplify(ST_GeomFromText('LINESTRING(0 0,0 1,1 1,1 2,2 2,2 3,3 3)'), 1.0)) UNION ALL "\
"SELECT ST_AsText(ST_Simplify(Point(1,2), 1)) UNION ALL "\
"SELECT ST_AsText(ST_Simplify(ST_GeomFromText('MULTIPOINT((0 0),(1 1),(2 2))'), 1)) UNION ALL "\
"SELECT ST_AsText(ST_Simplify(ST_GeomFromText('LINESTRING(0 0,1 0.1,2 0,3 0)'), 0.2)) UNION ALL "\
"SELECT ST_AsText(ST_Simplify(ST_GeomFromText('LINESTRING(0 0,1 0.1,2 0,3 0)'), 0.05)) UNION ALL "\
"SELECT ST_AsText(ST_Simplify(ST_GeomFromText('POLYGON((0 0,1 0.1,2 0,2 2,0 2,0 0))'), 0.2)) UNION ALL "\
"SELECT ST_AsText(ST_Simplify(ST_GeomFromText('POLYGON((0 0,0.1 0,0.1 0.1,0 0.1,0 0))'), 1)) UNION ALL "\
"SELECT ST_AsText(ST_Simplify(ST_GeomFromText('MULTILINESTRING((0 0,1 0.1,2 0),(0 0,0 1,0 2))'), 0.2)) UNION ALL "\
"SELECT ST_AsText(ST_Simplify(ST_GeomFromText('MULTIPOLYGON(((0 0,1 0.1,2 0,2 2,0 2,0 0)))'), 0.2)) UNION ALL "\
"SELECT ST_AsText(ST_Simplify(ST_GeomFromText('POLYGON((0 0,5 0,5 5,0 5,0 0),(1 1,1.1 1,1.1 1.1,1 1.1,1 1))'), 1)) UNION ALL "\
"SELECT ST_AsText(ST_Simplify(ST_GeomFromText('GEOMETRYCOLLECTION(POINT(1 2),LINESTRING(0 0,1 0.1,2 0))'), 0.2)) UNION ALL "\
"SELECT ST_AsText(ST_Simplify(ST_GeomFromText('GEOMETRYCOLLECTION(POLYGON((0 0,0.1 0,0.1 0.1,0 0.1,0 0)),LINESTRING(0 0,1 0.1,2 0))'), 1)) UNION ALL "\
"SELECT ST_AsText(ST_Simplify(ST_GeomFromText('GEOMETRYCOLLECTION EMPTY'), 1)) UNION ALL "\
"SELECT ST_AsText(ST_Simplify(NULL, 0)) UNION ALL "\
"SELECT ST_AsText(ST_Simplify(X'010203', NULL)) UNION ALL "\
"SELECT ST_AsText(ST_Simplify(Point(1,2), '1abc'));"

expect_error \
    "simplify parameter count" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'ST_Simplify'" \
    "SELECT ST_Simplify();"

expect_error \
    "simplify nonpositive distance" \
    1210 \
    HY000 \
    "Incorrect arguments to st_simplify" \
    "SELECT ST_Simplify(Point(1,2), 0);"

expect_error \
    "simplify nonnumeric distance" \
    1210 \
    HY000 \
    "Incorrect arguments to st_simplify" \
    "SELECT ST_Simplify(Point(1,2), 'abc');"

expect_error \
    "simplify invalid geometry" \
    3037 \
    22023 \
    "Invalid GIS data provided to function st_simplify" \
    "SELECT ST_Simplify(X'010203', 1);"

expect_error \
    "simplify geographic srs" \
    3618 \
    22S00 \
    "st_simplify(POINT, ...) has not been implemented for geographic spatial reference systems." \
    "SELECT ST_AsText(ST_Simplify(ST_PointFromGeoHash('mh2n0p0581',4326), 1));"

printf '%s\n' "mysql_baseline_spatial_simplify_function_expectations: ok"
