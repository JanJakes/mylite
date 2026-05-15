#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_information_schema_view_table_usage_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_information_schema_view_table_usage_expectations: $1" >&2
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
run_mysql "CREATE DATABASE ${DATABASE}; CREATE TABLE ${DATABASE}.base(id INT);" >/dev/null

expected_columns="VIEW_CATALOG	VIEW_SCHEMA	VIEW_NAME	TABLE_CATALOG	TABLE_SCHEMA	TABLE_NAME"

count=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE WHERE VIEW_SCHEMA = '${DATABASE}';")
expect_value "empty view table usage count" "0" "$count"

case_count=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.view_table_usage WHERE VIEW_SCHEMA = '${DATABASE}';")
expect_value "case-insensitive table name count" "0" "$case_count"

status=$(run_mysql \
    "SELECT * FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE WHERE VIEW_SCHEMA = '${DATABASE}'; "\
"SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "view table usage status" "0	-1" "$status"

run_mysql "CREATE VIEW ${DATABASE}.v AS SELECT id FROM ${DATABASE}.base;" >/dev/null

headers=$(run_mysql_with_headers \
    "SELECT * FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE "\
"WHERE VIEW_SCHEMA = '${DATABASE}' ORDER BY VIEW_NAME,TABLE_NAME;")
expect_value "view table usage headers" "$expected_columns" "$(printf '%s\n' "$headers" | sed -n '1p')"

alias_output=$(run_mysql_with_headers \
    "SELECT u.VIEW_NAME FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE AS u "\
"WHERE u.VIEW_SCHEMA = '${DATABASE}' ORDER BY u.VIEW_NAME LIMIT 1;")
expect_value "alias headers" "VIEW_NAME" "$(printf '%s\n' "$alias_output" | sed -n '1p')"

dependency_row=$(run_mysql \
    "SELECT VIEW_CATALOG,VIEW_SCHEMA,VIEW_NAME,TABLE_CATALOG,TABLE_SCHEMA,TABLE_NAME "\
"FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE WHERE VIEW_SCHEMA = '${DATABASE}' "\
"ORDER BY VIEW_NAME,TABLE_NAME;")
expect_value "mysql real view dependency observation" \
    "def	${DATABASE}	v	def	${DATABASE}	base" \
    "$dependency_row"

system_table_row=$(run_mysql \
    "SELECT TABLE_SCHEMA,TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS,DATA_LENGTH,AUTO_INCREMENT "\
"FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME = 'VIEW_TABLE_USAGE';")
expect_value "view table usage system table row" \
    "information_schema	VIEW_TABLE_USAGE	SYSTEM VIEW	NULL	10	NULL	0	0	NULL" \
    "$system_table_row"

expected_columns_metadata="VIEW_TABLE_USAGE	VIEW_CATALOG	1	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(64)	select
VIEW_TABLE_USAGE	VIEW_SCHEMA	2	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(64)	select
VIEW_TABLE_USAGE	VIEW_NAME	3	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(64)	select
VIEW_TABLE_USAGE	TABLE_CATALOG	4	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(64)	select
VIEW_TABLE_USAGE	TABLE_SCHEMA	5	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(64)	select
VIEW_TABLE_USAGE	TABLE_NAME	6	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(64)	select"
columns_metadata=$(run_mysql \
    "SELECT TABLE_NAME,COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,DATA_TYPE, "\
"CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,NUMERIC_PRECISION,NUMERIC_SCALE, "\
"DATETIME_PRECISION,CHARACTER_SET_NAME,COLLATION_NAME,COLUMN_TYPE,PRIVILEGES "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'VIEW_TABLE_USAGE' "\
"ORDER BY ORDINAL_POSITION;")
expect_value "view table usage columns metadata" "$expected_columns_metadata" "$columns_metadata"

printf '%s\n' "mysql_baseline_information_schema_view_table_usage_expectations: ok"
