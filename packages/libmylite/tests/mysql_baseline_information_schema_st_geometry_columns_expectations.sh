#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
PROBE_SCHEMA="mylite_st_geometry_columns_probe"

fail() {
    printf '%s\n' "mysql_baseline_information_schema_st_geometry_columns_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw --skip-column-names "$@"
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
    run_mysql "DROP DATABASE IF EXISTS \`$PROBE_SCHEMA\`;" >/dev/null
}

trap cleanup EXIT INT TERM

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup

default_count=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.ST_GEOMETRY_COLUMNS "\
"WHERE TABLE_SCHEMA = 'information_schema';")
expect_value "information_schema spatial column count" "0" "$default_count"

system_table_row=$(run_mysql \
    "SELECT TABLE_SCHEMA,TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS,DATA_LENGTH,AUTO_INCREMENT "\
"FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME = 'ST_GEOMETRY_COLUMNS';")
expect_value "st geometry system table row" \
    "information_schema	ST_GEOMETRY_COLUMNS	SYSTEM VIEW	NULL	10	NULL	0	0	NULL" \
    "$system_table_row"

expected_columns_metadata="ST_GEOMETRY_COLUMNS	TABLE_CATALOG	1	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(64)	select
ST_GEOMETRY_COLUMNS	TABLE_SCHEMA	2	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(64)	select
ST_GEOMETRY_COLUMNS	TABLE_NAME	3	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(64)	select
ST_GEOMETRY_COLUMNS	COLUMN_NAME	4	NULL	YES	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_tolower_ci	varchar(64)	select
ST_GEOMETRY_COLUMNS	SRS_NAME	5	NULL	YES	varchar	80	240	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(80)	select
ST_GEOMETRY_COLUMNS	SRS_ID	6	NULL	YES	int	NULL	NULL	10	0	NULL	NULL	NULL	int unsigned	select
ST_GEOMETRY_COLUMNS	GEOMETRY_TYPE_NAME	7	NULL	YES	longtext	4294967295	4294967295	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	longtext	select"
columns_metadata=$(run_mysql \
    "SELECT TABLE_NAME,COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,DATA_TYPE, "\
"CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,NUMERIC_PRECISION,NUMERIC_SCALE, "\
"DATETIME_PRECISION,CHARACTER_SET_NAME,COLLATION_NAME,COLUMN_TYPE,PRIVILEGES "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'ST_GEOMETRY_COLUMNS' "\
"ORDER BY ORDINAL_POSITION;")
expect_value "st geometry columns metadata" "$expected_columns_metadata" "$columns_metadata"

run_mysql \
    "CREATE DATABASE \`$PROBE_SCHEMA\`; "\
"USE \`$PROBE_SCHEMA\`; "\
"CREATE TABLE spatial_meta ("\
"g GEOMETRY, p POINT NOT NULL, ls LINESTRING, poly POLYGON, mp MULTIPOINT, "\
"mls MULTILINESTRING, mpoly MULTIPOLYGON, gc GEOMETRYCOLLECTION);" >/dev/null

expected_rows="def	$PROBE_SCHEMA	spatial_meta	g	NULL	NULL	geometry
def	$PROBE_SCHEMA	spatial_meta	gc	NULL	NULL	geomcollection
def	$PROBE_SCHEMA	spatial_meta	ls	NULL	NULL	linestring
def	$PROBE_SCHEMA	spatial_meta	mls	NULL	NULL	multilinestring
def	$PROBE_SCHEMA	spatial_meta	mp	NULL	NULL	multipoint
def	$PROBE_SCHEMA	spatial_meta	mpoly	NULL	NULL	multipolygon
def	$PROBE_SCHEMA	spatial_meta	p	NULL	NULL	point
def	$PROBE_SCHEMA	spatial_meta	poly	NULL	NULL	polygon"
rows=$(run_mysql \
    "SELECT TABLE_CATALOG,TABLE_SCHEMA,TABLE_NAME,COLUMN_NAME,SRS_NAME,SRS_ID,GEOMETRY_TYPE_NAME "\
"FROM INFORMATION_SCHEMA.ST_GEOMETRY_COLUMNS "\
"WHERE TABLE_SCHEMA = '$PROBE_SCHEMA' AND TABLE_NAME = 'spatial_meta' "\
"ORDER BY TABLE_NAME,COLUMN_NAME;")
expect_value "st geometry descriptor rows" "$expected_rows" "$rows"

case_count=$(run_mysql \
    "SELECT COUNT(*) FROM information_schema.st_geometry_columns "\
"WHERE TABLE_SCHEMA = '$PROBE_SCHEMA';")
expect_value "case-insensitive st geometry count" "8" "$case_count"

alias_row=$(run_mysql \
    "SELECT s.GEOMETRY_TYPE_NAME FROM INFORMATION_SCHEMA.ST_GEOMETRY_COLUMNS AS s "\
"WHERE s.TABLE_SCHEMA = '$PROBE_SCHEMA' AND s.COLUMN_NAME = 'p';")
expect_value "st geometry alias row" "point" "$alias_row"

status=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.ST_GEOMETRY_COLUMNS "\
"WHERE TABLE_SCHEMA = '$PROBE_SCHEMA'; "\
"SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "st geometry warning and row count status" "0	-1" "$status"

printf '%s\n' "mysql_baseline_information_schema_st_geometry_columns_expectations: ok"
