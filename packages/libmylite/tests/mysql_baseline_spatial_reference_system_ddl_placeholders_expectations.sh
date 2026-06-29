#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_srs_ddl_placeholders_$$"
SRS_ID=2004326

fail() {
    printf '%s\n' "mysql_baseline_spatial_reference_system_ddl_placeholders: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
}

expect_value() {
    label=$1
    expected=$2
    actual=$3

    if [ "$actual" != "$expected" ]; then
        fail "$label: expected [$expected], got [$actual]"
    fi
}

cleanup() {
    run_mysql "DROP SPATIAL REFERENCE SYSTEM IF EXISTS ${SRS_ID}; "\
"DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE};" >/dev/null

run_mysql \
    "CREATE SPATIAL REFERENCE SYSTEM ${SRS_ID} "\
"NAME 'Copy of WGS 84' "\
"ORGANIZATION 'EPSG' IDENTIFIED BY ${SRS_ID} "\
"DEFINITION 'GEOGCS[\"WGS 84\",DATUM[\"World Geodetic System 1984\","\
"SPHEROID[\"WGS 84\",6378137,298.257223563,AUTHORITY[\"EPSG\",\"7030\"]],"\
"AUTHORITY[\"EPSG\",\"6326\"]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],"\
"UNIT[\"degree\",0.017453292519943278,AUTHORITY[\"EPSG\",\"9122\"]],"\
"AXIS[\"Lat\",NORTH],AXIS[\"Lon\",EAST],AUTHORITY[\"EPSG\",\"4326\"]]';" \
    "$DATABASE" >/dev/null

created_row=$(run_mysql \
    "SELECT SRS_NAME,SRS_ID,ORGANIZATION,ORGANIZATION_COORDSYS_ID "\
"FROM INFORMATION_SCHEMA.ST_SPATIAL_REFERENCE_SYSTEMS WHERE SRS_ID = ${SRS_ID};" \
    "$DATABASE")
expect_value "created SRS row" "Copy of WGS 84	${SRS_ID}	EPSG	${SRS_ID}" "$created_row"

run_mysql "DROP SPATIAL REFERENCE SYSTEM ${SRS_ID};" "$DATABASE" >/dev/null
remaining=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.ST_SPATIAL_REFERENCE_SYSTEMS "\
"WHERE SRS_ID = ${SRS_ID};" \
    "$DATABASE")
expect_value "dropped SRS row" "0" "$remaining"

printf '%s\n' "mysql_baseline_spatial_reference_system_ddl_placeholders: ok"
