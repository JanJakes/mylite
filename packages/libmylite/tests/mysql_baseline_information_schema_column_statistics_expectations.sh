#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_column_statistics_$$"

fail() {
    printf '%s\n' \
        "mysql_baseline_information_schema_column_statistics_expectations: $1" >&2
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

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup

baseline_count=$(run_mysql "SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMN_STATISTICS;")
expect_value "column statistics baseline count" "0" "$baseline_count"

object_count=$(run_mysql \
    "CREATE DATABASE ${DATABASE}; "\
"CREATE TABLE ${DATABASE}.t (id INT PRIMARY KEY, v INT); "\
"CREATE VIEW ${DATABASE}.v AS SELECT id FROM ${DATABASE}.t; "\
"CREATE TEMPORARY TABLE ${DATABASE}.tmp (id INT); "\
"SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMN_STATISTICS "\
"WHERE SCHEMA_NAME = '${DATABASE}';" | tail -n 1)
expect_value "column statistics user object count without histograms" "0" "$object_count"

status=$(run_mysql \
    "SELECT * FROM INFORMATION_SCHEMA.COLUMN_STATISTICS "\
"WHERE SCHEMA_NAME = '${DATABASE}'; "\
"SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "column statistics status" "0	-1" "$status"

system_table_rows=$(run_mysql \
    "SELECT TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS,DATA_LENGTH "\
"FROM INFORMATION_SCHEMA.TABLES "\
"WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME = 'COLUMN_STATISTICS';")
expected_system_table_rows="COLUMN_STATISTICS	SYSTEM VIEW	NULL	10	NULL	0	0"
expect_value "column statistics system table row" \
    "$expected_system_table_rows" \
    "$system_table_rows"

columns_metadata=$(run_mysql \
    "SELECT COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,DATA_TYPE, "\
"CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,NUMERIC_PRECISION,NUMERIC_SCALE, "\
"DATETIME_PRECISION,CHARACTER_SET_NAME,COLLATION_NAME,COLUMN_TYPE,PRIVILEGES "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME = 'COLUMN_STATISTICS' ORDER BY ORDINAL_POSITION;")
expected_columns_metadata="SCHEMA_NAME	1	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(64)	select
TABLE_NAME	2	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(64)	select
COLUMN_NAME	3	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_tolower_ci	varchar(64)	select
HISTOGRAM	4	NULL	NO	json	NULL	NULL	NULL	NULL	NULL	NULL	NULL	json	select"
expect_value "column statistics columns metadata" \
    "$expected_columns_metadata" \
    "$columns_metadata"

run_mysql "ANALYZE TABLE ${DATABASE}.t UPDATE HISTOGRAM ON v WITH 2 BUCKETS;" >/dev/null
histogram_probe=$(run_mysql \
    "SELECT SCHEMA_NAME,TABLE_NAME,COLUMN_NAME,JSON_TYPE(HISTOGRAM), "\
"JSON_EXTRACT(HISTOGRAM, '$.\"number-of-buckets-specified\"') "\
"FROM INFORMATION_SCHEMA.COLUMN_STATISTICS "\
"WHERE SCHEMA_NAME = '${DATABASE}' ORDER BY TABLE_NAME,COLUMN_NAME;")
expected_histogram_probe="${DATABASE}	t	v	OBJECT	2"
expect_value "column statistics histogram future work probe" \
    "$expected_histogram_probe" \
    "$histogram_probe"

printf '%s\n' \
    "mysql_baseline_information_schema_column_statistics_expectations: ok"
