#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_spatial_validity_functions_expectations: $1" >&2
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

validity_expected=$(cat <<EXPECTED
NULL	NULL
1	POINT(1 2)
1	GEOMETRYCOLLECTION EMPTY
1	LINESTRING(0 0,1 1)
0	NULL
0	NULL
1	LINESTRING(0 0,0 0,1 1)
1	LINESTRING(0 0,1 1,0 0)
1	MULTIPOINT((0 0),(0 0))
1	POLYGON((0 0,4 0,4 4,0 4,0 0))
0	NULL
0	NULL
1	POLYGON((0 0,4 0,4 4,0 4,0 0),(1 1,2 1,2 2,1 1))
0	NULL
0	NULL
0	NULL
1	GEOMETRYCOLLECTION(POINT(1 2),LINESTRING(0 0,1 1))
0	NULL
EXPECTED
)
expect_output \
    "validity and validate results" \
    "$validity_expected" \
    "SELECT ST_IsValid(NULL), ST_AsText(ST_Validate(NULL)) UNION ALL "\
"SELECT ST_IsValid(Point(1,2)), ST_AsText(ST_Validate(Point(1,2))) UNION ALL "\
"SELECT ST_IsValid(ST_GeomFromText('GEOMETRYCOLLECTION EMPTY')), "\
"ST_AsText(ST_Validate(ST_GeomFromText('GEOMETRYCOLLECTION EMPTY'))) UNION ALL "\
"SELECT ST_IsValid(ST_GeomFromText('LINESTRING(0 0,1 1)')), "\
"ST_AsText(ST_Validate(ST_GeomFromText('LINESTRING(0 0,1 1)'))) UNION ALL "\
"SELECT ST_IsValid(ST_GeomFromText('LINESTRING(0 0,0 0)')), "\
"ST_AsText(ST_Validate(ST_GeomFromText('LINESTRING(0 0,0 0)'))) UNION ALL "\
"SELECT ST_IsValid(ST_GeomFromText('LINESTRING(0 0,-0.00 0,0.0 0)')), "\
"ST_AsText(ST_Validate(ST_GeomFromText('LINESTRING(0 0,-0.00 0,0.0 0)'))) UNION ALL "\
"SELECT ST_IsValid(ST_GeomFromText('LINESTRING(0 0,0 0,1 1)')), "\
"ST_AsText(ST_Validate(ST_GeomFromText('LINESTRING(0 0,0 0,1 1)'))) UNION ALL "\
"SELECT ST_IsValid(ST_GeomFromText('LINESTRING(0 0,1 1,0 0)')), "\
"ST_AsText(ST_Validate(ST_GeomFromText('LINESTRING(0 0,1 1,0 0)'))) UNION ALL "\
"SELECT ST_IsValid(ST_GeomFromText('MULTIPOINT((0 0),(0 0))')), "\
"ST_AsText(ST_Validate(ST_GeomFromText('MULTIPOINT((0 0),(0 0))'))) UNION ALL "\
"SELECT ST_IsValid(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))')), "\
"ST_AsText(ST_Validate(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0))'))) UNION ALL "\
"SELECT ST_IsValid(ST_GeomFromText('POLYGON((0 0,0 0,0 0,0 0,0 0))')), "\
"ST_AsText(ST_Validate(ST_GeomFromText('POLYGON((0 0,0 0,0 0,0 0,0 0))'))) UNION ALL "\
"SELECT ST_IsValid(ST_GeomFromText('POLYGON((0 0,2 2,2 0,0 2,0 0))')), "\
"ST_AsText(ST_Validate(ST_GeomFromText('POLYGON((0 0,2 2,2 0,0 2,0 0))'))) UNION ALL "\
"SELECT ST_IsValid(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0),(1 1,2 1,2 2,1 1))')), "\
"ST_AsText(ST_Validate(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0),(1 1,2 1,2 2,1 1))'))) UNION ALL "\
"SELECT ST_IsValid(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0),(5 5,6 5,6 6,5 5))')), "\
"ST_AsText(ST_Validate(ST_GeomFromText('POLYGON((0 0,4 0,4 4,0 4,0 0),(5 5,6 5,6 6,5 5))'))) UNION ALL "\
"SELECT ST_IsValid(ST_GeomFromText('MULTILINESTRING((0 0,1 1),(2 2,2 2))')), "\
"ST_AsText(ST_Validate(ST_GeomFromText('MULTILINESTRING((0 0,1 1),(2 2,2 2))'))) UNION ALL "\
"SELECT ST_IsValid(ST_GeomFromText('MULTIPOLYGON(((0 0,2 0,2 2,0 2,0 0)),((1 1,3 1,3 3,1 3,1 1)))')), "\
"ST_AsText(ST_Validate(ST_GeomFromText('MULTIPOLYGON(((0 0,2 0,2 2,0 2,0 0)),((1 1,3 1,3 3,1 3,1 1)))'))) UNION ALL "\
"SELECT ST_IsValid(ST_GeomFromText('GEOMETRYCOLLECTION(POINT(1 2),LINESTRING(0 0,1 1))')), "\
"ST_AsText(ST_Validate(ST_GeomFromText('GEOMETRYCOLLECTION(POINT(1 2),LINESTRING(0 0,1 1))'))) UNION ALL "\
"SELECT ST_IsValid(ST_GeomFromText('GEOMETRYCOLLECTION(POINT(1 2),LINESTRING(0 0,-0.00 0,0.0 0))')), "\
"ST_AsText(ST_Validate(ST_GeomFromText('GEOMETRYCOLLECTION(POINT(1 2),LINESTRING(0 0,-0.00 0,0.0 0))')));"

expect_error \
    "one-point linestring constructor rejection" \
    3037 \
    22023 \
    "Invalid GIS data provided to function st_geomfromtext" \
    "SELECT ST_IsValid(ST_GeomFromText('LINESTRING(0 0)'));"

expect_error \
    "isvalid parameter count" \
    1582 \
    42000 \
    "Incorrect parameter count" \
    "SELECT ST_IsValid();"

printf '%s\n' "mysql_baseline_spatial_validity_functions_expectations: ok"
