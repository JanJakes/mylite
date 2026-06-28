#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_spatial_latitude_longitude_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_spatial_latitude_longitude_functions_expectations: $1" >&2
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

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

scalar_expected=$(cat <<EXPECTED
45	90	-90	180	NULL	NULL
EXPECTED
)
expect_output \
    "scalar latitude longitude getters" \
    "$scalar_expected" \
    "SELECT ST_Latitude(ST_GeomFromGeoJSON('{\"type\":\"Point\",\"coordinates\":[90,45]}')), "\
"ST_Longitude(ST_GeomFromGeoJSON('{\"type\":\"Point\",\"coordinates\":[90,45]}')), "\
"ST_Latitude(ST_GeomFromGeoJSON('{\"type\":\"Point\",\"coordinates\":[180,-90]}')), "\
"ST_Longitude(ST_GeomFromGeoJSON('{\"type\":\"Point\",\"coordinates\":[180,-90]}')), "\
"ST_Latitude(NULL), ST_Longitude(NULL);"

cleanup
run_mysql "CREATE DATABASE ${DATABASE} DEFAULT CHARACTER SET utf8mb4 "\
"COLLATE utf8mb4_0900_ai_ci;" >/dev/null

table_expected=$(cat <<EXPECTED
1	45	90	POINT(45 90)	4326
2	-20	120	POINT(-20 120)	4326
3	NULL	NULL	NULL	NULL
EXPECTED
)
expect_output \
    "table-backed latitude longitude getters" \
    "$table_expected" \
    "CREATE TABLE points(id INT PRIMARY KEY, p POINT SRID 4326 NULL); "\
"INSERT INTO points VALUES "\
"(1, ST_GeomFromGeoJSON('{\"type\":\"Point\",\"coordinates\":[90,45]}')), "\
"(2, ST_GeomFromGeoJSON('{\"type\":\"Point\",\"coordinates\":[120,-20]}')), "\
"(3, NULL); "\
"SELECT id, ST_Latitude(p), ST_Longitude(p), ST_AsText(p), ST_SRID(p) "\
"FROM points ORDER BY id;" \
    "$DATABASE"

expect_error \
    "latitude rejects non-geographic point" \
    3726 \
    "22S00" \
    "Function st_latitude is only defined for geographic spatial reference systems" \
    "SELECT ST_Latitude(Point(1,2));"

expect_error \
    "longitude rejects non-geographic point" \
    3726 \
    "22S00" \
    "Function st_longitude is only defined for geographic spatial reference systems" \
    "SELECT ST_Longitude(Point(1,2));"

expect_error \
    "latitude rejects non-point" \
    3516 \
    "22S01" \
    "POINT value is a geometry of unexpected type LINESTRING in st_latitude" \
    "SELECT ST_Latitude(ST_GeomFromText('LINESTRING(0 0,1 1)'));"

expect_error \
    "longitude rejects non-point" \
    3516 \
    "22S01" \
    "POINT value is a geometry of unexpected type LINESTRING in st_longitude" \
    "SELECT ST_Longitude(ST_GeomFromText('LINESTRING(0 0,1 1)'));"

expect_error \
    "latitude rejects invalid GIS data" \
    3037 \
    "22023" \
    "Invalid GIS data provided to function st_latitude" \
    "SELECT ST_Latitude(X'00');"

expect_error \
    "longitude rejects invalid GIS data" \
    3037 \
    "22023" \
    "Invalid GIS data provided to function st_longitude" \
    "SELECT ST_Longitude(X'00');"

expect_error \
    "latitude rejects missing argument" \
    1582 \
    "42000" \
    "Incorrect parameter count in the call to native function 'ST_Latitude'" \
    "SELECT ST_Latitude();"

expect_error \
    "longitude rejects missing argument" \
    1582 \
    "42000" \
    "Incorrect parameter count in the call to native function 'ST_Longitude'" \
    "SELECT ST_Longitude();"

expect_error \
    "latitude unknown SRID rejected during construction" \
    3548 \
    "SR001" \
    "There's no spatial reference system with SRID 999999." \
    "SELECT ST_Latitude(ST_GeomFromGeoJSON('{\"type\":\"Point\",\"coordinates\":[90,45]}',1,999999));"

printf '%s\n' "mysql_baseline_spatial_latitude_longitude_functions_expectations: ok"
