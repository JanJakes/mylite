#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_information_schema_st_spatial_reference_systems_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names "$@"
}

expect_value() {
    label=$1
    expected=$2
    actual=$3

    if [ "$actual" != "$expected" ]; then
        fail "$label: expected [$expected], got [$actual]"
    fi
}

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

table_kind=$(run_mysql \
    "SHOW FULL TABLES FROM INFORMATION_SCHEMA WHERE Tables_in_INFORMATION_SCHEMA = "\
"'ST_SPATIAL_REFERENCE_SYSTEMS';")
expect_value "st spatial reference systems table kind" \
    "ST_SPATIAL_REFERENCE_SYSTEMS	SYSTEM VIEW" \
    "$table_kind"

system_table_row=$(run_mysql \
    "SELECT TABLE_SCHEMA,TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS, "\
"DATA_LENGTH,AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES "\
"WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME = 'ST_SPATIAL_REFERENCE_SYSTEMS';")
expect_value "st spatial reference systems system table row" \
    "information_schema	ST_SPATIAL_REFERENCE_SYSTEMS	SYSTEM VIEW	NULL	10	NULL	0	0	NULL" \
    "$system_table_row"

columns_metadata=$(run_mysql \
    "SELECT TABLE_NAME,COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,DATA_TYPE, "\
"CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,NUMERIC_PRECISION,NUMERIC_SCALE, "\
"DATETIME_PRECISION,CHARACTER_SET_NAME,COLLATION_NAME,COLUMN_TYPE,PRIVILEGES "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'ST_SPATIAL_REFERENCE_SYSTEMS' "\
"ORDER BY ORDINAL_POSITION;")
expected_columns_metadata="ST_SPATIAL_REFERENCE_SYSTEMS	SRS_NAME	1	NULL	NO	varchar	80	240	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(80)	select
ST_SPATIAL_REFERENCE_SYSTEMS	SRS_ID	2	NULL	NO	int	NULL	NULL	10	0	NULL	NULL	NULL	int unsigned	select
ST_SPATIAL_REFERENCE_SYSTEMS	ORGANIZATION	3	NULL	YES	varchar	256	768	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(256)	select
ST_SPATIAL_REFERENCE_SYSTEMS	ORGANIZATION_COORDSYS_ID	4	NULL	YES	int	NULL	NULL	10	0	NULL	NULL	NULL	int unsigned	select
ST_SPATIAL_REFERENCE_SYSTEMS	DEFINITION	5	NULL	NO	varchar	4096	12288	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(4096)	select
ST_SPATIAL_REFERENCE_SYSTEMS	DESCRIPTION	6	NULL	YES	varchar	2048	6144	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(2048)	select"
expect_value "st spatial reference systems columns metadata" \
    "$expected_columns_metadata" \
    "$columns_metadata"

srs_count=$(run_mysql "SELECT COUNT(*) FROM INFORMATION_SCHEMA.ST_SPATIAL_REFERENCE_SYSTEMS;")
expect_value "st spatial reference systems row count" "5238" "$srs_count"

srs_categories=$(run_mysql \
    "SELECT COUNT(*), CASE LEFT(DEFINITION,6) "\
"WHEN 'PROJCS' THEN 'Projected' WHEN 'GEOGCS' THEN 'Geographic' ELSE 'Other' END AS SRS_TYPE "\
"FROM INFORMATION_SCHEMA.ST_SPATIAL_REFERENCE_SYSTEMS "\
"GROUP BY SRS_TYPE ORDER BY SRS_TYPE;")
expect_value "st spatial reference systems category counts" \
    "545	Geographic
1	Other
4692	Projected" \
    "$srs_categories"

representative_rows=$(run_mysql \
    "SELECT SRS_NAME,SRS_ID,ORGANIZATION,ORGANIZATION_COORDSYS_ID,LEFT(DEFINITION,6), "\
"CHAR_LENGTH(DEFINITION),DESCRIPTION "\
"FROM INFORMATION_SCHEMA.ST_SPATIAL_REFERENCE_SYSTEMS "\
"WHERE SRS_ID IN (0, 4326) ORDER BY SRS_ID;")
expect_value "st spatial reference systems representative rows" \
    "	0	NULL	NULL		0	NULL
WGS 84	4326	EPSG	4326	GEOGCS	311	NULL" \
    "$representative_rows"

id_range=$(run_mysql \
    "SELECT MIN(SRS_ID),MAX(SRS_ID),COUNT(DISTINCT SRS_ID),COUNT(DISTINCT SRS_NAME) "\
"FROM INFORMATION_SCHEMA.ST_SPATIAL_REFERENCE_SYSTEMS;")
expect_value "st spatial reference systems id range" "0	32766	5238	5238" "$id_range"

use_row=$(run_mysql \
    "USE information_schema; "\
"SELECT SRS_NAME,SRS_ID FROM ST_SPATIAL_REFERENCE_SYSTEMS WHERE SRS_ID = 4326;")
expect_value "unqualified st spatial reference systems row" "WGS 84	4326" "$use_row"

status=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.ST_SPATIAL_REFERENCE_SYSTEMS; "\
"SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "st spatial reference systems warning and row count status" "0	-1" "$status"

printf '%s\n' "mysql_baseline_information_schema_st_spatial_reference_systems_expectations: ok"
