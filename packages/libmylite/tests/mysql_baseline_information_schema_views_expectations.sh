#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_information_schema_views_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_information_schema_views_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw --skip-column-names "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw "$@"
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

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE}; CREATE TABLE ${DATABASE}.t(id INT);" >/dev/null

expected_views_columns="TABLE_CATALOG	TABLE_SCHEMA	TABLE_NAME	VIEW_DEFINITION	CHECK_OPTION	IS_UPDATABLE	DEFINER	SECURITY_TYPE	CHARACTER_SET_CLIENT	COLLATION_CONNECTION"
count=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.VIEWS WHERE TABLE_SCHEMA = '${DATABASE}';")
expect_value "empty views count" "0" "$count"

case_count=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.views WHERE TABLE_SCHEMA = '${DATABASE}';")
expect_value "case-insensitive table name count" "0" "$case_count"

status=$(run_mysql \
    "SELECT * FROM INFORMATION_SCHEMA.VIEWS WHERE TABLE_SCHEMA = '${DATABASE}'; "\
"SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "views status" "0	-1" "$status"

run_mysql "CREATE VIEW ${DATABASE}.v AS SELECT id FROM ${DATABASE}.t;" >/dev/null
views_output=$(run_mysql_with_headers \
    "SELECT * FROM INFORMATION_SCHEMA.VIEWS WHERE TABLE_SCHEMA = '${DATABASE}' ORDER BY TABLE_NAME;")
expect_value "views headers" "$expected_views_columns" "$(printf '%s\n' "$views_output" | sed -n '1p')"
alias_output=$(run_mysql_with_headers \
    "SELECT v.TABLE_NAME FROM INFORMATION_SCHEMA.VIEWS AS v "\
"WHERE v.TABLE_SCHEMA = '${DATABASE}' ORDER BY v.TABLE_NAME LIMIT 1;")
expect_value "alias headers" "TABLE_NAME" "$(printf '%s\n' "$alias_output" | sed -n '1p')"

system_table_row=$(run_mysql \
    "SELECT TABLE_SCHEMA,TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS,DATA_LENGTH,AUTO_INCREMENT "\
"FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'VIEWS';")
expect_value "views system table row" \
    "information_schema	VIEWS	SYSTEM VIEW	NULL	10	NULL	0	0	NULL" \
    "$system_table_row"

expected_columns_metadata="VIEWS	TABLE_CATALOG	1	NULL	NO	varchar	64	192	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(64)	select
VIEWS	TABLE_SCHEMA	2	NULL	NO	varchar	64	192	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(64)	select
VIEWS	TABLE_NAME	3	NULL	NO	varchar	64	192	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(64)	select
VIEWS	VIEW_DEFINITION	4	NULL	YES	longtext	4294967295	4294967295	NULL	NULL	utf8mb3	utf8mb3_bin	longtext	select
VIEWS	CHECK_OPTION	5	NULL	YES	enum	8	24	NULL	NULL	utf8mb3	utf8mb3_bin	enum('NONE','LOCAL','CASCADED')	select
VIEWS	IS_UPDATABLE	6	NULL	YES	enum	3	9	NULL	NULL	utf8mb3	utf8mb3_bin	enum('NO','YES')	select
VIEWS	DEFINER	7	NULL	YES	varchar	288	864	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(288)	select
VIEWS	SECURITY_TYPE	8	NULL	YES	varchar	7	21	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(7)	select
VIEWS	CHARACTER_SET_CLIENT	9	NULL	NO	varchar	64	192	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(64)	select
VIEWS	COLLATION_CONNECTION	10	NULL	NO	varchar	64	192	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(64)	select"
columns_metadata=$(run_mysql \
    "SELECT TABLE_NAME,COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,DATA_TYPE, "\
"CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,NUMERIC_PRECISION,NUMERIC_SCALE, "\
"CHARACTER_SET_NAME,COLLATION_NAME,COLUMN_TYPE,PRIVILEGES "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'VIEWS' "\
"ORDER BY ORDINAL_POSITION;")
expect_value "views columns metadata" "$expected_columns_metadata" "$columns_metadata"

printf '%s\n' "mysql_baseline_information_schema_views_expectations: ok"
